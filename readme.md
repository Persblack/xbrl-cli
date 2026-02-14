# E-Bilanz CLI Tool

**Elektronische Bilanzübermittlung** ans deutsche Finanzamt — Ein C++17 CLI-Tool für Solo-GmbHs.

Liest Bilanzdaten aus Excel, generiert HGB-konforme XBRL-Dateien und übermittelt diese über das ERiC-SDK (ELSTER) ans Finanzamt.

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
| **Stammdaten** | Firmendaten, Steuernummer, BuFa-Nr, Wirtschaftsjahr, **HerstellerID** |
| **Bilanz** | Aktiva & Passiva nach HGB §266 (aktuelles + Vorjahr) |
| **GuV** | Gewinn- und Verlustrechnung (Gesamtkostenverfahren) |
| **Validierung** | Automatische Checks (eingebaute Formeln) |
| **Info** | Hilfe & Anleitung |

**Wichtig:**
- **HerstellerID** (Zeile 24): ELSTER-Hersteller-ID vom Entwicklerportal
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
./build/ebilanz-submit --output /tmp --hersteller-id 74458 meine-bilanz.xlsx
```

Erzeugt:
- `/tmp/ebilanz-2024.xbrl` — Pure XBRL (zum Inspizieren)
- `/tmp/ebilanz-2024-elster.xml` — ElsterBasis-Wrapper (zur Übermittlung)

### 6. Test-Übermittlung (optional)

```bash
./build/ebilanz-submit --test --cert zertifikat.pfx --hersteller-id 74458 meine-bilanz.xlsx
```

⚠️ **Hinweis:** ERiC SDK auf macOS hat Probleme mit Code-Signing der Plugin-Bibliotheken. Verwende stattdessen die generierten XML-Dateien für manuelle Übermittlung.

---

## HerstellerID & DatenLieferant

### HerstellerID (Pflicht!)

**Was ist das?**
- Eindeutige Kennung für **Software-Entwickler**
- Von ELSTER vergeben zur Identifikation der Software
- Wird in jeder Übermittlung mitgesendet

**Wo bekomme ich eine?**
1. Registrierung: https://www.elster.de/elsterweb/entwickler
2. Antrag auf Entwickler-Zugang stellen
3. ELSTER vergibt HerstellerID (z.B. `74458`)

**Wie nutzen?**
- **Via CLI:** `--hersteller-id 74458`
- **Via Excel:** Eintragen in Stammdaten Zeile 24

**Wichtig:** Pro Entwickler/Tool, NICHT pro Nutzer!

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
| `-p, --password <pw>` | Zertifikat-Passwort (sonst interaktiv) |
| `--hersteller-id <id>` | ELSTER HerstellerID (Pflicht für Versand!) |

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
./ebilanz-submit --output /tmp --hersteller-id 74458 bilanz-2024.xlsx
```

**Test-Übermittlung:**
```bash
./ebilanz-submit --test --cert zertifikat.pfx --hersteller-id 74458 bilanz-2024.xlsx
```

**Produktiv-Übermittlung:**
```bash
./ebilanz-submit --cert zertifikat.pfx --hersteller-id 74458 bilanz-2024.xlsx
```

**Mit Passwort-Datei:**
```bash
./ebilanz-submit --cert zertifikat.pfx --password "Geheim123!" --hersteller-id 74458 bilanz-2024.xlsx
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

### "HerstellerID erforderlich für Versand"
→ Registriere dich bei https://www.elster.de/elsterweb/entwickler
→ Trage die HerstellerID in Excel (Zeile 24) oder via `--hersteller-id` ein

### "Bilanz nicht ausgeglichen"
→ Prüfe Excel: Summe Aktiva muss exakt Summe Passiva entsprechen
→ Nutze Sheet "Validierung" für automatische Checks

### "Stammkapital < 25.000 EUR"
→ Dieses Tool ist nur für GmbH (nicht UG)
→ GmbH benötigt mindestens 25.000 € Stammkapital

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
