# ebilanz-submit

**A free, open-source command-line tool to file your German E-Bilanz — the electronic balance sheet that every German company is legally required to submit to the tax office.**

Fill in an Excel template, run one command. `ebilanz-submit` validates your figures, generates an HGB-compliant XBRL document, and transmits it to the Finanzamt through the official ERiC (ELSTER) interface. Local, free, open source — no cloud, no subscription, no telemetry.

---

## Why this exists

**The E-Bilanz can only be filed through software — and all of it costs money.**

Since 2013 (§5b EStG), every German company that prepares a balance sheet — every GmbH, and most other businesses — must transmit its balance sheet and profit-and-loss statement to the tax office electronically, in XBRL format. There is no opting out.

And there is no manual way to do it. Unlike an income-tax return, the E-Bilanz **cannot** be typed into a free ELSTER web form — it can only be transmitted through software that speaks the official ERiC interface. ERiC itself is free, but until now every application built on top of it has been commercial:

- **Priced for tax advisors, not founders.** Licenses run into the hundreds of euros per year.
- **Overblown for a small company.** Most packages are bloated with features a one-person GmbH will never touch.
- **So you must pay just to comply.** To meet a legal obligation, a small company either buys software it barely uses, or hands the job — and a few hundred euros — to its Steuerberater, every single year.

A legal requirement should not be paywalled. `ebilanz-submit` is the missing free path: it drives the same official, free ERiC interface from a simple Excel template, on your own machine — so a small, owner-run company can file its own E-Bilanz without paying anyone, and without its financial data ever leaving the building.

> **Scope:** GmbH (§266 Abs. 1 HGB), balance sheet + P&L (Gesamtkostenverfahren). No cash-flow statement, notes, or management report — deliberately focused on the simple balance sheets of small companies. Contributions extending the scope are welcome.

---

## Für wen ist das Tool?

- **Solo-Unternehmer mit kleiner GmbH** (§266 Abs. 1 HGB)
- Die **keine teure Steuerberater-Software** benötigen
- Die ihre **E-Bilanz selbst erstellen** und übermitteln möchten

**Scope:**
- ✅ Nur GmbH (keine UG/AG)
- ✅ Bilanz + GuV (Gesamtkostenverfahren)
- ❌ Keine Kapitalflussrechnung
- ❌ Kein Anhang, kein Lagebericht

---

## Features

✅ **Excel-basierte Dateneingabe** — Einfaches Template statt komplizierter Software
✅ **Automatische Validierung** — 12 Prüfregeln für korrekte Bilanzen
✅ **HGB Taxonomie 6.8** — Aktuelle XBRL-Standards
✅ **ElsterBasis-Wrapper** — Korrekte XML-Struktur für ELSTER
✅ **Test-Modus** — Vorproduktion-Server zum Testen
✅ **macOS-native** — Entwickelt und getestet auf macOS

---

## Schnellstart

### 1. Dependencies installieren (macOS)

```bash
brew install cmake pugixml cli11 spdlog nlohmann-json catch2
```

### 2. Projekt bauen

```bash
git clone <repository-url>
cd ebilanz
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(sysctl -n hw.ncpu)
```

### 3. Excel-Template ausfüllen

Nutze `templates/ebilanz-template.xlsx`:

| Sheet | Inhalt |
|-------|--------|
| **Stammdaten** | Firmendaten, Steuernummer, BuFa-Nr, Wirtschaftsjahr |
| **Bilanz** | Aktiva & Passiva nach HGB §266 (aktuelles + Vorjahr) |
| **GuV** | Gewinn- und Verlustrechnung (Gesamtkostenverfahren) |
| **Validierung** | Automatische Checks (eingebaute Formeln) |
| **Info** | Hilfe & Anleitung |

**Hinweis:**
- **HerstellerID** (Zeile 24): **Pflicht** für die Übermittlung. Jeder Hersteller benötigt eine eigene, bei ELSTER registrierte HerstellerID — siehe Abschnitt „HerstellerID & DatenLieferant". Alternativ via `--hersteller-id`.
- **DatenLieferant** (Zeile 25): Optional, falls abweichend vom Firmennamen

### 4. Daten validieren

```bash
./build/ebilanz-submit --validate meine-bilanz.xlsx
```

