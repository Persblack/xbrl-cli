#include "../src/elster_wrapper.h"
#include "../src/models.h"

#include <catch2/catch_test_macros.hpp>
#include <pugixml.hpp>

TEST_CASE("ElsterWrapper: Basic wrapper structure", "[elster]") {
    ElsterConfig config;
    config.herstellerID = "12345";

    Stammdaten stamm;
    stamm.finanzamtsNummer = "9181";
    stamm.firmenname = "Test GmbH";
    stamm.steuernummer = "91/123/45678";
    stamm.steuernummerBund = "9181012345678";
    stamm.bilanzstichtag = "2024-12-31";

    std::string xbrlContent = R"(<?xml version="1.0" encoding="UTF-8"?>
<xbrl xmlns="http://www.xbrl.org/2003/instance">
  <context id="current">
    <entity>
      <identifier scheme="http://www.rzf-nrw.de/Steuernummer">9181012345678</identifier>
    </entity>
  </context>
</xbrl>)";

    ElsterWrapper wrapper(config);
    std::string wrappedXml = wrapper.wrap(xbrlContent, stamm, false);

    // Parse wrapped XML
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_string(wrappedXml.c_str());
    REQUIRE(result);

    // Verify root element
    auto elster = doc.child("Elster");
    REQUIRE(elster);
    REQUIRE(std::string(elster.attribute("xmlns").value()) == "http://www.elster.de/elsterxml/schema/v11");

    // Verify TransferHeader
    auto transferHeader = elster.child("TransferHeader");
    REQUIRE(transferHeader);
    REQUIRE(std::string(transferHeader.attribute("version").value()) == "11");
    REQUIRE(std::string(transferHeader.child("Verfahren").text().as_string()) == "ElsterBilanz");
    REQUIRE(std::string(transferHeader.child("DatenArt").text().as_string()) == "Bilanz");
    REQUIRE(std::string(transferHeader.child("HerstellerID").text().as_string()) == "12345");
    REQUIRE(std::string(transferHeader.child("DatenLieferant").text().as_string()) == "Test GmbH");

    // Verify Empfaenger (Bundesland)
    auto empfaenger = transferHeader.child("Empfaenger");
    REQUIRE(empfaenger);
    REQUIRE(std::string(empfaenger.attribute("id").value()) == "L");
    REQUIRE(std::string(empfaenger.child("Ziel").text().as_string()) == "BY");  // Bayern

    // Verify DatenTeil structure
    auto datenTeil = elster.child("DatenTeil");
    REQUIRE(datenTeil);
    auto nutzdatenblock = datenTeil.child("Nutzdatenblock");
    REQUIRE(nutzdatenblock);

    // Verify NutzdatenHeader
    auto nutzdatenHeader = nutzdatenblock.child("NutzdatenHeader");
    REQUIRE(nutzdatenHeader);
    REQUIRE(std::string(nutzdatenHeader.attribute("version").value()) == "11");

    auto empfaengerFA = nutzdatenHeader.child("Empfaenger");
    REQUIRE(empfaengerFA);
    REQUIRE(std::string(empfaengerFA.attribute("id").value()) == "F");
    REQUIRE(std::string(empfaengerFA.text().as_string()) == "9181");

    // Verify Nutzdaten contains EBilanz wrapper with XBRL inside
    auto nutzdaten = nutzdatenblock.child("Nutzdaten");
    REQUIRE(nutzdaten);
    auto ebilanzEl = nutzdaten.child("ebilanz:EBilanz");
    REQUIRE(ebilanzEl);
    REQUIRE(std::string(ebilanzEl.attribute("xmlns:ebilanz").value()) == "http://rzf.fin-nrw.de/RMS/EBilanz/2016/XMLSchema");
    REQUIRE(std::string(ebilanzEl.attribute("version").value()) == "1");

    // Verify stichtag in YYYYMMDD format
    auto stichtag = ebilanzEl.child("ebilanz:stichtag");
    REQUIRE(stichtag);
    REQUIRE(std::string(stichtag.text().as_string()) == "20241231");

    // Verify XBRL root is inside EBilanz
    auto xbrlRoot = ebilanzEl.child("xbrl");
    REQUIRE(xbrlRoot);
}

