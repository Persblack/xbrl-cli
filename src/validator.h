#pragma once

#include "models.h"

class Validator {
public:
    // Validate all data according to the 12 rules defined in CLAUDE.md
    ValidationResult validate(const Stammdaten& stamm,
                              const BilanzData& bilanz,
                              const GuVData& guv) const;
};