Prüft:
- ✅ Bilanz ausgeglichen (Aktiva = Passiva)
- ✅ Jahresüberschuss konsistent (Bilanz = GuV)
- ✅ Stammkapital ≥ 25.000 EUR (GmbH)
- ✅ Steuernummer-Format korrekt
- ✅ Alle Pflichtfelder vorhanden

### 5. XBRL generieren

```bash
./build/ebilanz-submit --output /tmp meine-bilanz.xlsx
```

Erzeugt:
- `/tmp/ebilanz-2024.xbrl` — Pure XBRL (zum Inspizieren)
- `/tmp/ebilanz-2024-elster.xml` — ElsterBasis-Wrapper (zur Übermittlung)

### 6. Test-Übermittlung (optional)

```bash
./build/ebilanz-submit --test --cert zertifikat.pfx meine-bilanz.xlsx
```

⚠️ **Hinweis:** ERiC SDK auf macOS hat Probleme mit Code-Signing der Plugin-Bibliotheken. Verwende stattdessen die generierten XML-Dateien für manuelle Übermittlung.

---

## HerstellerID & DatenLieferant

### HerstellerID (Pflicht)

Die HerstellerID identifiziert die übermittelnde Software gegenüber ELSTER. **Es ist keine HerstellerID eingebaut** — jeder Nutzer/Hersteller muss seine eigene, bei ELSTER registrierte HerstellerID verwenden. Ohne HerstellerID bricht die Übermittlung mit einem Fehler ab.

HerstellerID beantragen: https://www.elster.de/elsterweb/entwickler

**Setzen:**
- **Via CLI:** `--hersteller-id XXXXX`
- **Via Excel:** Eintragen in Stammdaten Zeile 24

Priorität: CLI-Flag > Excel-Feld (B24)

### DatenLieferant (Optional)

**Was ist das?**
- Name des **Datenlieferanten** (wer übermittelt die Daten)
- Standard: Ihr Firmenname aus Stammdaten
- Kann überschrieben werden

**Wann überschreiben?**
- Steuerberater übermittelt für Mandant
- Dritter übermittelt in Ihrem Auftrag

**Wie nutzen?**
- **Standard:** Automatisch = Firmenname
- **Override:** Eintragen in Stammdaten Zeile 25

---

## CLI-Referenz

```bash
ebilanz-submit [OPTIONEN] <bilanz.xlsx>
```

### Pflicht-Parameter

| Parameter | Beschreibung |
|-----------|-------------|
| `<bilanz.xlsx>` | Excel-Datei mit Bilanzdaten |

### Modi

| Option | Beschreibung |
|--------|-------------|
| `-v, --validate` | Nur validieren, nicht übermitteln |
| `-t, --test` | Test-Modus (Vorproduktion-Server) |
| `-o, --output <dir>` | XBRL-Dateien speichern (ohne Übermittlung) |

### Übermittlung

| Option | Beschreibung |
|--------|-------------|
| `-c, --cert <file>` | ELSTER-Zertifikat (.pfx) |
| `-p, --password <pw>` | Zertifikat-Passwort (⚠️ sichtbar in Prozessliste/History — sonst interaktiv) |
| `--hersteller-id <id>` | ELSTER HerstellerID (Pflicht; alternativ Excel-Feld B24) |

### Logging

| Option | Beschreibung |
|--------|-------------|
| `--verbose` | Ausführliche Debug-Ausgabe |
| `--quiet` | Nur Fehler anzeigen |
| `--log <file>` | Log-Datei (Standard: ebilanz.log) |

### Konfiguration

| Option | Beschreibung |
|--------|-------------|
| `--taxonomie <json>` | Taxonomie-Config-Datei |
| `--bufa <json>` | BuFa-Verzeichnis-Datei |

### Sonstiges

| Option | Beschreibung |
|--------|-------------|
| `--version` | Versionsnummer anzeigen |
| `-h, --help` | Hilfe anzeigen |

---

## Beispiele

**Validierung:**
```bash
./ebilanz-submit --validate bilanz-2024.xlsx
```

**XBRL generieren (ohne Übermittlung):**
```bash
./ebilanz-submit --output /tmp bilanz-2024.xlsx
```

