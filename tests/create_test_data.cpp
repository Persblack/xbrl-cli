// Standalone program to generate test .xlsx files for the ebilanz test suite.
// Run once after build to populate tests/testdata/ directory.

#include <xlnt/xlnt.hpp>

#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

// Helper to set a range of cells to 0.0
void zeroRange(xlnt::worksheet& ws, const std::string& col, int startRow, int endRow) {
    for (int r = startRow; r <= endRow; ++r) {
        ws.cell(col + std::to_string(r)).value(0.0);
    }
}

void createValidBilanz(const std::string& path) {
    xlnt::workbook wb;

    // ── Stammdaten ──
    auto ws = wb.active_sheet();
    ws.title("Stammdaten");
    ws.cell("B2").value("12/345/67890");
    ws.cell("B3").value("5123045067890");
    ws.cell("B4").value("2893");
    ws.cell("B5").value("Muster GmbH");
    ws.cell("B6").value("GmbH");
    ws.cell("B7").value("Musterstraße 123");
    ws.cell("B8").value("12345");
    ws.cell("B9").value("Musterstadt");
    ws.cell("B10").value("Max Mustermann");
    ws.cell("B11").value("HRB 12345");
    ws.cell("B12").value("Amtsgericht Musterstadt");
    ws.cell("B13").value("01.01.2024");
    ws.cell("B14").value("31.12.2024");
    ws.cell("B15").value("31.12.2024");
    ws.cell("B16").value("62.01");
    ws.cell("B17").value("+49 123 456789");
    ws.cell("B18").value("info@beispiel.de");
    ws.cell("B19").value("Erstmalig");
    ws.cell("B20").value("Handelsrecht");
    ws.cell("B21").value("Nein");
    ws.cell("B22").value("Thesaurierung");
    ws.cell("B23").value(0.0);

    // ── Bilanz ──
    auto bilanz = wb.create_sheet();
    bilanz.title("Bilanz");

    // Initialize all cells to 0
    std::string cols[] = {"C", "D"};
    for (const auto& col : cols) {
        for (int r = 6; r <= 100; ++r) {
            bilanz.cell(col + std::to_string(r)).value(0.0);
        }
    }

    // Set actual values (from Lastenheft example)
    bilanz.cell("C16").value(1000.0);      // BGA
    bilanz.cell("C19").value(1000.0);      // Summe Sachanlagen
    bilanz.cell("C22").value(10000.0);     // Anteile verbundene U
    bilanz.cell("C29").value(10000.0);     // Summe Finanzanlagen
    bilanz.cell("C31").value(11000.0);     // Summe Anlagevermögen
    bilanz.cell("C45").value(3960.0);      // Sonstige Vermögensgegenstände
    bilanz.cell("C46").value(3960.0);      // Summe Forderungen
    bilanz.cell("C53").value(2948.66);     // Kassenbestand
    bilanz.cell("C55").value(6908.66);     // Summe Umlaufvermögen
    bilanz.cell("C61").value(17908.66);    // SUMME AKTIVA

    bilanz.cell("C67").value(25000.0);     // Gezeichnetes Kapital
    bilanz.cell("C68").value(0.0);         // Ausstehende Einlagen (nicht eingefordert)
    bilanz.cell("C69").value(0.0);         // Kapitalrücklage
    bilanz.cell("C76").value(-15548.64);   // Jahresüberschuss
    bilanz.cell("C78").value(9451.36);     // Summe Eigenkapital
    bilanz.cell("C94").value(8457.30);     // Sonstige Verbindlichkeiten
    bilanz.cell("C95").value(8457.30);     // Summe Verbindlichkeiten
    bilanz.cell("C100").value(17908.66);   // SUMME PASSIVA

    // Vorjahr (non-Erstjahr: some values)
    bilanz.cell("D67").value(25000.0);     // Gezeichnetes Kapital VJ
    bilanz.cell("D68").value(0.0);         // Ausstehende Einlagen VJ
    bilanz.cell("D69").value(0.0);         // Kapitalrücklage VJ
    bilanz.cell("D78").value(25000.0);     // Eigenkapital VJ
    bilanz.cell("D53").value(25000.0);     // Kassenbestand VJ
    bilanz.cell("D55").value(25000.0);     // Umlaufvermögen VJ
    bilanz.cell("D61").value(25000.0);     // Aktiva VJ
    bilanz.cell("D100").value(25000.0);    // Passiva VJ

    // ── GuV ──
    auto guv = wb.create_sheet();
    guv.title("GuV");

    for (const auto& col : cols) {
        for (int r = 3; r <= 40; ++r) {
            guv.cell(col + std::to_string(r)).value(0.0);
        }
    }

    guv.cell("C23").value(15548.64);       // Sonstige betriebliche Aufwendungen
    guv.cell("C31").value(-15548.64);      // Ergebnis gewöhnliche Geschäftstätigkeit
    guv.cell("C40").value(-15548.64);      // Jahresüberschuss

    wb.save(path);
    std::cout << "Erstellt: " << path << "\n";
}

