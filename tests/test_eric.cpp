#include <catch2/catch_test_macros.hpp>

#include "eric_connector.h"
#include "elster_wrapper.h"
#include "models.h"

#include <filesystem>

namespace fs = std::filesystem;

TEST_CASE("ERiCConnector: SDK verfügbar", "[eric]") {
#ifdef WITH_ERIC
    REQUIRE(ericSdkAvailable() == true);
#else
    REQUIRE(ericSdkAvailable() == false);
#endif
}

#ifdef WITH_ERIC_TESTS

TEST_CASE("ERiCConnector: Initialisierung", "[eric][integration]") {
    ERiCConnector connector;

    SECTION("SDK ist initialisiert") {
        REQUIRE(connector.isAvailable() == true);
    }
}

TEST_CASE("ERiCConnector: Zertifikat laden ohne Datei", "[eric][integration]") {
    ERiCConnector connector;
    REQUIRE(connector.isAvailable() == true);

    SECTION("Zertifikat laden mit nicht-existierender Datei schlägt fehl") {
        bool loaded = connector.loadCertificate("/tmp/nonexistent.pfx", "dummy");
        REQUIRE(loaded == false);
    }
}

TEST_CASE("ERiCConnector: Validierung mit ElsterBasis", "[eric][integration]") {
    ERiCConnector connector;

    REQUIRE(connector.isAvailable() == true);

    SECTION("XBRL validieren via ElsterBasis-Envelope") {
        // Minimal XBRL content
        std::string xbrlContent = R"(<?xml version="1.0" encoding="UTF-8"?>
<xbrl xmlns="http://www.xbrl.org/2003/instance">
  <context id="current_instant">
    <entity>
      <identifier scheme="http://www.rzf-nrw.de/Steuernummer">5133081508155</identifier>
    </entity>
    <period>
      <instant>2024-12-31</instant>
    </period>
  </context>
</xbrl>)";

        // Wrap in ElsterBasis envelope (what ERiC actually expects)
        ElsterConfig config;
        config.herstellerID = "00000";
        ElsterWrapper wrapper(config);

        Stammdaten stamm;
        stamm.finanzamtsNummer = "5133";
        stamm.steuernummerBund = "5133081508155";

        std::string elsterXml = wrapper.wrap(xbrlContent, stamm, true);

        auto result = connector.validate(elsterXml);

        // We expect validation might fail due to incomplete XBRL,
        // but the connector should not crash
        INFO("Validation result: " << (result.isValid ? "valid" : "invalid"));
        if (!result.isValid) {
            for (const auto& error : result.errors) {
                INFO("Error: " << error.message);
            }
        }

        // Test passes if we got here without crashing
        SUCCEED();
    }
}

TEST_CASE("ERiCConnector: Versand ohne Zertifikat", "[eric][integration]") {
    ERiCConnector connector;

    REQUIRE(connector.isAvailable() == true);

    SECTION("Versand schlägt fehl ohne Zertifikat") {
        std::string xbrlContent = "<?xml version=\"1.0\"?><test/>";

        auto result = connector.submit(xbrlContent, true);  // testMode = true

        REQUIRE(result.success == false);
        REQUIRE(result.errorCode == ErrorCode::ERIC_CERT_LOAD_FAILED);
        REQUIRE(result.message.find("Zertifikat") != std::string::npos);
    }
}

// NOTE: These tests require a real ELSTER test certificate (.pfx file)
// Uncomment and provide test certificate path to run full integration tests:
/*
TEST_CASE("ERiCConnector: Zertifikat laden [manual]", "[eric][integration][manual]") {
    ERiCConnector connector;

    REQUIRE(connector.isAvailable() == true);

    // IMPORTANT: Replace with path to your test certificate
    std::string certPath = "/path/to/test-zertifikat.pfx";
    std::string certPassword = "test-password";

    if (!fs::exists(certPath)) {
        SKIP("Test certificate not found at " << certPath);
    }

    SECTION("Gültiges Zertifikat laden") {
        bool loaded = connector.loadCertificate(certPath, certPassword);
        REQUIRE(loaded == true);
    }

    SECTION("Ungültiges Passwort") {
        bool loaded = connector.loadCertificate(certPath, "wrong-password");
        REQUIRE(loaded == false);
    }
}

TEST_CASE("ERiCConnector: Vollständiger Workflow [manual]", "[eric][integration][manual]") {
    ERiCConnector connector;

    REQUIRE(connector.isAvailable() == true);

    // Replace with your test certificate
    std::string certPath = "/path/to/test-zertifikat.pfx";
    std::string certPassword = "test-password";

    if (!fs::exists(certPath)) {
        SKIP("Test certificate not found");
    }

    SECTION("Validieren → Versenden (Test-Modus)") {
        // Load certificate
        REQUIRE(connector.loadCertificate(certPath, certPassword) == true);

        // Minimal test XBRL (would need to be a valid E-Bilanz for real test)
        std::string xbrlContent = "...";  // Full XBRL content here

        // Validate
        auto validationResult = connector.validate(xbrlContent);
        REQUIRE(validationResult.isValid == true);

        // Submit in test mode
        auto submitResult = connector.submit(xbrlContent, true);

        if (submitResult.success) {
            REQUIRE(!submitResult.transferTicket.empty());
            INFO("Transfer-Ticket: " << submitResult.transferTicket);
        } else {
            INFO("Submit failed: " << submitResult.message);
            // Might fail due to incomplete test data, but should not crash
        }
    }
}
*/

#endif // WITH_ERIC_TESTS
