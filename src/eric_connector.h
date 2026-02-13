#pragma once

#include "models.h"

#include <string>

class ERiCConnector {
public:
    ERiCConnector();
    ~ERiCConnector();

    // Non-copyable
    ERiCConnector(const ERiCConnector&) = delete;
    ERiCConnector& operator=(const ERiCConnector&) = delete;

    // Check if ERiC SDK is available
    bool isAvailable() const;

    // Load ELSTER certificate
    bool loadCertificate(const std::string& pfxPath, const std::string& password);

    // Validate XBRL content via ERiC
    ValidationResult validate(const std::string& xbrlContent);

    // Submit XBRL to Finanzamt
    SubmitResult submit(const std::string& xbrlContent, bool testMode = false);

    // Submit existing XBRL file
    SubmitResult submitXbrlFile(const std::string& xbrlFilePath, bool testMode = false);

private:
#ifdef WITH_ERIC
    void* ericHandle_ = nullptr;
    uint32_t certHandle_ = 0;  // EricZertifikatHandle
    std::string certPassword_;  // Stored for PIN usage in crypto operations
#endif
    bool initialized_ = false;
    bool certLoaded_ = false;
};

// Utility: check if ERiC SDK is available at runtime
bool ericSdkAvailable();