**Test-Übermittlung:**
```bash
./ebilanz-submit --test --cert zertifikat.pfx bilanz-2024.xlsx
```

**Produktiv-Übermittlung:**
```bash
./ebilanz-submit --cert zertifikat.pfx bilanz-2024.xlsx
```

**Mit Passwort-Datei:**
```bash
./ebilanz-submit --cert zertifikat.pfx --password "Geheim123!" bilanz-2024.xlsx
```

---

## ERiC SDK Setup (macOS)

⚠️ **Bekanntes Problem:** ERiC SDK auf macOS hat Schwierigkeiten mit Code-Signing der Plugin-Bibliotheken. Die XBRL-Generierung funktioniert einwandfrei, aber die direkte Übermittlung via ERiC kann blockiert werden.

**Alternative:** Nutze `--output` zum Generieren der XML-Dateien und übermittle diese manuell über das ELSTER-Portal.

### Installation (für Experimente)

1. **Download:**
   https://www.elster.de/elsterweb/entwickler
   Datei: `ERiC-43.3.2.0-Darwin-universal.jar` (oder aktueller)

2. **Extrahieren:**
   ```bash
   cd ~/Downloads
   unzip -q ERiC-*.jar -d eric-extracted
   ```

3. **Kopieren:**
   ```bash
   cd /pfad/zum/ebilanz-projekt
   mkdir -p external/eric-sdk
   cp -r ~/Downloads/eric-extracted/ERiC-*/Darwin-universal/include external/eric-sdk/
   cp -r ~/Downloads/eric-extracted/ERiC-*/Darwin-universal/lib external/eric-sdk/
   ```

4. **macOS Security Fix:**
   ```bash
   # Quarantine entfernen
   xattr -d com.apple.quarantine external/eric-sdk/lib/*.dylib 2>/dev/null || true
   find external/eric-sdk/lib/plugins -name "*.dylib" -exec xattr -d com.apple.quarantine {} \; 2>/dev/null || true

   # Ad-hoc Signieren
   codesign --force --deep --sign - external/eric-sdk/lib/*.dylib 2>/dev/null || true
   find external/eric-sdk/lib/plugins -name "*.dylib" -exec codesign --force --deep --sign - {} \; 2>/dev/null || true
   ```

5. **Neu bauen:**
   ```bash
   cmake -B build -DWITH_ERIC=ON
   cmake --build build
   ```

---

## Validierungsregeln

Das Tool prüft automatisch:

| # | Regel | Typ | Bedingung |
|---|-------|-----|-----------|
| 1 | Bilanz ausgeglichen | ERROR | \|Aktiva - Passiva\| < 0,01 € |
| 2 | Jahresüberschuss konsistent | ERROR | \|Bilanz.JÜ - GuV.JÜ\| < 0,01 € |
| 3 | GmbH-Kapital | ERROR | Stammkapital ≥ 25.000 € |
| 4 | Steuernummer Format | ERROR | Pattern: `XX/XXX/XXXXX` |
| 5 | BuFa-Nummer | ERROR | 4-stellig numerisch |
| 6 | Bundeseinh. Steuernummer | ERROR | 13-stellig numerisch |
| 7 | Handelsregister | ERROR | Pattern: `HRB XXXXX` |
| 8 | Pflichtfelder | ERROR | Alle Stammdaten vorhanden |
| 9 | Erstjahr-Konsistenz | WARNING | Vorjahr = 0 wenn Erstjahr |
| 10 | Ergebnisverwendung | ERROR | Ausschüttung > 0 wenn nicht Thesaurierung |
| 11 | Wirtschaftsjahr | WARNING | Dauer ≤ 18 Monate |
| 12 | Bilanzstichtag | ERROR | = Wirtschaftsjahr-Ende |

---

## Projektstruktur

