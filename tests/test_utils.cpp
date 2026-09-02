#include <catch2/catch_test_macros.hpp>

#include "utils.h"

TEST_CASE("Utils: parseDate", "[utils]") {
    SECTION("Deutsches Datum wird nach ISO konvertiert") {
        REQUIRE(parseDate("31.12.2024") == "2024-12-31");
        REQUIRE(parseDate("1.3.2024") == "2024-03-01");
    }

    SECTION("ISO-Datum wird unverändert durchgereicht") {
        REQUIRE(parseDate("2024-12-31") == "2024-12-31");
    }

    SECTION("Leerer String ergibt leeren String") {
        REQUIRE(parseDate("").empty());
    }

    SECTION("ISO-förmige Strings mit Nicht-Ziffern werden abgelehnt") {
        REQUIRE_THROWS(parseDate("abcd-ef-gh"));
        REQUIRE_THROWS(parseDate("2024-1a-31"));
        REQUIRE_THROWS(parseDate("2024-12-3x"));
        REQUIRE_THROWS(parseDate("2024-12-31T"));
        REQUIRE_THROWS(parseDate("2024/12/31"));
    }
}

TEST_CASE("Utils: Testmerker/--test Konsistenz", "[utils]") {
    const std::string withTestmerker =
        "<TransferHeader><Testmerker>700000004</Testmerker></TransferHeader>";
    const std::string withoutTestmerker = "<TransferHeader><Verfahren>ElsterBilanz</Verfahren></TransferHeader>";

    SECTION("Testmerker vorhanden + --test passt") {
        REQUIRE(testmerkerMatchesMode(withTestmerker, true));
    }
    SECTION("Kein Testmerker + Echtversand passt") {
        REQUIRE(testmerkerMatchesMode(withoutTestmerker, false));
    }
    SECTION("Testmerker vorhanden ohne --test wird abgelehnt") {
        REQUIRE_FALSE(testmerkerMatchesMode(withTestmerker, false));
    }
    SECTION("--test ohne Testmerker wird abgelehnt") {
        REQUIRE_FALSE(testmerkerMatchesMode(withoutTestmerker, true));
    }
}
