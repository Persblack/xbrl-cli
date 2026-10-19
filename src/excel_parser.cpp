#include "excel_parser.h"
#include "utils.h"

#include <spdlog/spdlog.h>
#include <xlnt/xlnt.hpp>

#include <algorithm>
#include <cctype>

namespace {

// Safely read a string cell value
std::string readString(xlnt::worksheet& ws, const std::string& cellRef) {
    auto cell = ws.cell(cellRef);
    if (cell.has_value()) {
        return cell.to_string();
    }
    return "";
}

// Safely read a numeric cell value
double readDouble(xlnt::worksheet& ws, const std::string& cellRef) {
    auto cell = ws.cell(cellRef);
    if (cell.has_value()) {
        try {
            return cell.value<double>();
        } catch (...) {
            // Try parsing string as double
            try {
                std::string str = cell.to_string();
                if (str.empty()) return 0.0;
                // Handle German number format: '.' = thousands separator, ',' = decimal.
                // Only strip '.' when a ',' decimal is also present, otherwise keep a lone
                // '.' as the decimal point (e.g. "1234.56"). Avoids turning "1.234,56" into
                // "1.234" by dropping the thousands separator.
                if (str.find(',') != std::string::npos) {
                    str.erase(std::remove(str.begin(), str.end(), '.'), str.end());
                    std::replace(str.begin(), str.end(), ',', '.');
                }
                return std::stod(str);
            } catch (...) {
                return 0.0;
            }
        }
    }
    return 0.0;
}

// Read a date cell — xlnt may return a serial number or formatted string
std::string readDate(xlnt::worksheet& ws, const std::string& cellRef) {
    auto cell = ws.cell(cellRef);
    if (!cell.has_value()) {
        return "";
    }

    // Try to get as date
    try {
        if (cell.is_date()) {
            auto dt = cell.value<xlnt::date>();
            std::ostringstream oss;
            oss << std::setfill('0')
                << std::setw(2) << dt.day << "."
                << std::setw(2) << dt.month << "."
                << std::setw(4) << dt.year;
            return oss.str();
        }
    } catch (...) {
        // Fall through to string parsing
    }

    // Try as string
    return cell.to_string();
}

// Normalize a boolean field from Excel ("Ja"/"Nein"/true/false/1/0)
bool readBool(xlnt::worksheet& ws, const std::string& cellRef) {
    std::string val = readString(ws, cellRef);
    // Lowercase for comparison
    std::string lower = val;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    return (lower == "ja" || lower == "yes" || lower == "true" || lower == "1" ||
            lower == "wahr");
}

// Normalize rechtsform to internal format
// Template cell B6 offers "GmbH" and "UG (haftungsbeschränkt)"
std::string normalizeRechtsform(const std::string& val) {
    std::string lower = val;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    // "gmbh" first: text like "Komplementaer GmbH" must never fall through to UG
    if (lower.find("gmbh") != std::string::npos) return "gmbh";
    // No bare find("ug"): company names like "Zugspitz AG" must not match
    if (lower.find("unternehmergesellschaft") != std::string::npos ||
        lower == "ug" || lower.rfind("ug ", 0) == 0 || lower.rfind("ug(", 0) == 0) {
        return "ug";
    }
    return lower;
}

// Normalize ergebnisverwendung to internal format
std::string normalizeErgebnisverwendung(const std::string& val) {
    std::string lower = val;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    if (lower.find("thesaur") != std::string::npos) return "thesaurierung";
    if (lower.find("teilaussch") != std::string::npos) return "teilausschuettung";
    if (lower.find("aussch") != std::string::npos) return "ausschuettung";
    return lower;
}

// Normalize uebermittlungsanlass to internal format
std::string normalizeUebermittlungsanlass(const std::string& val) {
    std::string lower = val;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    if (lower.find("erstm") != std::string::npos) return "erstmalig";
    if (lower.find("korri") != std::string::npos) return "korrigiert";
    return lower;
}

} // anonymous namespace

