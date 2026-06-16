# CLAUDE.md — E-Bilanz CLI-Tool

> Projektanweisungen für die Entwicklung mit Claude Code.
> Dieses Dokument wird automatisch geladen und steuert wie Claude Code am Projekt arbeitet.

---

## Projekt-Überblick

**Was:** Ein C++17 CLI-Tool (`ebilanz-submit`) das GmbH-Bilanzdaten aus einem Excel-Template liest, validiert, eine XBRL-Datei nach HGB-Taxonomie generiert und über das ERiC-SDK (ELSTER) ans deutsche Finanzamt übermittelt.

**Für wen:** Solo-Unternehmer mit kleiner GmbH (§266 Abs. 1 HGB), die keine teure Steuerberater-Software benötigen.

**Scope:** Nur GmbH (keine UG/AG), nur Bilanz + GuV (Gesamtkostenverfahren), keine Kapitalflussrechnung, kein Anhang, kein Lagebericht. Ausführliches Lastenheft: `docs/lastenheft-ebilanz-cli-tool.md`

---

## Technischer Stack

| Komponente | Technologie | Version | Zweck |
|---|---|---|---|
| Sprache | C++ | 17+ | Core |
| Build | CMake | 3.20+ | Build-System |
| Excel | xlnt | latest | .xlsx lesen |
| XML | pugixml | latest | XBRL generieren |
| CLI | CLI11 | latest | Argument Parsing |
| Logging | spdlog | latest | Datei + Console Logging |
| JSON | nlohmann/json | latest | Taxonomie-Config laden |
| Testing | Catch2 | v3 | Unit + Integration Tests |
| ELSTER | ERiC-SDK | aktuell | Finanzamts-Übermittlung |

**Compiler:**
- macOS (primär): Clang++ via Xcode Command Line Tools
- Windows (sekundär): MSVC oder MinGW-w64

---

## Projektstruktur

```
ebilanz-tool/
├── CMakeLists.txt                    # Haupt-CMake
├── CLAUDE.md                         # ← Du bist hier
├── README.md
├── LICENSE
├── config/
│   ├── taxonomie-6.8.json            # XBRL-Tag-Mapping (austauschbar bei Updates)
│   └── bufa-verzeichnis.json         # BuFa-Nummern → Bundesland-Konvertierung
├── templates/
│   └── ebilanz-template.xlsx         # Excel-Vorlage für Nutzer
├── external/
│   └── eric-sdk/                     # ERiC-SDK (NICHT im Git, lokal)
│       ├── include/ericapi.h
│       └── lib/
├── src/
│   ├── main.cpp                      # CLI Entry Point (CLI11)
│   ├── excel_parser.h / .cpp         # xlnt-basiertes Excel-Parsing
│   ├── validator.h / .cpp            # Daten-Validierung
│   ├── xbrl_generator.h / .cpp       # XBRL-XML-Generierung (pugixml)
│   ├── eric_connector.h / .cpp       # ERiC-SDK Wrapper
│   ├── steuernummer.h / .cpp         # Steuernummer-Konvertierung
│   ├── taxonomie_config.h / .cpp     # JSON-Config für Taxonomie-Mapping
│   ├── logger.h / .cpp               # spdlog Setup
│   ├── models.h                      # Alle Datenstrukturen
│   └── utils.h / .cpp                # Hilfsfunktionen
├── tests/
│   ├── test_excel_parser.cpp
│   ├── test_validator.cpp
│   ├── test_xbrl_generator.cpp
│   ├── test_steuernummer.cpp
│   └── testdata/
│       ├── test-bilanz.xlsx
│       ├── test-bilanz-erstjahr.xlsx
│       └── test-bilanz-invalid.xlsx
└── docs/
    ├── lastenheft-ebilanz-cli-tool.md
    ├── MANUAL.md
    └── XBRL_MAPPING.md
```

---

## Build & Run

