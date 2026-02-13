#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "steuernummer.h"

// Helper to create a minimal BufaVerzeichnis for testing
static BufaVerzeichnis createTestBufa() {
    BufaVerzeichnis bufa;

    // NRW (prefix 51, 52, 53)
    bufa.prefixMap["51"] = {"nordrhein_westfalen", "nrw"};
    bufa.prefixMap["52"] = {"nordrhein_westfalen", "nrw"};
    bufa.prefixMap["53"] = {"nordrhein_westfalen", "nrw"};

    // Bayern (prefix 91, 92)
    bufa.prefixMap["91"] = {"bayern", "bayern"};
    bufa.prefixMap["92"] = {"bayern", "bayern"};

    // Berlin (prefix 11)
    bufa.prefixMap["11"] = {"berlin", "standard"};

    // Hessen (prefix 26)
    bufa.prefixMap["26"] = {"hessen", "standard"};

    // Hamburg (prefix 22)
    bufa.prefixMap["22"] = {"hamburg", "standard"};

    // Niedersachsen (prefix 23)
    bufa.prefixMap["23"] = {"niedersachsen", "standard"};

    return bufa;
}

TEST_CASE("Steuernummer: Format-Validierung") {
    SECTION("Gültiges Format") {
        REQUIRE(validateSteuernummerFormat("12/345/67890"));
        REQUIRE(validateSteuernummerFormat("99/999/99999"));
        REQUIRE(validateSteuernummerFormat("00/000/00000"));
    }

    SECTION("Ungültiges Format") {
        REQUIRE_FALSE(validateSteuernummerFormat(""));
        REQUIRE_FALSE(validateSteuernummerFormat("12345678901"));
        REQUIRE_FALSE(validateSteuernummerFormat("12/345/6789"));   // Too short
        REQUIRE_FALSE(validateSteuernummerFormat("12/345/678901")); // Too long
        REQUIRE_FALSE(validateSteuernummerFormat("AB/CDE/FGHIJ"));  // Letters
        REQUIRE_FALSE(validateSteuernummerFormat("12-345-67890"));  // Wrong separator
    }
}

TEST_CASE("Steuernummer: BuFa-Format-Validierung") {
    SECTION("Gültiges Format") {
        REQUIRE(validateBufaFormat("2893"));
        REQUIRE(validateBufaFormat("5214"));
        REQUIRE(validateBufaFormat("9143"));
    }

    SECTION("Ungültiges Format") {
        REQUIRE_FALSE(validateBufaFormat(""));
        REQUIRE_FALSE(validateBufaFormat("123"));     // Too short
        REQUIRE_FALSE(validateBufaFormat("12345"));   // Too long
        REQUIRE_FALSE(validateBufaFormat("ABCD"));    // Letters
    }
}

TEST_CASE("Steuernummer: Bundeseinheitliche Format-Validierung") {
    SECTION("Gültiges Format") {
        REQUIRE(validateBundesSteuernummerFormat("5214021577012"));
        REQUIRE(validateBundesSteuernummerFormat("1234567890123"));
    }

    SECTION("Ungültiges Format") {
        REQUIRE_FALSE(validateBundesSteuernummerFormat(""));
        REQUIRE_FALSE(validateBundesSteuernummerFormat("123456789012"));   // 12 digits
        REQUIRE_FALSE(validateBundesSteuernummerFormat("12345678901234")); // 14 digits
        REQUIRE_FALSE(validateBundesSteuernummerFormat("ABCDEFGHIJKLM"));  // Letters
    }
}

TEST_CASE("Steuernummer: Konvertierung NRW") {
    auto bufa = createTestBufa();

    SECTION("NRW Standardfall") {
        // NRW: BuFa 5FFF, StNr FF/FFF/UUUUP -> 5FFF0FFFUUUUP (13 digits)
        // Using valid XX/XXX/XXXXX format for NRW
        auto result = convertSteuernummer("21/257/70123", "5214", bufa);
        REQUIRE(result.length() == 13);
        REQUIRE(result.substr(0, 4) == "5214");  // BuFa prefix
        REQUIRE(result[4] == '0');                 // Fixed zero
    }
}

TEST_CASE("Steuernummer: Konvertierung Bayern") {
    auto bufa = createTestBufa();

    SECTION("Bayern Standardfall") {
        // Bayern: BuFa 9FFF, StNr FFF/BBB/UUUUP -> 9FFF0BBBUUUUP
        auto result = convertSteuernummer("14/323/45678", "9143", bufa);
        REQUIRE(result.length() == 13);
        REQUIRE(result[0] == '9');  // Bayern prefix
    }
}

TEST_CASE("Steuernummer: Konvertierung Berlin") {
    auto bufa = createTestBufa();

    SECTION("Berlin Standardfall") {
        // Berlin: BuFa 11FF, StNr FF/BBB/UUUUP -> 11FF0BBBUUUUP
        auto result = convertSteuernummer("36/123/45678", "1136", bufa);
        REQUIRE(result.length() == 13);
        REQUIRE(result.substr(0, 2) == "11");  // Berlin prefix
    }
}

TEST_CASE("Steuernummer: Konvertierung Hessen") {
    auto bufa = createTestBufa();

    SECTION("Hessen Standardfall") {
        auto result = convertSteuernummer("13/456/78901", "2613", bufa);
        REQUIRE(result.length() == 13);
        REQUIRE(result.substr(0, 2) == "26");
    }
}

TEST_CASE("Steuernummer: Ungültige Eingaben") {
    auto bufa = createTestBufa();

    SECTION("Leere Steuernummer") {
        auto result = convertSteuernummer("", "5214", bufa);
        REQUIRE(result.empty());
    }

    SECTION("Leere BuFa-Nr") {
        auto result = convertSteuernummer("12/345/67890", "", bufa);
        REQUIRE(result.empty());
    }

    SECTION("Ungültiges Format") {
        auto result = convertSteuernummer("invalid", "5214", bufa);
        REQUIRE(result.empty());
    }

    SECTION("Unbekannter BuFa-Prefix") {
        auto result = convertSteuernummer("12/345/67890", "9999", bufa);
        REQUIRE(result.empty());
    }
}

TEST_CASE("Steuernummer: BuFa-Verzeichnis laden") {
    // This test requires the config file to be available
    // In CI/build it should be copied to build dir by CMake
    try {
        auto bufa = loadBufaVerzeichnis("config/bufa-verzeichnis.json");
        REQUIRE(!bufa.prefixMap.empty());

        // Check a few known prefixes
        REQUIRE(bufa.prefixMap.count("51") > 0);  // NRW
        REQUIRE(bufa.prefixMap.count("91") > 0);  // Bayern
        REQUIRE(bufa.prefixMap.count("11") > 0);  // Berlin

        REQUIRE(bufa.prefixMap["51"].conversionType == "nrw");
        REQUIRE(bufa.prefixMap["91"].conversionType == "bayern");
        REQUIRE(bufa.prefixMap["11"].conversionType == "standard");
    } catch (const std::exception&) {
        WARN("BuFa-Verzeichnis config/bufa-verzeichnis.json nicht gefunden — Test übersprungen");
    }
}