Stammdaten ExcelParser::readStammdaten(const std::string& filepath,
                                        const BufaVerzeichnis& bufa) const {
    spdlog::debug("Lese Stammdaten aus: {}", filepath);

    xlnt::workbook wb;
    wb.load(filepath);
    auto ws = wb.sheet_by_title("Stammdaten");

    Stammdaten data;
    data.steuernummer = readString(ws, "B2");
    data.steuernummerBund = readString(ws, "B3");
    data.finanzamtsNummer = readString(ws, "B4");
    data.firmenname = readString(ws, "B5");
    data.rechtsform = normalizeRechtsform(readString(ws, "B6"));
    data.strasse = readString(ws, "B7");
    data.plz = readString(ws, "B8");
    data.ort = readString(ws, "B9");
    data.geschaeftsfuehrer = readString(ws, "B10");
    data.handelsregisterNr = readString(ws, "B11");
    data.registergericht = readString(ws, "B12");

    // Dates: parse German format to ISO
    std::string wjVon = readDate(ws, "B13");
    std::string wjBis = readDate(ws, "B14");
    std::string stichtag = readDate(ws, "B15");

    data.wirtschaftsjahrVon = parseDate(wjVon);
    data.wirtschaftsjahrBis = parseDate(wjBis);
    data.bilanzstichtag = parseDate(stichtag);

    data.wzCode = readString(ws, "B16");
    data.telefon = readString(ws, "B17");
    data.email = readString(ws, "B18");

    data.uebermittlungsanlass = normalizeUebermittlungsanlass(readString(ws, "B19"));
    data.bilanzaufstellung = "handelsrecht"; // Always handelsrecht
    data.erstjahr = readBool(ws, "B21");
    data.ergebnisverwendung = normalizeErgebnisverwendung(readString(ws, "B22"));
    data.ausschuettungsbetrag = readDouble(ws, "B23");
    data.herstellerID = readString(ws, "B24");
    data.datenLieferant = readString(ws, "B25");

    // Compute bundeseinheitliche Steuernummer if not provided
    if (data.steuernummerBund.empty() && !data.steuernummer.empty() && !data.finanzamtsNummer.empty()) {
        data.steuernummerBund = convertSteuernummer(data.steuernummer, data.finanzamtsNummer, bufa);
    }

    spdlog::debug("Stammdaten gelesen: {} (StNr: {})", data.firmenname, data.steuernummer);
    return data;
}

