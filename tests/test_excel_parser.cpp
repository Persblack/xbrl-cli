#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "excel_parser.h"
#include "utils.h"

#include <xlnt/xlnt.hpp>

#include <cmath>
#include <filesystem>

namespace fs = std::filesystem;

// ── Test Helper: Create test xlsx programmatically ───────────────────────────

static std::string getTestDataDir() {
    // Try common paths
    if (fs::exists("tests/testdata")) return "tests/testdata";
    if (fs::exists("../tests/testdata")) return "../tests/testdata";
    if (fs::exists("testdata")) return "testdata";
    // Create it
    fs::create_directories("tests/testdata");
    return "tests/testdata";
}

static std::string createValidTestXlsx() {
    std::string dir = getTestDataDir();
    std::string path = dir + "/test-bilanz-unit.xlsx";

    xlnt::workbook wb;

    // ── Sheet: Stammdaten ──
    auto ws = wb.active_sheet();
    ws.title("Stammdaten");
    ws.cell("A1").value("Feld");
    ws.cell("B1").value("Wert");
    ws.cell("A2").value("Steuernummer (vereinfacht)");
    ws.cell("B2").value("12/345/67890");
    ws.cell("A3").value("Steuernummer (bundeseinheitlich)");
    ws.cell("B3").value("5123045067890");
    ws.cell("A4").value("Finanzamts-Nummer");
    ws.cell("B4").value("2893");
    ws.cell("A5").value("Firmenname");
    ws.cell("B5").value("Muster GmbH");
    ws.cell("A6").value("Rechtsform");
    ws.cell("B6").value("GmbH");
    ws.cell("A7").value("Straße");
    ws.cell("B7").value("Musterstraße 123");
    ws.cell("A8").value("PLZ");
    ws.cell("B8").value("12345");
    ws.cell("A9").value("Ort");
    ws.cell("B9").value("Musterstadt");
    ws.cell("A10").value("Geschäftsführer");
    ws.cell("B10").value("Max Mustermann");
    ws.cell("A11").value("Handelsregister-Nr.");
    ws.cell("B11").value("HRB 12345");
    ws.cell("A12").value("Registergericht");
    ws.cell("B12").value("Amtsgericht Musterstadt");
    ws.cell("A13").value("Wirtschaftsjahr von");
    ws.cell("B13").value("01.01.2024");
    ws.cell("A14").value("Wirtschaftsjahr bis");
    ws.cell("B14").value("31.12.2024");
    ws.cell("A15").value("Bilanzstichtag");
    ws.cell("B15").value("31.12.2024");
    ws.cell("A16").value("WZ-Code");
    ws.cell("B16").value("62.01");
    ws.cell("A17").value("Telefon");
    ws.cell("B17").value("+49 123 456789");
    ws.cell("A18").value("E-Mail");
    ws.cell("B18").value("info@beispiel.de");
    ws.cell("A19").value("Übermittlungsanlass");
    ws.cell("B19").value("Erstmalig");
    ws.cell("A20").value("Bilanzaufstellung");
    ws.cell("B20").value("Handelsrecht");
    ws.cell("A21").value("Erstjahr");
    ws.cell("B21").value("Ja");
    ws.cell("A22").value("Ergebnisverwendung");
    ws.cell("B22").value("Thesaurierung");
    ws.cell("A23").value("Ausschüttungsbetrag");
    ws.cell("B23").value(0.0);

    // ── Sheet: Bilanz ──
    auto bilanzSheet = wb.create_sheet();
    bilanzSheet.title("Bilanz");

    // Aktiva — Immaterielle VG
    bilanzSheet.cell("C6").value(0.0);   bilanzSheet.cell("D6").value(0.0);
    bilanzSheet.cell("C7").value(0.0);   bilanzSheet.cell("D7").value(0.0);
    bilanzSheet.cell("C8").value(0.0);   bilanzSheet.cell("D8").value(0.0);
    bilanzSheet.cell("C9").value(0.0);   bilanzSheet.cell("D9").value(0.0);
    bilanzSheet.cell("C10").value(0.0);  bilanzSheet.cell("D10").value(0.0);
    bilanzSheet.cell("C11").value(0.0);  bilanzSheet.cell("D11").value(0.0);

    // Aktiva — Sachanlagen
    bilanzSheet.cell("C14").value(0.0);  bilanzSheet.cell("D14").value(0.0);
    bilanzSheet.cell("C15").value(0.0);  bilanzSheet.cell("D15").value(0.0);
    bilanzSheet.cell("C16").value(1000.0); bilanzSheet.cell("D16").value(0.0);
    bilanzSheet.cell("C17").value(0.0);  bilanzSheet.cell("D17").value(0.0);
    bilanzSheet.cell("C18").value(0.0);  bilanzSheet.cell("D18").value(0.0);
    bilanzSheet.cell("C19").value(1000.0); bilanzSheet.cell("D19").value(0.0);

    // Aktiva — Finanzanlagen
    bilanzSheet.cell("C22").value(10000.0); bilanzSheet.cell("D22").value(0.0);
    bilanzSheet.cell("C23").value(0.0);  bilanzSheet.cell("D23").value(0.0);
    bilanzSheet.cell("C24").value(0.0);  bilanzSheet.cell("D24").value(0.0);
    bilanzSheet.cell("C25").value(0.0);  bilanzSheet.cell("D25").value(0.0);
    bilanzSheet.cell("C26").value(0.0);  bilanzSheet.cell("D26").value(0.0);
    bilanzSheet.cell("C27").value(0.0);  bilanzSheet.cell("D27").value(0.0);
    bilanzSheet.cell("C28").value(0.0);  bilanzSheet.cell("D28").value(0.0);
    bilanzSheet.cell("C29").value(10000.0); bilanzSheet.cell("D29").value(0.0);

    bilanzSheet.cell("C31").value(11000.0); bilanzSheet.cell("D31").value(0.0);

    // Aktiva — Vorräte
    bilanzSheet.cell("C35").value(0.0); bilanzSheet.cell("D35").value(0.0);
    bilanzSheet.cell("C36").value(0.0); bilanzSheet.cell("D36").value(0.0);
    bilanzSheet.cell("C37").value(0.0); bilanzSheet.cell("D37").value(0.0);
    bilanzSheet.cell("C38").value(0.0); bilanzSheet.cell("D38").value(0.0);
    bilanzSheet.cell("C39").value(0.0); bilanzSheet.cell("D39").value(0.0);

    // Aktiva — Eingeforderte ausstehende Einlagen
    bilanzSheet.cell("C41").value(0.0); bilanzSheet.cell("D41").value(0.0);

    // Aktiva — Forderungen
    bilanzSheet.cell("C42").value(0.0);    bilanzSheet.cell("D42").value(0.0);
    bilanzSheet.cell("C43").value(0.0);    bilanzSheet.cell("D43").value(0.0);
    bilanzSheet.cell("C44").value(0.0);    bilanzSheet.cell("D44").value(0.0);
    bilanzSheet.cell("C45").value(3960.0); bilanzSheet.cell("D45").value(0.0);
    bilanzSheet.cell("C46").value(3960.0); bilanzSheet.cell("D46").value(0.0);

    // Aktiva — Wertpapiere UV
    bilanzSheet.cell("C49").value(0.0); bilanzSheet.cell("D49").value(0.0);
    bilanzSheet.cell("C50").value(0.0); bilanzSheet.cell("D50").value(0.0);
    bilanzSheet.cell("C51").value(0.0); bilanzSheet.cell("D51").value(0.0);

    // Aktiva — Kasse/Bank
    bilanzSheet.cell("C53").value(2948.66); bilanzSheet.cell("D53").value(0.0);

    bilanzSheet.cell("C55").value(6908.66); bilanzSheet.cell("D55").value(0.0);

    // Aktiva — Sonstiges
    bilanzSheet.cell("C57").value(0.0); bilanzSheet.cell("D57").value(0.0);
    bilanzSheet.cell("C58").value(0.0); bilanzSheet.cell("D58").value(0.0);
    bilanzSheet.cell("C59").value(0.0); bilanzSheet.cell("D59").value(0.0);

    // Summe Aktiva
    bilanzSheet.cell("C61").value(17908.66); bilanzSheet.cell("D61").value(0.0);

    // Passiva — Eigenkapital
    bilanzSheet.cell("C67").value(25000.0);  bilanzSheet.cell("D67").value(0.0);  // Gezeichnetes Kapital
    bilanzSheet.cell("C68").value(0.0);      bilanzSheet.cell("D68").value(0.0);  // Ausstehende Einlagen (nicht eingefordert)
    bilanzSheet.cell("C69").value(0.0);      bilanzSheet.cell("D69").value(0.0);  // Kapitalrücklage
    bilanzSheet.cell("C70").value(0.0);      bilanzSheet.cell("D70").value(0.0);
    bilanzSheet.cell("C71").value(0.0);      bilanzSheet.cell("D71").value(0.0);
    bilanzSheet.cell("C72").value(0.0);      bilanzSheet.cell("D72").value(0.0);
    bilanzSheet.cell("C73").value(0.0);      bilanzSheet.cell("D73").value(0.0);
    bilanzSheet.cell("C74").value(0.0);      bilanzSheet.cell("D74").value(0.0);
    bilanzSheet.cell("C75").value(0.0);      bilanzSheet.cell("D75").value(0.0);
    bilanzSheet.cell("C76").value(-15548.64); bilanzSheet.cell("D76").value(0.0);
    bilanzSheet.cell("C78").value(9451.36);  bilanzSheet.cell("D78").value(0.0);

    // Passiva — Rückstellungen
    bilanzSheet.cell("C81").value(0.0); bilanzSheet.cell("D81").value(0.0);
    bilanzSheet.cell("C82").value(0.0); bilanzSheet.cell("D82").value(0.0);
    bilanzSheet.cell("C83").value(0.0); bilanzSheet.cell("D83").value(0.0);
    bilanzSheet.cell("C84").value(0.0); bilanzSheet.cell("D84").value(0.0);

    // Passiva — Verbindlichkeiten
    bilanzSheet.cell("C87").value(0.0);      bilanzSheet.cell("D87").value(0.0);
    bilanzSheet.cell("C88").value(0.0);      bilanzSheet.cell("D88").value(0.0);
    bilanzSheet.cell("C89").value(0.0);      bilanzSheet.cell("D89").value(0.0);
    bilanzSheet.cell("C90").value(0.0);      bilanzSheet.cell("D90").value(0.0);
    bilanzSheet.cell("C91").value(0.0);      bilanzSheet.cell("D91").value(0.0);
    bilanzSheet.cell("C92").value(0.0);      bilanzSheet.cell("D92").value(0.0);
    bilanzSheet.cell("C93").value(0.0);      bilanzSheet.cell("D93").value(0.0);
    bilanzSheet.cell("C94").value(8457.30);  bilanzSheet.cell("D94").value(0.0);
    bilanzSheet.cell("C95").value(8457.30);  bilanzSheet.cell("D95").value(0.0);

    // Passiva — Sonstiges
    bilanzSheet.cell("C97").value(0.0);  bilanzSheet.cell("D97").value(0.0);
    bilanzSheet.cell("C98").value(0.0);  bilanzSheet.cell("D98").value(0.0);

    // Summe Passiva
    bilanzSheet.cell("C100").value(17908.66); bilanzSheet.cell("D100").value(0.0);

    // ── Sheet: GuV ──
    auto guvSheet = wb.create_sheet();
    guvSheet.title("GuV");

    guvSheet.cell("C3").value(0.0);      guvSheet.cell("D3").value(0.0);
    guvSheet.cell("C4").value(0.0);      guvSheet.cell("D4").value(0.0);
    guvSheet.cell("C5").value(0.0);      guvSheet.cell("D5").value(0.0);
    guvSheet.cell("C6").value(0.0);      guvSheet.cell("D6").value(0.0);
    guvSheet.cell("C9").value(0.0);      guvSheet.cell("D9").value(0.0);
    guvSheet.cell("C10").value(0.0);     guvSheet.cell("D10").value(0.0);
    guvSheet.cell("C11").value(0.0);     guvSheet.cell("D11").value(0.0);
    guvSheet.cell("C14").value(0.0);     guvSheet.cell("D14").value(0.0);
    guvSheet.cell("C15").value(0.0);     guvSheet.cell("D15").value(0.0);
    guvSheet.cell("C16").value(0.0);     guvSheet.cell("D16").value(0.0);
    guvSheet.cell("C19").value(0.0);     guvSheet.cell("D19").value(0.0);
    guvSheet.cell("C20").value(0.0);     guvSheet.cell("D20").value(0.0);
    guvSheet.cell("C21").value(0.0);     guvSheet.cell("D21").value(0.0);
    guvSheet.cell("C23").value(15548.64); guvSheet.cell("D23").value(0.0);
    guvSheet.cell("C25").value(0.0);     guvSheet.cell("D25").value(0.0);
    guvSheet.cell("C26").value(0.0);     guvSheet.cell("D26").value(0.0);
    guvSheet.cell("C27").value(0.0);     guvSheet.cell("D27").value(0.0);
    guvSheet.cell("C28").value(0.0);     guvSheet.cell("D28").value(0.0);
    guvSheet.cell("C29").value(0.0);     guvSheet.cell("D29").value(0.0);
    guvSheet.cell("C31").value(-15548.64); guvSheet.cell("D31").value(0.0);
    guvSheet.cell("C33").value(0.0);     guvSheet.cell("D33").value(0.0);
    guvSheet.cell("C34").value(0.0);     guvSheet.cell("D34").value(0.0);
    guvSheet.cell("C35").value(0.0);     guvSheet.cell("D35").value(0.0);
    guvSheet.cell("C37").value(0.0);     guvSheet.cell("D37").value(0.0);
    guvSheet.cell("C38").value(0.0);     guvSheet.cell("D38").value(0.0);
    guvSheet.cell("C40").value(-15548.64); guvSheet.cell("D40").value(0.0);

    wb.save(path);
    return path;
}

