#pragma once

#include "models.h"
#include "taxonomie_config.h"

#include <string>

class XBRLGenerator {
public:
    explicit XBRLGenerator(const TaxonomieConfig& config);

    // Generate XBRL XML string from data
    std::string generate(const Stammdaten& stamm,
                         const BilanzData& bilanz,
                         const GuVData& guv) const;

private:
    TaxonomieConfig config_;

    // Calculate prior year dates from Stammdaten
    std::string calculateVorjahrVon(const Stammdaten& stamm) const;
    std::string calculateVorjahrBis(const Stammdaten& stamm) const;
};