BilanzData ExcelParser::readBilanz(const std::string& filepath) const {
    spdlog::debug("Lese Bilanz aus: {}", filepath);

    xlnt::workbook wb;
    wb.load(filepath);
    auto ws = wb.sheet_by_title("Bilanz");

    BilanzData bilanz;

    // ── Aktiva — Anlagevermögen — I. Immaterielle VG ──
    bilanz.positionen["immaterielleVG_schutzrechte"] = readDouble(ws, "C6");
    bilanz.positionen["immaterielleVG_konzessionen"] = readDouble(ws, "C7");
    bilanz.positionen["immaterielleVG_firmenwert"] = readDouble(ws, "C8");
    bilanz.positionen["immaterielleVG_anzahlungen"] = readDouble(ws, "C9");
    bilanz.positionen["immaterielleVG_andere"] = readDouble(ws, "C10");
    bilanz.positionen["immaterielleVG_summe"] = readDouble(ws, "C11");

    // ── Aktiva — Anlagevermögen — II. Sachanlagen ──
    bilanz.positionen["sachanlagen_grundstuecke"] = readDouble(ws, "C14");
    bilanz.positionen["sachanlagen_maschinen"] = readDouble(ws, "C15");
    bilanz.positionen["sachanlagen_bga"] = readDouble(ws, "C16");
    bilanz.positionen["sachanlagen_anzahlungen"] = readDouble(ws, "C17");
    bilanz.positionen["sachanlagen_andere"] = readDouble(ws, "C18");
    bilanz.positionen["sachanlagen_summe"] = readDouble(ws, "C19");

    // ── Aktiva — Anlagevermögen — III. Finanzanlagen ──
    bilanz.positionen["finanzanlagen_verbundenU"] = readDouble(ws, "C22");
    bilanz.positionen["finanzanlagen_ausleihungenVerbundenU"] = readDouble(ws, "C23");
    bilanz.positionen["finanzanlagen_beteiligungen"] = readDouble(ws, "C24");
    bilanz.positionen["finanzanlagen_ausleihungenBeteiligungen"] = readDouble(ws, "C25");
    bilanz.positionen["finanzanlagen_wertpapiere"] = readDouble(ws, "C26");
    bilanz.positionen["finanzanlagen_sonstigeAusleihungen"] = readDouble(ws, "C27");
    bilanz.positionen["finanzanlagen_andere"] = readDouble(ws, "C28");
    bilanz.positionen["finanzanlagen_summe"] = readDouble(ws, "C29");

    bilanz.positionen["anlagevermoegen_summe"] = readDouble(ws, "C31");

    // ── Aktiva — Umlaufvermögen — I. Vorräte ──
    bilanz.positionen["vorraete_roh"] = readDouble(ws, "C35");
    bilanz.positionen["vorraete_unfertig"] = readDouble(ws, "C36");
    bilanz.positionen["vorraete_fertig"] = readDouble(ws, "C37");
    bilanz.positionen["vorraete_anzahlungen"] = readDouble(ws, "C38");
    bilanz.positionen["vorraete_summe"] = readDouble(ws, "C39");

    // ── Aktiva — Umlaufvermögen — Eingeforderte ausstehende Einlagen ──
    bilanz.positionen["forderungen_eingeforderte_einlagen"] = readDouble(ws, "C41");

    // ── Aktiva — Umlaufvermögen — II. Forderungen ──
    bilanz.positionen["forderungen_lul"] = readDouble(ws, "C42");
    bilanz.positionen["forderungen_verbundenU"] = readDouble(ws, "C43");
    bilanz.positionen["forderungen_beteiligungen"] = readDouble(ws, "C44");
    bilanz.positionen["forderungen_sonstige"] = readDouble(ws, "C45");
    bilanz.positionen["forderungen_summe"] = readDouble(ws, "C46");

    // ── Aktiva — Umlaufvermögen — III. Wertpapiere ──
    bilanz.positionen["wertpapiere_uv_verbundenU"] = readDouble(ws, "C49");
    bilanz.positionen["wertpapiere_uv_sonstige"] = readDouble(ws, "C50");
    bilanz.positionen["wertpapiere_uv_summe"] = readDouble(ws, "C51");

    // ── Aktiva — Umlaufvermögen — IV. Kasse/Bank ──
    bilanz.positionen["kassenbestand"] = readDouble(ws, "C53");

    bilanz.positionen["umlaufvermoegen_summe"] = readDouble(ws, "C55");

    // ── Aktiva — Sonstiges ──
    bilanz.positionen["rechnungsabgrenzung_aktiv"] = readDouble(ws, "C57");
    bilanz.positionen["latente_steuern_aktiv"] = readDouble(ws, "C58");
    bilanz.positionen["unterschiedsbetrag_aktiv"] = readDouble(ws, "C59");
    bilanz.positionen["fehlbetrag_nicht_durch_eigenkapital_gedeckt"] = readDouble(ws, "C60");

    bilanz.positionen["summe_aktiva"] = readDouble(ws, "C61");
    bilanz.summeAktiva = bilanz.positionen["summe_aktiva"];

    // ── Passiva — Eigenkapital ──
    bilanz.positionen["gezeichnetes_kapital"] = readDouble(ws, "C67");
    bilanz.positionen["ausstehende_einlagen_nicht_eingefordert"] = readDouble(ws, "C68");
    bilanz.positionen["kapitalruecklage"] = readDouble(ws, "C69");
    bilanz.positionen["gewinnruecklage_gesetzlich"] = readDouble(ws, "C70");
    bilanz.positionen["gewinnruecklage_herrschend"] = readDouble(ws, "C71");
    bilanz.positionen["gewinnruecklage_satzungsmaessig"] = readDouble(ws, "C72");
    bilanz.positionen["gewinnruecklage_andere"] = readDouble(ws, "C73");
    bilanz.positionen["gewinnruecklage_summe"] = readDouble(ws, "C74");
    bilanz.positionen["gewinnvortrag"] = readDouble(ws, "C75");
    bilanz.positionen["jahresueberschuss"] = readDouble(ws, "C76");
    bilanz.positionen["eigenkapital_summe"] = readDouble(ws, "C78");

    bilanz.gezeichnetesKapital = bilanz.positionen["gezeichnetes_kapital"];
    bilanz.jahresueberschuss = bilanz.positionen["jahresueberschuss"];

    // ── Passiva — Rückstellungen ──
    bilanz.positionen["rueckstellungen_pensionen"] = readDouble(ws, "C81");
    bilanz.positionen["rueckstellungen_steuern"] = readDouble(ws, "C82");
    bilanz.positionen["rueckstellungen_sonstige"] = readDouble(ws, "C83");
    bilanz.positionen["rueckstellungen_summe"] = readDouble(ws, "C84");

    // ── Passiva — Verbindlichkeiten ──
    bilanz.positionen["verbindlichkeiten_anleihen"] = readDouble(ws, "C87");
    bilanz.positionen["verbindlichkeiten_banken"] = readDouble(ws, "C88");
    bilanz.positionen["verbindlichkeiten_anzahlungen"] = readDouble(ws, "C89");
    bilanz.positionen["verbindlichkeiten_lul"] = readDouble(ws, "C90");
    bilanz.positionen["verbindlichkeiten_wechsel"] = readDouble(ws, "C91");
    bilanz.positionen["verbindlichkeiten_verbundenU"] = readDouble(ws, "C92");
    bilanz.positionen["verbindlichkeiten_beteiligungen"] = readDouble(ws, "C93");
    bilanz.positionen["verbindlichkeiten_sonstige"] = readDouble(ws, "C94");
    bilanz.positionen["verbindlichkeiten_summe"] = readDouble(ws, "C95");

    // ── Passiva — Sonstiges ──
    bilanz.positionen["rechnungsabgrenzung_passiv"] = readDouble(ws, "C97");
    bilanz.positionen["latente_steuern_passiv"] = readDouble(ws, "C98");

    bilanz.positionen["summe_passiva"] = readDouble(ws, "C100");
    bilanz.summePassiva = bilanz.positionen["summe_passiva"];

    // ── Vorjahreswerte (Spalte D) ──
    bilanz.vorjahrPositionen["immaterielleVG_schutzrechte"] = readDouble(ws, "D6");
    bilanz.vorjahrPositionen["immaterielleVG_konzessionen"] = readDouble(ws, "D7");
    bilanz.vorjahrPositionen["immaterielleVG_firmenwert"] = readDouble(ws, "D8");
    bilanz.vorjahrPositionen["immaterielleVG_anzahlungen"] = readDouble(ws, "D9");
    bilanz.vorjahrPositionen["immaterielleVG_andere"] = readDouble(ws, "D10");
    bilanz.vorjahrPositionen["immaterielleVG_summe"] = readDouble(ws, "D11");

    bilanz.vorjahrPositionen["sachanlagen_grundstuecke"] = readDouble(ws, "D14");
    bilanz.vorjahrPositionen["sachanlagen_maschinen"] = readDouble(ws, "D15");
    bilanz.vorjahrPositionen["sachanlagen_bga"] = readDouble(ws, "D16");
    bilanz.vorjahrPositionen["sachanlagen_anzahlungen"] = readDouble(ws, "D17");
    bilanz.vorjahrPositionen["sachanlagen_andere"] = readDouble(ws, "D18");
    bilanz.vorjahrPositionen["sachanlagen_summe"] = readDouble(ws, "D19");

    bilanz.vorjahrPositionen["finanzanlagen_verbundenU"] = readDouble(ws, "D22");
    bilanz.vorjahrPositionen["finanzanlagen_ausleihungenVerbundenU"] = readDouble(ws, "D23");
    bilanz.vorjahrPositionen["finanzanlagen_beteiligungen"] = readDouble(ws, "D24");
    bilanz.vorjahrPositionen["finanzanlagen_ausleihungenBeteiligungen"] = readDouble(ws, "D25");
    bilanz.vorjahrPositionen["finanzanlagen_wertpapiere"] = readDouble(ws, "D26");
    bilanz.vorjahrPositionen["finanzanlagen_sonstigeAusleihungen"] = readDouble(ws, "D27");
    bilanz.vorjahrPositionen["finanzanlagen_andere"] = readDouble(ws, "D28");
    bilanz.vorjahrPositionen["finanzanlagen_summe"] = readDouble(ws, "D29");

    bilanz.vorjahrPositionen["anlagevermoegen_summe"] = readDouble(ws, "D31");

    bilanz.vorjahrPositionen["vorraete_roh"] = readDouble(ws, "D35");
    bilanz.vorjahrPositionen["vorraete_unfertig"] = readDouble(ws, "D36");
    bilanz.vorjahrPositionen["vorraete_fertig"] = readDouble(ws, "D37");
    bilanz.vorjahrPositionen["vorraete_anzahlungen"] = readDouble(ws, "D38");
    bilanz.vorjahrPositionen["vorraete_summe"] = readDouble(ws, "D39");

    // ── Vorjahr — Eingeforderte ausstehende Einlagen ──
    bilanz.vorjahrPositionen["forderungen_eingeforderte_einlagen"] = readDouble(ws, "D41");

    bilanz.vorjahrPositionen["forderungen_lul"] = readDouble(ws, "D42");
    bilanz.vorjahrPositionen["forderungen_verbundenU"] = readDouble(ws, "D43");
    bilanz.vorjahrPositionen["forderungen_beteiligungen"] = readDouble(ws, "D44");
    bilanz.vorjahrPositionen["forderungen_sonstige"] = readDouble(ws, "D45");
    bilanz.vorjahrPositionen["forderungen_summe"] = readDouble(ws, "D46");

    bilanz.vorjahrPositionen["wertpapiere_uv_verbundenU"] = readDouble(ws, "D49");
    bilanz.vorjahrPositionen["wertpapiere_uv_sonstige"] = readDouble(ws, "D50");
    bilanz.vorjahrPositionen["wertpapiere_uv_summe"] = readDouble(ws, "D51");

    bilanz.vorjahrPositionen["kassenbestand"] = readDouble(ws, "D53");

    bilanz.vorjahrPositionen["umlaufvermoegen_summe"] = readDouble(ws, "D55");

    bilanz.vorjahrPositionen["rechnungsabgrenzung_aktiv"] = readDouble(ws, "D57");
    bilanz.vorjahrPositionen["latente_steuern_aktiv"] = readDouble(ws, "D58");
    bilanz.vorjahrPositionen["unterschiedsbetrag_aktiv"] = readDouble(ws, "D59");
    bilanz.vorjahrPositionen["fehlbetrag_nicht_durch_eigenkapital_gedeckt"] = readDouble(ws, "D60");

    bilanz.vorjahrPositionen["summe_aktiva"] = readDouble(ws, "D61");
    bilanz.vorjahr_summeAktiva = bilanz.vorjahrPositionen["summe_aktiva"];

    bilanz.vorjahrPositionen["gezeichnetes_kapital"] = readDouble(ws, "D67");
    bilanz.vorjahrPositionen["ausstehende_einlagen_nicht_eingefordert"] = readDouble(ws, "D68");
    bilanz.vorjahrPositionen["kapitalruecklage"] = readDouble(ws, "D69");
    bilanz.vorjahrPositionen["gewinnruecklage_gesetzlich"] = readDouble(ws, "D70");
    bilanz.vorjahrPositionen["gewinnruecklage_herrschend"] = readDouble(ws, "D71");
    bilanz.vorjahrPositionen["gewinnruecklage_satzungsmaessig"] = readDouble(ws, "D72");
    bilanz.vorjahrPositionen["gewinnruecklage_andere"] = readDouble(ws, "D73");
    bilanz.vorjahrPositionen["gewinnruecklage_summe"] = readDouble(ws, "D74");
    bilanz.vorjahrPositionen["gewinnvortrag"] = readDouble(ws, "D75");
    bilanz.vorjahrPositionen["jahresueberschuss"] = readDouble(ws, "D76");
    bilanz.vorjahrPositionen["eigenkapital_summe"] = readDouble(ws, "D78");

    bilanz.vorjahrPositionen["rueckstellungen_pensionen"] = readDouble(ws, "D81");
    bilanz.vorjahrPositionen["rueckstellungen_steuern"] = readDouble(ws, "D82");
    bilanz.vorjahrPositionen["rueckstellungen_sonstige"] = readDouble(ws, "D83");
    bilanz.vorjahrPositionen["rueckstellungen_summe"] = readDouble(ws, "D84");

    bilanz.vorjahrPositionen["verbindlichkeiten_anleihen"] = readDouble(ws, "D87");
    bilanz.vorjahrPositionen["verbindlichkeiten_banken"] = readDouble(ws, "D88");
    bilanz.vorjahrPositionen["verbindlichkeiten_anzahlungen"] = readDouble(ws, "D89");
    bilanz.vorjahrPositionen["verbindlichkeiten_lul"] = readDouble(ws, "D90");
    bilanz.vorjahrPositionen["verbindlichkeiten_wechsel"] = readDouble(ws, "D91");
    bilanz.vorjahrPositionen["verbindlichkeiten_verbundenU"] = readDouble(ws, "D92");
    bilanz.vorjahrPositionen["verbindlichkeiten_beteiligungen"] = readDouble(ws, "D93");
    bilanz.vorjahrPositionen["verbindlichkeiten_sonstige"] = readDouble(ws, "D94");
    bilanz.vorjahrPositionen["verbindlichkeiten_summe"] = readDouble(ws, "D95");

    bilanz.vorjahrPositionen["rechnungsabgrenzung_passiv"] = readDouble(ws, "D97");
    bilanz.vorjahrPositionen["latente_steuern_passiv"] = readDouble(ws, "D98");

    bilanz.vorjahrPositionen["summe_passiva"] = readDouble(ws, "D100");
    bilanz.vorjahr_summePassiva = bilanz.vorjahrPositionen["summe_passiva"];

    spdlog::debug("Bilanz gelesen: Aktiva={}, Passiva={}", bilanz.summeAktiva, bilanz.summePassiva);
    return bilanz;
}