void createErstjahrBilanz(const std::string& path) {
    xlnt::workbook wb;

    // ── Stammdaten ──
    auto ws = wb.active_sheet();
    ws.title("Stammdaten");
    ws.cell("B2").value("12/345/67890");
    ws.cell("B3").value("5123045067890");
    ws.cell("B4").value("2893");
    ws.cell("B5").value("Muster GmbH");
    ws.cell("B6").value("GmbH");
    ws.cell("B7").value("Musterstraße 123");
    ws.cell("B8").value("12345");
    ws.cell("B9").value("Musterstadt");
    ws.cell("B10").value("Max Mustermann");
    ws.cell("B11").value("HRB 12345");
    ws.cell("B12").value("Amtsgericht Musterstadt");
    ws.cell("B13").value("01.01.2024");
    ws.cell("B14").value("31.12.2024");
    ws.cell("B15").value("31.12.2024");
    ws.cell("B16").value("62.01");
    ws.cell("B19").value("Erstmalig");
    ws.cell("B20").value("Handelsrecht");
    ws.cell("B21").value("Ja");  // ERSTJAHR!
    ws.cell("B22").value("Thesaurierung");
    ws.cell("B23").value(0.0);

    // ── Bilanz ── (same as valid, but ALL Vorjahr = 0)
    auto bilanz = wb.create_sheet();
    bilanz.title("Bilanz");

    std::string cols[] = {"C", "D"};
    for (const auto& col : cols) {
        for (int r = 6; r <= 100; ++r) {
            bilanz.cell(col + std::to_string(r)).value(0.0);
        }
    }

    bilanz.cell("C53").value(25000.0);     // Kassenbestand (Stammeinlage)
    bilanz.cell("C55").value(25000.0);
    bilanz.cell("C61").value(25000.0);     // SUMME AKTIVA
    bilanz.cell("C67").value(25000.0);     // Gezeichnetes Kapital
    bilanz.cell("C68").value(0.0);         // Ausstehende Einlagen (nicht eingefordert)
    bilanz.cell("C69").value(0.0);         // Kapitalrücklage
    bilanz.cell("C78").value(25000.0);
    bilanz.cell("C100").value(25000.0);    // SUMME PASSIVA

    // ── GuV ──
    auto guv = wb.create_sheet();
    guv.title("GuV");
    for (const auto& col : cols) {
        for (int r = 3; r <= 40; ++r) {
            guv.cell(col + std::to_string(r)).value(0.0);
        }
    }

    wb.save(path);
    std::cout << "Erstellt: " << path << "\n";
}