// ── Tests ────────────────────────────────────────────────────────────────────

TEST_CASE("ExcelParser: Stammdaten lesen") {
    std::string path = createValidTestXlsx();

    BufaVerzeichnis bufa;
    bufa.prefixMap["28"] = {"baden_wuerttemberg", "standard"};
    bufa.prefixMap["51"] = {"nordrhein_westfalen", "nrw"};

    ExcelParser parser;
    auto stamm = parser.readStammdaten(path, bufa);

    SECTION("Steuernummer") {
        REQUIRE(stamm.steuernummer == "12/345/67890");
    }

    SECTION("Bundeseinheitliche Steuernummer") {
        REQUIRE(stamm.steuernummerBund == "5123045067890");
    }

    SECTION("Finanzamtsnummer") {
        REQUIRE(stamm.finanzamtsNummer == "2893");
    }

    SECTION("Firmenname") {
        REQUIRE(stamm.firmenname == "Muster GmbH");
    }

    SECTION("Adresse") {
        REQUIRE(stamm.plz == "12345");
        REQUIRE(stamm.ort == "Musterstadt");
    }

    SECTION("Handelsregister") {
        REQUIRE(stamm.handelsregisterNr == "HRB 12345");
    }

    SECTION("Dates parsed to ISO") {
        REQUIRE(stamm.wirtschaftsjahrVon == "2024-01-01");
        REQUIRE(stamm.wirtschaftsjahrBis == "2024-12-31");
        REQUIRE(stamm.bilanzstichtag == "2024-12-31");
    }

    SECTION("Erstjahr") {
        REQUIRE(stamm.erstjahr == true);
    }

    SECTION("Ergebnisverwendung normalized") {
        REQUIRE(stamm.ergebnisverwendung == "thesaurierung");
    }

    SECTION("Uebermittlungsanlass normalized") {
        REQUIRE(stamm.uebermittlungsanlass == "erstmalig");
    }

    // Cleanup
    fs::remove(path);
}

