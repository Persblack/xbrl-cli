#include "eric_connector.h"
#include "utils.h"

#include <spdlog/spdlog.h>

#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>

#ifdef WITH_ERIC
#include "ericapi.h"
#include "eric_types.h"
#include "eric_fehlercodes.h"
#endif

namespace fs = std::filesystem;

#ifdef WITH_ERIC
namespace {

// RAII for EricRueckgabepuffer: freed on every exit path, no leak when a second
// Erzeugen() fails. h == 0 means allocation failed.
struct Puffer {
    EricRueckgabepufferHandle h = EricRueckgabepufferErzeugen();
    Puffer() = default;
    Puffer(const Puffer&) = delete;
    Puffer& operator=(const Puffer&) = delete;
    ~Puffer() {
        if (h) EricRueckgabepufferFreigeben(h);
    }
    // Buffer content, never nullptr
    const char* text() const {
        const char* t = h ? EricRueckgabepufferInhalt(h) : nullptr;
        return t ? t : "";
    }
};

std::string ericErrorText(int rc) {
    Puffer buf;
    if (buf.h) {
        EricHoleFehlerText(rc, buf.h);
        std::string t = buf.text();
        if (!t.empty()) return t;
    }
    return "Unbekannter ERiC-Fehler (Code " + std::to_string(rc) + ")";
}

fs::path ebilanzDir() {
    return fs::path(userHomeDir()) / ".ebilanz";
}

} // namespace
#endif

bool ericSdkAvailable() {
#ifdef WITH_ERIC
    return true;
#else
    return false;
#endif
}

ERiCConnector::ERiCConnector() {
#ifdef WITH_ERIC
    spdlog::debug("ERiC-SDK Initialisierung...");

    // Plugins: nullptr → ERiC searches next to libericapi itself (works from any cwd).
    // Log dir: ~/.ebilanz/logs (falls back to the OS temp dir if it cannot be created).
    fs::path logDir = ebilanzDir() / "logs";
    std::error_code ec;
    fs::create_directories(logDir, ec);
    std::string logDirStr = logDir.string();

    int rc = EricInitialisiere(nullptr, ec ? nullptr : logDirStr.c_str());
    initialized_ = (rc == ERIC_OK);

    if (!initialized_) {
        spdlog::error("ERiC Initialisierung fehlgeschlagen: {}", ericErrorText(rc));
    } else {
        spdlog::info("ERiC-SDK erfolgreich initialisiert");
        spdlog::debug("  Plugin-Verzeichnis: (neben libericapi)");
        spdlog::debug("  Log-Verzeichnis: {}", ec ? "(System-Temp)" : logDirStr);
    }
#else
    spdlog::debug("ERiC-SDK nicht verfügbar (ohne WITH_ERIC kompiliert)");
    initialized_ = false;
#endif
}

ERiCConnector::~ERiCConnector() {
#ifdef WITH_ERIC
    if (certLoaded_ && certHandle_) {
        EricCloseHandleToCertificate(certHandle_);
        spdlog::debug("ERiC Zertifikat-Handle geschlossen");
    }
    if (initialized_) {
        EricBeende();
        spdlog::debug("ERiC beendet");
    }
    // SECURITY: scrub the stored certificate password from memory on teardown,
    // even if submit() was never reached (e.g. an exception after loadCertificate).
    secureZero(certPassword_);
#endif
}

bool ERiCConnector::isAvailable() const {
    return initialized_;
}

