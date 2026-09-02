#include "validator.h"
#include "steuernummer.h"
#include "utils.h"

#include <spdlog/spdlog.h>

#include <cmath>
#include <regex>
#include <set>
#include <string>

namespace {

constexpr double EPSILON = 0.01;

void addError(ValidationResult& result, const std::string& rule, const std::string& message) {
    result.errors.push_back({rule, message});
    result.isValid = false;
    spdlog::error("Validierung [{}]: {}", rule, message);
}

void addWarning(ValidationResult& result, const std::string& rule, const std::string& message) {
    result.warnings.push_back({rule, message});
    spdlog::warn("Validierung [{}]: {}", rule, message);
}

bool isNonEmpty(const std::string& s) {
    return !s.empty();
}

} // anonymous namespace

ValidationResult Validator::validate(const Stammdaten& stamm,
                                      const BilanzData& bilanz,
                                      const GuVData& guv) const {
    ValidationResult result;

    // Rule 1: Bilanz ausgeglichen
    {
        double diff = std::abs(bilanz.summeAktiva - bilanz.summePassiva);
        if (diff >= EPSILON) {
            addError(result, "bilanz_ausgeglichen",
                     "Bilanz nicht ausgeglichen! Differenz: " + formatCurrency(diff) +
                     " EUR (Aktiva: " + formatCurrency(bilanz.summeAktiva) +
                     ", Passiva: " + formatCurrency(bilanz.summePassiva) + ")");
        }
    }

    // Rule 2: Jahresüberschuss konsistent (Bilanz == GuV)
    {
        double diff = std::abs(bilanz.jahresueberschuss - guv.jahresueberschuss);
        if (diff >= EPSILON) {
            addError(result, "jahresueberschuss_konsistent",
                     "Jahresüberschuss in Bilanz (" + formatCurrency(bilanz.jahresueberschuss) +
                     ") und GuV (" + formatCurrency(guv.jahresueberschuss) +
                     ") stimmen nicht überein! Differenz: " + formatCurrency(diff) + " EUR");
        }
    }

    // Rule 3: GmbH-Kapital >= 25.000 (ERROR, not warning — UG not supported)
    {
        if (bilanz.gezeichnetesKapital < 25000.0) {
            addError(result, "gmbh_kapital",
                     "Gezeichnetes Kapital (" + formatCurrency(bilanz.gezeichnetesKapital) +
                     " EUR) unter 25.000 EUR. Dieses Tool unterstützt nur GmbH, keine UG (haftungsbeschränkt).");
        }
    }

    // Rule 4: Steuernummer Format (XX/XXX/XXXXX)
    {
        if (!validateSteuernummerFormat(stamm.steuernummer)) {
            addError(result, "steuernummer_format",
                     "Steuernummer ungültiges Format: '" + stamm.steuernummer +
                     "' (erwartet: XX/XXX/XXXXX oder XXXXX/XXXXX)");
        }
    }

    // Rule 5: BuFa-Nummer Format (4 digits)
    {
        if (!validateBufaFormat(stamm.finanzamtsNummer)) {
            addError(result, "bufa_format",
                     "BuFa-Nummer ungültiges Format: '" + stamm.finanzamtsNummer +
                     "' (erwartet: 4-stellige Zahl)");
        }
    }

    // Rule 6: Bundeseinheitliche Steuernummer (13 digits)
    {
        if (!validateBundesSteuernummerFormat(stamm.steuernummerBund)) {
            addError(result, "bundessteuernummer_format",
                     "Bundeseinheitliche Steuernummer ungültig: '" + stamm.steuernummerBund +
                     "' (erwartet: 13-stellige Zahl)");
        }
    }

    // Rule 7: Handelsregister-Nummer (HRB XXXXX)
    {
        static const std::regex hrb_pattern(R"(HRB\s*\d+)");
        if (!std::regex_match(stamm.handelsregisterNr, hrb_pattern)) {
            addError(result, "handelsregister_format",
                     "Handelsregisternummer ungültiges Format: '" + stamm.handelsregisterNr +
                     "' (erwartet: HRB XXXXX)");
        }
    }

    // Rule 8: All Pflichtfelder non-empty
    {
        std::vector<std::pair<std::string, std::string>> pflichtfelder = {
            {"Steuernummer", stamm.steuernummer},
            {"Finanzamtsnummer", stamm.finanzamtsNummer},
            {"Firmenname", stamm.firmenname},
            {"Straße", stamm.strasse},
            {"PLZ", stamm.plz},
            {"Ort", stamm.ort},
            {"Geschäftsführer", stamm.geschaeftsfuehrer},
            {"Handelsregister-Nr.", stamm.handelsregisterNr},
            {"Registergericht", stamm.registergericht},
            {"Wirtschaftsjahr von", stamm.wirtschaftsjahrVon},
            {"Wirtschaftsjahr bis", stamm.wirtschaftsjahrBis},
            {"Bilanzstichtag", stamm.bilanzstichtag},
            {"Übermittlungsanlass", stamm.uebermittlungsanlass},
            {"Ergebnisverwendung", stamm.ergebnisverwendung},
        };

        std::vector<std::string> missing;
        for (const auto& [name, value] : pflichtfelder) {
            if (!isNonEmpty(value)) {
                missing.push_back(name);
            }
        }

        if (!missing.empty()) {
            std::string msg = "Pflichtfelder nicht ausgefüllt: ";
            for (size_t i = 0; i < missing.size(); ++i) {
                if (i > 0) msg += ", ";
                msg += missing[i];
            }
            addError(result, "pflichtfelder", msg);
        }
    }

    // Rule 9: Erstjahr consistency — Vorjahr values should be 0 (WARNING)
    {
        if (stamm.erstjahr) {
            bool hasNonZeroVorjahr = false;
            for (const auto& [key, value] : bilanz.vorjahrPositionen) {
                if (std::abs(value) >= EPSILON) {
                    hasNonZeroVorjahr = true;
                    break;
                }
            }
            if (!hasNonZeroVorjahr) {
                for (const auto& [key, value] : guv.vorjahrPositionen) {
                    if (std::abs(value) >= EPSILON) {
                        hasNonZeroVorjahr = true;
                        break;
                    }
                }
            }
            if (hasNonZeroVorjahr) {
                addWarning(result, "erstjahr_konsistenz",
                           "Erstjahr markiert, aber Vorjahreswerte ungleich 0 vorhanden. "
                           "Vorjahreswerte werden auf 0 gesetzt.");
            }
        }
    }

    // Rule 10: Ergebnisverwendung — Ausschüttungsbetrag > 0 when not Thesaurierung
    {
        static const std::set<std::string> knownVerwendung = {"thesaurierung", "ausschuettung", "teilausschuettung"};
        if (knownVerwendung.count(stamm.ergebnisverwendung) == 0) {
            addError(result, "ergebnisverwendung",
                     "Ergebnisverwendung '" + stamm.ergebnisverwendung +
                     "' unbekannt (erlaubt: Thesaurierung, Ausschüttung, Teilausschüttung)");
        } else if (stamm.ergebnisverwendung != "thesaurierung" && stamm.ausschuettungsbetrag <= 0.0) {

            addError(result, "ergebnisverwendung",
                     "Ergebnisverwendung ist '" + stamm.ergebnisverwendung +
                     "', aber kein Ausschüttungsbetrag angegeben (muss > 0).");
        }
    }

    // Rule 11: Wirtschaftsjahr duration <= 18 months (WARNING)
    {
        if (!stamm.wirtschaftsjahrVon.empty() && !stamm.wirtschaftsjahrBis.empty()) {
            int months = monthsBetween(stamm.wirtschaftsjahrVon, stamm.wirtschaftsjahrBis);
            if (months > 18) {
                addWarning(result, "wirtschaftsjahr_dauer",
                           "Wirtschaftsjahr dauert " + std::to_string(months) +
                           " Monate (maximal 18 Monate empfohlen).");
            }
        }
    }

    // Rule 12: Bilanzstichtag == Wirtschaftsjahr-Ende
    {
        if (!stamm.bilanzstichtag.empty() && !stamm.wirtschaftsjahrBis.empty()) {
            if (stamm.bilanzstichtag != stamm.wirtschaftsjahrBis) {
                addError(result, "bilanzstichtag",
                         "Bilanzstichtag (" + stamm.bilanzstichtag +
                         ") stimmt nicht mit Wirtschaftsjahr-Ende (" +
                         stamm.wirtschaftsjahrBis + ") überein.");
            }
        }
    }

    if (result.isValid) {
        spdlog::info("Validierung erfolgreich ({} Warnungen)", result.warnings.size());
    } else {
        spdlog::error("Validierung fehlgeschlagen: {} Fehler, {} Warnungen",
                      result.errors.size(), result.warnings.size());
    }

    return result;
}
