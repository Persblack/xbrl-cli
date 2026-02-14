# Excel Template Guide

## Stammdaten Sheet - Updated Fields

Die Excel-Vorlage (`templates/ebilanz-template.xlsx`) wurde aktualisiert mit zwei neuen Pflichtfeldern für ELSTER-Übermittlung:

### Neue Felder (Zeilen 24-25)

| Zeile | Feld | Pflicht? | Beschreibung |
|-------|------|----------|--------------|
| **24** | **HerstellerID** | ✅ JA | ELSTER Hersteller-ID vom Entwicklerportal |
| **25** | **DatenLieferant** | ❌ Optional | Überschreibt Firmenname als Datenlieferant |

---

## HerstellerID (Zeile 24) - PFLICHT!

### Was eintragen?
Ihre ELSTER HerstellerID (z.B. `74458`)

### Wo bekomme ich die?
1. Registrierung: https://www.elster.de/elsterweb/entwickler
2. Antrag auf Entwickler-Zugang
3. ELSTER vergibt Ihnen eine HerstellerID

### Beispiel:
```
Zeile 24, Spalte B: 74458
```

### Was passiert wenn leer?
❌ Übermittlung schlägt fehl mit Fehler:
```
FEHLER: HerstellerID erforderlich für Versand
```

**Workaround:** CLI-Parameter `--hersteller-id 74458`

---

## DatenLieferant (Zeile 25) - Optional

### Was eintragen?
Name des Datenlieferanten (nur wenn abweichend vom Firmennamen)

### Wann ausfüllen?
- Steuerberater übermittelt für Mandant
- Dritte Person übermittelt in Ihrem Auftrag
- Sie möchten einen anderen Namen als Datenlieferant angeben

### Standard (wenn leer):
Automatisch = Ihr Firmenname aus Zeile 5

### Beispiel:
```
Zeile 25, Spalte B: Steuerberatung Müller GmbH
```

### Was passiert wenn leer?
✅ Kein Problem - verwendet automatisch Ihren Firmennamen

---

## Vollständige Stammdaten-Struktur

```
Zeile | Feld                      | Beispiel              | Pflicht
------|---------------------------|-----------------------|--------
  2   | Steuernummer             | 12/345/67890          | ✅
  3   | Steuernummer (13-stellig)| 9181012345678         | Auto
  4   | BuFa-Nummer              | 9181                  | ✅
  5   | Firmenname               | Meine GmbH            | ✅
  6   | Rechtsform               | GmbH                  | ✅
  7   | Straße                   | Hauptstr. 1           | ✅
  8   | PLZ                      | 80331                 | ✅
  9   | Ort                      | München               | ✅
 10   | Geschäftsführer          | Max Mustermann        | ✅
 11   | Handelsregister          | HRB 12345             | ✅
 12   | Registergericht          | München               | ✅
 13   | Wirtschaftsjahr von      | 01.01.2024            | ✅
 14   | Wirtschaftsjahr bis      | 31.12.2024            | ✅
 15   | Bilanzstichtag           | 31.12.2024            | ✅
 16   | WZ-Code                  | 62.01                 | ❌
 17   | Telefon                  | +49 89 123456         | ❌
 18   | Email                    | info@firma.de         | ❌
 19   | Übermittlungsanlass      | Erstmalig             | ✅
 20   | Bilanzaufstellung        | Handelsrecht          | ✅
 21   | Erstjahr                 | Ja                    | ✅
 22   | Ergebnisverwendung       | Thesaurierung         | ✅
 23   | Ausschüttungsbetrag      | 0                     | ✅
 24   | HerstellerID             | 74458                 | ✅ NEU!
 25   | DatenLieferant           |                       | ❌ NEU!
```

---

## Nutzung der Template

### 1. Validierung (ohne HerstellerID)
```bash
./build/ebilanz-submit --validate meine-bilanz.xlsx
```
✅ Funktioniert auch ohne HerstellerID

### 2. XBRL generieren
```bash
./build/ebilanz-submit --output /tmp --hersteller-id 74458 meine-bilanz.xlsx
```
- **Mit HerstellerID:** Erzeugt pure XBRL + ElsterBasis-wrapped XML
- **Ohne HerstellerID:** Nur pure XBRL (Warnung, aber kein Fehler)

### 3. Übermittlung
```bash
./build/ebilanz-submit --cert zertifikat.pfx --hersteller-id 74458 meine-bilanz.xlsx
```
❌ **Ohne HerstellerID:** Fehler, Abbruch

---

## Häufige Fragen

### Kann ich die HerstellerID via CLI übergeben statt in Excel?
✅ Ja: `--hersteller-id 74458`
- CLI-Parameter überschreibt Excel-Wert
- Nützlich für automatisierte Skripte

### Braucht jeder Nutzer eine eigene HerstellerID?
❌ Nein! Die HerstellerID ist **pro Software/Tool**, nicht pro Nutzer.
- Sie als Tool-Entwickler beantragen EINE HerstellerID
- Alle Nutzer Ihres Tools verwenden DIESELBE HerstellerID

### Was ist der Unterschied: HerstellerID vs. DatenLieferant?
- **HerstellerID:** Identifiziert die Software (= Tool-Entwickler)
- **DatenLieferant:** Identifiziert wer die Daten übermittelt (= Firma/Steuerberater)

### Kann ich mehrere Firmen mit derselben HerstellerID übermitteln?
✅ Ja! HerstellerID bleibt gleich, Firmendaten (Stammdaten) ändern sich.

---

## Migrationshinweis

**Alte Excel-Dateien (ohne Zeilen 24-25):**
- Parser liest leere Strings für HerstellerID/DatenLieferant
- Validierung funktioniert
- Übermittlung benötigt `--hersteller-id` CLI-Parameter

**Empfehlung:**
Aktualisieren Sie alte Excel-Dateien:
1. Zeile 24 manuell einfügen: "HerstellerID" | "74458" | "Hinweis"
2. Zeile 25 manuell einfügen: "DatenLieferant" | "" | "Hinweis"

Oder nutzen Sie die neue Template-Datei als Basis.

---

## Changelog

### Version 1.1.0 (2024-02-14)
- ✅ Zeile 24: HerstellerID hinzugefügt (PFLICHT für Übermittlung)
- ✅ Zeile 25: DatenLieferant hinzugefügt (Optional)

### Version 1.0.0 (Initial)
- Zeilen 1-23: Stammdaten, Bilanz, GuV, Validierung, Info