```bash
# Dependencies installieren (macOS)
brew install xlnt pugixml cli11 spdlog nlohmann-json catch2

# Build
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)

# Tests
cd build && ctest --output-on-failure

# Run
./build/ebilanz-submit --validate templates/ebilanz-template.xlsx
./build/ebilanz-submit --test --cert pfad/zu/zertifikat.pfx bilanz.xlsx
```

**ERiC-SDK Setup (manuell):**
Das ERiC-SDK ist proprietär und muss von https://www.elster.de/elsterweb/entwickler heruntergeladen werden. Entpacken nach `external/eric-sdk/`. Die Library wird per CMake manuell gelinkt — nicht über find_package.

---

## Code-Konventionen

### Sprache & Stil
- **Sprache im Code:** Englisch (Variablen, Funktionen, Kommentare)
- **Sprache in UI-Output:** Deutsch (Fehlermeldungen, CLI-Ausgabe)
- **Sprache in Docs:** Deutsch (Lastenheft, README, MANUAL)
- **Naming:** camelCase für Variablen/Funktionen, PascalCase für Klassen/Structs, UPPER_CASE für Enums/Konstanten
- **Header Guards:** `#pragma once`
- **Includes:** Standard Library zuerst, dann externe Libraries, dann Projekt-Header
- **Formatierung:** 4 Spaces Indent, öffnende Klammer auf gleicher Zeile

### Architektur-Prinzipien
- **Single Responsibility:** Jede Klasse hat eine Aufgabe (Parser, Validator, Generator, Connector)
- **Dependency Injection:** Klassen erhalten ihre Abhängigkeiten über Konstruktor (z.B. `XBRLGenerator` bekommt `TaxonomieConfig`)
- **Kein globaler State:** Alles über Parameter und Rückgabewerte
- **Error Handling:** Return-Types mit Result-Pattern (`struct ValidationResult`, `struct SubmitResult`), keine Exceptions für erwartete Fehler
- **RAII:** ERiC-Handles und Zertifikate über RAII verwalten (Konstruktor init, Destruktor cleanup)

### Wichtige Patterns

**Datenfluss:**
```
Excel (.xlsx)
  → ExcelParser → Stammdaten + BilanzData + GuVData (models.h)
    → Validator → ValidationResult (errors/warnings)
      → XBRLGenerator → std::string (XBRL-XML)
        → ERiCConnector → SubmitResult (ticket/error)
```

**Steuernummer-Konvertierung:**
Die vereinfachte Steuernummer (XX/XXX/XXXXX) + BuFa-Nr (4-stellig) muss in die bundeseinheitliche 13-stellige Steuernummer konvertiert werden. Die Konvertierung ist Bundesland-spezifisch. Regeln aus `config/bufa-verzeichnis.json` laden.

**Taxonomie-Mapping:**
Das Mapping HGB-Position → XBRL-Tag ist in `config/taxonomie-6.8.json` konfiguriert, NICHT hard-coded. Bei jährlichen Taxonomie-Updates wird nur die JSON-Datei ausgetauscht.

**Auffangpositionen:**
Wenn Detailpositionen in der Bilanz nicht befüllt sind (= 0), muss der Gesamtwert in die XBRL-Auffangposition geschrieben werden. Dies ist ein Pflichtmechanismus der HGB-Taxonomie.

---

## Datenmodelle (models.h)

Die zentralen Structs — bei Änderungen hier zuerst aktualisieren:

