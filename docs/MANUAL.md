# E-Bilanz Submitter — Benutzerhandbuch

> Elektronische Bilanzübermittlung ans Finanzamt per Kommandozeile.
> Version 1.1.0

---

## Inhaltsverzeichnis

1. [Übersicht](#übersicht)
2. [Installation](#installation)
3. [Schnellstart](#schnellstart)
4. [Excel-Vorlage](#excel-vorlage)
5. [CLI-Referenz](#cli-referenz)
6. [Modi & Pipelines](#modi--pipelines)
7. [Konfigurationsdateien](#konfigurationsdateien)
8. [ELSTER-Zertifikat](#elster-zertifikat)
9. [Ausgabedateien](#ausgabedateien)
10. [Validierungsregeln](#validierungsregeln)
11. [Exit-Codes](#exit-codes)
12. [Fehlerbehebung](#fehlerbehebung)
13. [Sicherheit](#sicherheit)
14. [Beispiele](#beispiele)

---

## Übersicht

`ebilanz-submit` ist ein CLI-Tool für die elektronische Übermittlung von GmbH-Bilanzen (E-Bilanz) an das deutsche Finanzamt über die ELSTER-Schnittstelle.

**Was es kann:**
- Bilanzdaten aus einer Excel-Vorlage (.xlsx) einlesen
- Daten validieren (12 Prüfregeln)
- XBRL-Datei nach HGB-Taxonomie 6.8 generieren
- Über das ERiC-SDK (ELSTER) ans Finanzamt senden

**Scope:**
- Nur GmbH (keine UG/AG)
- Bilanz + GuV (Gesamtkostenverfahren)
- Keine Kapitalflussrechnung, kein Anhang, kein Lagebericht
- Einheitsbilanz (Handelsbilanz = Steuerbilanz)

---

## Installation

### Voraussetzungen (macOS)

```bash
brew install xlnt pugixml cli11 spdlog nlohmann-json catch2
```

### Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

Die ausführbare Datei liegt danach unter `./build/ebilanz-submit`.

### ERiC-SDK (optional, für Versand)

Das ERiC-SDK ist proprietär und muss separat von ELSTER heruntergeladen werden:

1. SDK herunterladen: https://www.elster.de/elsterweb/entwickler
2. JAR-Datei entpacken (ist ein ZIP-Archiv)
3. Native Libraries nach `external/eric-sdk/lib/` kopieren
4. Header nach `external/eric-sdk/include/` kopieren

```bash
# macOS: Quarantine-Attribut entfernen
xattr -d com.apple.quarantine external/eric-sdk/lib/*.dylib

# Ad-hoc Code-Signatur
codesign -s - external/eric-sdk/lib/*.dylib
```

Ohne ERiC-SDK kann das Tool trotzdem XBRL-Dateien generieren und validieren — nur der Versand ans Finanzamt ist nicht möglich.

### Tests ausführen

```bash
cd build && ctest --output-on-failure
```

---

## Schnellstart

### 1. Excel-Vorlage ausfüllen

Kopieren Sie `templates/ebilanz-template.xlsx` und füllen Sie alle Sheets aus (Stammdaten, Bilanz, GuV).

### 2. Validieren

```bash
./ebilanz-submit --validate bilanz-2024.xlsx
```

### 3. XBRL generieren (ohne Versand)

```bash
./ebilanz-submit --output ./output bilanz-2024.xlsx
```

### 4. Test-Versand (Vorproduktion)

```bash
./ebilanz-submit --test --cert zertifikat.pfx bilanz-2024.xlsx
```

### 5. Echtversand

```bash
./ebilanz-submit --cert zertifikat.pfx bilanz-2024.xlsx
```

---

## Excel-Vorlage

Die Vorlage `templates/ebilanz-template.xlsx` enthält folgende Sheets:

### Sheet: Stammdaten

Unternehmensdaten in Spalte B:

| Zelle | Feld | Pflicht | Format / Werte |
|-------|------|---------|----------------|
| B2 | Steuernummer | ja | `XX/XXX/XXXXX` |
| B3 | Steuernummer bundeseinheitlich | ja | 13 Ziffern |
| B4 | Finanzamtsnummer (BuFa) | ja | 4 Ziffern |
| B5 | Firmenname | ja | Freitext |
| B6 | Rechtsform | ja | `GmbH` |
| B7 | Straße | ja | Freitext |
| B8 | PLZ | ja | 5 Ziffern |
| B9 | Ort | ja | Freitext |
| B10 | Geschäftsführer | ja | Freitext |
| B11 | Handelsregisternummer | ja | `HRB XXXXX` |
| B12 | Registergericht | ja | Freitext |
| B13 | Wirtschaftsjahr von | ja | `TT.MM.JJJJ` |
| B14 | Wirtschaftsjahr bis | ja | `TT.MM.JJJJ` |
| B15 | Bilanzstichtag | ja | `TT.MM.JJJJ` (= Wirtschaftsjahr-Ende) |
| B16 | WZ-Code | nein | z.B. `62.01.0` |
| B17 | Telefon | nein | Freitext |
| B18 | E-Mail | nein | Freitext |
| B19 | Übermittlungsanlass | ja | `erstmalig` oder `korrigiert` |
| B20 | Bilanzaufstellung | ja | `handelsrecht` |
| B21 | Erstjahr | ja | `Ja` / `Nein` |
| B22 | Ergebnisverwendung | ja | `thesaurierung`, `ausschuettung`, `teilausschuettung` |
| B23 | Ausschüttungsbetrag | bedingt | Zahl (> 0 wenn nicht Thesaurierung) |
| B24 | HerstellerID | für Versand | Eigene, bei ELSTER registrierte HerstellerID |
| B25 | DatenLieferant | nein | Standard: Firmenname |

**Datumsformate:** Im Excel deutsches Format `TT.MM.JJJJ` verwenden. Intern wird automatisch nach ISO `JJJJ-MM-TT` konvertiert.

### Sheet: Bilanz

Bilanzpositionen nach §266 HGB:

| Spalte | Inhalt |
|--------|--------|
| A | Positionsbezeichnung |
| B | Positions-ID |
| C | Aktuelles Jahr (EUR) |
| D | Vorjahr (EUR) |

Aktiva und Passiva inkl. Summenzeilen. Die Bilanz muss ausgeglichen sein (`Aktiva = Passiva`).

### Sheet: GuV

Gewinn- und Verlustrechnung nach Gesamtkostenverfahren (§275 Abs. 2 HGB):

| Spalte | Inhalt |
|--------|--------|
| A | Positionsbezeichnung |
| B | Positions-ID |
| C | Aktuelles Jahr (EUR) |
| D | Vorjahr (EUR) |

Der Jahresüberschuss in der GuV muss mit dem Wert in der Bilanz übereinstimmen.

### Erstjahr-Besonderheiten

Wenn `Erstjahr = Ja` gesetzt ist:
- Vorjahreswerte in Bilanz und GuV werden auf 0 gesetzt (auch wenn befüllt)
- Eine Warnung wird ausgegeben, falls Vorjahreswerte vorhanden sind
- Die XBRL-Datei enthält trotzdem Prior-Period-Referenzen (Taxonomie-Pflicht)

---

## CLI-Referenz

### Syntax

```
ebilanz-submit [OPTIONEN] <bilanz.xlsx>
```

### Positionales Argument

| Argument | Beschreibung |
|----------|-------------|
| `bilanz` | Pfad zur Excel-Datei mit Bilanzdaten (.xlsx). Muss existieren. |

### Modi

| Flag | Beschreibung |
|------|-------------|
| `-v, --validate` | Nur validieren, nicht senden. |
| `-t, --test` | Test-Modus (Vorproduktion). Daten werden nicht ans echte Finanzamt gesendet. |

Modi sind kombinierbar: `--validate --test` validiert und sendet dann an die Vorproduktion.

### Zertifikat & Passwort

| Flag | Beschreibung |
|------|-------------|
| `-c, --cert <Datei>` | ELSTER-Zertifikat (.pfx). Pflicht bei Versand. |
| `-p, --password <Text>` | Zertifikat-Passwort. Wird interaktiv abgefragt wenn nicht angegeben. |

### Ausgabe

| Flag | Beschreibung |
|------|-------------|
| `-o, --output <Verzeichnis>` | Verzeichnis zum Speichern der XBRL-Dateien. Wird automatisch erstellt. |
| `-x, --xbrl <Datei>` | XBRL-Datei direkt senden (überspringt Excel-Parsing und XBRL-Generierung). |

### ELSTER-Optionen

| Flag | Beschreibung |
|------|-------------|
| `--hersteller-id <ID>` | ELSTER HerstellerID (Pflicht für Übermittlung; alternativ Excel-Feld B24). |

Priorität der HerstellerID: CLI-Flag > Excel-Feld B24.

### Logging

| Flag | Beschreibung |
|------|-------------|
| `--verbose` | Debug-Level Ausgabe. |
| `--quiet` | Nur Fehler ausgeben. |
| `--log <Datei>` | Log-Datei. Standard: `./ebilanz.log`. |

### Konfiguration

| Flag | Standard | Beschreibung |
|------|----------|-------------|
| `--taxonomie <Datei>` | `config/taxonomie-6.8.json` | XBRL-Taxonomie-Mapping. |
| `--bufa <Datei>` | `config/bufa-verzeichnis.json` | Bundesfinanzamt-Verzeichnis. |

### Sonstige

| Flag | Beschreibung |
|------|-------------|
| `-h, --help` | Hilfe anzeigen. |
| `--version` | Version anzeigen. |

---

## Modi & Pipelines

Je nach Kombination der Flags durchläuft das Tool unterschiedliche Pipelines.

### Nur Validierung (`--validate`)

```
Excel einlesen → Validieren → Ergebnis ausgeben
```

```bash
./ebilanz-submit --validate bilanz-2024.xlsx
```

Ausgabe bei Erfolg:
```
E-Bilanz Submitter v1.1.0
=================================================
[1/2] Excel-Datei einlesen...  OK
[2/2] Validierung...
  Bilanz ausgeglichen (100.000,00 EUR)  OK
  Jahresüberschuss konsistent  OK
  Alle Pflichtfelder vorhanden  OK
  Steuernummer konvertiert (5123045067890)  OK
  Formale Prüfung  OK

Validierung erfolgreich! Keine Fehler gefunden.
```

### XBRL generieren (`--output` ohne `--cert`)

```
Excel einlesen → Validieren → XBRL generieren → Dateien speichern
```

```bash
./ebilanz-submit --output ./output bilanz-2024.xlsx
```

Erzeugt zwei Dateien:
- `output/ebilanz-2024.xbrl` — Reines XBRL-Instanzdokument
- `output/ebilanz-2024-elster.xml` — ElsterBasis-Wrapper (bereit für ERiC)

### Test-Versand (`--test --cert`)

```
Excel einlesen → Validieren → XBRL generieren → ERiC-Validierung → Vorproduktion senden
```

```bash
./ebilanz-submit --test --cert zertifikat.pfx bilanz-2024.xlsx
```

Sendet an den ELSTER-Testserver. Die Daten werden nicht an das echte Finanzamt übermittelt. Im Test-Modus wird ein `<Testmerker>` in die XML-Datei geschrieben.

### Echtversand (`--cert` ohne `--test`)

```
Excel einlesen → Validieren → XBRL generieren → ERiC-Validierung → Produktiv senden
```

```bash
./ebilanz-submit --cert zertifikat.pfx bilanz-2024.xlsx
```

Ausgabe bei Erfolg:
```
E-Bilanz Submitter v1.1.0
=================================================
[1/5] Excel-Datei einlesen...  OK
[2/5] Validierung...  OK
[3/5] XBRL-Datei generieren...  OK
[4/5] ERiC-Validierung...  OK
[5/5] An Finanzamt senden...  OK

Transfer-Ticket: 0000000000000000000000000000abcd
Status: Erfolgreich übermittelt
Datum: 2026-02-28T15:30:45Z
```

### Direkt-XBRL-Modus (`--xbrl`)

```
XBRL laden → ERiC-Validierung → Senden
```

```bash
./ebilanz-submit --xbrl ebilanz-2024.xbrl --cert zertifikat.pfx
```

Überspringt Excel-Parsing und XBRL-Generierung. Nützlich zum erneuten Senden einer bereits generierten XBRL-Datei.

---

## Konfigurationsdateien

### taxonomie-6.8.json

Enthält das Mapping von Bilanz-/GuV-Positionen auf XBRL-Tags der HGB-Taxonomie.

```json
{
  "version": "6.8",
  "namespace": "http://www.xbrl.de/taxonomies/de-gaap-ci-2024-...",
  "schemaRef": "http://...",
  "mapping": {
    "immaterielleVG_konzessionen": "de-gaap-ci:bs.ass.fixAss.intangAss.concessionsPIR",
    "sachanlagen_bga": "de-gaap-ci:bs.ass.fixAss.tan.otherEq"
  },
  "auffangpositionen": {
    "immaterielleVG": "de-gaap-ci:bs.ass.fixAss.intangAss.other"
  },
  "mussfelder": [
    "de-gaap-ci:bs.ass",
    "de-gaap-ci:bs.eqLiab"
  ]
}
```

**Auffangpositionen:** Wenn alle Detailpositionen einer Gruppe = 0 sind, aber die Gruppensumme > 0, wird der Wert automatisch in die Auffangposition geschrieben (Pflicht der HGB-Taxonomie).

**Taxonomie aktualisieren:** Bei jährlichen Updates wird nur die JSON-Datei ausgetauscht. Im Code ändert sich nichts.

### bufa-verzeichnis.json

Enthält die Zuordnung von Finanzamtsnummern zu Bundesländern und die Regeln zur Steuernummer-Konvertierung.

```json
{
  "bundeslaender": {
    "bayern": {
      "bufaPrefix": ["91", "92"],
      "bundeslandCode": "BY",
      "format": "9FFF0BBBUUUUUP"
    }
  }
}
```

Die Steuernummer wird automatisch vom vereinfachten Format (`XX/XXX/XXXXX`) in das bundeseinheitliche 13-stellige Format konvertiert. Die Konvertierungsregeln sind bundeslandspezifisch.

---

## ELSTER-Zertifikat

### Beschaffung

Ein ELSTER-Zertifikat (.pfx) erhalten Sie über https://www.elster.de. Für die E-Bilanz-Übermittlung benötigen Sie ein **Organisationszertifikat** (nicht das persönliche Zertifikat).

### Verwendung

```bash
# Passwort interaktiv eingeben (empfohlen)
./ebilanz-submit --cert zertifikat.pfx bilanz-2024.xlsx

# Passwort als Parameter (nur für Automatisierung)
./ebilanz-submit --cert zertifikat.pfx --password "geheim" bilanz-2024.xlsx
```

Die interaktive Passwort-Eingabe unterdrückt das Terminal-Echo — das Passwort wird nicht auf dem Bildschirm angezeigt.

### Sicherheitshinweise

- Das Passwort wird **niemals** in Log-Dateien geschrieben
- Das Passwort wird nach Verwendung aus dem Speicher gelöscht (`secureZero`)
- Bei Skript-Automatisierung: Passwort per Umgebungsvariable übergeben, nicht im Klartext in Skripten

---

## Ausgabedateien

### XBRL-Instanzdokument (`ebilanz-JJJJ.xbrl`)

Reines XBRL-Instanzdokument nach HGB-Taxonomie. UTF-8 kodiert. Enthält:
- Contexts: `current` (Wirtschaftsjahr), `current_instant` (Bilanzstichtag)
- Entity-Identifier: Bundeseinheitliche 13-stellige Steuernummer
- Bilanz- und GuV-Positionen als XBRL-Facts
- Mussfelder der Taxonomie

### ElsterBasis-XML (`ebilanz-JJJJ-elster.xml`)

XBRL-Dokument eingebettet in den ElsterBasis-Umschlag. Struktur:

```xml
<Elster xmlns="http://www.elster.de/elsterxml/schema/v11">
  <TransferHeader version="11">
    <Verfahren>ElsterBilanz</Verfahren>
    <DatenArt>Bilanz</DatenArt>
    <Vorgang>send-Auth</Vorgang>
    <HerstellerID>00000</HerstellerID>
    ...
  </TransferHeader>
  <DatenTeil>
    <Nutzdatenblock>
      <Nutzdaten>
        <!-- XBRL-Instanzdokument -->
      </Nutzdaten>
    </Nutzdatenblock>
  </DatenTeil>
</Elster>
```

Im Test-Modus enthält der TransferHeader zusätzlich `<Testmerker>700000004</Testmerker>`.

### Transfer-History (`~/.ebilanz/history.json`)

Protokolliert alle Übermittlungen. Enthält **keine** Finanzdaten — nur Metadaten:

```json
{
  "transfers": [
    {
      "timestamp": "2026-02-28T15:30:45Z",
      "ticket": "0000000000000000000000000000abcd",
      "steuernummer": "12/345/67890",
      "wirtschaftsjahr": "2024",
      "testMode": true,
      "xbrlFile": "./output/ebilanz-2024.xbrl"
    }
  ]
}
```

### Log-Datei (`ebilanz.log`)

Detailliertes Protokoll der letzten Ausführung. Format:

```
[2026-02-28 15:30:45.123] [info] E-Bilanz Submitter v1.1.0 gestartet
[2026-02-28 15:30:45.124] [info] Stammdaten gelesen: Meine GmbH
[2026-02-28 15:30:45.156] [info] XBRL gespeichert: /tmp/ebilanz-2024.xbrl
```

Standard-Pfad: `./ebilanz.log`. Ändern mit `--log <Pfad>`.

---

## Validierungsregeln

Alle Regeln werden in Schritt 2 der Pipeline geprüft. Fehler (ERROR) brechen den Vorgang ab. Warnungen (WARNING) werden angezeigt, blockieren aber nicht.

| # | Regel | Typ | Bedingung |
|---|-------|-----|-----------|
| 1 | Bilanz ausgeglichen | ERROR | \|Aktiva − Passiva\| < 0,01 EUR |
| 2 | Jahresüberschuss konsistent | ERROR | \|Bilanz-JÜ − GuV-JÜ\| < 0,01 EUR |
| 3 | GmbH-Stammkapital | ERROR | Gezeichnetes Kapital ≥ 25.000 EUR |
| 4 | Steuernummer-Format | ERROR | Regex `\d{2}/\d{3}/\d{5}` |
| 5 | BuFa-Nummer | ERROR | Regex `\d{4}` |
| 6 | Bundeseinheitl. Steuernummer | ERROR | 13 Ziffern |
| 7 | Handelsregisternummer | ERROR | Regex `HRB\s*\d+` |
| 8 | Pflichtfelder | ERROR | Alle Pflichtfelder nicht leer |
| 9 | Erstjahr-Konsistenz | WARNING | Vorjahreswerte = 0 wenn Erstjahr |
| 10 | Ergebnisverwendung | ERROR | Ausschüttungsbetrag > 0 wenn nicht Thesaurierung |
| 11 | Wirtschaftsjahr-Dauer | WARNING | ≤ 18 Monate |
| 12 | Bilanzstichtag | ERROR | Muss = Wirtschaftsjahr-Ende |

**Hinweis:** Regel 3 ist bewusst als ERROR implementiert. Das Tool unterstützt nur GmbH (Stammkapital ≥ 25.000 EUR), keine UG.

---

## Exit-Codes

| Code | Bezeichnung | Beschreibung |
|------|-------------|-------------|
| 0 | SUCCESS | Alle Operationen erfolgreich abgeschlossen |
| 1 | EXCEL_FILE_NOT_FOUND | Keine Excel-Datei angegeben oder Datei nicht gefunden |
| 2 | EXCEL_PARSE_ERROR | Excel-Datei konnte nicht gelesen werden (fehlende Sheets, falsches Format) |
| 3 | VALIDATION_FAILED | Mindestens eine Validierungsregel fehlgeschlagen |
| 4 | XBRL_GENERATION_FAILED | XBRL-Generierung fehlgeschlagen |
| 5 | XBRL_FILE_NOT_FOUND | Per `--xbrl` angegebene Datei nicht gefunden |
| 6 | CONFIG_LOAD_FAILED | Taxonomie- oder BuFa-Config fehlt oder ungültig |
| 10 | ERIC_INIT_FAILED | ERiC-SDK Initialisierung fehlgeschlagen |
| 11 | ERIC_CERT_LOAD_FAILED | Zertifikat konnte nicht geladen werden |
| 12 | ERIC_VALIDATION_FAILED | ERiC-Validierung fehlgeschlagen |
| 13 | ERIC_SUBMIT_FAILED | Übermittlung vom Finanzamt abgelehnt |
| 14 | ERIC_TIMEOUT | Netzwerk-Timeout bei ELSTER-Kommunikation |
| 20 | STEUERNUMMER_CONVERT_FAILED | Steuernummer-Konvertierung fehlgeschlagen |
| 99 | UNKNOWN_ERROR | Unbehandelter Fehler (Details in Log-Datei) |

Verwendung in Skripten:

```bash
./ebilanz-submit --validate bilanz.xlsx
if [ $? -eq 0 ]; then
    echo "Validierung OK"
elif [ $? -eq 3 ]; then
    echo "Validierungsfehler"
fi
```

---

## Fehlerbehebung

### „FEHLER: Excel-Datei konnte nicht gelesen werden"

**Exit-Code:** 2

- Prüfen, ob die Datei im `.xlsx`-Format vorliegt (nicht `.xls`)
- Prüfen, ob die Sheets `Stammdaten`, `Bilanz` und `GuV` vorhanden sind
- Prüfen, ob die Datei nicht beschädigt oder durch ein anderes Programm gesperrt ist

### „FEHLER: Config laden fehlgeschlagen"

**Exit-Code:** 6

- Prüfen, ob `config/taxonomie-6.8.json` und `config/bufa-verzeichnis.json` existieren
- Das Tool sucht relativ zum aktuellen Arbeitsverzeichnis
- Alternativ explizit angeben: `--taxonomie /pfad/zu/taxonomie.json`

### „FEHLER: Validierung fehlgeschlagen"

**Exit-Code:** 3

- Die Fehlermeldungen zeigen genau welche Regel(n) verletzt sind
- Häufige Ursachen:
  - Bilanz nicht ausgeglichen (Aktiva ≠ Passiva)
  - Steuernummer im falschen Format (richtig: `12/345/67890`)
  - Handelsregisternummer ohne „HRB" Präfix
  - Ausschüttungsbetrag fehlt bei Ergebnisverwendung „ausschuettung"

### „ERiC-SDK nicht verfügbar"

- Das ERiC-SDK ist nicht installiert oder nicht auffindbar
- Validierung und XBRL-Generierung funktionieren trotzdem
- Für den Versand: SDK installieren (siehe [Installation](#installation))

### „FEHLER: ELSTER-Zertifikat konnte nicht geladen werden"

**Exit-Code:** 11

- Pfad zum Zertifikat prüfen
- Passwort prüfen (Groß-/Kleinschreibung beachten)
- Zertifikat muss im .pfx-Format vorliegen
- Zertifikat darf nicht abgelaufen sein

### ERiC-Validierung schlägt fehl, aber Versand wird trotzdem versucht

Das ist gewolltes Verhalten. Die lokale ERiC-Validierung ist strenger als die Serverseite. Bei lokalen Validierungsfehlern wird trotzdem ein Versand versucht, da der Server die Daten möglicherweise akzeptiert. Die Warnungen werden in der Konsole angezeigt.

### macOS: „Library not loaded" bei ERiC-SDK

```bash
# Quarantine-Attribut entfernen
xattr -d com.apple.quarantine external/eric-sdk/lib/*.dylib

# Ad-hoc signieren
codesign -s - external/eric-sdk/lib/*.dylib
```

### Plugin-Fehler (Error 610001083)

Das ERiC-SDK benötigt ein `plugins/`-Verzeichnis. Sicherstellen, dass es im `lib/`-Verzeichnis neben den `.dylib`-Dateien liegt.

---

## Sicherheit

### Passwort-Schutz

- Zertifikat-Passwörter werden **niemals** geloggt (weder Konsole noch Datei)
- Passwörter werden nach Verwendung sicher aus dem Speicher gelöscht (`secureZero` mit `volatile`)
- Interaktive Passwort-Eingabe ohne Terminal-Echo

### Datenschutz

- **Keine Telemetrie**, keine Cloud-Anbindung, keine Analytics
- XBRL-Dateien werden nur lokal gespeichert (außer beim ELSTER-Versand)
- Transfer-History enthält **keine** Bilanzzahlen — nur Ticket + Zeitstempel
- Alle Daten verbleiben auf dem lokalen Rechner

### Empfehlungen

- Zertifikat-Datei mit restriktiven Berechtigungen schützen (`chmod 600`)
- Generierte XBRL-Dateien nach dem Versand sicher aufbewahren
- Passwort nicht in Shell-History hinterlassen (interaktive Eingabe bevorzugen)
- Log-Dateien prüfen, bevor sie geteilt werden (enthalten keine Passwörter, aber Firmennamen/Steuernummern)

---

## Beispiele

### Nur validieren

```bash
./ebilanz-submit --validate bilanz-2024.xlsx
```

Prüft die Excel-Datei auf Fehler, ohne XBRL zu generieren oder zu senden.

### XBRL generieren und lokal speichern

```bash
./ebilanz-submit --output ./output bilanz-2024.xlsx
```

Generiert `output/ebilanz-2024.xbrl` und `output/ebilanz-2024-elster.xml`. Kein Versand.

### Test-Versand an Vorproduktion

```bash
./ebilanz-submit --test --cert zertifikat.pfx bilanz-2024.xlsx
```

Sendet an den ELSTER-Testserver. Kein Effekt auf echte Steuerdaten.

### Echtversand mit interaktivem Passwort

```bash
./ebilanz-submit --cert zertifikat.pfx bilanz-2024.xlsx
# Passwort wird interaktiv abgefragt
```

### Echtversand mit Passwort-Parameter

```bash
./ebilanz-submit --cert zertifikat.pfx --password "meinPasswort" bilanz-2024.xlsx
```

**Achtung:** Passwort erscheint in der Shell-History. Nur für automatisierte Pipelines verwenden.

### XBRL generieren UND gleichzeitig senden

```bash
./ebilanz-submit --output ./output --cert zertifikat.pfx bilanz-2024.xlsx
```

Speichert die XBRL-Dateien lokal und sendet gleichzeitig ans Finanzamt.

### Bereits generierte XBRL erneut senden

```bash
./ebilanz-submit --xbrl ./output/ebilanz-2024.xbrl --cert zertifikat.pfx
```

Überspringt Excel-Parsing und XBRL-Generierung. Nützlich zum erneuten Senden nach einem Netzwerkfehler.

### Verbose-Modus mit eigener Log-Datei

```bash
./ebilanz-submit --verbose --log debug.log --validate bilanz-2024.xlsx
```

Zeigt Debug-Informationen und schreibt das ausführliche Log in `debug.log`.

### Stille Ausführung (für Skripte)

```bash
./ebilanz-submit --quiet --cert zertifikat.pfx bilanz-2024.xlsx
echo "Exit-Code: $?"
```

Gibt nur Fehler aus. Ergebnis über Exit-Code prüfen.

### Eigene HerstellerID verwenden

```bash
./ebilanz-submit --hersteller-id 99999 --test --cert zertifikat.pfx bilanz-2024.xlsx
```

### Alternative Konfigurationsdateien

```bash
./ebilanz-submit \
    --taxonomie /pfad/zu/taxonomie-7.0.json \
    --bufa /pfad/zu/bufa-verzeichnis.json \
    --validate bilanz-2024.xlsx
```

### Typischer Workflow

```bash
# 1. Erst validieren
./ebilanz-submit --validate bilanz-2024.xlsx

# 2. XBRL prüfen
./ebilanz-submit --output ./output bilanz-2024.xlsx

# 3. Test-Versand
./ebilanz-submit --test --output ./output --cert zertifikat.pfx bilanz-2024.xlsx

# 4. Echtversand
./ebilanz-submit --output ./output --cert zertifikat.pfx bilanz-2024.xlsx
```