void createTemplate(const std::string& path) {
    xlnt::workbook wb;

    // ── Stammdaten ──
    auto ws = wb.active_sheet();
    ws.title("Stammdaten");

    // Labels in column A, values in column B (pre-filled from bilanz-2024.xlsx)
    ws.cell("A1").value("Feld");
    ws.cell("B1").value("Wert");
    ws.cell("A2").value("Steuernummer (vereinfacht)");
    ws.cell("B2").value("12/345/67890");
    ws.cell("A3").value("Steuernummer (bundeseinheitlich)");
    ws.cell("B3").value("");  // Computed automatically
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
    ws.cell("B12").value("Ulm");
    ws.cell("A13").value("Wirtschaftsjahr von");
    ws.cell("B13").value("01.01.2024");
    ws.cell("A14").value("Wirtschaftsjahr bis");
    ws.cell("B14").value("31.12.2024");
    ws.cell("A15").value("Bilanzstichtag");
    ws.cell("B15").value("31.12.2024");
    ws.cell("A16").value("WZ-Code");
    ws.cell("B16").value("");
    ws.cell("A17").value("Telefon");
    ws.cell("B17").value("");
    ws.cell("A18").value("E-Mail");
    ws.cell("B18").value("");
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

    // ── Bilanz ──
    // Labels aligned with data rows from excel_parser.cpp:
    // C6-C11: Immaterielle VG, C14-C19: Sachanlagen, C22-C29: Finanzanlagen, C31: Summe AV
    // C35-C39: Vorräte, C41: Eingef. Einlagen, C42-C46: Forderungen, C49-C51: Wertpapiere
    // C53: Kasse, C55: Summe UV, C57-C59: Sonstiges, C61: SUMME AKTIVA
    // C67-C78: Eigenkapital, C81-C84: Rückstellungen, C87-C95: Verbindlichkeiten
    // C97-C98: Sonstiges, C100: SUMME PASSIVA
    auto bilanz = wb.create_sheet();
    bilanz.title("Bilanz");

    // Headers
    bilanz.cell("A1").value("Position");
    bilanz.cell("C1").value("Aktuelles Jahr");
    bilanz.cell("D1").value("Vorjahr");

    // ── AKTIVA ──
    bilanz.cell("A3").value("AKTIVA");
    bilanz.cell("A5").value("A. Anlagevermögen");

    // I. Immaterielle VG (C6-C11)
    bilanz.cell("A6").value("I.1. Selbst geschaffene gewerbliche Schutzrechte");
    bilanz.cell("A7").value("I.2. Konzessionen, Lizenzen, ähnliche Rechte");
    bilanz.cell("A8").value("I.3. Geschäfts- oder Firmenwert");
    bilanz.cell("A9").value("I.4. Geleistete Anzahlungen auf immaterielle VG");
    bilanz.cell("A10").value("I.5. Andere immaterielle Vermögensgegenstände");
    bilanz.cell("A11").value("Summe Immaterielle Vermögensgegenstände");

    // II. Sachanlagen (C14-C19)
    bilanz.cell("A14").value("II.1. Grundstücke und Bauten");
    bilanz.cell("A15").value("II.2. Technische Anlagen und Maschinen");
    bilanz.cell("A16").value("II.3. Andere Anlagen, Betriebs- und Geschäftsausstattung");
    bilanz.cell("A17").value("II.4. Geleistete Anzahlungen und Anlagen im Bau");
    bilanz.cell("A18").value("II.5. Andere Sachanlagen");
    bilanz.cell("A19").value("Summe Sachanlagen");

    // III. Finanzanlagen (C22-C29)
    bilanz.cell("A22").value("III.1. Anteile an verbundenen Unternehmen");
    bilanz.cell("A23").value("III.2. Ausleihungen an verbundene Unternehmen");
    bilanz.cell("A24").value("III.3. Beteiligungen");
    bilanz.cell("A25").value("III.4. Ausleihungen an Beteiligungsunternehmen");
    bilanz.cell("A26").value("III.5. Wertpapiere des Anlagevermögens");
    bilanz.cell("A27").value("III.6. Sonstige Ausleihungen");
    bilanz.cell("A28").value("III.7. Andere Finanzanlagen");
    bilanz.cell("A29").value("Summe Finanzanlagen");

    bilanz.cell("A31").value("Summe Anlagevermögen");

    // B. Umlaufvermögen
    bilanz.cell("A33").value("B. Umlaufvermögen");

    // I. Vorräte (C35-C39)
    bilanz.cell("A35").value("I.1. Roh-, Hilfs- und Betriebsstoffe");
    bilanz.cell("A36").value("I.2. Unfertige Erzeugnisse");
    bilanz.cell("A37").value("I.3. Fertige Erzeugnisse und Waren");
    bilanz.cell("A38").value("I.4. Geleistete Anzahlungen auf Vorräte");
    bilanz.cell("A39").value("Summe Vorräte");

    // Eingeforderte ausstehende Einlagen (C41)
    bilanz.cell("A41").value("Eingeforderte noch ausstehende Einlagen");

    // II. Forderungen (C42-C46)
    bilanz.cell("A42").value("II.1. Forderungen aus Lieferungen und Leistungen");
    bilanz.cell("A43").value("II.2. Forderungen gegen verbundene Unternehmen");
    bilanz.cell("A44").value("II.3. Forderungen gegen Beteiligungsunternehmen");
    bilanz.cell("A45").value("II.4. Sonstige Vermögensgegenstände");
    bilanz.cell("A46").value("Summe Forderungen");

    // III. Wertpapiere (C49-C51)
    bilanz.cell("A49").value("III.1. Anteile an verbundenen Unternehmen (UV)");
    bilanz.cell("A50").value("III.2. Sonstige Wertpapiere");
    bilanz.cell("A51").value("Summe Wertpapiere");

    // IV. Kassenbestand (C53)
    bilanz.cell("A53").value("IV. Kassenbestand, Bankguthaben");

    bilanz.cell("A55").value("Summe Umlaufvermögen");

    // C-E. Sonstiges (C57-C59)
    bilanz.cell("A57").value("C. Rechnungsabgrenzungsposten");
    bilanz.cell("A58").value("D. Aktive latente Steuern");
    bilanz.cell("A59").value("E. Aktiver Unterschiedsbetrag aus Vermögensverrechnung");

    bilanz.cell("A61").value("SUMME AKTIVA");

    // ── PASSIVA ──
    bilanz.cell("A63").value("PASSIVA");
    bilanz.cell("A65").value("A. Eigenkapital");

    // Eigenkapital (C67-C78)
    bilanz.cell("A67").value("I. Gezeichnetes Kapital");
    bilanz.cell("A68").value("   ./. nicht eingeforderte ausstehende Einlagen");
    bilanz.cell("A69").value("II. Kapitalrücklage");
    bilanz.cell("A70").value("III.1. Gesetzliche Rücklage");
    bilanz.cell("A71").value("III.2. Rücklage für Anteile an herrschendem Unternehmen");
    bilanz.cell("A72").value("III.3. Satzungsmäßige Rücklagen");
    bilanz.cell("A73").value("III.4. Andere Gewinnrücklagen");
    bilanz.cell("A74").value("Summe Gewinnrücklagen");
    bilanz.cell("A75").value("IV. Gewinnvortrag/Verlustvortrag");
    bilanz.cell("A76").value("V. Jahresüberschuss/Jahresfehlbetrag");
    bilanz.cell("A78").value("Summe Eigenkapital");

    // B. Rückstellungen (C81-C84)
    bilanz.cell("A80").value("B. Rückstellungen");
    bilanz.cell("A81").value("   1. Rückstellungen für Pensionen");
    bilanz.cell("A82").value("   2. Steuerrückstellungen");
    bilanz.cell("A83").value("   3. Sonstige Rückstellungen");
    bilanz.cell("A84").value("Summe Rückstellungen");

    // C. Verbindlichkeiten (C87-C95)
    bilanz.cell("A86").value("C. Verbindlichkeiten");
    bilanz.cell("A87").value("   1. Anleihen");
    bilanz.cell("A88").value("   2. Verbindlichkeiten gegenüber Kreditinstituten");
    bilanz.cell("A89").value("   3. Erhaltene Anzahlungen");
    bilanz.cell("A90").value("   4. Verbindlichkeiten aus Lieferungen und Leistungen");
    bilanz.cell("A91").value("   5. Verbindlichkeiten aus Wechseln");
    bilanz.cell("A92").value("   6. Verbindlichkeiten gegenüber verbundenen Unternehmen");
    bilanz.cell("A93").value("   7. Verbindlichkeiten gegenüber Beteiligungsunternehmen");
    bilanz.cell("A94").value("   8. Sonstige Verbindlichkeiten");
    bilanz.cell("A95").value("Summe Verbindlichkeiten");

    // D-E. Sonstiges (C97-C98)
    bilanz.cell("A97").value("D. Rechnungsabgrenzungsposten");
    bilanz.cell("A98").value("E. Passive latente Steuern");

    bilanz.cell("A100").value("SUMME PASSIVA");

    // Initialize all value cells to 0
    std::vector<int> bilanzRows = {6,7,8,9,10,11,14,15,16,17,18,19,22,23,24,25,26,27,28,29,31,
                                    35,36,37,38,39,41,42,43,44,45,46,49,50,51,53,55,57,58,59,61,
                                    67,68,69,70,71,72,73,74,75,76,78,81,82,83,84,87,88,89,90,91,92,93,94,95,97,98,100};
    for (int row : bilanzRows) {
        bilanz.cell("C" + std::to_string(row)).value(0.0);
        bilanz.cell("D" + std::to_string(row)).value(0.0);
    }

    // ── GuV ──
    auto guv = wb.create_sheet();
    guv.title("GuV");

    guv.cell("A1").value("Position");
    guv.cell("C1").value("Aktuelles Jahr");
    guv.cell("D1").value("Vorjahr");

    guv.cell("A3").value("1. Umsatzerlöse");
    guv.cell("A4").value("2. Erhöhung/Verminderung des Bestands");
    guv.cell("A5").value("3. Andere aktivierte Eigenleistungen");
    guv.cell("A6").value("4. Sonstige betriebliche Erträge");

    guv.cell("A8").value("5. Materialaufwand");
    guv.cell("A9").value("   a) Aufwendungen für Roh-, Hilfs- und Betriebsstoffe");
    guv.cell("A10").value("   b) Aufwendungen für bezogene Leistungen");
    guv.cell("A11").value("   Summe Materialaufwand");

    guv.cell("A13").value("6. Personalaufwand");
    guv.cell("A14").value("   a) Löhne und Gehälter");
    guv.cell("A15").value("   b) Soziale Abgaben");
    guv.cell("A16").value("   Summe Personalaufwand");

    guv.cell("A18").value("7. Abschreibungen");
    guv.cell("A19").value("   a) Auf immaterielle VG und Sachanlagen");
    guv.cell("A20").value("   b) Auf Vermögensgegenstände des Umlaufvermögens");
    guv.cell("A21").value("   Summe Abschreibungen");

    guv.cell("A23").value("8. Sonstige betriebliche Aufwendungen");

    guv.cell("A25").value("9. Erträge aus Beteiligungen");
    guv.cell("A26").value("10. Erträge aus anderen Wertpapieren");
    guv.cell("A27").value("11. Sonstige Zinsen und ähnliche Erträge");
    guv.cell("A28").value("12. Abschreibungen auf Finanzanlagen");
    guv.cell("A29").value("13. Zinsen und ähnliche Aufwendungen");

    guv.cell("A31").value("14. Ergebnis der gewöhnlichen Geschäftstätigkeit");

    guv.cell("A33").value("15. Außerordentliche Erträge");
    guv.cell("A34").value("16. Außerordentliche Aufwendungen");
    guv.cell("A35").value("17. Außerordentliches Ergebnis");

    guv.cell("A37").value("18. Steuern vom Einkommen und Ertrag");
    guv.cell("A38").value("19. Sonstige Steuern");

    guv.cell("A40").value("20. Jahresüberschuss/Jahresfehlbetrag");

    // Initialize all value cells to 0
    std::vector<int> guvRows = {3,4,5,6,9,10,11,14,15,16,19,20,21,23,25,26,27,28,29,31,33,34,35,37,38,40};
    for (int row : guvRows) {
        guv.cell("C" + std::to_string(row)).value(0.0);
        guv.cell("D" + std::to_string(row)).value(0.0);
    }

    wb.save(path);
    std::cout << "Erstellt: " << path << "\n";
}