bool ERiCConnector::loadCertificate(const std::string& pfxPath, const std::string& password) {
    // SECURITY: Never log the password!
    spdlog::debug("Lade ELSTER-Zertifikat: {}", pfxPath);

#ifdef WITH_ERIC
    if (!initialized_) {
        spdlog::error("ERiC nicht initialisiert");
        return false;
    }

    // A previously loaded handle is closed first (also recommended by ericapi.h
    // before EricPruefeZertifikatPin).
    if (certLoaded_ && certHandle_) {
        EricCloseHandleToCertificate(certHandle_);
        certHandle_ = 0;
        certLoaded_ = false;
        secureZero(certPassword_);
    }

    // Verify the PIN up front so a wrong password fails here with a clear message
    // instead of deep inside submit(). keyType 0 = signature key (soft PSE: same key).
    int rc = EricPruefeZertifikatPin(pfxPath.c_str(), password.c_str(), 0);
    if (rc != ERIC_OK) {
        if (rc == ERIC_CRYPT_E_PIN_WRONG) {
            spdlog::error("Zertifikat-Passwort/PIN ist falsch ({})", pfxPath);
        } else {
            spdlog::error("Zertifikat-PIN-Prüfung fehlgeschlagen: {}", ericErrorText(rc));
        }
        return false;
    }

    uint32_t pinInfo = 0;
    rc = EricGetHandleToCertificate(&certHandle_, &pinInfo, pfxPath.c_str());

    certLoaded_ = (rc == ERIC_OK);

    if (!certLoaded_) {
        certHandle_ = 0;
        spdlog::error("Zertifikat laden fehlgeschlagen: {}", ericErrorText(rc));
        return false;
    }

    // Store password for later use (certificate operations)
    certPassword_ = password;

    spdlog::info("ELSTER-Zertifikat erfolgreich geladen");
    return true;
#else
    spdlog::error("ERiC-SDK nicht verfügbar — Zertifikat kann nicht geladen werden");
    return false;
#endif
}

ValidationResult ERiCConnector::validate(const std::string& xbrlContent) {
    ValidationResult result;

#ifdef WITH_ERIC
    if (!initialized_) {
        result.isValid = false;
        result.errors.push_back({"eric_init", "ERiC nicht initialisiert"});
        return result;
    }

    spdlog::debug("ERiC-Validierung ({} Bytes XBRL)", xbrlContent.size());

    // Return buffers (RAII: freed on every path)
    Puffer rueckgabe;
    Puffer serverantwort;

    if (!rueckgabe.h || !serverantwort.h) {
        result.isValid = false;
        result.errors.push_back({"eric_buffer", "Konnte Rückgabepuffer nicht erstellen"});
        return result;
    }

    // Validation flags - ERIC_VALIDIERE validates content
    // Note: ERIC_PRUEFE_HINWEISE omitted to separate blocking errors from non-blocking Hinweise
    uint32_t flags = ERIC_VALIDIERE;

    // No certificate needed for validation
    // Note: datenartVersion must match taxonomy version (e.g., "Bilanz_6_8" for taxonomie 6.8)
    const char* datenartVersion = "Bilanz_6_8";

    int rc = EricBearbeiteVorgang(
        xbrlContent.c_str(),
        datenartVersion,
        flags,
        nullptr,  // No print parameter
        nullptr,  // No crypto parameter (validation only)
        nullptr,  // No transfer handle
        rueckgabe.h,
        serverantwort.h
    );

    // Check result
    if (rc == ERIC_OK) {
        result.isValid = true;
        spdlog::info("ERiC-Validierung erfolgreich");

        // Check for warnings in return buffer
        const char* rueckgabeText = rueckgabe.text();
        if (strlen(rueckgabeText) > 0) {
            spdlog::debug("ERiC Rückgabe: {}", rueckgabeText);
        }
    } else {
        result.isValid = false;
        std::string errorMsg = ericErrorText(rc);

        // Write the full response next to the history/logs (not into cwd);
        // spdlog may truncate large messages.
        const char* rueckgabeText = rueckgabe.text();
        if (strlen(rueckgabeText) > 0) {
            spdlog::debug("ERiC Rückgabe: {} Bytes", strlen(rueckgabeText));
            fs::path errPath = ebilanzDir() / "eric-validation-errors.xml";
            std::error_code ec;
            fs::create_directories(errPath.parent_path(), ec);
            std::ofstream errFile(errPath);
            errFile << rueckgabeText;
            errFile.close();
            if (errFile) {
                errorMsg += " (Details: " + errPath.string() + ")";
            } else {
                spdlog::warn("Konnte {} nicht schreiben", errPath.string());
            }
        }

        result.errors.push_back({"eric_validation", errorMsg});
        spdlog::error("ERiC-Validierung fehlgeschlagen: {}", errorMsg);
    }

#else
    spdlog::warn("ERiC-SDK nicht verfügbar — Validierung übersprungen");
    result.isValid = true;
    result.warnings.push_back({"eric_unavailable",
                                "ERiC-SDK nicht verfügbar — ERiC-Validierung übersprungen"});
#endif

    return result;
}

