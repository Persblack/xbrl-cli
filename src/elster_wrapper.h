#pragma once

#include "models.h"

#include <string>

// ── ElsterBasis Configuration ───────────────────────────────────────────────

struct ElsterConfig {
    std::string herstellerID = "00000";
    std::string datenLieferant;
    std::string verfahren = "ElsterBilanz";
    std::string vorgang = "send-Auth";
    std::string elsterNamespace = "http://www.elster.de/elsterxml/schema/v11";
    std::string ebilanzNamespace = "http://rzf.fin-nrw.de/RMS/EBilanz/2016/XMLSchema";
    int version = 11;
};

// ── ElsterWrapper ───────────────────────────────────────────────────────────

/**
 * @brief Wraps XBRL content in ElsterBasis XML envelope for ERiC submission
 *
 * ERiC expects XBRL content to be wrapped in an ElsterBasis XML structure
 * with TransferHeader and Nutzdatenblock elements. This class generates
 * the required envelope around pure XBRL instance documents.
 */
class ElsterWrapper {
public:
    explicit ElsterWrapper(const ElsterConfig& config);

    /**
     * @brief Wrap XBRL content in ElsterBasis envelope
     *
     * @param xbrlContent Pure XBRL instance document (from XBRLGenerator)
     * @param stamm Stammdaten for BuFa, Bundesland, etc.
     * @param testMode If true, adds Testmerker element for Vorproduktion
     * @return Complete ElsterBasis XML ready for ERiC submission
     */
    std::string wrap(const std::string& xbrlContent,
                     const Stammdaten& stamm,
                     bool testMode = false) const;

private:
    ElsterConfig config_;

    // Generate unique NutzdatenTicket (UUID-like identifier)
    std::string generateNutzdatenTicket() const;

    // Map BuFa number to Bundesland code (e.g., "9181" -> "BY")
    std::string getBundeslandCode(const std::string& bufaNummer) const;
};