void createInvalidBilanz(const std::string& path) {
    xlnt::workbook wb;

    // ── Stammdaten (some deliberately invalid) ──
    auto ws = wb.active_sheet();
    ws.title("Stammdaten");
    ws.cell("B2").value("invalid-stnr");       // Invalid format
    ws.cell("B3").value("");                     // Missing
    ws.cell("B4").value("12");                   // Invalid BuFa (too short)
    ws.cell("B5").value("Test UG");
    ws.cell("B6").value("GmbH");
    ws.cell("B7").value("Str. 1");
    ws.cell("B8").value("12345");
    ws.cell("B9").value("Berlin");
    ws.cell("B10").value("Max Muster");
    ws.cell("B11").value("12345");               // Invalid: missing HRB
    ws.cell("B12").value("AG Berlin");
    ws.cell("B13").value("01.01.2024");
    ws.cell("B14").value("31.12.2024");
    ws.cell("B15").value("30.12.2024");          // Mismatch with B14!
    ws.cell("B19").value("Erstmalig");
    ws.cell("B20").value("Handelsrecht");
    ws.cell("B21").value("Nein");
    ws.cell("B22").value("Ausschüttung");
    ws.cell("B23").value(0.0);                   // Missing amount!

    // ── Bilanz (unbalanced + UG capital) ──
    auto bilanz = wb.create_sheet();
    bilanz.title("Bilanz");

    for (int r = 6; r <= 100; ++r) {
        bilanz.cell("C" + std::to_string(r)).value(0.0);
        bilanz.cell("D" + std::to_string(r)).value(0.0);
    }

    bilanz.cell("C53").value(10000.0);
    bilanz.cell("C55").value(10000.0);
    bilanz.cell("C61").value(10000.0);     // Aktiva
    bilanz.cell("C67").value(1000.0);      // UG capital (< 25000)!
    bilanz.cell("C76").value(4000.0);
    bilanz.cell("C78").value(5000.0);
    bilanz.cell("C100").value(5000.0);     // Passiva (unbalanced!)

    // ── GuV (mismatched Jahresüberschuss) ──
    auto guv = wb.create_sheet();
    guv.title("GuV");
    for (int r = 3; r <= 40; ++r) {
        guv.cell("C" + std::to_string(r)).value(0.0);
        guv.cell("D" + std::to_string(r)).value(0.0);
    }
    guv.cell("C40").value(9999.0);  // Doesn't match bilanz.C76!

    wb.save(path);
    std::cout << "Erstellt: " << path << "\n";
}

int main() {
    std::string dir = "tests/testdata";
    std::string templateDir = "templates";

    if (!fs::exists(dir)) {
        // Try from build directory
        if (fs::exists("../tests/testdata")) {
            dir = "../tests/testdata";
            templateDir = "../templates";
        } else {
            fs::create_directories(dir);
        }
    }

    if (!fs::exists(templateDir)) {
        fs::create_directories(templateDir);
    }

    createValidBilanz(dir + "/test-bilanz.xlsx");
    createErstjahrBilanz(dir + "/test-bilanz-erstjahr.xlsx");
    createInvalidBilanz(dir + "/test-bilanz-invalid.xlsx");
    createTemplate(templateDir + "/ebilanz-template.xlsx");

    std::cout << "\nAlle Test-Dateien erfolgreich erstellt in: " << dir << "\n";
    std::cout << "Template erstellt in: " << templateDir << "\n";
    return 0;
}
