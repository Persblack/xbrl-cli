#include "elster_wrapper.h"
#include "logger.h"

#include <pugixml.hpp>
#include <spdlog/spdlog.h>

#include <chrono>
#include <iomanip>
#include <random>
#include <sstream>

// ── ElsterWrapper ───────────────────────────────────────────────────────────

ElsterWrapper::ElsterWrapper(const ElsterConfig& config)
    : config_(config) {
    if (config_.herstellerID.empty()) {
        throw std::runtime_error("HerstellerID darf nicht leer sein");
    }
}

std::string ElsterWrapper::generateNutzdatenTicket() const {
    // Generate UUID-like ticket using timestamp + random component
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1000, 9999);

    std::ostringstream oss;
    oss << timestamp << "-" << dis(gen);
    return oss.str();
}

std::string ElsterWrapper::getBundeslandCode(const std::string& bufaNummer) const {
    // Map BuFa prefix to Bundesland code
    // This is a simplified mapping - should ideally load from bufa-verzeichnis.json
    if (bufaNummer.empty() || bufaNummer.length() < 2) {
        return "XX";  // Fallback
    }

    std::string prefix = bufaNummer.substr(0, 2);

    // Mapping based on BuFa prefixes
    if (prefix == "28") return "BW";  // Baden-Württemberg
    if (prefix == "91" || prefix == "92") return "BY";  // Bayern
    if (prefix == "11") return "BE";  // Berlin
    if (prefix == "30") return "BB";  // Brandenburg
    if (prefix == "24") return "HB";  // Bremen
    if (prefix == "22") return "HH";  // Hamburg
    if (prefix == "26") return "HE";  // Hessen
    if (prefix == "40") return "MV";  // Mecklenburg-Vorpommern
    if (prefix == "23") return "NI";  // Niedersachsen
    if (prefix == "51" || prefix == "52" || prefix == "53") return "NW";  // NRW
    if (prefix == "27") return "RP";  // Rheinland-Pfalz
    if (prefix == "10") return "SL";  // Saarland
    if (prefix == "32") return "SN";  // Sachsen
    if (prefix == "31") return "ST";  // Sachsen-Anhalt
    if (prefix == "21") return "SH";  // Schleswig-Holstein
    if (prefix == "41") return "TH";  // Thüringen

    spdlog::warn("Unbekannte BuFa-Nummer: {} - verwende Fallback 'XX'", bufaNummer);
    return "XX";
}

std::string ElsterWrapper::wrap(const std::string& xbrlContent,
                                 const Stammdaten& stamm,
                                 bool testMode) const {
    spdlog::debug("ElsterWrapper: Wrapping XBRL content ({} bytes)", xbrlContent.size());

    // Create XML document for ElsterBasis envelope
    pugi::xml_document doc;

    // Add XML declaration
    auto decl = doc.prepend_child(pugi::node_declaration);
    decl.append_attribute("version") = "1.0";
    decl.append_attribute("encoding") = "UTF-8";

    // Root: <Elster>
    auto elster = doc.append_child("Elster");
    elster.append_attribute("xmlns") = config_.elsterNamespace.c_str();

    // ── TransferHeader ──
    auto transferHeader = elster.append_child("TransferHeader");
    transferHeader.append_attribute("version") = std::to_string(config_.version).c_str();

    transferHeader.append_child("Verfahren").text() = config_.verfahren.c_str();
    transferHeader.append_child("DatenArt").text() = "Bilanz";
    transferHeader.append_child("Vorgang").text() = config_.vorgang.c_str();

    // Testmerker only in test mode
    if (testMode) {
        transferHeader.append_child("Testmerker").text() = "700000004";
    }

    // Empfaenger (Bundesland)
    auto empfaenger = transferHeader.append_child("Empfaenger");
    empfaenger.append_attribute("id") = "L";
    std::string bundeslandCode = getBundeslandCode(stamm.finanzamtsNummer);
    empfaenger.append_child("Ziel").text() = bundeslandCode.c_str();

    transferHeader.append_child("HerstellerID").text() = config_.herstellerID.c_str();

    // DatenLieferant (use override or firmenname)
    std::string datenLieferant = config_.datenLieferant.empty() ? stamm.firmenname : config_.datenLieferant;
    transferHeader.append_child("DatenLieferant").text() = datenLieferant.c_str();

    // Datei (encryption/compression metadata)
    auto datei = transferHeader.append_child("Datei");
    datei.append_child("Verschluesselung").text() = "CMSEncryptedData";
    datei.append_child("Kompression").text() = "GZIP";
    datei.append_child("TransportSchluessel");  // Empty element

    // ── DatenTeil ──
    auto datenTeil = elster.append_child("DatenTeil");
    auto nutzdatenblock = datenTeil.append_child("Nutzdatenblock");

    // NutzdatenHeader
    auto nutzdatenHeader = nutzdatenblock.append_child("NutzdatenHeader");
    nutzdatenHeader.append_attribute("version") = std::to_string(config_.version).c_str();

    nutzdatenHeader.append_child("NutzdatenTicket").text() = generateNutzdatenTicket().c_str();

    // Empfaenger (Finanzamt)
    auto empfaengerFA = nutzdatenHeader.append_child("Empfaenger");
    empfaengerFA.append_attribute("id") = "F";
    empfaengerFA.text() = stamm.finanzamtsNummer.c_str();

    // ── Nutzdaten (XBRL content) ──
    auto nutzdaten = nutzdatenblock.append_child("Nutzdaten");

    // Parse XBRL content and insert into Nutzdaten
    pugi::xml_document xbrlDoc;
    pugi::xml_parse_result parseResult = xbrlDoc.load_string(xbrlContent.c_str());

    if (!parseResult) {
        std::string error = "XBRL-Parsing fehlgeschlagen: ";
        error += parseResult.description();
        spdlog::error(error);
        throw std::runtime_error(error);
    }

    // Copy XBRL root element into Nutzdaten
    nutzdaten.append_copy(xbrlDoc.document_element());

    // Serialize to string
    std::ostringstream oss;
    doc.save(oss, "  ", pugi::format_default, pugi::encoding_utf8);

    std::string result = oss.str();
    spdlog::debug("ElsterWrapper: Generated wrapped XML ({} bytes)", result.size());

    return result;
}
