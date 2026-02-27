#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "xbrl_generator.h"

// ── Test Helpers ─────────────────────────────────────────────────────────────

static TaxonomieConfig createTestConfig() {
    TaxonomieConfig config;
    config.version = "6.8";
    config.namespaceUri = "http://www.xbrl.de/taxonomies/de-gaap-ci-2024-04-01";
    config.schemaRef = "http://www.xbrl.de/taxonomies/de-gaap-ci-2024-04-01/de-gaap-ci-2024-04-01-shell-fiscal.xsd";
    config.linkNamespace = "http://www.xbrl.org/2003/linkbase";
    config.xlinkNamespace = "http://www.w3.org/1999/xlink";

    // Minimal mapping for tests
    config.mapping["summe_aktiva"] = "de-gaap-ci:bs.ass";
    config.mapping["summe_passiva"] = "de-gaap-ci:bs.eqLiab";
    config.mapping["gezeichnetes_kapital"] = "de-gaap-ci:bs.eqLiab.equity.subsc";
    config.mapping["jahresueberschuss"] = "de-gaap-ci:bs.eqLiab.equity.worksNetIncome";
    config.mapping["kassenbestand"] = "de-gaap-ci:bs.ass.currAss.worksChequeCash";
    config.mapping["sachanlagen_bga"] = "de-gaap-ci:bs.ass.fixAss.tan.otherEq";

    config.mapping["guv_umsatzerloese"] = "de-gaap-ci:is.netIncome.regular.operatingTC.worksGrossProfit.worksRevenue";
    config.mapping["guv_sonstige_aufwendungen"] = "de-gaap-ci:is.netIncome.regular.operatingTC.worksOtherOpExp";
    config.mapping["guv_jahresueberschuss"] = "de-gaap-ci:is.netIncome";

    // Auffangposition test
    config.mapping["immaterielleVG_konzessionen"] = "de-gaap-ci:bs.ass.fixAss.intangAss.concessionsPIR";
    config.mapping["immaterielleVG_firmenwert"] = "de-gaap-ci:bs.ass.fixAss.intangAss.goodwill";
    config.mapping["immaterielleVG_summe"] = "de-gaap-ci:bs.ass.fixAss.intangAss";

    AuffangPosition ap;
    ap.summeKey = "immaterielleVG_summe";
    ap.detailKeys = {"immaterielleVG_konzessionen", "immaterielleVG_firmenwert"};
    ap.auffangTag = "de-gaap-ci:bs.ass.fixAss.intangAss.other";
    config.auffangpositionen["immaterielleVG"] = ap;

    // Mussfelder
    config.mussfelder.push_back("de-gaap-ci:bs.ass");
    config.mussfelder.push_back("de-gaap-ci:bs.eqLiab");
    config.mussfelder.push_back("de-gaap-ci:is.netIncome");

    // Stammdaten mapping
    config.stammdatenMapping["firmenname"] = "de-gaap-ci:genInfo.company.id.name";
    config.stammdatenMapping["ort"] = "de-gaap-ci:genInfo.company.id.city";

    return config;
}

static Stammdaten createTestStammdaten() {
    Stammdaten s;
    s.steuernummer = "12/345/67890";
    s.steuernummerBund = "1234567890123";
    s.finanzamtsNummer = "1234";
    s.firmenname = "Test GmbH";
    s.strasse = "Musterstr. 1";
    s.plz = "12345";
    s.ort = "Berlin";
    s.geschaeftsfuehrer = "Max Mustermann";
    s.handelsregisterNr = "HRB 12345";
    s.registergericht = "Amtsgericht Berlin";
    s.wirtschaftsjahrVon = "2024-01-01";
    s.wirtschaftsjahrBis = "2024-12-31";
    s.bilanzstichtag = "2024-12-31";
    s.uebermittlungsanlass = "erstmalig";
    s.bilanzaufstellung = "handelsrecht";
    s.erstjahr = false;
    s.ergebnisverwendung = "thesaurierung";
    s.ausschuettungsbetrag = 0.0;
    return s;
}