```cpp
struct Stammdaten {
    std::string steuernummer;          // Vereinfacht: XX/XXX/XXXXX
    std::string steuernummerBund;      // Bundeseinheitlich: 13-stellig
    std::string finanzamtsNummer;      // BuFa-Nr: 4-stellig
    std::string firmenname;
    std::string strasse;
    std::string plz;
    std::string ort;
    std::string geschaeftsfuehrer;
    std::string handelsregisterNr;     // HRB XXXXX
    std::string registergericht;
    std::string wirtschaftsjahrVon;    // ISO: YYYY-MM-DD
    std::string wirtschaftsjahrBis;    // ISO: YYYY-MM-DD
    std::string bilanzstichtag;        // ISO: YYYY-MM-DD
    std::string wzCode;                // optional
    std::string uebermittlungsanlass;  // "erstmalig" | "korrigiert"
    std::string bilanzaufstellung;     // "handelsrecht"
    bool erstjahr;                     // Steuert Vorjahr-Handling
    std::string ergebnisverwendung;    // "thesaurierung" | "ausschuettung" | "teilausschuettung"
    double ausschuettungsbetrag;
};

struct BilanzData {
    // Aktiva — jede Position einzeln + Summen
    // Vorjahreswerte als separate Map oder parallele Felder
    std::map<std::string, double> positionen;       // posId → Betrag
    std::map<std::string, double> vorjahrPositionen; // posId → Vorjahres-Betrag
    double summeAktiva;
    double summePassiva;
    double gezeichnetesKapital;
    double jahresueberschuss;
    // Vorjahr
    double vorjahr_summeAktiva;
    double vorjahr_summePassiva;
};

struct GuVData {
    std::map<std::string, double> positionen;
    std::map<std::string, double> vorjahrPositionen;
    double jahresueberschuss;
};
```

---

## Validierungsregeln

Diese Regeln MÜSSEN alle implementiert sein. Wenn du eine Validierung änderst, aktualisiere diese Liste:

| # | Check | Typ | Bedingung |
|---|---|---|---|
| 1 | Bilanz ausgeglichen | ERROR | `abs(summeAktiva - summePassiva) < 0.01` |
| 2 | Jahresüberschuss konsistent | ERROR | `abs(bilanz.jahresueberschuss - guv.jahresueberschuss) < 0.01` |
| 3 | GmbH-Kapital | ERROR | `gezeichnetesKapital >= 25000.0` (NICHT Warning — UG nicht unterstützt) |
| 4 | Steuernummer Format | ERROR | Regex `\d{2}/\d{3}/\d{5}` |
| 5 | BuFa-Nummer | ERROR | Regex `\d{4}` |
| 6 | Bundeseinh. Steuernummer | ERROR | 13-stellig numerisch |
| 7 | Handelsregister | ERROR | Regex `HRB\s*\d+` |
| 8 | Pflichtfelder | ERROR | Alle Pflichtfelder non-empty |
| 9 | Erstjahr-Konsistenz | WARNING | Vorjahreswerte = 0 wenn Erstjahr |
| 10 | Ergebnisverwendung | ERROR | Ausschüttungsbetrag > 0 wenn nicht Thesaurierung |
| 11 | Wirtschaftsjahr Dauer | WARNING | ≤ 18 Monate |
| 12 | Bilanzstichtag | ERROR | Muss = Wirtschaftsjahr-Ende |

---

## CLI-Interface

```
ebilanz-submit [OPTIONS] <bilanz.xlsx>

Pflicht (bei Versand):
  -c, --cert <file>    ELSTER-Zertifikat (.pfx)

Modi:
  -v, --validate       Nur validieren
  -t, --test           Test-Modus (Vorproduktion)
  (kein Flag)          Echtversand

Optionen:
  -p, --password       Zertifikat-Passwort (sonst interaktiv)
  -o, --output <dir>   XBRL-Datei speichern
  -x, --xbrl <file>    XBRL direkt senden (skip Excel-Parsing)
      --verbose        Debug-Level Ausgabe
      --quiet          Nur Fehler
      --log <file>     Log-Datei (default: ./ebilanz.log)
      --version        Version anzeigen
  -h, --help           Hilfe
```

**Exit-Codes:** Entsprechen `ErrorCode` enum in `models.h` (0 = Erfolg, 1-99 = spezifische Fehler).

