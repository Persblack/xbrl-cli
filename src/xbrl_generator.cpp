#include "xbrl_generator.h"

#include <pugixml.hpp>
#include <spdlog/spdlog.h>

#include <cmath>
#include <iomanip>
#include <set>
#include <sstream>

namespace {

constexpr double EPSILON = 0.01;

std::string formatDecimal(double value) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << value;
    return oss.str();
}

void createContext(pugi::xml_node& root, const std::string& id,
                   const std::string& startDate, const std::string& endDate,
                   const std::string& steuernummerBund) {
    auto context = root.append_child("xbrli:context");
    context.append_attribute("id") = id.c_str();

    auto entity = context.append_child("xbrli:entity");
    auto identifier = entity.append_child("xbrli:identifier");
    identifier.append_attribute("scheme") = "http://www.rzf-nrw.de/Steuernummer";
    identifier.text().set(steuernummerBund.c_str());

    auto period = context.append_child("xbrli:period");
    period.append_child("xbrli:startDate").text().set(startDate.c_str());
    period.append_child("xbrli:endDate").text().set(endDate.c_str());
}

void createInstantContext(pugi::xml_node& root, const std::string& id,
                          const std::string& date,
                          const std::string& steuernummerBund) {
    auto context = root.append_child("xbrli:context");
    context.append_attribute("id") = id.c_str();

    auto entity = context.append_child("xbrli:entity");
    auto identifier = entity.append_child("xbrli:identifier");
    identifier.append_attribute("scheme") = "http://www.rzf-nrw.de/Steuernummer";
    identifier.text().set(steuernummerBund.c_str());

    auto period = context.append_child("xbrli:period");
    period.append_child("xbrli:instant").text().set(date.c_str());
}

void addMonetaryFact(pugi::xml_node& root, const std::string& tag,
                     double value, const std::string& contextRef,
                     const std::string& unitRef) {
    auto fact = root.append_child(tag.c_str());
    fact.append_attribute("contextRef") = contextRef.c_str();
    fact.append_attribute("unitRef") = unitRef.c_str();
    fact.append_attribute("decimals") = "2";
    fact.text().set(formatDecimal(value).c_str());
}

void addStringFact(pugi::xml_node& root, const std::string& tag,
                   const std::string& value, const std::string& contextRef) {
    if (value.empty()) return;
    auto fact = root.append_child(tag.c_str());
    fact.append_attribute("contextRef") = contextRef.c_str();
    fact.text().set(value.c_str());
}

} // anonymous namespace

XBRLGenerator::XBRLGenerator(const TaxonomieConfig& config)
    : config_(config) {}