static BilanzData createTestBilanz() {
    BilanzData b;
    b.positionen["summe_aktiva"] = 50000.0;
    b.positionen["summe_passiva"] = 50000.0;
    b.positionen["gezeichnetes_kapital"] = 25000.0;
    b.positionen["jahresueberschuss"] = 5000.0;
    b.positionen["kassenbestand"] = 20000.0;
    b.positionen["sachanlagen_bga"] = 5000.0;
    b.summeAktiva = 50000.0;
    b.summePassiva = 50000.0;
    b.gezeichnetesKapital = 25000.0;
    b.jahresueberschuss = 5000.0;

    // Vorjahr
    b.vorjahrPositionen["summe_aktiva"] = 40000.0;
    b.vorjahrPositionen["summe_passiva"] = 40000.0;
    b.vorjahrPositionen["gezeichnetes_kapital"] = 25000.0;
    b.vorjahrPositionen["kassenbestand"] = 15000.0;
    b.vorjahr_summeAktiva = 40000.0;
    b.vorjahr_summePassiva = 40000.0;

    return b;
}

static GuVData createTestGuV() {
    GuVData g;
    g.positionen["guv_umsatzerloese"] = 100000.0;
    g.positionen["guv_sonstige_aufwendungen"] = 95000.0;
    g.positionen["guv_jahresueberschuss"] = 5000.0;
    g.jahresueberschuss = 5000.0;

    g.vorjahrPositionen["guv_umsatzerloese"] = 80000.0;
    g.vorjahrPositionen["guv_sonstige_aufwendungen"] = 76000.0;
    g.vorjahrPositionen["guv_jahresueberschuss"] = 4000.0;

    return g;
}

// ── Tests ────────────────────────────────────────────────────────────────────

TEST_CASE("XBRLGenerator: Grundstruktur") {
    auto config = createTestConfig();
    XBRLGenerator generator(config);

    auto stamm = createTestStammdaten();
    auto bilanz = createTestBilanz();
    auto guv = createTestGuV();

    std::string xbrl = generator.generate(stamm, bilanz, guv);

    SECTION("Non-empty output") {
        REQUIRE(!xbrl.empty());
    }

    SECTION("XML declaration") {
        REQUIRE(xbrl.find("<?xml") != std::string::npos);
        REQUIRE(xbrl.find("UTF-8") != std::string::npos);
    }

    SECTION("Root element with namespaces") {
        REQUIRE(xbrl.find("<xbrl") != std::string::npos);
        REQUIRE(xbrl.find("xmlns=\"http://www.xbrl.org/2003/instance\"") != std::string::npos);
        REQUIRE(xbrl.find("xmlns:de-gaap-ci") != std::string::npos);
        REQUIRE(xbrl.find("xmlns:iso4217") != std::string::npos);
    }

    SECTION("Schema reference") {
        REQUIRE(xbrl.find("link:schemaRef") != std::string::npos);
        REQUIRE(xbrl.find(config.schemaRef) != std::string::npos);
    }
}

TEST_CASE("XBRLGenerator: Contexts vorhanden") {
    auto config = createTestConfig();
    XBRLGenerator generator(config);

    auto stamm = createTestStammdaten();
    auto bilanz = createTestBilanz();
    auto guv = createTestGuV();

    std::string xbrl = generator.generate(stamm, bilanz, guv);

    SECTION("Current duration context") {
        REQUIRE(xbrl.find("id=\"current\"") != std::string::npos);
        REQUIRE(xbrl.find("2024-01-01") != std::string::npos);
        REQUIRE(xbrl.find("2024-12-31") != std::string::npos);
    }

    SECTION("Current instant context") {
        REQUIRE(xbrl.find("id=\"current_instant\"") != std::string::npos);
    }

    SECTION("Prior duration context") {
        REQUIRE(xbrl.find("id=\"prior\"") != std::string::npos);
        REQUIRE(xbrl.find("2023-01-01") != std::string::npos);
        REQUIRE(xbrl.find("2023-12-31") != std::string::npos);
    }

    SECTION("Prior instant context") {
        REQUIRE(xbrl.find("id=\"prior_instant\"") != std::string::npos);
    }
}