TEST_CASE("ExcelParser: Bilanz lesen") {
    std::string path = createValidTestXlsx();

    ExcelParser parser;
    auto bilanz = parser.readBilanz(path);

    SECTION("Summe Aktiva") {
        REQUIRE(std::abs(bilanz.summeAktiva - 17908.66) < 0.01);
    }

    SECTION("Summe Passiva") {
        REQUIRE(std::abs(bilanz.summePassiva - 17908.66) < 0.01);
    }

    SECTION("Bilanz ausgeglichen") {
        REQUIRE(std::abs(bilanz.summeAktiva - bilanz.summePassiva) < 0.01);
    }

    SECTION("Gezeichnetes Kapital") {
        REQUIRE(std::abs(bilanz.gezeichnetesKapital - 25000.0) < 0.01);
    }

    SECTION("Jahresüberschuss") {
        REQUIRE(std::abs(bilanz.jahresueberschuss - (-15548.64)) < 0.01);
    }

    SECTION("Sachanlagen BGA") {
        REQUIRE(std::abs(bilanz.positionen["sachanlagen_bga"] - 1000.0) < 0.01);
    }

    SECTION("Kassenbestand") {
        REQUIRE(std::abs(bilanz.positionen["kassenbestand"] - 2948.66) < 0.01);
    }

    SECTION("Finanzanlagen") {
        REQUIRE(std::abs(bilanz.positionen["finanzanlagen_verbundenU"] - 10000.0) < 0.01);
    }

    SECTION("Vorjahreswerte sind 0 (Erstjahr)") {
        REQUIRE(std::abs(bilanz.vorjahr_summeAktiva) < 0.01);
        REQUIRE(std::abs(bilanz.vorjahr_summePassiva) < 0.01);
    }

    SECTION("Verbindlichkeiten sonstige") {
        REQUIRE(std::abs(bilanz.positionen["verbindlichkeiten_sonstige"] - 8457.30) < 0.01);
    }

    fs::remove(path);
}