std::string XBRLGenerator::generate(const Stammdaten& stamm,
                                     const BilanzData& bilanz,
                                     const GuVData& guv) const {
    spdlog::debug("Generiere XBRL (Taxonomie v{})", config_.version);

    pugi::xml_document doc;

    // XML declaration
    auto decl = doc.prepend_child(pugi::node_declaration);
    decl.append_attribute("version") = "1.0";
    decl.append_attribute("encoding") = "UTF-8";

    // Root element with namespaces
    auto root = doc.append_child("xbrli:xbrl");
    root.append_attribute("xmlns:xbrli") = "http://www.xbrl.org/2003/instance";
    root.append_attribute("xmlns:link") = config_.linkNamespace.c_str();
    root.append_attribute("xmlns:xlink") = config_.xlinkNamespace.c_str();
    root.append_attribute("xmlns:de-gaap-ci") = config_.namespaceUri.c_str();
    root.append_attribute("xmlns:iso4217") = "http://www.xbrl.org/2003/iso4217";
    root.append_attribute("xmlns:xbrldi") = "http://xbrl.org/2006/xbrldi";

    // Schema reference
    auto schemaRef = root.append_child("link:schemaRef");
    schemaRef.append_attribute("xlink:type") = "simple";
    schemaRef.append_attribute("xlink:href") = config_.schemaRef.c_str();

    // ── Contexts ──

    // Current period (duration) — for GuV and Stammdaten
    createContext(root, "current", stamm.wirtschaftsjahrVon,
                  stamm.wirtschaftsjahrBis, stamm.steuernummerBund);

    // Current instant — for Bilanz positions
    createInstantContext(root, "current_instant", stamm.bilanzstichtag,
                         stamm.steuernummerBund);

    // Prior period (duration) — even for Erstjahr!
    std::string vorjahrVon = calculateVorjahrVon(stamm);
    std::string vorjahrBis = calculateVorjahrBis(stamm);
    createContext(root, "prior", vorjahrVon, vorjahrBis, stamm.steuernummerBund);

    // Prior instant — even for Erstjahr!
    createInstantContext(root, "prior_instant", vorjahrBis, stamm.steuernummerBund);

    // ── Unit (EUR) ──
    auto unit = root.append_child("xbrli:unit");
    unit.append_attribute("id") = "EUR";
    unit.append_child("xbrli:measure").text().set("iso4217:EUR");

    // ── Stammdaten as XBRL facts ──
    for (const auto& [field, tag] : config_.stammdatenMapping) {
        std::string value;
        if (field == "firmenname") value = stamm.firmenname;
        else if (field == "rechtsform") value = "GmbH";
        else if (field == "strasse") value = stamm.strasse;
        else if (field == "plz") value = stamm.plz;
        else if (field == "ort") value = stamm.ort;
        else if (field == "geschaeftsfuehrer") value = stamm.geschaeftsfuehrer;
        else if (field == "handelsregisterNr") value = stamm.handelsregisterNr;
        else if (field == "registergericht") value = stamm.registergericht;
        else if (field == "wirtschaftsjahrVon") value = stamm.wirtschaftsjahrVon;
        else if (field == "wirtschaftsjahrBis") value = stamm.wirtschaftsjahrBis;
        else if (field == "bilanzstichtag") value = stamm.bilanzstichtag;
        else if (field == "wzCode") value = stamm.wzCode;
        else if (field == "bilanzaufstellung") value = stamm.bilanzaufstellung;
        else if (field == "uebermittlungsanlass") value = stamm.uebermittlungsanlass;
        else if (field == "ergebnisverwendung") value = stamm.ergebnisverwendung;
        else if (field == "ausschuettungsbetrag") {
            if (stamm.ausschuettungsbetrag > 0.0) {
                addMonetaryFact(root, tag, stamm.ausschuettungsbetrag, "current", "EUR");
            }
            continue;
        }

        addStringFact(root, tag, value, "current");
    }

    // ── Bilanz positions (current year) in current_instant context ──
    // Track which tags have been written (for Mussfelder check)
    std::set<std::string> writtenTags;

    for (const auto& [posId, value] : bilanz.positionen) {
        auto it = config_.mapping.find(posId);
        if (it == config_.mapping.end()) {
            spdlog::warn("Keine XBRL-Zuordnung für Position: {}", posId);
            continue;
        }
        addMonetaryFact(root, it->second, value, "current_instant", "EUR");
        writtenTags.insert(it->second);
    }

    // ── Auffangpositionen logic ──
    for (const auto& [groupName, auffang] : config_.auffangpositionen) {
        // Check if all detail positions are 0
        bool allDetailZero = true;
        for (const auto& detailKey : auffang.detailKeys) {
            auto it = bilanz.positionen.find(detailKey);
            if (it != bilanz.positionen.end() && std::abs(it->second) >= EPSILON) {
                allDetailZero = false;
                break;
            }
        }

        // Check if the sum position is non-zero
        auto sumIt = bilanz.positionen.find(auffang.summeKey);
        double sumValue = (sumIt != bilanz.positionen.end()) ? sumIt->second : 0.0;

        // If all details are 0 but sum > 0, write to Auffangposition
        if (allDetailZero && std::abs(sumValue) >= EPSILON) {
            addMonetaryFact(root, auffang.auffangTag, sumValue, "current_instant", "EUR");
            writtenTags.insert(auffang.auffangTag);
            spdlog::debug("Auffangposition {}: {} = {}", groupName, auffang.auffangTag, sumValue);
        }
    }

    // ── GuV positions (current year) in current duration context ──
    for (const auto& [posId, value] : guv.positionen) {
        auto it = config_.mapping.find(posId);
        if (it == config_.mapping.end()) {
            spdlog::warn("Keine XBRL-Zuordnung für GuV-Position: {}", posId);
            continue;
        }
        addMonetaryFact(root, it->second, value, "current", "EUR");
        writtenTags.insert(it->second);
    }

    // ── Prior year Bilanz positions in prior_instant context ──
    for (const auto& [posId, value] : bilanz.vorjahrPositionen) {
        auto it = config_.mapping.find(posId);
        if (it == config_.mapping.end()) continue;
        addMonetaryFact(root, it->second, value, "prior_instant", "EUR");
    }

    // ── Prior year GuV positions in prior duration context ──
    for (const auto& [posId, value] : guv.vorjahrPositionen) {
        auto it = config_.mapping.find(posId);
        if (it == config_.mapping.end()) continue;
        addMonetaryFact(root, it->second, value, "prior", "EUR");
    }

    // ── Mussfelder: ensure mandatory fields are present ──
    for (const auto& mussfeld : config_.mussfelder) {
        if (writtenTags.find(mussfeld) == writtenTags.end()) {
            // Write 0 for missing mandatory fields in current context
            // Determine if it's a balance sheet (instant) or income statement (duration) field
            if (mussfeld.find("bs.") != std::string::npos) {
                addMonetaryFact(root, mussfeld, 0.0, "current_instant", "EUR");
            } else {
                addMonetaryFact(root, mussfeld, 0.0, "current", "EUR");
            }
            spdlog::debug("Mussfeld mit 0 befüllt: {}", mussfeld);
        }
    }

    // ── Serialize to string ──
    std::ostringstream oss;
    doc.save(oss, "  ");
    std::string result = oss.str();

    spdlog::info("XBRL generiert: {} Bytes, {} Positionen", result.size(), writtenTags.size());
    return result;
}

std::string XBRLGenerator::calculateVorjahrVon(const Stammdaten& stamm) const {
    // Prior year start: one year before wirtschaftsjahrVon
    if (stamm.wirtschaftsjahrVon.size() < 10) return "";

    int year = std::stoi(stamm.wirtschaftsjahrVon.substr(0, 4));
    return std::to_string(year - 1) + stamm.wirtschaftsjahrVon.substr(4);
}

std::string XBRLGenerator::calculateVorjahrBis(const Stammdaten& stamm) const {
    // Prior year end: one year before wirtschaftsjahrBis
    if (stamm.wirtschaftsjahrBis.size() < 10) return "";

    int year = std::stoi(stamm.wirtschaftsjahrBis.substr(0, 4));
    return std::to_string(year - 1) + stamm.wirtschaftsjahrBis.substr(4);
}
