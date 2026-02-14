#include "elster_wrapper.h"
#include "eric_connector.h"
#include "excel_parser.h"
#include "logger.h"
#include "models.h"
#include "taxonomie_config.h"
#include "utils.h"
#include "validator.h"
#include "xbrl_generator.h"

#include <CLI/CLI.hpp>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

static constexpr const char* VERSION = "1.1.0";

// ── Console output helpers ──────────────────────────────────────────────────

void printHeader() {
    std::cout << "E-Bilanz Submitter v" << VERSION << "\n";
    std::cout << std::string(49, '=') << "\n";
}

void printStep(int step, int total, const std::string& description) {
    std::cout << "[" << step << "/" << total << "] " << description << "..." << std::flush;
}

void printOk() {
    std::cout << "  OK\n";
}

void printFail() {
    std::cout << "  FEHLER\n";
}

// ── Transfer History ────────────────────────────────────────────────────────

void saveTransferHistory(const TransferHistoryEntry& entry) {
    std::string homeDir;
#ifdef _WIN32
    homeDir = std::getenv("USERPROFILE") ? std::getenv("USERPROFILE") : ".";
#else
    homeDir = std::getenv("HOME") ? std::getenv("HOME") : ".";
#endif
    fs::path histDir = fs::path(homeDir) / ".ebilanz";
    fs::path histFile = histDir / "history.json";

    // Create directory if needed
    fs::create_directories(histDir);

    // Load existing history
    nlohmann::json history;
    if (fs::exists(histFile)) {
        std::ifstream in(histFile);
        if (in.is_open()) {
            try {
                in >> history;
            } catch (...) {
                history = nlohmann::json::object();
            }
        }
    }

    if (!history.contains("transfers")) {
        history["transfers"] = nlohmann::json::array();
    }

    // Add new entry (NO financial data — only ticket + metadata)
    nlohmann::json e;
    e["timestamp"] = entry.timestamp;
    e["ticket"] = entry.transferTicket;
    e["steuernummer"] = entry.steuernummer;
    e["wirtschaftsjahr"] = entry.wirtschaftsjahr;
    e["testMode"] = entry.testMode;
    e["xbrlFile"] = entry.xbrlFile;
    history["transfers"].push_back(e);

    // Save
    std::ofstream out(histFile);
    out << history.dump(2);
}