TEST_CASE("ExcelParser: GuV lesen") {
    std::string path = createValidTestXlsx();

    ExcelParser parser;
    auto guv = parser.readGuV(path);

    SECTION("Jahresüberschuss") {
        REQUIRE(std::abs(guv.jahresueberschuss - (-15548.64)) < 0.01);
    }

    SECTION("Sonstige betriebliche Aufwendungen") {
        REQUIRE(std::abs(guv.positionen["guv_sonstige_aufwendungen"] - 15548.64) < 0.01);
    }

    SECTION("Vorjahreswerte sind 0") {
        for (const auto& [key, val] : guv.vorjahrPositionen) {
            REQUIRE(std::abs(val) < 0.01);
        }
    }

    fs::remove(path);
}

TEST_CASE("ExcelParser: Bilanz und GuV Jahresüberschuss konsistent") {
    std::string path = createValidTestXlsx();

    ExcelParser parser;
    auto bilanz = parser.readBilanz(path);
    auto guv = parser.readGuV(path);

    REQUIRE(std::abs(bilanz.jahresueberschuss - guv.jahresueberschuss) < 0.01);

    fs::remove(path);
}

// ── Test Helper: Create xlsx with ausstehende Einlagen ───────────────────────

static std::string createAusstehendeEinlagenTestXlsx() {
    std::string dir = getTestDataDir();
    std::string path = dir + "/test-bilanz-ausstehende-einlagen.xlsx";

    xlnt::workbook wb;

    // ── Sheet: Stammdaten (minimal) ──
    auto ws = wb.active_sheet();
    ws.title("Stammdaten");
    ws.cell("B2").value("12/345/67890");
    ws.cell("B3").value("5123045067890");
    ws.cell("B4").value("2893");
    ws.cell("B5").value("Test GmbH");
    ws.cell("B7").value("Teststraße 1");
    ws.cell("B8").value("12345");
    ws.cell("B9").value("Teststadt");
    ws.cell("B10").value("Max Test");
    ws.cell("B11").value("HRB 99999");
    ws.cell("B12").value("AG Teststadt");
    ws.cell("B13").value("01.01.2024");
    ws.cell("B14").value("31.12.2024");
    ws.cell("B15").value("31.12.2024");
    ws.cell("B19").value("Erstmalig");
    ws.cell("B21").value("Ja");
    ws.cell("B22").value("Thesaurierung");
    ws.cell("B23").value(0.0);

    // ── Sheet: Bilanz ──
    auto bilanzSheet = wb.create_sheet();
    bilanzSheet.title("Bilanz");

    // Fill all required cells with 0.0 first
    for (int row : {6,7,8,9,10,11,14,15,16,17,18,19,22,23,24,25,26,27,28,29,31,
                    35,36,37,38,39,41,42,43,44,45,46,49,50,51,53,55,57,58,59,61,
                    67,68,69,70,71,72,73,74,75,76,78,81,82,83,84,87,88,89,90,91,92,93,94,95,97,98,100}) {
        bilanzSheet.cell("C" + std::to_string(row)).value(0.0);
        bilanzSheet.cell("D" + std::to_string(row)).value(0.0);
    }

    // Specific values for ausstehende Einlagen scenario:
    // GmbH with 25,000 EUR Stammkapital but only 12,500 EUR paid in
    bilanzSheet.cell("C67").value(25000.0);  // Gezeichnetes Kapital
    bilanzSheet.cell("C68").value(12500.0);  // Nicht eingeforderte ausstehende Einlagen (passivisch abgesetzt)
    bilanzSheet.cell("C76").value(0.0);      // Jahresüberschuss
    bilanzSheet.cell("C78").value(12500.0);  // Eigenkapital Summe (25000 - 12500)

    // Aktiva to balance: 12,500 EUR in Bank
    bilanzSheet.cell("C53").value(12500.0);  // Kassenbestand
    bilanzSheet.cell("C55").value(12500.0);  // Umlaufvermögen
    bilanzSheet.cell("C61").value(12500.0);  // Summe Aktiva

    // Summe Passiva
    bilanzSheet.cell("C100").value(12500.0); // Summe Passiva

    // ── Sheet: GuV (minimal) ──
    auto guvSheet = wb.create_sheet();
    guvSheet.title("GuV");
    for (int row : {3,4,5,6,9,10,11,14,15,16,19,20,21,23,25,26,27,28,29,31,33,34,35,37,38,40}) {
        guvSheet.cell("C" + std::to_string(row)).value(0.0);
        guvSheet.cell("D" + std::to_string(row)).value(0.0);
    }

    wb.save(path);
    return path;
}

