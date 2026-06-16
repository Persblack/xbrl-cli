#include "eric_connector.h"
#include "utils.h"

#include <spdlog/spdlog.h>

#include <filesystem>
#include <fstream>
#include <sstream>

#ifdef WITH_ERIC
#include "ericapi.h"
#include "eric_types.h"
#include "eric_fehlercodes.h"
#endif

namespace fs = std::filesystem;

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

    // Determine plugin and log paths
    fs::path buildDir = fs::current_path();
    fs::path pluginDir = buildDir / "lib" / "plugins";
    fs::path logDir = buildDir / "logs";

    // Create log directory if needed
    fs::create_directories(logDir);

    // Initialize ERiC
    int rc = EricInitialisiere(pluginDir.string().c_str(), logDir.string().c_str());
    initialized_ = (rc == ERIC_OK);

    if (!initialized_) {
        spdlog::error("ERiC Initialisierung fehlgeschlagen: Fehlercode {}", rc);
    } else {
        spdlog::info("ERiC-SDK erfolgreich initialisiert");
        spdlog::debug("  Plugin-Verzeichnis: {}", pluginDir.string());
        spdlog::debug("  Log-Verzeichnis: {}", logDir.string());
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

    uint32_t pinInfo = 0;
    int rc = EricGetHandleToCertificate(&certHandle_, &pinInfo, pfxPath.c_str());

    certLoaded_ = (rc == ERIC_OK);

    if (!certLoaded_) {
        spdlog::error("Zertifikat laden fehlgeschlagen: Fehlercode {}", rc);

        // Get error message from ERiC
        EricRueckgabepufferHandle errorBuffer = EricRueckgabepufferErzeugen();
        if (errorBuffer) {
            EricHoleFehlerText(rc, errorBuffer);
            const char* errorText = EricRueckgabepufferInhalt(errorBuffer);
            if (errorText) {
                spdlog::error("  ERiC-Fehler: {}", errorText);
            }
            EricRueckgabepufferFreigeben(errorBuffer);
        }
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

    // Create return buffers
    EricRueckgabepufferHandle rueckgabe = EricRueckgabepufferErzeugen();
    EricRueckgabepufferHandle serverantwort = EricRueckgabepufferErzeugen();

    if (!rueckgabe || !serverantwort) {
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
        rueckgabe,
        serverantwort
    );

    // Check result
    if (rc == ERIC_OK) {
        result.isValid = true;
        spdlog::info("ERiC-Validierung erfolgreich");

        // Check for warnings in return buffer
        const char* rueckgabeText = EricRueckgabepufferInhalt(rueckgabe);
        if (rueckgabeText && strlen(rueckgabeText) > 0) {
            spdlog::debug("ERiC Rückgabe: {}", rueckgabeText);
        }
    } else {
        result.isValid = false;

        // Get error message
        EricRueckgabepufferHandle errorBuffer = EricRueckgabepufferErzeugen();
        if (errorBuffer) {
            EricHoleFehlerText(rc, errorBuffer);
            const char* errorText = EricRueckgabepufferInhalt(errorBuffer);
            std::string errorMsg = errorText ? errorText : "Unbekannter ERiC-Fehler";
            result.errors.push_back({"eric_validation", errorMsg});
            spdlog::error("ERiC-Validierung fehlgeschlagen: {}", errorMsg);
            EricRueckgabepufferFreigeben(errorBuffer);
        }

        // Also log rueckgabe content for details
        const char* rueckgabeText = EricRueckgabepufferInhalt(rueckgabe);
        if (rueckgabeText && strlen(rueckgabeText) > 0) {
            spdlog::debug("ERiC Rückgabe: {} Bytes", strlen(rueckgabeText));
            // Write full response to file (spdlog may truncate large messages)
            std::ofstream errFile("eric-validation-errors.xml");
            if (errFile.is_open()) {
                errFile << rueckgabeText;
                errFile.close();
                spdlog::debug("ERiC Fehler gespeichert: eric-validation-errors.xml");
            }
        }
    }

    // Clean up buffers
    EricRueckgabepufferFreigeben(rueckgabe);
    EricRueckgabepufferFreigeben(serverantwort);

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

    // Create return buffers
    EricRueckgabepufferHandle rueckgabe = EricRueckgabepufferErzeugen();
    EricRueckgabepufferHandle serverantwort = EricRueckgabepufferErzeugen();

    if (!rueckgabe || !serverantwort) {
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
        rueckgabe,
        serverantwort
    );

    // Check result
    if (rc == ERIC_OK) {
        result.success = true;
        result.message = "Erfolgreich übermittelt";
        result.errorCode = ErrorCode::SUCCESS;

        // Extract transfer ticket from return buffer
        const char* rueckgabeText = EricRueckgabepufferInhalt(rueckgabe);
        if (rueckgabeText && strlen(rueckgabeText) > 0) {
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

        // Log server response
        const char* serverText = EricRueckgabepufferInhalt(serverantwort);
        if (serverText && strlen(serverText) > 0) {
            spdlog::debug("Server-Antwort: {}", serverText);
        }

        spdlog::info("Transfer-Ticket: {}", result.transferTicket);

    } else {
        result.success = false;
        result.errorCode = ErrorCode::ERIC_SUBMIT_FAILED;

        // Get error message
        EricRueckgabepufferHandle errorBuffer = EricRueckgabepufferErzeugen();
        if (errorBuffer) {
            EricHoleFehlerText(rc, errorBuffer);
            const char* errorText = EricRueckgabepufferInhalt(errorBuffer);
            result.message = errorText ? errorText : "Unbekannter ERiC-Fehler";
            spdlog::error("ERiC-Versand fehlgeschlagen: {}", result.message);
            EricRueckgabepufferFreigeben(errorBuffer);
        }

        // Log rueckgabe and server response for debugging
        const char* rueckgabeText = EricRueckgabepufferInhalt(rueckgabe);
        if (rueckgabeText && strlen(rueckgabeText) > 0) {
            spdlog::debug("ERiC Rückgabe: {}", rueckgabeText);
        }

        const char* serverText = EricRueckgabepufferInhalt(serverantwort);
        if (serverText && strlen(serverText) > 0) {
            spdlog::debug("Server-Antwort: {}", serverText);
        }
    }

    // Clean up buffers
    EricRueckgabepufferFreigeben(rueckgabe);
    EricRueckgabepufferFreigeben(serverantwort);

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
