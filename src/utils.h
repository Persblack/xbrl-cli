#pragma once

#include <string>

// Securely zero a string's memory (volatile to prevent compiler optimization)
void secureZero(std::string& s);

// Read password from terminal without echo
std::string readPassword();

// Parse German date "DD.MM.YYYY" to ISO "YYYY-MM-DD"
std::string parseDate(const std::string& germanDate);

// Format double as German currency display (e.g. "1.234,56")
std::string formatCurrency(double value);

// Calculate months between two ISO dates (YYYY-MM-DD)
int monthsBetween(const std::string& dateFrom, const std::string& dateTo);

// Read entire file content to string
std::string readFileContent(const std::string& path);

// Get current timestamp in ISO 8601 format
std::string currentTimestamp();

// Extract year from ISO date string
std::string extractYear(const std::string& isoDate);

// User home directory ($HOME / %USERPROFILE%), "." if unset
std::string userHomeDir();

// true iff the presence of <Testmerker> in the (wrapped) XML matches --test
bool testmerkerMatchesMode(const std::string& xml, bool testMode);