GuVData ExcelParser::readGuV(const std::string& filepath) const {
    spdlog::debug("Lese GuV aus: {}", filepath);

    xlnt::workbook wb;
    wb.load(filepath);
    auto ws = wb.sheet_by_title("GuV");

    GuVData guv;

    // ── Erträge ──
    guv.positionen["guv_umsatzerloese"] = readDouble(ws, "C3");
    guv.positionen["guv_bestandsveraenderungen"] = readDouble(ws, "C4");
    guv.positionen["guv_eigenleistungen"] = readDouble(ws, "C5");
    guv.positionen["guv_sonstige_ertraege"] = readDouble(ws, "C6");

    // ── Materialaufwand ──
    guv.positionen["guv_material_roh"] = readDouble(ws, "C9");
    guv.positionen["guv_material_leistungen"] = readDouble(ws, "C10");
    guv.positionen["guv_material_summe"] = readDouble(ws, "C11");

    // ── Personalaufwand ──
    guv.positionen["guv_personal_loehne"] = readDouble(ws, "C14");
    guv.positionen["guv_personal_sozial"] = readDouble(ws, "C15");
    guv.positionen["guv_personal_summe"] = readDouble(ws, "C16");

    // ── Abschreibungen ──
    guv.positionen["guv_abschreibungen_immateriell_sach"] = readDouble(ws, "C19");
    guv.positionen["guv_abschreibungen_umlauf"] = readDouble(ws, "C20");
    guv.positionen["guv_abschreibungen_summe"] = readDouble(ws, "C21");

    // ── Sonstige betriebliche Aufwendungen ──
    guv.positionen["guv_sonstige_aufwendungen"] = readDouble(ws, "C23");

    // ── Finanzergebnis ──
    guv.positionen["guv_ertraege_beteiligungen"] = readDouble(ws, "C25");
    guv.positionen["guv_ertraege_wertpapiere"] = readDouble(ws, "C26");
    guv.positionen["guv_zins_ertraege"] = readDouble(ws, "C27");
    guv.positionen["guv_abschreibungen_finanz"] = readDouble(ws, "C28");
    guv.positionen["guv_zins_aufwendungen"] = readDouble(ws, "C29");

    // ── Ergebnis der gewöhnlichen Geschäftstätigkeit ──
    guv.positionen["guv_ergebnis_gewoehnlich"] = readDouble(ws, "C31");

    // ── Außerordentliches Ergebnis ──
    guv.positionen["guv_ao_ertraege"] = readDouble(ws, "C33");
    guv.positionen["guv_ao_aufwendungen"] = readDouble(ws, "C34");
    guv.positionen["guv_ao_ergebnis"] = readDouble(ws, "C35");

    // ── Steuern ──
    guv.positionen["guv_steuern_einkommen"] = readDouble(ws, "C37");
    guv.positionen["guv_steuern_sonstige"] = readDouble(ws, "C38");

    // ── Jahresüberschuss ──
    guv.positionen["guv_jahresueberschuss"] = readDouble(ws, "C40");
    guv.jahresueberschuss = guv.positionen["guv_jahresueberschuss"];

    // ── Vorjahreswerte (Spalte D) ──
    guv.vorjahrPositionen["guv_umsatzerloese"] = readDouble(ws, "D3");
    guv.vorjahrPositionen["guv_bestandsveraenderungen"] = readDouble(ws, "D4");
    guv.vorjahrPositionen["guv_eigenleistungen"] = readDouble(ws, "D5");
    guv.vorjahrPositionen["guv_sonstige_ertraege"] = readDouble(ws, "D6");

    guv.vorjahrPositionen["guv_material_roh"] = readDouble(ws, "D9");
    guv.vorjahrPositionen["guv_material_leistungen"] = readDouble(ws, "D10");
    guv.vorjahrPositionen["guv_material_summe"] = readDouble(ws, "D11");

    guv.vorjahrPositionen["guv_personal_loehne"] = readDouble(ws, "D14");
    guv.vorjahrPositionen["guv_personal_sozial"] = readDouble(ws, "D15");
    guv.vorjahrPositionen["guv_personal_summe"] = readDouble(ws, "D16");

    guv.vorjahrPositionen["guv_abschreibungen_immateriell_sach"] = readDouble(ws, "D19");
    guv.vorjahrPositionen["guv_abschreibungen_umlauf"] = readDouble(ws, "D20");
    guv.vorjahrPositionen["guv_abschreibungen_summe"] = readDouble(ws, "D21");

    guv.vorjahrPositionen["guv_sonstige_aufwendungen"] = readDouble(ws, "D23");

    guv.vorjahrPositionen["guv_ertraege_beteiligungen"] = readDouble(ws, "D25");
    guv.vorjahrPositionen["guv_ertraege_wertpapiere"] = readDouble(ws, "D26");
    guv.vorjahrPositionen["guv_zins_ertraege"] = readDouble(ws, "D27");
    guv.vorjahrPositionen["guv_abschreibungen_finanz"] = readDouble(ws, "D28");
    guv.vorjahrPositionen["guv_zins_aufwendungen"] = readDouble(ws, "D29");

    guv.vorjahrPositionen["guv_ergebnis_gewoehnlich"] = readDouble(ws, "D31");

    guv.vorjahrPositionen["guv_ao_ertraege"] = readDouble(ws, "D33");
    guv.vorjahrPositionen["guv_ao_aufwendungen"] = readDouble(ws, "D34");
    guv.vorjahrPositionen["guv_ao_ergebnis"] = readDouble(ws, "D35");

    guv.vorjahrPositionen["guv_steuern_einkommen"] = readDouble(ws, "D37");
    guv.vorjahrPositionen["guv_steuern_sonstige"] = readDouble(ws, "D38");

    guv.vorjahrPositionen["guv_jahresueberschuss"] = readDouble(ws, "D40");

    spdlog::debug("GuV gelesen: Jahresueberschuss={}", guv.jahresueberschuss);
    return guv;
}
