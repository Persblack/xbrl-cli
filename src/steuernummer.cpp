#include "steuernummer.h"

#include <algorithm>
#include <fstream>
#include <regex>
#include <spdlog/spdlog.h>

BufaVerzeichnis loadBufaVerzeichnis(const std::string& jsonPath) {
    std::ifstream file(jsonPath);
    if (!file.is_open()) {
        throw std::runtime_error("BuFa-Verzeichnis nicht gefunden: " + jsonPath);
    }

    nlohmann::json j;
    file >> j;

    BufaVerzeichnis bufa;

    auto& bundeslaender = j["bundeslaender"];
    for (auto& [key, value] : bundeslaender.items()) {
        auto& prefixes = value["bufaPrefix"];
        std::string convType = "standard";

        // Determine conversion type from key
        if (key == "nordrhein_westfalen") {
            convType = "nrw";
        } else if (key == "bayern") {
            convType = "bayern";
        }

        for (auto& prefix : prefixes) {
            BufaEntry entry;
            entry.bundesland = key;
            entry.conversionType = convType;
            bufa.prefixMap[prefix.get<std::string>()] = entry;
        }
    }

    return bufa;
}

std::string convertSteuernummer(const std::string& vereinfacht,
                                 const std::string& bufaNr,
                                 const BufaVerzeichnis& bufa) {
    // Validate inputs
    if (!validateSteuernummerFormat(vereinfacht) || !validateBufaFormat(bufaNr)) {
        spdlog::error("Ungültige Eingabe für Steuernummer-Konvertierung: {} / {}", vereinfacht, bufaNr);
        return "";
    }

    // Extract digits from simplified number (remove slashes)
    std::string digits;
    for (char c : vereinfacht) {
        if (std::isdigit(static_cast<unsigned char>(c))) {
            digits += c;
        }
    }
    // digits should be 10 characters: FFBBBUUUUP

    // Look up BuFa prefix (first 2 digits of BuFa number)
    std::string bufaPrefix = bufaNr.substr(0, 2);
    auto it = bufa.prefixMap.find(bufaPrefix);
    if (it == bufa.prefixMap.end()) {
        spdlog::error("BuFa-Prefix {} nicht im Verzeichnis gefunden", bufaPrefix);
        return "";
    }

    const auto& entry = it->second;
    std::string result;

    if (entry.conversionType == "standard") {
        // Standard: LLFF0BBBUUUUP (13 digits)
        // BuFa: LLFF (4 digits)
        // Steuernummer digits: FF BBB UUUUP (2+3+5 = 10)
        // Result: BuFa(4) + "0" + Bezirk(3) + UnterNr(4) + Prüf(1) = 4+1+3+4+1 = 13
        // Actually: LLFF + 0 + BBB + UUUUP = LLFF + 0 + last 8 digits of vereinfacht
        result = bufaNr + "0" + digits.substr(2, 3) + digits.substr(5, 5);
    } else if (entry.conversionType == "nrw") {
        // NRW: 5FFF0BBBBUUUP (13 digits)
        // BuFa: 5FFF (4 digits)
        // Steuernummer digits: FFF BBBB UUUP (3+4+3 = 10)
        // Result: BuFa(4) + "0" + BBBB(4) + UUUP(4) = 4+1+4+4 = 13
        result = bufaNr + "0" + digits.substr(3, 4) + digits.substr(7, 3);
        // Pad to 13 if needed
        while (result.size() < 13) {
            result += "0";
        }
    } else if (entry.conversionType == "bayern") {
        // Bayern: 9FFF0BBBUUUUUP (13 digits)
        // BuFa: 9FFF (4 digits)
        // Steuernummer digits: FFF BBB UUUUP (3+3+5 = 11... but format is 3/3/5)
        // Actually Bayern uses different split: XXX/XXX/XXXXXP
        // Result: BuFa(4) + "0" + BBB(3) + UUUUUP(5) = 4+1+3+5 = 13
        result = bufaNr + "0" + digits.substr(3, 3) + digits.substr(6, 5);
        // Pad to 13 if needed
        while (result.size() < 13) {
            result += "0";
        }
    }

    // Trim to 13 digits
    if (result.size() > 13) {
        result = result.substr(0, 13);
    }

    if (result.size() != 13) {
        spdlog::error("Steuernummer-Konvertierung ergab {} statt 13 Stellen: {}", result.size(), result);
        return "";
    }

    spdlog::debug("Steuernummer konvertiert: {} + {} -> {}", vereinfacht, bufaNr, result);
    return result;
}

bool validateSteuernummerFormat(const std::string& nr) {
    // The Finanzamt Wangen print format omits the separator after the first two
    // digits (e.g. 91060/35565 = 91/060/35565). Both represent ten digits.
    static const std::regex standard(R"(\d{2}/\d{3}/\d{5})");
    static const std::regex compact(R"(\d{5}/\d{5})");
    return std::regex_match(nr, standard) || std::regex_match(nr, compact);
}

bool validateBufaFormat(const std::string& nr) {
    static const std::regex pattern(R"(\d{4})");
    return std::regex_match(nr, pattern);
}

bool validateBundesSteuernummerFormat(const std::string& nr) {
    static const std::regex pattern(R"(\d{13})");
    return std::regex_match(nr, pattern);
}