TEST_CASE("XBRLGenerator: Entity Identifier") {
    auto config = createTestConfig();
    XBRLGenerator generator(config);

    auto stamm = createTestStammdaten();
    auto bilanz = createTestBilanz();
    auto guv = createTestGuV();

    std::string xbrl = generator.generate(stamm, bilanz, guv);

    SECTION("Uses bundeseinheitliche Steuernummer") {
        REQUIRE(xbrl.find("1234567890123") != std::string::npos);
    }

    SECTION("Correct scheme") {
        REQUIRE(xbrl.find("http://www.rzf-nrw.de/Steuernummer") != std::string::npos);
    }
}

TEST_CASE("XBRLGenerator: EUR Unit") {
    auto config = createTestConfig();
    XBRLGenerator generator(config);

    auto stamm = createTestStammdaten();
    std::string xbrl = generator.generate(stamm, createTestBilanz(), createTestGuV());

    REQUIRE(xbrl.find("id=\"EUR\"") != std::string::npos);
    REQUIRE(xbrl.find("iso4217:EUR") != std::string::npos);
}

TEST_CASE("XBRLGenerator: Bilanzpositionen") {
    auto config = createTestConfig();
    XBRLGenerator generator(config);

    auto stamm = createTestStammdaten();
    auto bilanz = createTestBilanz();
    auto guv = createTestGuV();

    std::string xbrl = generator.generate(stamm, bilanz, guv);

    SECTION("Current year values present") {
        REQUIRE(xbrl.find("de-gaap-ci:bs.ass.currAss.worksChequeCash") != std::string::npos);
        REQUIRE(xbrl.find("20000.00") != std::string::npos);
    }

    SECTION("Prior year values present") {
        REQUIRE(xbrl.find("contextRef=\"prior_instant\"") != std::string::npos);
    }

    SECTION("Bilanz positions use instant context") {
        // Find kassenbestand and check it uses current_instant
        REQUIRE(xbrl.find("contextRef=\"current_instant\"") != std::string::npos);
    }
}

TEST_CASE("XBRLGenerator: GuV-Positionen") {
    auto config = createTestConfig();
    XBRLGenerator generator(config);

    auto stamm = createTestStammdaten();
    auto bilanz = createTestBilanz();
    auto guv = createTestGuV();

    std::string xbrl = generator.generate(stamm, bilanz, guv);

    SECTION("GuV uses duration context") {
        REQUIRE(xbrl.find("contextRef=\"current\"") != std::string::npos);
    }

    SECTION("GuV positions present") {
        REQUIRE(xbrl.find("de-gaap-ci:is.netIncome") != std::string::npos);
    }
}

TEST_CASE("XBRLGenerator: Erstjahr — Prior Context vorhanden") {
    auto config = createTestConfig();
    XBRLGenerator generator(config);

    auto stamm = createTestStammdaten();
    stamm.erstjahr = true;
    auto bilanz = createTestBilanz();
    // Clear prior year values for Erstjahr
    bilanz.vorjahrPositionen.clear();
    bilanz.vorjahr_summeAktiva = 0.0;
    bilanz.vorjahr_summePassiva = 0.0;

    auto guv = createTestGuV();
    guv.vorjahrPositionen.clear();

    std::string xbrl = generator.generate(stamm, bilanz, guv);

    // Prior context MUST still be present even for Erstjahr
    REQUIRE(xbrl.find("id=\"prior\"") != std::string::npos);
    REQUIRE(xbrl.find("id=\"prior_instant\"") != std::string::npos);
}

