#pragma once

#include <string>

#include <nlohmann/json.hpp>

// BuFa entry: maps a BuFa number to its Bundesland and conversion type
struct BufaEntry {
    std::string bundesland;
    std::string conversionType; // "standard", "nrw", "bayern"
};

// BuFa directory loaded from config
struct BufaVerzeichnis {
    // Maps BuFa 2-digit prefix to BufaEntry
    std::map<std::string, BufaEntry> prefixMap;
};

// Load BuFa directory from JSON file
BufaVerzeichnis loadBufaVerzeichnis(const std::string& jsonPath);

// Convert simplified tax number (XX/XXX/XXXXX) + BuFa number (4-digit)
// to 13-digit federal tax number (bundeseinheitlich)
// Returns empty string on failure
std::string convertSteuernummer(const std::string& vereinfacht,
                                 const std::string& bufaNr,
                                 const BufaVerzeichnis& bufa);

// Validate simplified tax number format (XX/XXX/XXXXX)
bool validateSteuernummerFormat(const std::string& nr);

// Validate BuFa number format (4 digits)
bool validateBufaFormat(const std::string& nr);

// Validate federal tax number format (13 digits)
bool validateBundesSteuernummerFormat(const std::string& nr);