TEST_CASE("ExcelParser: Ausstehende Einlagen parsen") {
    std::string path = createAusstehendeEinlagenTestXlsx();

    ExcelParser parser;
    auto bilanz = parser.readBilanz(path);

    SECTION("Gezeichnetes Kapital ist 25000") {
        REQUIRE(std::abs(bilanz.gezeichnetesKapital - 25000.0) < 0.01);
    }

    SECTION("Nicht eingeforderte ausstehende Einlagen ist 12500") {
        REQUIRE(std::abs(bilanz.positionen["ausstehende_einlagen_nicht_eingefordert"] - 12500.0) < 0.01);
    }

    SECTION("Eigenkapital Summe reduziert auf 12500") {
        REQUIRE(std::abs(bilanz.positionen["eigenkapital_summe"] - 12500.0) < 0.01);
    }

    SECTION("Bilanz ist ausgeglichen") {
        REQUIRE(std::abs(bilanz.summeAktiva - bilanz.summePassiva) < 0.01);
    }

    SECTION("Eingeforderte Einlagen Position existiert (0 wenn nicht eingefordert)") {
        REQUIRE(std::abs(bilanz.positionen["forderungen_eingeforderte_einlagen"]) < 0.01);
    }

    fs::remove(path);
}

// ── Date parsing ─────────────────────────────────────────────────────────────

TEST_CASE("parseDate: German to ISO") {
    SECTION("Standard format") {
        REQUIRE(parseDate("01.01.2024") == "2024-01-01");
        REQUIRE(parseDate("31.12.2024") == "2024-12-31");
        REQUIRE(parseDate("15.06.2023") == "2023-06-15");
    }

    SECTION("Already ISO") {
        REQUIRE(parseDate("2024-01-01") == "2024-01-01");
    }

    SECTION("Empty string") {
        REQUIRE(parseDate("") == "");
    }

    SECTION("Invalid date throws") {
        REQUIRE_THROWS(parseDate("invalid"));
    }
}