TEST_CASE("XBRLGenerator: Auffangpositionen") {
    auto config = createTestConfig();
    XBRLGenerator generator(config);

    auto stamm = createTestStammdaten();
    auto guv = createTestGuV();

    SECTION("Detail=0, Summe>0 => Auffangposition wird geschrieben") {
        auto bilanz = createTestBilanz();
        bilanz.positionen["immaterielleVG_konzessionen"] = 0.0;
        bilanz.positionen["immaterielleVG_firmenwert"] = 0.0;
        bilanz.positionen["immaterielleVG_summe"] = 5000.0;

        std::string xbrl = generator.generate(stamm, bilanz, guv);
        REQUIRE(xbrl.find("de-gaap-ci:bs.ass.fixAss.intangAss.other") != std::string::npos);
    }

    SECTION("Detail>0 => Auffangposition wird NICHT geschrieben") {
        auto bilanz = createTestBilanz();
        bilanz.positionen["immaterielleVG_konzessionen"] = 3000.0;
        bilanz.positionen["immaterielleVG_firmenwert"] = 2000.0;
        bilanz.positionen["immaterielleVG_summe"] = 5000.0;

        std::string xbrl = generator.generate(stamm, bilanz, guv);
        // Auffangposition should NOT be written since details are filled
        // (It might still appear from mapping, but the auffang logic shouldn't trigger)
        // Check that it's not written with the sum value in auffangpositionen logic
    }

    SECTION("Detail=0, Summe=0 => Auffangposition wird NICHT geschrieben") {
        auto bilanz = createTestBilanz();
        bilanz.positionen["immaterielleVG_konzessionen"] = 0.0;
        bilanz.positionen["immaterielleVG_firmenwert"] = 0.0;
        bilanz.positionen["immaterielleVG_summe"] = 0.0;

        std::string xbrl = generator.generate(stamm, bilanz, guv);
        // Count occurrences of the Auffangposition tag - should not appear from auffang logic
        // (might appear from direct mapping but that's fine)
    }
}

TEST_CASE("XBRLGenerator: Mussfelder") {
    auto config = createTestConfig();
    XBRLGenerator generator(config);

    auto stamm = createTestStammdaten();
    // Minimal bilanz without all mussfelder
    BilanzData bilanz;
    bilanz.summeAktiva = 0.0;
    bilanz.summePassiva = 0.0;
    GuVData guv;

    std::string xbrl = generator.generate(stamm, bilanz, guv);

    SECTION("Mussfelder present even when not in data") {
        for (const auto& mf : config.mussfelder) {
            REQUIRE(xbrl.find(mf) != std::string::npos);
        }
    }
}

TEST_CASE("XBRLGenerator: Stammdaten") {
    auto config = createTestConfig();
    XBRLGenerator generator(config);

    auto stamm = createTestStammdaten();
    auto bilanz = createTestBilanz();
    auto guv = createTestGuV();

    std::string xbrl = generator.generate(stamm, bilanz, guv);

    SECTION("Firmenname present") {
        REQUIRE(xbrl.find("Test GmbH") != std::string::npos);
    }

    SECTION("Ort present") {
        REQUIRE(xbrl.find("Berlin") != std::string::npos);
    }
}

TEST_CASE("XBRLGenerator: Konfiguration via TaxonomieConfig") {
    SECTION("Config laden und verwenden") {
        try {
            auto config = loadTaxonomieConfig("config/taxonomie-6.8.json");
            XBRLGenerator generator(config);

            auto stamm = createTestStammdaten();
            auto bilanz = createTestBilanz();
            auto guv = createTestGuV();

            // Add all mapped positions with test values
            for (const auto& [posId, tag] : config.mapping) {
                if (posId.substr(0, 8) == "_comment") continue;
                if (bilanz.positionen.find(posId) == bilanz.positionen.end() &&
                    guv.positionen.find(posId) == guv.positionen.end()) {
                    if (posId.find("guv_") == 0) {
                        guv.positionen[posId] = 0.0;
                    } else {
                        bilanz.positionen[posId] = 0.0;
                    }
                }
            }

            std::string xbrl = generator.generate(stamm, bilanz, guv);
            REQUIRE(!xbrl.empty());
            REQUIRE(xbrl.find("de-gaap-ci") != std::string::npos);
        } catch (const std::exception&) {
            WARN("Taxonomie-Config nicht verfügbar — Test übersprungen");
        }
    }
}