```
ebilanz/
├── CMakeLists.txt              # Build-Konfiguration
├── README.md                   # Diese Datei
├── CLAUDE.md                   # Entwickler-Anweisungen
├── config/
│   ├── taxonomie-6.8.json      # HGB Taxonomie 6.8 Mapping
│   └── bufa-verzeichnis.json   # Bundesland → BuFa Mapping
├── templates/
│   └── ebilanz-template.xlsx   # Excel-Vorlage für Nutzer
├── external/
│   └── eric-sdk/               # ERiC SDK (optional, nicht im Repo)
├── src/
│   ├── main.cpp                # CLI Entry Point
│   ├── excel_parser.*          # Excel-Parsing (xlnt)
│   ├── validator.*             # Datenvalidierung
│   ├── xbrl_generator.*        # XBRL-Generierung (HGB 6.8)
│   ├── elster_wrapper.*        # ElsterBasis XML Wrapper
│   ├── eric_connector.*        # ERiC SDK Integration
│   ├── steuernummer.*          # Steuernummer-Konvertierung
│   └── ...
├── tests/
│   ├── test_*.cpp              # Unit & Integration Tests
│   └── testdata/               # Test-Excel-Dateien
└── docs/
    └── lastenheft-*.md         # Vollständige Spezifikation
```

---

## Tests ausführen

```bash
cd build
ctest --output-on-failure
```

**Test-Abdeckung:**
- ✅ Excel-Parsing (4 Test-Cases)
- ✅ Validierung (12 Regeln)
- ✅ Steuernummer-Konvertierung (3 Bundesländer)
- ✅ XBRL-Generierung (5 Test-Cases)
- ✅ ElsterBasis-Wrapper (8 Test-Cases, 42 Assertions)
- ✅ ERiC-Integration (2 Test-Cases)

---

## Troubleshooting

### "Library not loaded" Fehler (macOS)
→ ERiC SDK Bibliotheken müssen signiert werden (siehe ERiC SDK Setup)

### "ERiC Initialisierung fehlgeschlagen: 610001083"
→ macOS blockiert ERiC-Plugins aufgrund von Code-Signing-Problemen
→ **Workaround:** Nutze `--output` und übermittle XML manuell

### "Bilanz nicht ausgeglichen"
→ Prüfe Excel: Summe Aktiva muss exakt Summe Passiva entsprechen
→ Nutze Sheet "Validierung" für automatische Checks

### "Stammkapital < 25.000 EUR"
→ Dieses Tool ist nur für GmbH (nicht UG)
→ GmbH benötigt mindestens 25.000 € Stammkapital

---

## Contributing

Contributions are very welcome — this tool exists to save small companies money, and every improvement compounds across everyone who uses it. Good places to start:

- **Broaden the scope:** UG (haftungsbeschränkt) and other legal forms, or balance-sheet positions not yet mapped.
- **Annual taxonomy updates:** the HGB taxonomy changes most years; the mapping is data, not code — see `config/taxonomie-6.8.json`.
- **Cross-platform:** Windows/Linux build and packaging (the core is portable C++17; only password entry and the ERiC setup are platform-specific).
- **Tests** for new validation rules or taxonomy positions.

Please open an issue to discuss anything non-trivial first, and run `ctest` before opening a PR. Codebase conventions and architecture live in [`CLAUDE.md`](CLAUDE.md).

> **Disclaimer:** This tool does not constitute tax advice. You are responsible for the correctness of the data you submit. When in doubt, consult a Steuerberater.

---

## Lizenz

MIT License — siehe LICENSE-Datei

---

## Credits

Entwickelt für Solo-GmbH-Unternehmer, die die Kontrolle über ihre E-Bilanz behalten möchten.

**Dependencies:**
- [xlnt](https://github.com/tfussell/xlnt) — Excel-Verarbeitung
- [pugixml](https://pugixml.org/) — XML-Generierung
- [CLI11](https://github.com/CLIUtils/CLI11) — Command-Line Parsing
- [spdlog](https://github.com/gabime/spdlog) — Logging
- [nlohmann/json](https://github.com/nlohmann/json) — JSON-Parsing
- [Catch2](https://github.com/catchorg/Catch2) — Testing

**ELSTER:**
- ERiC SDK © Bayerisches Landesamt für Steuern
- HGB Taxonomie © XBRL Deutschland e.V.

---

## Kontakt & Support

- **Issues:** GitHub Issues
- **Fragen:** Siehe `docs/lastenheft-*.md` für Details
- **ELSTER Support:** https://www.elster.de/elsterweb/entwickler

---

**Hinweis:** Dieses Tool ersetzt keine steuerliche Beratung. Konsultiere bei Unsicherheiten einen Steuerberater.