TEST_CASE("ElsterWrapper: Testmerker handling", "[elster]") {
    ElsterConfig config;
    config.herstellerID = "12345";

    Stammdaten stamm;
    stamm.finanzamtsNummer = "2801";
    stamm.firmenname = "Test GmbH";
    stamm.bilanzstichtag = "2024-12-31";

    std::string xbrlContent = R"(<?xml version="1.0" encoding="UTF-8"?>
<xbrl xmlns="http://www.xbrl.org/2003/instance">
</xbrl>)";

    ElsterWrapper wrapper(config);

    SECTION("Production mode - no Testmerker") {
        std::string wrappedXml = wrapper.wrap(xbrlContent, stamm, false);
        pugi::xml_document doc;
        doc.load_string(wrappedXml.c_str());

        auto transferHeader = doc.child("Elster").child("TransferHeader");
        REQUIRE_FALSE(transferHeader.child("Testmerker"));
    }

    SECTION("Test mode - includes Testmerker") {
        std::string wrappedXml = wrapper.wrap(xbrlContent, stamm, true);
        pugi::xml_document doc;
        doc.load_string(wrappedXml.c_str());

        auto transferHeader = doc.child("Elster").child("TransferHeader");
        auto testmerker = transferHeader.child("Testmerker");
        REQUIRE(testmerker);
        REQUIRE(std::string(testmerker.text().as_string()) == "700000004");
    }
}

TEST_CASE("ElsterWrapper: Bundesland mapping", "[elster]") {
    ElsterConfig config;
    config.herstellerID = "12345";

    Stammdaten stamm;
    stamm.firmenname = "Test GmbH";
    stamm.bilanzstichtag = "2024-12-31";

    std::string xbrlContent = R"(<?xml version="1.0" encoding="UTF-8"?>
<xbrl xmlns="http://www.xbrl.org/2003/instance"></xbrl>)";

    ElsterWrapper wrapper(config);

    SECTION("Bayern (91xx)") {
        stamm.finanzamtsNummer = "9181";
        std::string wrappedXml = wrapper.wrap(xbrlContent, stamm, false);
        pugi::xml_document doc;
        doc.load_string(wrappedXml.c_str());

        auto ziel = doc.child("Elster").child("TransferHeader").child("Empfaenger").child("Ziel");
        REQUIRE(std::string(ziel.text().as_string()) == "BY");
    }

    SECTION("Baden-Württemberg (28xx)") {
        stamm.finanzamtsNummer = "2801";
        std::string wrappedXml = wrapper.wrap(xbrlContent, stamm, false);
        pugi::xml_document doc;
        doc.load_string(wrappedXml.c_str());

        auto ziel = doc.child("Elster").child("TransferHeader").child("Empfaenger").child("Ziel");
        REQUIRE(std::string(ziel.text().as_string()) == "BW");
    }

    SECTION("Nordrhein-Westfalen (5xxx)") {
        stamm.finanzamtsNummer = "5101";
        std::string wrappedXml = wrapper.wrap(xbrlContent, stamm, false);
        pugi::xml_document doc;
        doc.load_string(wrappedXml.c_str());

        auto ziel = doc.child("Elster").child("TransferHeader").child("Empfaenger").child("Ziel");
        REQUIRE(std::string(ziel.text().as_string()) == "NW");
    }

    SECTION("Berlin (11xx)") {
        stamm.finanzamtsNummer = "1101";
        std::string wrappedXml = wrapper.wrap(xbrlContent, stamm, false);
        pugi::xml_document doc;
        doc.load_string(wrappedXml.c_str());

        auto ziel = doc.child("Elster").child("TransferHeader").child("Empfaenger").child("Ziel");
        REQUIRE(std::string(ziel.text().as_string()) == "BE");
    }
}

TEST_CASE("ElsterWrapper: NutzdatenTicket uniqueness", "[elster]") {
    ElsterConfig config;
    config.herstellerID = "12345";

    Stammdaten stamm;
    stamm.finanzamtsNummer = "9181";
    stamm.firmenname = "Test GmbH";
    stamm.bilanzstichtag = "2024-12-31";

    std::string xbrlContent = R"(<?xml version="1.0" encoding="UTF-8"?>
<xbrl xmlns="http://www.xbrl.org/2003/instance"></xbrl>)";

    ElsterWrapper wrapper(config);

    // Generate two wrapped XMLs
    std::string wrapped1 = wrapper.wrap(xbrlContent, stamm, false);
    std::string wrapped2 = wrapper.wrap(xbrlContent, stamm, false);

    pugi::xml_document doc1, doc2;
    doc1.load_string(wrapped1.c_str());
    doc2.load_string(wrapped2.c_str());

    auto ticket1 = doc1.child("Elster").child("DatenTeil")
                      .child("Nutzdatenblock").child("NutzdatenHeader")
                      .child("NutzdatenTicket").text().as_string();
    auto ticket2 = doc2.child("Elster").child("DatenTeil")
                      .child("Nutzdatenblock").child("NutzdatenHeader")
                      .child("NutzdatenTicket").text().as_string();

    // Tickets should be different
    REQUIRE(std::string(ticket1) != std::string(ticket2));
    REQUIRE(std::string(ticket1).length() > 0);
    REQUIRE(std::string(ticket2).length() > 0);
}

