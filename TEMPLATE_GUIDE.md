# Excel Template Guide

## Stammdaten Sheet

Die Excel-Vorlage (`templates/ebilanz-template.xlsx`) enthält neben den Bilanzdaten zwei
Felder für die ELSTER-Übermittlung:

### Felder (Zeilen 24-25)

| Zeile | Feld | Pflicht? | Beschreibung |
|-------|------|----------|--------------|
| **24** | **HerstellerID** | ✅ für Übermittlung | Eigene, bei ELSTER registrierte HerstellerID |
| **25** | **DatenLieferant** | ❌ Optional | Überschreibt Firmenname als Datenlieferant |

---

## HerstellerID (Zeile 24) – Pflicht für die Übermittlung

Die HerstellerID identifiziert die übermittelnde Software gegenüber ELSTER. Es ist **keine
HerstellerID eingebaut** — jeder Hersteller muss eine eigene, bei ELSTER registrierte
HerstellerID verwenden.

HerstellerID beantragen: https://www.elster.de/elsterweb/entwickler

### Was passiert wenn leer?
Validierung und reine XBRL-Generierung funktionieren auch ohne HerstellerID. Die
**Übermittlung** (Test wie Produktiv) bricht ohne HerstellerID mit einem Fehler ab.

### Priorität
1. CLI-Flag `--hersteller-id` (höchste)
2. Excel-Feld (Zeile 24)

---

## DatenLieferant (Zeile 25) – Optional

### Was eintragen?
Name des Datenlieferanten (nur wenn abweichend vom Firmennamen).

### Wann ausfüllen?
- Steuerberater übermittelt für Mandant
- Dritte Person übermittelt in Ihrem Auftrag

### Standard (wenn leer)
Automatisch = Ihr Firmenname aus Zeile 5.

### Beispiel
```
Zeile 25, Spalte B: Steuerberatung Muster GmbH
```

---

## Vollständige Stammdaten-Struktur

Beispielwerte sind fiktiv — durch eigene Daten ersetzen.

```
Zeile | Feld                      | Beispiel              | Pflicht
------|---------------------------|-----------------------|--------
  2   | Steuernummer             | 12/345/67890          | ✅
  3   | Steuernummer (13-stellig)|                       | Auto
  4   | BuFa-Nummer              | 2893                  | ✅
  5   | Firmenname               | Muster GmbH           | ✅
  6   | Rechtsform               | GmbH                  | ✅
  7   | Straße                   | Musterstraße 123      | ✅
  8   | PLZ                      | 12345                 | ✅
  9   | Ort                      | Musterstadt           | ✅
 10   | Geschäftsführer          | Max Mustermann        | ✅
 11   | Handelsregister          | HRB 12345             | ✅
 12   | Registergericht          | Amtsgericht Musterstadt| ✅
 13   | Wirtschaftsjahr von      | 01.01.2024            | ✅
 14   | Wirtschaftsjahr bis      | 31.12.2024            | ✅
 15   | Bilanzstichtag           | 31.12.2024            | ✅
 16   | WZ-Code                  | 62.01                 | ❌
 17   | Telefon                  | +49 123 456789        | ❌
 18   | Email                    | info@beispiel.de      | ❌
 19   | Übermittlungsanlass      | Erstmalig             | ✅
 20   | Bilanzaufstellung        | Handelsrecht          | ✅
 21   | Erstjahr                 | Ja                    | ✅
 22   | Ergebnisverwendung       | Thesaurierung         | ✅
 23   | Ausschüttungsbetrag      | 0                     | ✅
 24   | HerstellerID             | (eigene ID)           | ✅ für Übermittlung
 25   | DatenLieferant           |                       | ❌
```

---

## Nutzung der Template

### 1. Validierung (ohne HerstellerID möglich)
```bash
./build/ebilanz-submit --validate meine-bilanz.xlsx
```

### 2. XBRL generieren (ohne HerstellerID möglich)
```bash
./build/ebilanz-submit --output /tmp meine-bilanz.xlsx
```
Erzeugt pure XBRL + ElsterBasis-wrapped XML.

### 3. Übermittlung (HerstellerID erforderlich)
```bash
./build/ebilanz-submit --cert zertifikat.pfx --hersteller-id XXXXX meine-bilanz.xlsx
```
Alternativ HerstellerID in Excel-Zeile 24 eintragen.

---

## Häufige Fragen

### Kann ich die HerstellerID via CLI übergeben statt in Excel?
✅ Ja: `--hersteller-id XXXXX`. Das CLI-Flag überschreibt den Excel-Wert.

### Braucht jeder eine eigene HerstellerID?
Die HerstellerID ist **pro Software/Hersteller**. Wer dieses Tool unter eigenem Namen
betreibt und Daten übermittelt, beantragt dafür eine eigene HerstellerID bei ELSTER und
trägt die Verantwortung für die unter dieser ID übermittelten Daten.

### Was ist der Unterschied: HerstellerID vs. DatenLieferant?
- **HerstellerID:** Identifiziert die Software (= Hersteller)
- **DatenLieferant:** Identifiziert, wer die Daten übermittelt (= Firma/Steuerberater)

### Kann ich mehrere Firmen mit derselben HerstellerID übermitteln?
✅ Ja. HerstellerID bleibt gleich, Firmendaten (Stammdaten) ändern sich.

---

## Migrationshinweis

**Alte Excel-Dateien (ohne Zeilen 24-25):**
- Parser liest leere Strings für HerstellerID/DatenLieferant
- Validierung und XBRL-Generierung funktionieren
- Für die Übermittlung muss die HerstellerID per `--hersteller-id` oder in Zeile 24 gesetzt werden