---

## Sicherheits-Regeln

Diese Regeln sind NICHT verhandelbar:

1. **Zertifikat-Passwort NIEMALS loggen** — weder Console noch Datei
2. **Passwort nach Verwendung aus Speicher löschen** — `secureZero()` mit volatile
3. **Keine Telemetrie, keine Cloud, keine Analytics**
4. **XBRL nur lokal speichern** (außer ELSTER-Versand)
5. **Transfer-History enthält KEINE Bilanzzahlen** — nur Ticket + Timestamp
6. **Interaktive Passwort-Eingabe** ohne Terminal-Echo (POSIX: termios, Windows: _getch)

---

## ERiC-SDK Hinweise

Das ERiC-SDK ist schlecht dokumentiert. Wichtige Hinweise:

- Die exakten Funktionsnamen und Signaturen aus der Header-Datei `ericapi.h` verwenden — das Lastenheft enthält nur Pseudocode
- Die Haupt-Funktion heißt typischerweise `EricBearbeiteVorgang()`, nicht die vereinfachten Namen aus dem Lastenheft
- Handle-Management: `EricInitialisiere()` → Handle → `EricBeende()`
- Zertifikat: `EricGetHandleToCertificate()` → CertHandle → `EricCloseHandleToCertificate()`
- Test-Modus nutzt einen separaten Server (Vorproduktion)
- **Bei API-Unklarheiten:** Immer die Header-Datei und die ERiC-Beispielprogramme als Referenz nehmen, nicht den Pseudocode im Lastenheft

---

## XBRL-Generierung: Wichtige Details

### Contexts
Jede XBRL-Datei braucht mindestens diese Contexts:
- `current` (duration): Wirtschaftsjahr-Zeitraum für GuV
- `current_instant`: Bilanzstichtag für Bilanzpositionen