TEST_CASE("ElsterWrapper: XBRL content preservation", "[elster]") {
    ElsterConfig config;
    config.herstellerID = "12345";

    Stammdaten stamm;
    stamm.finanzamtsNummer = "9181";
    stamm.firmenname = "Test GmbH";
    stamm.bilanzstichtag = "2023-12-31";

    std::string xbrlContent = R"(<?xml version="1.0" encoding="UTF-8"?>
<xbrl xmlns="http://www.xbrl.org/2003/instance" xmlns:de-gaap-ci="http://www.xbrl.de/taxonomies/de-gaap-ci">
  <context id="current_instant">
    <entity>
      <identifier scheme="http://www.rzf-nrw.de/Steuernummer">9181012345678</identifier>
    </entity>
    <period>
      <instant>2023-12-31</instant>
    </period>
  </context>
  <de-gaap-ci:bs.ass>150000.00</de-gaap-ci:bs.ass>
</xbrl>)";

    ElsterWrapper wrapper(config);
    std::string wrappedXml = wrapper.wrap(xbrlContent, stamm, false);

    pugi::xml_document doc;
    doc.load_string(wrappedXml.c_str());

    auto nutzdaten = doc.child("Elster").child("DatenTeil")
                        .child("Nutzdatenblock").child("Nutzdaten");
    auto ebilanzEl = nutzdaten.child("ebilanz:EBilanz");
    REQUIRE(ebilanzEl);
    auto xbrlRoot = ebilanzEl.child("xbrl");

    REQUIRE(xbrlRoot);

    // Verify XBRL namespace is preserved
    REQUIRE(std::string(xbrlRoot.attribute("xmlns").value()) == "http://www.xbrl.org/2003/instance");

    // Verify context is preserved
    auto context = xbrlRoot.child("context");
    REQUIRE(context);
    REQUIRE(std::string(context.attribute("id").value()) == "current_instant");

    // Verify data element is preserved (using namespace prefix)
    auto dataElement = xbrlRoot.child("de-gaap-ci:bs.ass");
    REQUIRE(dataElement);
    REQUIRE(dataElement.text().as_double() == 150000.00);
}

TEST_CASE("ElsterWrapper: DatenLieferant override", "[elster]") {
    ElsterConfig config;
    config.herstellerID = "12345";
    config.datenLieferant = "Override Steuerberater GmbH";

    Stammdaten stamm;
    stamm.finanzamtsNummer = "9181";
    stamm.firmenname = "Original Firmenname";
    stamm.bilanzstichtag = "2024-12-31";

    std::string xbrlContent = R"(<?xml version="1.0" encoding="UTF-8"?>
<xbrl xmlns="http://www.xbrl.org/2003/instance"></xbrl>)";

    ElsterWrapper wrapper(config);
    std::string wrappedXml = wrapper.wrap(xbrlContent, stamm, false);

    pugi::xml_document doc;
    doc.load_string(wrappedXml.c_str());

    auto datenLieferant = doc.child("Elster").child("TransferHeader").child("DatenLieferant");
    REQUIRE(std::string(datenLieferant.text().as_string()) == "Override Steuerberater GmbH");
}

TEST_CASE("ElsterWrapper: HerstellerID validation", "[elster]") {
    ElsterConfig config;
    config.herstellerID = "";  // Empty HerstellerID

    REQUIRE_THROWS_AS(ElsterWrapper(config), std::runtime_error);
}

TEST_CASE("ElsterWrapper: XML well-formedness", "[elster]") {
    ElsterConfig config;
    config.herstellerID = "12345";

    Stammdaten stamm;
    stamm.finanzamtsNummer = "9181";
    stamm.firmenname = "Test GmbH";
    stamm.bilanzstichtag = "2024-12-31";

    std::string xbrlContent = R"(<?xml version="1.0" encoding="UTF-8"?>
<xbrl xmlns="http://www.xbrl.org/2003/instance"></xbrl>)";

    ElsterWrapper wrapper(config);
    std::string wrappedXml = wrapper.wrap(xbrlContent, stamm, false);

    // Parse the output XML to verify it's well-formed
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_string(wrappedXml.c_str());

    REQUIRE(result);
    REQUIRE(result.status == pugi::status_ok);

    // Verify it starts with XML declaration
    REQUIRE(wrappedXml.find("<?xml version=\"1.0\" encoding=\"UTF-8\"?>") == 0);
}