// ── Main Pipeline ───────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    CLI::App app{"E-Bilanz Submitter — Elektronische Bilanzübermittlung ans Finanzamt"};
    app.get_formatter()->column_width(35);

    // Positional: Excel file
    std::string excelFile;
    app.add_option("bilanz", excelFile, "Excel-Datei mit Bilanzdaten (.xlsx)")
        ->check(CLI::ExistingFile);

    // Modes
    bool validateOnly = false;
    bool testMode = false;
    app.add_flag("-v,--validate", validateOnly, "Nur validieren, nicht senden");
    app.add_flag("-t,--test", testMode, "Test-Modus (Vorproduktion)");

    // Certificate
    std::string certFile;
    std::string certPassword;
    app.add_option("-c,--cert", certFile, "ELSTER-Zertifikat (.pfx)")
        ->check(CLI::ExistingFile);
    app.add_option("-p,--password", certPassword, "Zertifikat-Passwort (sonst interaktiv)");

    // Output
    std::string outputDir;
    app.add_option("-o,--output", outputDir, "XBRL-Datei speichern in Verzeichnis");

    // Direct XBRL submit
    std::string xbrlFile;
    app.add_option("-x,--xbrl", xbrlFile, "XBRL-Datei direkt senden (überspringt Excel-Parsing)")
        ->check(CLI::ExistingFile);

    // HerstellerID
    std::string herstellerID;
    app.add_option("--hersteller-id", herstellerID, "ELSTER HerstellerID (erforderlich für Versand)");

    // Logging
    bool verbose = false;
    bool quiet = false;
    std::string logFile = "ebilanz.log";
    app.add_flag("--verbose", verbose, "Ausführliche Ausgabe (Debug-Level)");
    app.add_flag("--quiet", quiet, "Nur Fehler ausgeben");
    app.add_option("--log", logFile, "Log-Datei (Standard: ./ebilanz.log)");

    // Config paths
    std::string taxonomieConfig = "config/taxonomie-6.8.json";
    std::string bufaConfig = "config/bufa-verzeichnis.json";
    app.add_option("--taxonomie", taxonomieConfig, "Taxonomie-Config-Datei")->capture_default_str();
    app.add_option("--bufa", bufaConfig, "BuFa-Verzeichnis-Datei")->capture_default_str();

    // Version
    app.set_version_flag("--version", std::string("E-Bilanz Submitter v") + VERSION);

    CLI11_PARSE(app, argc, argv);

    // Setup logger
    setupLogger(logFile, verbose, quiet);
    spdlog::info("E-Bilanz Submitter v{} gestartet", VERSION);

    if (!quiet) {
        printHeader();
    }

    // Determine pipeline steps
    bool directXbrl = !xbrlFile.empty();
    bool outputOnly = !outputDir.empty() && certFile.empty() && !validateOnly;
    bool needSubmit = !validateOnly && !outputOnly;
    int totalSteps = directXbrl ? 2 : (validateOnly ? 2 : (outputOnly ? 3 : 5));

    // Validate that required args are provided
    if (!directXbrl && excelFile.empty()) {
        std::cerr << "FEHLER: Bitte eine Excel-Datei oder --xbrl angeben.\n";
        std::cerr << "Hilfe: " << argv[0] << " --help\n";
        return static_cast<int>(ErrorCode::EXCEL_FILE_NOT_FOUND);
    }

    if (needSubmit && !directXbrl && certFile.empty()) {
        std::cerr << "FEHLER: Für den Versand wird ein Zertifikat benötigt (--cert).\n";
        return static_cast<int>(ErrorCode::ERIC_CERT_LOAD_FAILED);
    }

    try {
        // ── Load configs ──
        TaxonomieConfig taxConfig;
        BufaVerzeichnis bufaVerz;

        try {
            taxConfig = loadTaxonomieConfig(taxonomieConfig);
            bufaVerz = loadBufaVerzeichnis(bufaConfig);
        } catch (const std::exception& ex) {
            std::cerr << "FEHLER: Config laden fehlgeschlagen: " << ex.what() << "\n";
            return static_cast<int>(ErrorCode::CONFIG_LOAD_FAILED);
        }

        Stammdaten stamm;
        BilanzData bilanz;
        GuVData guv;
        std::string xbrlContent;

        if (directXbrl) {
            // ── Direct XBRL mode ──
            if (!quiet) printStep(1, totalSteps, "XBRL-Datei laden");
            xbrlContent = readFileContent(xbrlFile);
            if (!quiet) printOk();
            spdlog::info("XBRL-Datei geladen: {} ({} Bytes)", xbrlFile, xbrlContent.size());

        } else {
            // ── Step 1: Parse Excel ──
            if (!quiet) printStep(1, totalSteps, "Excel-Datei einlesen");

            ExcelParser parser;
            try {
                stamm = parser.readStammdaten(excelFile, bufaVerz);
                bilanz = parser.readBilanz(excelFile);
                guv = parser.readGuV(excelFile);
            } catch (const std::exception& ex) {
                if (!quiet) printFail();
                std::cerr << "FEHLER: Excel-Datei konnte nicht gelesen werden: " << ex.what() << "\n";
                return static_cast<int>(ErrorCode::EXCEL_PARSE_ERROR);
            }
            if (!quiet) printOk();

            // ── Step 2: Validate ──
            if (!quiet) printStep(2, totalSteps, "Validierung");

            Validator validator;
            auto validationResult = validator.validate(stamm, bilanz, guv);

            // Print validation details
            if (!quiet) {
                std::cout << "\n";
                if (validationResult.isValid) {
                    std::cout << "  Bilanz ausgeglichen (" << formatCurrency(bilanz.summeAktiva) << " EUR)  OK\n";
                    std::cout << "  Jahresüberschuss konsistent  OK\n";
                    std::cout << "  Alle Pflichtfelder vorhanden  OK\n";
                    std::cout << "  Steuernummer konvertiert (" << stamm.steuernummerBund << ")  OK\n";
                    std::cout << "  Formale Prüfung  OK\n";
                }

                for (const auto& w : validationResult.warnings) {
                    std::cout << "  WARNUNG: " << w.message << "\n";
                }

                for (const auto& e : validationResult.errors) {
                    std::cerr << "  FEHLER: " << e.message << "\n";
                }
            }

            if (!validationResult.isValid) {
                std::cerr << "\nValidierung fehlgeschlagen! Bitte Excel-Datei prüfen.\n";
                return static_cast<int>(ErrorCode::VALIDATION_FAILED);
            }

            if (validateOnly) {
                if (!quiet) {
                    std::cout << "\nValidierung erfolgreich! Keine Fehler gefunden.\n";
                }
                return static_cast<int>(ErrorCode::SUCCESS);
            }

            // ── Step 3: Generate XBRL ──
            if (!quiet) printStep(3, totalSteps, "XBRL-Datei generieren");

            XBRLGenerator generator(taxConfig);
            try {
                xbrlContent = generator.generate(stamm, bilanz, guv);
            } catch (const std::exception& ex) {
                if (!quiet) printFail();
                std::cerr << "FEHLER: XBRL-Generierung fehlgeschlagen: " << ex.what() << "\n";
                return static_cast<int>(ErrorCode::XBRL_GENERATION_FAILED);
            }
            if (!quiet) printOk();

            // Save XBRL if output dir specified
            if (!outputDir.empty()) {
                fs::create_directories(outputDir);
                std::string year = extractYear(stamm.wirtschaftsjahrBis);
                fs::path outPath = fs::path(outputDir) / ("ebilanz-" + year + ".xbrl");
                std::ofstream out(outPath);
                out << xbrlContent;
                spdlog::info("XBRL gespeichert: {}", outPath.string());
                if (!quiet) {
                    std::cout << "XBRL gespeichert: " << outPath.string() << "\n";
                }

                // Also generate wrapped XML for debugging (if HerstellerID provided)
                std::string effectiveHerstellerID = herstellerID.empty() ? stamm.herstellerID : herstellerID;
                if (!effectiveHerstellerID.empty()) {
                    try {
                        ElsterConfig elsterConfig;
                        elsterConfig.herstellerID = effectiveHerstellerID;
                        elsterConfig.datenLieferant = stamm.datenLieferant.empty() ? stamm.firmenname : stamm.datenLieferant;

                        ElsterWrapper wrapper(elsterConfig);
                        std::string wrappedXml = wrapper.wrap(xbrlContent, stamm, testMode);

                        fs::path wrappedPath = fs::path(outputDir) / ("ebilanz-" + year + "-elster.xml");
                        std::ofstream wrappedOut(wrappedPath);
                        wrappedOut << wrappedXml;
                        spdlog::info("ElsterBasis XML gespeichert: {}", wrappedPath.string());
                        if (!quiet) {
                            std::cout << "ElsterBasis XML gespeichert: " << wrappedPath.string() << "\n";
                        }
                    } catch (const std::exception& ex) {
                        spdlog::warn("Warnung: ElsterBasis-Wrapping fehlgeschlagen: {}", ex.what());
                    }
                }
            }

            // If output-only mode (no cert provided), stop here
            if (outputOnly) {
                if (!quiet) {
                    std::cout << "\nXBRL-Datei erfolgreich generiert.\n";
                }
                return static_cast<int>(ErrorCode::SUCCESS);
            }

            // ── Wrap XBRL in ElsterBasis envelope for ERiC submission ──
            if (!validateOnly && !outputOnly) {
                // Determine HerstellerID: CLI arg > Excel field
                std::string effectiveHerstellerID = herstellerID.empty() ? stamm.herstellerID : herstellerID;

                if (effectiveHerstellerID.empty()) {
                    std::cerr << "FEHLER: HerstellerID erforderlich für Versand.\n";
                    std::cerr << "Bitte angeben: --hersteller-id XXXXX oder in Excel-Datei (Zeile 24)\n";
                    std::cerr << "HerstellerID beantragen: https://www.elster.de/elsterweb/entwickler\n";
                    return static_cast<int>(ErrorCode::VALIDATION_FAILED);
                }

                ElsterConfig elsterConfig;
                elsterConfig.herstellerID = effectiveHerstellerID;
                elsterConfig.datenLieferant = stamm.datenLieferant.empty() ? stamm.firmenname : stamm.datenLieferant;

                ElsterWrapper wrapper(elsterConfig);
                xbrlContent = wrapper.wrap(xbrlContent, stamm, testMode);

                // Save wrapped XML if output dir specified
                if (!outputDir.empty()) {
                    std::string year = extractYear(stamm.wirtschaftsjahrBis);
                    fs::path wrappedPath = fs::path(outputDir) / ("ebilanz-" + year + "-elster.xml");
                    std::ofstream wrappedOut(wrappedPath);
                    wrappedOut << xbrlContent;
                    spdlog::info("ElsterBasis XML gespeichert: {}", wrappedPath.string());
                    if (!quiet) {
                        std::cout << "ElsterBasis XML gespeichert: " << wrappedPath.string() << "\n";
                    }
                }
            }
        }

        // ── Step 4: ERiC Validation ──
        int ericStepStart = directXbrl ? 2 : 4;
        if (!quiet) printStep(ericStepStart, totalSteps, "ERiC-Validierung");

        ERiCConnector eric;
        if (!eric.isAvailable()) {
            if (!quiet) {
                std::cout << "  ÜBERSPRUNGEN (ERiC-SDK nicht verfügbar)\n";
            }
            spdlog::warn("ERiC-SDK nicht verfügbar — Validierung und Versand übersprungen");

            if (!quiet) {
                std::cout << "\nHinweis: ERiC-SDK nicht installiert. XBRL-Datei wurde generiert.\n";
                std::cout << "Installieren Sie das ERiC-SDK für den Versand ans Finanzamt.\n";
            }
            return static_cast<int>(ErrorCode::SUCCESS);
        }

        // Load certificate
        if (certPassword.empty() && !certFile.empty()) {
            certPassword = readPassword();
        }

        if (!certFile.empty()) {
            if (!eric.loadCertificate(certFile, certPassword)) {
                if (!quiet) printFail();
                secureZero(certPassword);
                std::cerr << "FEHLER: ELSTER-Zertifikat konnte nicht geladen werden.\n";
                return static_cast<int>(ErrorCode::ERIC_CERT_LOAD_FAILED);
            }
        }

        // Clear password from memory
        secureZero(certPassword);

        auto ericValidation = eric.validate(xbrlContent);
        if (!ericValidation.isValid) {
            if (!quiet) printFail();
            for (const auto& e : ericValidation.errors) {
                std::cerr << "ERiC-Fehler: " << e.message << "\n";
            }
            return static_cast<int>(ErrorCode::ERIC_VALIDATION_FAILED);
        }
        if (!quiet) printOk();

        // ── Step 5: Submit ──
        if (!quiet) printStep(ericStepStart + 1, totalSteps, testMode ? "An Finanzamt senden (TEST)" : "An Finanzamt senden");

        auto submitResult = eric.submit(xbrlContent, testMode);

        if (!submitResult.success) {
            if (!quiet) printFail();
            std::cerr << "FEHLER: " << submitResult.message << "\n";
            return static_cast<int>(submitResult.errorCode);
        }
        if (!quiet) printOk();

        // Print success info
        if (!quiet) {
            std::cout << "\nTransfer-Ticket: " << submitResult.transferTicket << "\n";
            std::cout << "Status: Erfolgreich übermittelt" << (testMode ? " (Vorproduktion)" : "") << "\n";
            std::cout << "Datum: " << currentTimestamp() << "\n";
        }

        // Save to transfer history (no financial data!)
        TransferHistoryEntry histEntry;
        histEntry.timestamp = currentTimestamp();
        histEntry.transferTicket = submitResult.transferTicket;
        histEntry.steuernummer = stamm.steuernummer;
        histEntry.wirtschaftsjahr = extractYear(stamm.wirtschaftsjahrBis);
        histEntry.testMode = testMode;
        if (!outputDir.empty()) {
            histEntry.xbrlFile = (fs::path(outputDir) / ("ebilanz-" + histEntry.wirtschaftsjahr + ".xbrl")).string();
        }
        saveTransferHistory(histEntry);

        spdlog::info("Übermittlung erfolgreich. Ticket: {}", submitResult.transferTicket);

    } catch (const std::exception& ex) {
        spdlog::error("Unerwarteter Fehler: {}", ex.what());
        std::cerr << "Unerwarteter Fehler: " << ex.what() << "\n";
        std::cerr << "Details in Log-Datei: " << logFile << "\n";
        return static_cast<int>(ErrorCode::UNKNOWN_ERROR);
    }

    return static_cast<int>(ErrorCode::SUCCESS);
}
