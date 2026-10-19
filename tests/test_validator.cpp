#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "validator.h"

// ── Test Helpers ─────────────────────────────────────────────────────────────

static Stammdaten createValidStammdaten() {
    Stammdaten s;
    s.steuernummer = "12/345/67890";
    s.steuernummerBund = "1234567890123";
    s.finanzamtsNummer = "1234";
    s.firmenname = "Test GmbH";
    s.rechtsform = "gmbh";
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

static BilanzData createBalancedBilanz() {
    BilanzData b;
    b.summeAktiva = 50000.0;
    b.summePassiva = 50000.0;
    b.gezeichnetesKapital = 25000.0;
    b.jahresueberschuss = 5000.0;
    return b;
}

static GuVData createMatchingGuV(double jahresueberschuss = 5000.0) {
    GuVData g;
    g.jahresueberschuss = jahresueberschuss;
    return g;
}

// ── Rule 1: Bilanz ausgeglichen ──────────────────────────────────────────────

TEST_CASE("Validator: Bilanz ausgeglichen") {
    Validator v;
    auto stamm = createValidStammdaten();
    auto bilanz = createBalancedBilanz();
    auto guv = createMatchingGuV();

    SECTION("Exakt ausgeglichen") {
        auto result = v.validate(stamm, bilanz, guv);
        REQUIRE(result.isValid);
    }

    SECTION("Differenz innerhalb Epsilon (0.005 < 0.01)") {
        bilanz.summeAktiva = 50000.005;
        auto result = v.validate(stamm, bilanz, guv);
        REQUIRE(result.isValid);
    }

    SECTION("Nicht ausgeglichen") {
        bilanz.summeAktiva = 50100.0;
        auto result = v.validate(stamm, bilanz, guv);
        REQUIRE_FALSE(result.isValid);
        REQUIRE(result.errors.size() >= 1);
        REQUIRE(result.errors[0].rule == "bilanz_ausgeglichen");
    }
}

// ── Rule 2: Jahresüberschuss konsistent ──────────────────────────────────────

TEST_CASE("Validator: Jahresüberschuss konsistent") {
    Validator v;
    auto stamm = createValidStammdaten();
    auto bilanz = createBalancedBilanz();

    SECTION("Konsistent") {
        auto guv = createMatchingGuV(5000.0);
        auto result = v.validate(stamm, bilanz, guv);
        REQUIRE(result.isValid);
    }

    SECTION("Inkonsistent") {
        auto guv = createMatchingGuV(6000.0);
        auto result = v.validate(stamm, bilanz, guv);
        REQUIRE_FALSE(result.isValid);
        bool found = false;
        for (const auto& e : result.errors) {
            if (e.rule == "jahresueberschuss_konsistent") found = true;
        }
        REQUIRE(found);
    }
}

// ── Rule 3: GmbH-Kapital >= 25.000 ──────────────────────────────────────────

TEST_CASE("Validator: GmbH-Kapital") {
    Validator v;
    auto stamm = createValidStammdaten();
    auto guv = createMatchingGuV();

    SECTION("Exakt 25.000 — OK") {
        auto bilanz = createBalancedBilanz();
        bilanz.gezeichnetesKapital = 25000.0;
        auto result = v.validate(stamm, bilanz, guv);
        // Should not have gmbh_kapital error
        bool hasKapitalError = false;
        for (const auto& e : result.errors) {
            if (e.rule == "gmbh_kapital") hasKapitalError = true;
        }
        REQUIRE_FALSE(hasKapitalError);
    }

    SECTION("Über 25.000 — OK") {
        auto bilanz = createBalancedBilanz();
        bilanz.gezeichnetesKapital = 50000.0;
        auto result = v.validate(stamm, bilanz, guv);
        bool hasKapitalError = false;
        for (const auto& e : result.errors) {
            if (e.rule == "gmbh_kapital") hasKapitalError = true;
        }
        REQUIRE_FALSE(hasKapitalError);
    }

    SECTION("GmbH unter 25.000 — ERROR") {
        auto bilanz = createBalancedBilanz();
        bilanz.gezeichnetesKapital = 12500.0;
        auto result = v.validate(stamm, bilanz, guv);
        REQUIRE_FALSE(result.isValid);
        bool hasKapitalError = false;
        for (const auto& e : result.errors) {
            if (e.rule == "gmbh_kapital") hasKapitalError = true;
        }
        REQUIRE(hasKapitalError);
    }

    SECTION("GmbH mit 1 EUR — ERROR") {
        auto bilanz = createBalancedBilanz();
        bilanz.gezeichnetesKapital = 1.0;
        auto result = v.validate(stamm, bilanz, guv);
        REQUIRE_FALSE(result.isValid);
        bool hasKapitalError = false;
        for (const auto& e : result.errors) {
            if (e.rule == "gmbh_kapital") hasKapitalError = true;
        }
        REQUIRE(hasKapitalError);
    }
}

// ── Rule 3b: UG-Kapital >= 1 ─────────────────────────────────────────────────

TEST_CASE("Validator: UG-Kapital") {
    Validator v;
    auto stamm = createValidStammdaten();
    stamm.rechtsform = "ug";
    auto guv = createMatchingGuV();

    SECTION("1 EUR — OK") {
        auto bilanz = createBalancedBilanz();
        bilanz.gezeichnetesKapital = 1.0;
        auto result = v.validate(stamm, bilanz, guv);
        REQUIRE(result.isValid);
        REQUIRE(result.errors.empty());
    }

    SECTION("12.500 EUR — OK") {
        auto bilanz = createBalancedBilanz();
        bilanz.gezeichnetesKapital = 12500.0;
        auto result = v.validate(stamm, bilanz, guv);
        REQUIRE(result.isValid);
        REQUIRE(result.errors.empty());
    }

    SECTION("0 EUR — ERROR") {
        auto bilanz = createBalancedBilanz();
        bilanz.gezeichnetesKapital = 0.0;
        auto result = v.validate(stamm, bilanz, guv);
        REQUIRE_FALSE(result.isValid);
        bool hasKapitalError = false;
        for (const auto& e : result.errors) {
            if (e.rule == "ug_kapital") hasKapitalError = true;
        }
        REQUIRE(hasKapitalError);
    }

    SECTION("30.000 EUR — WARNING (Umfirmierung zur GmbH möglich)") {
        auto bilanz = createBalancedBilanz();
        bilanz.gezeichnetesKapital = 30000.0;
        auto result = v.validate(stamm, bilanz, guv);
        REQUIRE(result.errors.empty());
        bool hasKapitalWarning = false;
        for (const auto& w : result.warnings) {
            if (w.rule == "ug_kapital") hasKapitalWarning = true;
        }
        REQUIRE(hasKapitalWarning);
    }
}

// ── Rule 3c: Rechtsform bekannt ──────────────────────────────────────────────

TEST_CASE("Validator: Rechtsform") {
    Validator v;
    auto bilanz = createBalancedBilanz();
    auto guv = createMatchingGuV();

    SECTION("Leer — ERROR, keine Kapitalregel") {
        auto stamm = createValidStammdaten();
        stamm.rechtsform = "";
        auto result = v.validate(stamm, bilanz, guv);
        REQUIRE_FALSE(result.isValid);
        REQUIRE(result.errors.size() == 1);
        REQUIRE(result.errors[0].rule == "rechtsform");
        for (const auto& w : result.warnings) {
            REQUIRE(w.rule != "gmbh_kapital");
            REQUIRE(w.rule != "ug_kapital");
        }
    }

    SECTION("Unbekannt ('ag') — ERROR, keine Kapitalregel") {
        auto stamm = createValidStammdaten();
        stamm.rechtsform = "ag";
        auto result = v.validate(stamm, bilanz, guv);
        REQUIRE_FALSE(result.isValid);
        REQUIRE(result.errors.size() == 1);
        REQUIRE(result.errors[0].rule == "rechtsform");
        for (const auto& w : result.warnings) {
            REQUIRE(w.rule != "gmbh_kapital");
            REQUIRE(w.rule != "ug_kapital");
        }
    }

    SECTION("GmbH — kein Rechtsform-Fehler") {
        auto stamm = createValidStammdaten();
        stamm.rechtsform = "gmbh";
        auto result = v.validate(stamm, bilanz, guv);
        bool hasError = false;
        for (const auto& e : result.errors) {
            if (e.rule == "rechtsform") hasError = true;
        }
        REQUIRE_FALSE(hasError);
    }

    SECTION("UG — kein Rechtsform-Fehler") {
        auto stamm = createValidStammdaten();
        stamm.rechtsform = "ug";
        auto result = v.validate(stamm, bilanz, guv);
        bool hasError = false;
        for (const auto& e : result.errors) {
            if (e.rule == "rechtsform") hasError = true;
        }
        REQUIRE_FALSE(hasError);
    }
}

// ── Rule 3d: UG gesetzliche Rücklage ─────────────────────────────────────────

TEST_CASE("Validator: UG gesetzliche Rücklage (§ 5a Abs. 3 GmbHG)") {
    Validator v;
    auto stamm = createValidStammdaten();
    stamm.rechtsform = "ug";
    stamm.ergebnisverwendung = "ausschuettung";

    auto bilanz = createBalancedBilanz();
    bilanz.gezeichnetesKapital = 1.0;
    bilanz.jahresueberschuss = 10000.0;
    auto guv = createMatchingGuV(10000.0);

    SECTION("Ausschüttung über 75 % des Jahresüberschusses — WARNING") {
        stamm.ausschuettungsbetrag = 9000.0;
        auto result = v.validate(stamm, bilanz, guv);
        bool hasWarning = false;
        for (const auto& w : result.warnings) {
            if (w.rule == "ug_ruecklage") hasWarning = true;
        }
        REQUIRE(hasWarning);
        // Warning only — Rücklagenbildung is not machine-verifiable from the Bilanz
        REQUIRE(result.isValid);
    }

    SECTION("Ausschüttung innerhalb 75 % — keine Warnung") {
        stamm.ausschuettungsbetrag = 7000.0;
        auto result = v.validate(stamm, bilanz, guv);
        bool hasWarning = false;
        for (const auto& w : result.warnings) {
            if (w.rule == "ug_ruecklage") hasWarning = true;
        }
        REQUIRE_FALSE(hasWarning);
        REQUIRE(result.isValid);
    }
}

// ── Rule 4: Steuernummer Format ──────────────────────────────────────────────

TEST_CASE("Validator: Steuernummer Format") {
    Validator v;
    auto bilanz = createBalancedBilanz();
    auto guv = createMatchingGuV();

    SECTION("Gültiges Format") {
        auto stamm = createValidStammdaten();
        auto result = v.validate(stamm, bilanz, guv);
        bool hasStNrError = false;
        for (const auto& e : result.errors) {
            if (e.rule == "steuernummer_format") hasStNrError = true;
        }
        REQUIRE_FALSE(hasStNrError);
    }

    SECTION("Ungültiges Format") {
        auto stamm = createValidStammdaten();
        stamm.steuernummer = "invalid";
        auto result = v.validate(stamm, bilanz, guv);
        REQUIRE_FALSE(result.isValid);
    }
}

// ── Rule 5: BuFa-Nummer Format ──────────────────────────────────────────────

TEST_CASE("Validator: BuFa-Nummer Format") {
    Validator v;
    auto bilanz = createBalancedBilanz();
    auto guv = createMatchingGuV();

    SECTION("Gültiges Format") {
        auto stamm = createValidStammdaten();
        stamm.finanzamtsNummer = "2893";
        auto result = v.validate(stamm, bilanz, guv);
        bool hasBufaError = false;
        for (const auto& e : result.errors) {
            if (e.rule == "bufa_format") hasBufaError = true;
        }
        REQUIRE_FALSE(hasBufaError);
    }

    SECTION("Ungültiges Format") {
        auto stamm = createValidStammdaten();
        stamm.finanzamtsNummer = "12";
        auto result = v.validate(stamm, bilanz, guv);
        REQUIRE_FALSE(result.isValid);
    }
}

// ── Rule 6: Bundeseinheitliche Steuernummer ──────────────────────────────────

TEST_CASE("Validator: Bundeseinheitliche Steuernummer") {
    Validator v;
    auto bilanz = createBalancedBilanz();
    auto guv = createMatchingGuV();

    SECTION("13 Stellen — OK") {
        auto stamm = createValidStammdaten();
        stamm.steuernummerBund = "1234567890123";
        auto result = v.validate(stamm, bilanz, guv);
        bool hasError = false;
        for (const auto& e : result.errors) {
            if (e.rule == "bundessteuernummer_format") hasError = true;
        }
        REQUIRE_FALSE(hasError);
    }

    SECTION("Weniger als 13 Stellen — ERROR") {
        auto stamm = createValidStammdaten();
        stamm.steuernummerBund = "123456789";
        auto result = v.validate(stamm, bilanz, guv);
        REQUIRE_FALSE(result.isValid);
    }
}

// ── Rule 7: Handelsregister-Nr ───────────────────────────────────────────────

TEST_CASE("Validator: Handelsregister-Nr") {
    Validator v;
    auto bilanz = createBalancedBilanz();
    auto guv = createMatchingGuV();

    SECTION("HRB 12345 — OK") {
        auto stamm = createValidStammdaten();
        stamm.handelsregisterNr = "HRB 12345";
        auto result = v.validate(stamm, bilanz, guv);
        bool hasError = false;
        for (const auto& e : result.errors) {
            if (e.rule == "handelsregister_format") hasError = true;
        }
        REQUIRE_FALSE(hasError);
    }

    SECTION("HRB12345 (kein Leerzeichen) — OK") {
        auto stamm = createValidStammdaten();
        stamm.handelsregisterNr = "HRB12345";
        auto result = v.validate(stamm, bilanz, guv);
        bool hasError = false;
        for (const auto& e : result.errors) {
            if (e.rule == "handelsregister_format") hasError = true;
        }
        REQUIRE_FALSE(hasError);
    }

    SECTION("Ungültig — ERROR") {
        auto stamm = createValidStammdaten();
        stamm.handelsregisterNr = "12345";
        auto result = v.validate(stamm, bilanz, guv);
        REQUIRE_FALSE(result.isValid);
    }
}

// ── Rule 8: Pflichtfelder ────────────────────────────────────────────────────

TEST_CASE("Validator: Pflichtfelder") {
    Validator v;
    auto bilanz = createBalancedBilanz();
    auto guv = createMatchingGuV();

    SECTION("Alle ausgefüllt — OK") {
        auto stamm = createValidStammdaten();
        auto result = v.validate(stamm, bilanz, guv);
        bool hasError = false;
        for (const auto& e : result.errors) {
            if (e.rule == "pflichtfelder") hasError = true;
        }
        REQUIRE_FALSE(hasError);
    }

    SECTION("Firmenname leer — ERROR") {
        auto stamm = createValidStammdaten();
        stamm.firmenname = "";
        auto result = v.validate(stamm, bilanz, guv);
        REQUIRE_FALSE(result.isValid);
        bool hasError = false;
        for (const auto& e : result.errors) {
            if (e.rule == "pflichtfelder") hasError = true;
        }
        REQUIRE(hasError);
    }
}

// ── Rule 9: Erstjahr-Konsistenz ──────────────────────────────────────────────

TEST_CASE("Validator: Erstjahr-Konsistenz") {
    Validator v;
    auto bilanz = createBalancedBilanz();
    auto guv = createMatchingGuV();

    SECTION("Erstjahr mit Vorjahr=0 — OK, keine Warnung") {
        auto stamm = createValidStammdaten();
        stamm.erstjahr = true;
        auto result = v.validate(stamm, bilanz, guv);
        bool hasWarning = false;
        for (const auto& w : result.warnings) {
            if (w.rule == "erstjahr_konsistenz") hasWarning = true;
        }
        REQUIRE_FALSE(hasWarning);
    }

    SECTION("Erstjahr mit Vorjahr!=0 — WARNING") {
        auto stamm = createValidStammdaten();
        stamm.erstjahr = true;
        bilanz.vorjahrPositionen["gezeichnetes_kapital"] = 25000.0;
        auto result = v.validate(stamm, bilanz, guv);
        bool hasWarning = false;
        for (const auto& w : result.warnings) {
            if (w.rule == "erstjahr_konsistenz") hasWarning = true;
        }
        REQUIRE(hasWarning);
        // Should still be valid (it's a warning, not error)
        // Note: isValid is only affected by errors, not warnings
    }

    SECTION("Nicht Erstjahr — keine Warnung unabhängig von Vorjahreswerten") {
        auto stamm = createValidStammdaten();
        stamm.erstjahr = false;
        bilanz.vorjahrPositionen["gezeichnetes_kapital"] = 25000.0;
        auto result = v.validate(stamm, bilanz, guv);
        bool hasWarning = false;
        for (const auto& w : result.warnings) {
            if (w.rule == "erstjahr_konsistenz") hasWarning = true;
        }
        REQUIRE_FALSE(hasWarning);
    }
}

// ── Rule 10: Ergebnisverwendung ──────────────────────────────────────────────

TEST_CASE("Validator: Ergebnisverwendung") {
    Validator v;
    auto bilanz = createBalancedBilanz();
    auto guv = createMatchingGuV();

    SECTION("Thesaurierung ohne Betrag — OK") {
        auto stamm = createValidStammdaten();
        stamm.ergebnisverwendung = "thesaurierung";
        stamm.ausschuettungsbetrag = 0.0;
        auto result = v.validate(stamm, bilanz, guv);
        bool hasError = false;
        for (const auto& e : result.errors) {
            if (e.rule == "ergebnisverwendung") hasError = true;
        }
        REQUIRE_FALSE(hasError);
    }

    SECTION("Ausschüttung mit Betrag — OK") {
        auto stamm = createValidStammdaten();
        stamm.ergebnisverwendung = "ausschuettung";
        stamm.ausschuettungsbetrag = 5000.0;
        auto result = v.validate(stamm, bilanz, guv);
        bool hasError = false;
        for (const auto& e : result.errors) {
            if (e.rule == "ergebnisverwendung") hasError = true;
        }
        REQUIRE_FALSE(hasError);
    }

    SECTION("Ausschüttung ohne Betrag — ERROR") {
        auto stamm = createValidStammdaten();
        stamm.ergebnisverwendung = "ausschuettung";
        stamm.ausschuettungsbetrag = 0.0;
        auto result = v.validate(stamm, bilanz, guv);
        REQUIRE_FALSE(result.isValid);
    }

    SECTION("Teilausschüttung ohne Betrag — ERROR") {
        auto stamm = createValidStammdaten();
        stamm.ergebnisverwendung = "teilausschuettung";
        stamm.ausschuettungsbetrag = 0.0;
        auto result = v.validate(stamm, bilanz, guv);
        REQUIRE_FALSE(result.isValid);
    }
}

// ── Rule 11: Wirtschaftsjahr Dauer ───────────────────────────────────────────

TEST_CASE("Validator: Wirtschaftsjahr Dauer") {
    Validator v;
    auto bilanz = createBalancedBilanz();
    auto guv = createMatchingGuV();

    SECTION("12 Monate — OK") {
        auto stamm = createValidStammdaten();
        auto result = v.validate(stamm, bilanz, guv);
        bool hasWarning = false;
        for (const auto& w : result.warnings) {
            if (w.rule == "wirtschaftsjahr_dauer") hasWarning = true;
        }
        REQUIRE_FALSE(hasWarning);
    }

    SECTION("Über 18 Monate — WARNING") {
        auto stamm = createValidStammdaten();
        stamm.wirtschaftsjahrVon = "2023-01-01";
        stamm.wirtschaftsjahrBis = "2024-12-31";
        auto result = v.validate(stamm, bilanz, guv);
        bool hasWarning = false;
        for (const auto& w : result.warnings) {
            if (w.rule == "wirtschaftsjahr_dauer") hasWarning = true;
        }
        REQUIRE(hasWarning);
    }
}

// ── Rule 12: Bilanzstichtag = Wirtschaftsjahr-Ende ───────────────────────────

TEST_CASE("Validator: Bilanzstichtag") {
    Validator v;
    auto bilanz = createBalancedBilanz();
    auto guv = createMatchingGuV();

    SECTION("Stichtag = Ende — OK") {
        auto stamm = createValidStammdaten();
        stamm.bilanzstichtag = "2024-12-31";
        stamm.wirtschaftsjahrBis = "2024-12-31";
        auto result = v.validate(stamm, bilanz, guv);
        bool hasError = false;
        for (const auto& e : result.errors) {
            if (e.rule == "bilanzstichtag") hasError = true;
        }
        REQUIRE_FALSE(hasError);
    }

    SECTION("Stichtag != Ende — ERROR") {
        auto stamm = createValidStammdaten();
        stamm.bilanzstichtag = "2024-12-30";
        stamm.wirtschaftsjahrBis = "2024-12-31";
        auto result = v.validate(stamm, bilanz, guv);
        REQUIRE_FALSE(result.isValid);
    }
}

// ── Combined: All rules pass ─────────────────────────────────────────────────

TEST_CASE("Validator: Vollständig gültige Daten") {
    Validator v;
    auto stamm = createValidStammdaten();
    auto bilanz = createBalancedBilanz();
    auto guv = createMatchingGuV();

    auto result = v.validate(stamm, bilanz, guv);
    REQUIRE(result.isValid);
    REQUIRE(result.errors.empty());
}
