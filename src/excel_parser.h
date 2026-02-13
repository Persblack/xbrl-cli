#pragma once

#include "models.h"
#include "steuernummer.h"

#include <string>

class ExcelParser {
public:
    // Read Stammdaten from "Stammdaten" sheet
    Stammdaten readStammdaten(const std::string& filepath,
                               const BufaVerzeichnis& bufa) const;

    // Read Bilanz data from "Bilanz" sheet
    BilanzData readBilanz(const std::string& filepath) const;

    // Read GuV data from "GuV" sheet
    GuVData readGuV(const std::string& filepath) const;
};