SubmitResult ERiCConnector::submit(const std::string& xbrlContent, bool testMode) {
    SubmitResult result;

#ifdef WITH_ERIC
    if (!initialized_) {
        result.success = false;
        result.message = "ERiC nicht initialisiert";
        result.errorCode = ErrorCode::ERIC_INIT_FAILED;
        return result;
    }

    if (!certLoaded_) {
        result.success = false;
        result.message = "Kein ELSTER-Zertifikat geladen";
        result.errorCode = ErrorCode::ERIC_CERT_LOAD_FAILED;
        return result;
    }

    spdlog::info("Sende an Finanzamt ({})...", testMode ? "Vorproduktion" : "Echtversand");

    // Return buffers (RAII: freed on every path)
    Puffer rueckgabe;
    Puffer serverantwort;

    if (!rueckgabe.h || !serverantwort.h) {
        result.success = false;
        result.message = "Konnte Rückgabepuffer nicht erstellen";
        result.errorCode = ErrorCode::ERIC_SUBMIT_FAILED;
        return result;
    }

    // Setup crypto parameter with certificate
    eric_verschluesselungs_parameter_t cryptoParam;
    cryptoParam.version = 3;
    cryptoParam.zertifikatHandle = certHandle_;
    cryptoParam.pin = certPassword_.empty() ? nullptr : certPassword_.c_str();

    // Set flags for submission
    // Note: Test mode is handled via testmerker in the XBRL data itself,
    // not through flags here
    uint32_t flags = ERIC_SENDE | ERIC_VALIDIERE;

    // Note: datenartVersion must match taxonomy version
    const char* datenartVersion = "Bilanz_6_8";

    int rc = EricBearbeiteVorgang(
        xbrlContent.c_str(),
        datenartVersion,
        flags,
        nullptr,  // No print parameter
        &cryptoParam,
        nullptr,  // No transfer handle (not Datenabholung)
        rueckgabe.h,
        serverantwort.h
    );

    // Check result
    if (rc == ERIC_OK) {
        result.success = true;
        result.message = "Erfolgreich übermittelt";
        result.errorCode = ErrorCode::SUCCESS;

        // Extract transfer ticket from return buffer
        const char* rueckgabeText = rueckgabe.text();
        if (strlen(rueckgabeText) > 0) {
            std::string rueckgabeStr(rueckgabeText);
            spdlog::debug("ERiC Rückgabe: {}", rueckgabeStr);

            // Parse transfer ticket from XML (simplified - would need proper XML parsing)
            size_t ticketPos = rueckgabeStr.find("<TransferTicket>");
            if (ticketPos != std::string::npos) {
                ticketPos += 16;  // length of "<TransferTicket>"
                size_t endPos = rueckgabeStr.find("</TransferTicket>", ticketPos);
                if (endPos != std::string::npos) {
                    result.transferTicket = rueckgabeStr.substr(ticketPos, endPos - ticketPos);
                }
            }
        }

        // Log server response (debug only — may echo submitted data)
        const char* serverText = serverantwort.text();
        if (strlen(serverText) > 0) {
            spdlog::debug("Server-Antwort: {}", serverText);
        }

        spdlog::info("Transfer-Ticket: {}", result.transferTicket);

    } else {
        result.success = false;
        result.errorCode = ErrorCode::ERIC_SUBMIT_FAILED;
        result.message = ericErrorText(rc);
        spdlog::error("ERiC-Versand fehlgeschlagen: {}", result.message);

        // Log rueckgabe and server response for debugging
        const char* rueckgabeText = rueckgabe.text();
        if (strlen(rueckgabeText) > 0) {
            spdlog::debug("ERiC Rückgabe: {}", rueckgabeText);
        }

        const char* serverText = serverantwort.text();
        if (strlen(serverText) > 0) {
            spdlog::debug("Server-Antwort: {}", serverText);
        }
    }

    // SECURITY: Clear password from memory
    secureZero(certPassword_);

#else
    result.success = false;
    result.message = "ERiC-SDK nicht verfügbar — Versand nicht möglich";
    result.errorCode = ErrorCode::ERIC_INIT_FAILED;
    spdlog::error("{}", result.message);
#endif

    return result;
}

SubmitResult ERiCConnector::submitXbrlFile(const std::string& xbrlFilePath, bool testMode) {
    spdlog::debug("Lade XBRL-Datei: {}", xbrlFilePath);

    std::string content = readFileContent(xbrlFilePath);
    return submit(content, testMode);
}