**Keine Prior-Contexts (HAOE/Einheitsbilanz):** ERiC weist Vorjahres-Contexts für die
HAOE-Einheitsbilanz ab („keine Handelsbilanz vor", Fehlercode 170205105). Daher werden
`prior`/`prior_instant` bewusst NICHT erzeugt — auch nicht im Erstjahr. Siehe
`xbrl_generator.cpp`.

### Identifier
Der Entity-Identifier ist die **bundeseinheitliche 13-stellige Steuernummer** (nicht das vereinfachte Format). Schema: `http://www.rzf-nrw.de/Steuernummer`

### Taxonomie-Config Format
```json
{
  "version": "6.8",
  "namespace": "http://www.xbrl.de/taxonomies/de-gaap-ci-2024-...",
  "schemaRef": "http://...",
  "mapping": {
    "immaterielleVG_konzessionen": "de-gaap-ci:bs.ass.fixAss.intangAss.concessionsPIR",
    "sachanlagen_bga": "de-gaap-ci:bs.ass.fixAss.tan.otherEq",
    ...
  },
  "auffangpositionen": {
    "immaterielleVG": "de-gaap-ci:bs.ass.fixAss.intangAss.other",
    ...
  },
  "mussfelder": [
    "de-gaap-ci:bs.ass",
    "de-gaap-ci:bs.eqLiab",
    ...
  ]
}
```

---

## Testing-Richtlinien

### Was MUSS getestet sein
- Jede Validierungsregel (positive + negative Cases)
- Steuernummer-Konvertierung (mindestens 3 Bundesländer)
- Excel-Parsing aller 4 Daten-Sheets
- XBRL-Generierung mit current + prior Contexts
- Erstjahr-Handling (Vorjahre = 0, prior-Context trotzdem vorhanden)
- Auffangpositionen-Logik
- Error-Code-Mapping
- CLI Argument-Parsing (alle Flags)

### Test-Benennung
```cpp
TEST_CASE("Komponente: Was wird getestet") {
    SECTION("Spezifischer Fall") { ... }
}
```

### Testdaten
- `testdata/test-bilanz.xlsx` — Valide Bilanz mit Beispielwerten aus Lastenheft
- `testdata/test-bilanz-erstjahr.xlsx` — Erstjahr, alle Vorjahre = 0
- `testdata/test-bilanz-invalid.xlsx` — Diverse Fehler (unausgeglichen, fehlendes Kapital, etc.)

### ERiC-Tests
ERiC-Tests nur ausführen wenn SDK vorhanden. Prüfen mit:
```cpp
if (ericSdkAvailable()) { ... }
```
Oder CMake-Option: `-DWITH_ERIC_TESTS=ON`

---

## Häufige Aufgaben

### Neue Bilanzposition hinzufügen
1. Position in `models.h` → `BilanzData` ergänzen
2. Excel-Zelle in `excel_parser.cpp` → `readBilanz()` mappen
3. XBRL-Tag in `config/taxonomie-6.8.json` → `mapping` eintragen
4. Test in `test_excel_parser.cpp` ergänzen
5. Ggf. Auffangposition in Config prüfen

### Taxonomie-Version aktualisieren
1. Neue `config/taxonomie-X.Y.json` erstellen
2. Namespace-URI und Schema-Ref aktualisieren
3. Mapping-Änderungen eintragen
4. Alte Config als Referenz behalten
5. Tests mit neuer Config durchlaufen lassen

### Neuen CLI-Flag hinzufügen
1. Flag in `main.cpp` mit CLI11 registrieren
2. In diesem CLAUDE.md unter "CLI-Interface" dokumentieren
3. In README.md + MANUAL.md ergänzen
4. Test für Argument-Parsing ergänzen

---

## Bekannte Fallstricke

1. **xlnt und Datumsformate:** Excel speichert Daten als Seriennummern. `xlnt` kann das, aber Vorsicht bei `to_string()` — immer explizit als Date parsen und nach ISO konvertieren.

2. **Floating Point Vergleiche:** Bilanzwerte IMMER mit Epsilon (0.01) vergleichen, nie mit `==`. Die 0.01 Toleranz ist bewusst gewählt (Cent-Genauigkeit).

3. **pugixml Encoding:** XBRL muss UTF-8 sein. pugixml nutzt per Default UTF-8, aber Vorsicht bei deutschen Sonderzeichen (ä, ö, ü, ß) in Firmennamen.

4. **ERiC-SDK Thread Safety:** ERiC ist NICHT thread-safe. Niemals parallel aufrufen. Für dieses Single-User-Tool kein Problem, aber bei etwaiger Erweiterung beachten.

5. **Windows vs. POSIX:** Die interaktive Passwort-Eingabe (`readPassword()`) braucht platform-spezifischen Code. `#ifdef _WIN32` verwenden.

6. **Steuernummer-Konvertierung:** Die Regeln sind Bundesland-spezifisch und nicht trivial. Immer aus Config laden, nie hard-coden.

7. **XBRL Auffangpositionen:** Wenn ALLE Detailpositionen einer Gruppe 0 sind, die Summe aber auch 0, dann muss die Auffangposition NICHT befüllt werden. Nur wenn eine Summe > 0 ist aber keine Detailposition, greift die Auffangposition.

---

## Referenzen

- **Lastenheft (vollständig):** `docs/lastenheft-ebilanz-cli-tool.md`
- **ELSTER Entwickler-Portal:** https://www.elster.de/elsterweb/entwickler
- **HGB-Taxonomie:** https://www.xbrl.de/taxonomie/
- **xlnt Docs:** https://github.com/tfussell/xlnt
- **pugixml Docs:** https://pugixml.org/docs/
- **CLI11 Docs:** https://github.com/CLIUtils/CLI11
- **spdlog Docs:** https://github.com/gabime/spdlog
- **ERiC-SDK Doku:** Nach Download im SDK enthalten (`doc/` Verzeichnis)
