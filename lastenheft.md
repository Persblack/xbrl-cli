# Lastenheft: E-Bilanz CLI-Tool mit Excel-Input

## 1. Projektübersicht

**Projektname:** E-Bilanz Excel Submitter  
**Version:** 1.1  
**Datum:** Februar 2026  
**Auftraggeber:** Solo-GmbH-Unternehmer

### 1.1 Ausgangssituation
GmbHs in Deutschland müssen ihre Bilanz elektronisch im XBRL-Format über die ERiC-Schnittstelle ans Finanzamt übermitteln (E-Bilanz-Pflicht). Professionelle Software ist teuer und überdimensioniert für kleine GmbHs mit einfachen Bilanzen.

### 1.2 Zielsetzung
Entwicklung eines **C++ Command-Line-Tools**, das:
1. Ein vorgefertigtes Excel-Template als Input nimmt
2. Die Bilanzdaten einliest und validiert
3. Eine valide XBRL-Datei nach HGB-Taxonomie generiert
4. Diese über ERiC-SDK ans Finanzamt übermittelt

### 1.3 Zielgruppe
- Solo-Unternehmer mit kleiner GmbH
- Nutzer mit fertigen Bilanzzahlen
- Technisch versierte Personen ohne Steuerberater-Software

### 1.4 Scope

**In Scope:**
- **Bilanzierungsgröße:** Kleine GmbH (§266 Abs. 1 HGB)
- **Rechtsform:** Nur GmbH (keine UG, AG, KG, OHG)
- **Umfang:** Bilanz + GuV (Gesamtkostenverfahren)
- **Deployment:** Lokales Tool (keine Cloud, kein Multi-User)
- **Plattform:** macOS (primär), Windows (sekundär)
- **Taxonomie:** HGB-Taxonomie 6.8 (Wirtschaftsjahr 2024), konfigurierbar für zukünftige Versionen
- **Übermittlungsanlass:** Erstmalige Übermittlung und korrigierte Übermittlung

**Explizit Out of Scope:**
- Umsatzkostenverfahren (nur Gesamtkostenverfahren)
- Kapitalflussrechnung
- Eigenkapitalspiegel
- Anlagespiegel
- Anhang (§284 HGB)
- Lagebericht (§289 HGB)
- Ergebnisverwendungsrechnung (wird als vereinfachte Angabe im Stammdaten-Sheet erfasst)
- Konzernbilanz / Konsolidierung
- Sonderbilanzen, Eröffnungsbilanzen
- UG (haftungsbeschränkt), AG oder andere Rechtsformen
- Steuerliche Überleitungsrechnung
- Multi-User / Mandantenfähigkeit

---

## 2. Funktionale Anforderungen

### 2.1 Excel-Template

**Dateiname:** `ebilanz-template.xlsx`

**Struktur:** 5 Sheets

#### Sheet 1: Stammdaten

```
| Feld                          | Zelle | Beispielwert              | Pflicht | Hinweis                           |
|-------------------------------|-------|---------------------------|---------|-----------------------------------|
| Steuernummer (vereinfacht)    | B2    | 12/345/67890              | Ja      | Landesformat XX/XXX/XXXXX         |
| Steuernummer (bundeseinheitl.)| B3    | 5123045067890             | Auto    | 13-stellig, wird aus B2+B4 berechnet |
| Finanzamts-Nummer (BuFa-Nr.) | B4    | 2893                      | Ja      | 4-stellig, ELSTER-Verzeichnis     |
| Firmenname                    | B5    | Muster GmbH               | Ja      |                                   |
| Rechtsform                    | B6    | GmbH (fest)               | Ja      | Dropdown, nur "GmbH"             |
| Straße + Hausnummer           | B7    | Musterstraße 123          | Ja      |                                   |
| PLZ                           | B8    | 12345                     | Ja      | 5 Stellen                        |
| Ort                           | B9    | Musterstadt               | Ja      |                                   |
| Geschäftsführer               | B10   | Max Mustermann            | Ja      |                                   |
| Handelsregister-Nummer        | B11   | HRB 12345                 | Ja      | Muster: HRB XXXXX                |
| Registergericht               | B12   | Amtsgericht Musterstadt   | Ja      |                                   |
| Wirtschaftsjahr von           | B13   | 01.01.2024                | Ja      | Datumsformat TT.MM.JJJJ          |
| Wirtschaftsjahr bis           | B14   | 31.12.2024                | Ja      | Datumsformat TT.MM.JJJJ          |
| Bilanzstichtag                | B15   | 31.12.2024                | Ja      | Muss = Wirtschaftsjahr bis        |
| Wirtschaftszweig (WZ-Code)    | B16   | 62.01                     | Nein    |                                   |
| Telefon                       | B17   | +49 123 456789            | Nein    |                                   |
| E-Mail                        | B18   | info@beispiel.de          | Nein    |                                   |
| Übermittlungsanlass           | B19   | Erstmalig                 | Ja      | Dropdown: Erstmalig / Korrigiert  |
| Art der Bilanzaufstellung     | B20   | Handelsrecht              | Ja      | Dropdown: Handelsrecht            |
| Erstjahr (ja/nein)            | B21   | Ja                        | Ja      | Steuert Vorjahres-Handling        |
| Ergebnisverwendung            | B22   | Thesaurierung             | Ja      | Dropdown: Thesaurierung / Ausschüttung / Teilausschüttung |
| Ausschüttungsbetrag           | B23   | 0,00                      | Bedingt | Pflicht wenn B22 ≠ Thesaurierung  |
```

**Steuernummer-Konvertierung:**
Die bundeseinheitliche 13-stellige Steuernummer (Feld B3) wird automatisch aus der vereinfachten Steuernummer (B2) und der BuFa-Nr. (B4) berechnet. Das Tool führt die Konvertierung gemäß ELSTER-Schema durch. Nutzer können alternativ direkt die bundeseinheitliche Nummer eingeben.

**Datenvalidierung in Excel:**
- B2: Textlänge = 13 Zeichen (Format: XX/XXX/XXXXX)
- B4: 4-stellige Zahl
- B6: Dropdown mit "GmbH" (fest)
- B8: Zahlen, 5 Stellen
- B11: Text beginnend mit "HRB"
- B13-B15: Datumsformat
- B19: Dropdown
- B20: Dropdown

---

#### Sheet 2: Bilanz

**Struktur nach §266 HGB (kleine GmbH):**

```
| Zeile | Bilanzposition                                              | Betrag (€) | Vorjahr (€) | Zelle |
|-------|-------------------------------------------------------------|------------|-------------|-------|
| 3     | AKTIVA                                                      |            |             |       |
| 4     | A. ANLAGEVERMÖGEN                                           |            |             |       |
| 5     | I. Immaterielle Vermögensgegenstände                        |            |             |       |
| 6     | 1. Selbst geschaffene gewerbliche Schutzrechte              | 0,00       | 0,00        | C6    |
| 7     | 2. Entgeltlich erworbene Konzessionen, Lizenzen             | 0,00       | 0,00        | C7    |
| 8     | 3. Geschäfts- oder Firmenwert                               | 0,00       | 0,00        | C8    |
| 9     | 4. Geleistete Anzahlungen                                   | 0,00       | 0,00        | C9    |
| 10    | 5. Andere immaterielle Vermögensgegenstände                 | 0,00       | 0,00        | C10   |
| 11    | Summe I.                                                    | =SUM(C6:C10)|=SUM(D6:D10)| C11   |
| 12    |                                                             |            |             |       |
| 13    | II. Sachanlagen                                             |            |             |       |
| 14    | 1. Grundstücke, Bauten                                      | 0,00       | 0,00        | C14   |
| 15    | 2. Technische Anlagen und Maschinen                         | 0,00       | 0,00        | C15   |
| 16    | 3. Andere Anlagen, Betriebs- und Geschäftsausstattung       | 1.000,00   | 0,00        | C16   |
| 17    | 4. Geleistete Anzahlungen und Anlagen im Bau                | 0,00       | 0,00        | C17   |
| 18    | 5. Andere Sachanlagen                                       | 0,00       | 0,00        | C18   |
| 19    | Summe II.                                                   | =SUM(C14:C18)|=SUM(D14:D18)| C19 |
| 20    |                                                             |            |             |       |
| 21    | III. Finanzanlagen                                          |            |             |       |
| 22    | 1. Anteile an verbundenen Unternehmen                       | 10.000,00  | 0,00        | C22   |
| 23    | 2. Ausleihungen an verbundene Unternehmen                   | 0,00       | 0,00        | C23   |
| 24    | 3. Beteiligungen                                            | 0,00       | 0,00        | C24   |
| 25    | 4. Ausleihungen an Unternehmen                              | 0,00       | 0,00        | C25   |
| 26    | 5. Wertpapiere des Anlagevermögens                          | 0,00       | 0,00        | C26   |
| 27    | 6. Sonstige Ausleihungen                                    | 0,00       | 0,00        | C27   |
| 28    | 7. Andere Finanzanlagen                                     | 0,00       | 0,00        | C28   |
| 29    | Summe III.                                                  | =SUM(C22:C28)|=SUM(D22:D28)| C29 |
| 30    |                                                             |            |             |       |
| 31    | SUMME ANLAGEVERMÖGEN (A)                                    | =C11+C19+C29|=D11+D19+D29| C31   |
| 32    |                                                             |            |             |       |
| 33    | B. UMLAUFVERMÖGEN                                           |            |             |       |
| 34    | I. Vorräte                                                  |            |             |       |
| 35    | 1. Roh-, Hilfs- und Betriebsstoffe                          | 0,00       | 0,00        | C35   |
| 36    | 2. Unfertige Erzeugnisse, unfertige Leistungen              | 0,00       | 0,00        | C36   |
| 37    | 3. Fertige Erzeugnisse und Waren                            | 0,00       | 0,00        | C37   |
| 38    | 4. Geleistete Anzahlungen                                   | 0,00       | 0,00        | C38   |
| 39    | Summe I.                                                    | =SUM(C35:C38)|=SUM(D35:D38)| C39 |
| 40    |                                                             |            |             |       |
| 41    | II. Forderungen und sonstige Vermögensgegenstände           |            |             |       |
| 42    | 1. Forderungen aus Lieferungen und Leistungen               | 0,00       | 0,00        | C42   |
| 43    | 2. Forderungen gegen verbundene Unternehmen                 | 0,00       | 0,00        | C43   |
| 44    | 3. Forderungen gegen Unternehmen (Beteiligungsverhältnis)   | 0,00       | 0,00        | C44   |
| 45    | 4. Sonstige Vermögensgegenstände                            | 3.960,00   | 0,00        | C45   |
| 46    | Summe II.                                                   | =SUM(C42:C45)|=SUM(D42:D45)| C46 |
| 47    |                                                             |            |             |       |
| 48    | III. Wertpapiere                                            |            |             |       |
| 49    | 1. Anteile an verbundenen Unternehmen                       | 0,00       | 0,00        | C49   |
| 50    | 2. Sonstige Wertpapiere                                     | 0,00       | 0,00        | C50   |
| 51    | Summe III.                                                  | =SUM(C49:C50)|=SUM(D49:D50)| C51 |
| 52    |                                                             |            |             |       |
| 53    | IV. Kassenbestand, Guthaben bei Kreditinstituten            | 2.948,66   | 0,00        | C53   |
| 54    |                                                             |            |             |       |
| 55    | SUMME UMLAUFVERMÖGEN (B)                                    | =C39+C46+C51+C53|=D39+D46+D51+D53| C55 |
| 56    |                                                             |            |             |       |
| 57    | C. RECHNUNGSABGRENZUNGSPOSTEN                               | 0,00       | 0,00        | C57   |
| 58    | D. AKTIVE LATENTE STEUERN                                   | 0,00       | 0,00        | C58   |
| 59    | E. AKTIVER UNTERSCHIEDSBETRAG VERMÖGENSVERRECHNUNG          | 0,00       | 0,00        | C59   |
| 60    |                                                             |            |             |       |
| 61    | SUMME AKTIVA                                                | =C31+C55+C57+C58+C59|=D31+D55+D57+D58+D59| C61 |
| 62    |                                                             |            |             |       |
| 63    | ─────────────────────────────────────────────────────────── |            |             |       |
| 64    |                                                             |            |             |       |
| 65    | PASSIVA                                                     |            |             |       |
| 66    | A. EIGENKAPITAL                                             |            |             |       |
| 67    | I. Gezeichnetes Kapital                                     | 12.500,00  | 12.500,00   | C67   |
| 68    | II. Kapitalrücklage                                         | 0,00       | 0,00        | C68   |
| 69    | III. Gewinnrücklagen                                        |            |             |       |
| 70    | 1. Gesetzliche Rücklage                                     | 0,00       | 0,00        | C70   |
| 71    | 2. Rücklage für Anteile an herrschendem Unternehmen         | 0,00       | 0,00        | C71   |
| 72    | 3. Satzungsmäßige Rücklagen                                 | 0,00       | 0,00        | C72   |
| 73    | 4. Andere Gewinnrücklagen                                   | 0,00       | 0,00        | C73   |
| 74    | Summe III.                                                  | =SUM(C70:C73)|=SUM(D70:D73)| C74 |
| 75    | IV. Gewinnvortrag / Verlustvortrag                          | 0,00       | 0,00        | C75   |
| 76    | V. Jahresüberschuss / Jahresfehlbetrag                      | -3.048,64  | 0,00        | C76   |
| 77    |                                                             |            |             |       |
| 78    | SUMME EIGENKAPITAL (A)                                      | =C67+C68+C74+C75+C76|=D67+D68+D74+D75+D76| C78 |
| 79    |                                                             |            |             |       |
| 80    | B. RÜCKSTELLUNGEN                                           |            |             |       |
| 81    | 1. Rückstellungen für Pensionen und ähnliche Verpflichtungen| 0,00       | 0,00        | C81   |
| 82    | 2. Steuerrückstellungen                                     | 0,00       | 0,00        | C82   |
| 83    | 3. Sonstige Rückstellungen                                  | 0,00       | 0,00        | C83   |
| 84    | SUMME RÜCKSTELLUNGEN (B)                                    | =SUM(C81:C83)|=SUM(D81:D83)| C84 |
| 85    |                                                             |            |             |       |
| 86    | C. VERBINDLICHKEITEN                                        |            |             |       |
| 87    | 1. Anleihen                                                 | 0,00       | 0,00        | C87   |
| 88    | 2. Verbindlichkeiten gegenüber Kreditinstituten             | 0,00       | 0,00        | C88   |
| 89    | 3. Erhaltene Anzahlungen auf Bestellungen                   | 0,00       | 0,00        | C89   |
| 90    | 4. Verbindlichkeiten aus Lieferungen und Leistungen         | 0,00       | 0,00        | C90   |
| 91    | 5. Verbindlichkeiten aus Annahme von Wechseln               | 0,00       | 0,00        | C91   |
| 92    | 6. Verbindlichkeiten gegenüber verbundenen Unternehmen      | 0,00       | 0,00        | C92   |
| 93    | 7. Verbindlichkeiten gegenüber Unternehmen (Beteiligung)    | 0,00       | 0,00        | C93   |
| 94    | 8. Sonstige Verbindlichkeiten                               | 8.457,30   | 0,00        | C94   |
| 95    | SUMME VERBINDLICHKEITEN (C)                                 | =SUM(C87:C94)|=SUM(D87:D94)| C95 |
| 96    |                                                             |            |             |       |
| 97    | D. RECHNUNGSABGRENZUNGSPOSTEN                               | 0,00       | 0,00        | C97   |
| 98    | E. PASSIVE LATENTE STEUERN                                  | 0,00       | 0,00        | C98   |
| 99    |                                                             |            |             |       |
| 100   | SUMME PASSIVA                                               | =C78+C84+C95+C97+C98|=D78+D84+D95+D97+D98| C100 |
```

**Excel-Formeln (automatisch):**
```excel
C61: =C31+C55+C57+C58+C59    // Summe Aktiva
C100: =C78+C84+C95+C97+C98   // Summe Passiva
```

**Vorjahreswerte (Erstjahr-Handling):**
Wenn im Stammdaten-Sheet B21 = "Ja" (Erstjahr), werden alle Vorjahres-Spalten (Spalte D) automatisch auf 0,00 gesetzt und im XBRL als leerer `prior`-Kontext übertragen. Das Tool generiert trotzdem den `prior`-Context, da ERiC diesen auch für Erstjahre erwartet — mit allen Werten = 0.

---

#### Sheet 3: GuV (Gesamtkostenverfahren)

```
| Zeile | GuV-Position                                           | Betrag (€) | Vorjahr (€) | Zelle |
|-------|--------------------------------------------------------|------------|-------------|-------|
| 3     | 1. Umsatzerlöse                                        | 0,00       | 0,00        | C3    |
| 4     | 2. Erhöhung/Verminderung Bestand                       | 0,00       | 0,00        | C4    |
| 5     | 3. Andere aktivierte Eigenleistungen                   | 0,00       | 0,00        | C5    |
| 6     | 4. Sonstige betriebliche Erträge                       | 0,00       | 0,00        | C6    |
| 7     |                                                        |            |             |       |
| 8     | 5. Materialaufwand                                     |            |             |       |
| 9     |    a) Roh-, Hilfs- und Betriebsstoffe                  | 0,00       | 0,00        | C9    |
| 10    |    b) Bezogene Leistungen                              | 0,00       | 0,00        | C10   |
| 11    | Summe Materialaufwand                                  | =C9+C10    | =D9+D10     | C11   |
| 12    |                                                        |            |             |       |
| 13    | 6. Personalaufwand                                     |            |             |       |
| 14    |    a) Löhne und Gehälter                               | 0,00       | 0,00        | C14   |
| 15    |    b) Soziale Abgaben und Altersversorgung             | 0,00       | 0,00        | C15   |
| 16    | Summe Personalaufwand                                  | =C14+C15   | =D14+D15    | C16   |
| 17    |                                                        |            |             |       |
| 18    | 7. Abschreibungen                                      |            |             |       |
| 19    |    a) auf immaterielle VG und Sachanlagen              | 0,00       | 0,00        | C19   |
| 20    |    b) auf Vermögensgegenstände Umlaufvermögen          | 0,00       | 0,00        | C20   |
| 21    | Summe Abschreibungen                                   | =C19+C20   | =D19+D20    | C21   |
| 22    |                                                        |            |             |       |
| 23    | 8. Sonstige betriebliche Aufwendungen                  | 3.048,64   | 0,00        | C23   |
| 24    |                                                        |            |             |       |
| 25    | 9. Erträge aus Beteiligungen                           | 0,00       | 0,00        | C25   |
| 26    | 10. Erträge aus anderen Wertpapieren                   | 0,00       | 0,00        | C26   |
| 27    | 11. Sonstige Zinsen und ähnliche Erträge               | 0,00       | 0,00        | C27   |
| 28    | 12. Abschreibungen auf Finanzanlagen                   | 0,00       | 0,00        | C28   |
| 29    | 13. Zinsen und ähnliche Aufwendungen                   | 0,00       | 0,00        | C29   |
| 30    |                                                        |            |             |       |
| 31    | 14. Ergebnis der gewöhnlichen Geschäftstätigkeit       | =C3+C4+C5+C6-C11-C16-C21-C23+C25+C26+C27-C28-C29| | C31 |
| 32    |                                                        |            |             |       |
| 33    | 15. Außerordentliche Erträge                           | 0,00       | 0,00        | C33   |
| 34    | 16. Außerordentliche Aufwendungen                      | 0,00       | 0,00        | C34   |
| 35    | 17. Außerordentliches Ergebnis                         | =C33-C34   | =D33-D34    | C35   |
| 36    |                                                        |            |             |       |
| 37    | 18. Steuern vom Einkommen und vom Ertrag               | 0,00       | 0,00        | C37   |
| 38    | 19. Sonstige Steuern                                   | 0,00       | 0,00        | C38   |
| 39    |                                                        |            |             |       |
| 40    | 20. JAHRESÜBERSCHUSS / JAHRESFEHLBETRAG                | =C31+C35-C37-C38|=D31+D35-D37-D38| C40 |
```

**Wichtig:** C40 (Jahresüberschuss GuV) **MUSS** = C76 (Jahresüberschuss Bilanz) sein!

---

#### Sheet 4: Validierung (automatisch)

```
| Check                                    | Status      | Details               |
|------------------------------------------|-------------|-----------------------|
| Aktiva = Passiva                         | =WENN(...)  | Differenz: ...        |
| Jahresüberschuss Bilanz = GuV            | =WENN(...)  | Differenz: ...        |
| Gezeichnetes Kapital >= 25.000 EUR (GmbH)| =WENN(...)  | Aktuell: ...          |
| Steuernummer Format                      | =WENN(...)  | XX/XXX/XXXXX          |
| BuFa-Nr. Format (4-stellig)             | =WENN(...)  | Aktuell: ...          |
| Bundeseinh. Steuernr. (13-stellig)      | =WENN(...)  | Aktuell: ...          |
| Handelsregister-Nr Format                | =WENN(...)  | HRB XXXXX             |
| Wirtschaftsjahr Dauer <= 18 Monate       | =WENN(...)  | Aktuell: X Monate     |
| Bilanzstichtag = Ende Wirtschaftsjahr    | =WENN(...)  | ...                   |
| Alle Pflichtfelder ausgefüllt            | =WENN(...)  | X von Y ausgefüllt    |
| Ergebnisverwendung konsistent            | =WENN(...)  | ...                   |
| Vorjahreswerte bei Erstjahr = 0          | =WENN(...)  | ...                   |
```

**Excel-Formeln für Validierung:**
```excel
// Bilanz-Check
=WENN(ABS(Bilanz!C61-Bilanz!C100)<0.01;"✅ OK";"❌ FEHLER: Bilanz nicht ausgeglichen")

// Jahresüberschuss-Check
=WENN(ABS(Bilanz!C76-GuV!C40)<0.01;"✅ OK";"❌ FEHLER: Jahresüberschuss stimmt nicht überein")

// Kapital-Check (GmbH - Pflicht!)
=WENN(Bilanz!C67>=25000;"✅ OK";"❌ FEHLER: Gezeichnetes Kapital < 25.000 EUR — Tool unterstützt nur GmbH, keine UG")
```

---

#### Sheet 5: Info & Hilfe

Statisches Sheet mit:
- Ausfüllanleitung pro Feld
- Hinweis auf Pflichtfelder
- Links zur ELSTER-Registrierung
- Erklärung der Validierungschecks
- Disclaimer (keine Steuerberatung)
- Versionshinweis der unterstützten Taxonomie

---

### 2.2 C++ CLI-Tool

**Executable-Name:** `ebilanz-submit`

**Aufruf:**
```bash
./ebilanz-submit [OPTIONS] <bilanz.xlsx>

Options:
  -t, --test           Test-Modus (Vorproduktion)
  -v, --validate       Nur validieren, nicht senden
  -c, --cert <file>    ELSTER-Zertifikat (.pfx)
  -p, --password       Zertifikat-Passwort (interaktiv wenn leer)
  -o, --output <dir>   XBRL-Datei speichern
  -x, --xbrl <file>    Bereits generierte XBRL-Datei direkt senden (skip Excel-Parsing)
      --verbose        Ausführliche Ausgabe (Debug-Level)
      --quiet          Nur Fehler ausgeben
      --log <file>     Log in Datei schreiben (default: ./ebilanz.log)
      --version        Versionsnummer anzeigen
  -h, --help           Hilfe anzeigen
```

**Beispiele:**
```bash
# 1. Nur validieren
./ebilanz-submit --validate bilanz-2024.xlsx

# 2. XBRL generieren und speichern
./ebilanz-submit --output ./output bilanz-2024.xlsx

# 3. Test-Versand (Vorproduktion)
./ebilanz-submit --test --cert zertifikat.pfx bilanz-2024.xlsx

# 4. Echtversand
./ebilanz-submit --cert zertifikat.pfx bilanz-2024.xlsx

# 5. Bereits generierte XBRL erneut senden (Retry nach Abbruch)
./ebilanz-submit --xbrl ./output/ebilanz-2024.xbrl --cert zertifikat.pfx

# 6. Debug-Modus mit Log-Datei
./ebilanz-submit --verbose --log debug.log --test --cert zertifikat.pfx bilanz-2024.xlsx
```

**Output:**
```
E-Bilanz Submitter v1.1
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
[1/5] Excel-Datei einlesen...          ✓
[2/5] Validierung...
  ├─ Bilanz ausgeglichen (17.908,66€)  ✓
  ├─ Jahresüberschuss konsistent       ✓
  ├─ Alle Pflichtfelder vorhanden      ✓
  ├─ Steuernummer konvertiert          ✓
  └─ Formale Prüfung                   ✓
[3/5] XBRL-Datei generieren...         ✓
[4/5] ERiC-Validierung...              ✓
[5/5] An Finanzamt senden...           ✓

Transfer-Ticket: 12345-67890-ABCDE-FGHIJ
Status: Erfolgreich übermittelt
Datum: 08.02.2026 14:32:15

XBRL gespeichert: ./output/ebilanz-2024.xbrl
Log gespeichert:  ./ebilanz.log
```

---

### 2.3 Funktionen im Detail

#### 2.3.1 Excel-Parsing

**Library:** [xlnt](https://github.com/tfussell/xlnt) (Modern C++, MIT License)

```cpp
// HINWEIS: Pseudocode — exakte API-Aufrufe nach xlnt-Dokumentation

#include <xlnt/xlnt.hpp>

struct Stammdaten {
    std::string steuernummer;          // Vereinfachtes Format
    std::string steuernummerBund;      // Bundeseinheitlich 13-stellig
    std::string finanzamtsNummer;      // BuFa-Nr 4-stellig
    std::string firmenname;
    std::string strasse;
    std::string plz;
    std::string ort;
    std::string geschaeftsfuehrer;
    std::string handelsregisterNr;
    std::string registergericht;
    std::string wirtschaftsjahrVon;    // ISO-Format: YYYY-MM-DD
    std::string wirtschaftsjahrBis;
    std::string bilanzstichtag;
    std::string wzCode;                // optional
    std::string uebermittlungsanlass;  // "erstmalig" | "korrigiert"
    std::string bilanzaufstellung;     // "handelsrecht"
    bool erstjahr;
    std::string ergebnisverwendung;    // "thesaurierung" | "ausschuettung" | "teilausschuettung"
    double ausschuettungsbetrag;       // 0.0 bei Thesaurierung
};

class ExcelParser {
public:
    Stammdaten readStammdaten(const std::string& filepath) {
        xlnt::workbook wb;
        wb.load(filepath);
        
        auto ws = wb.sheet_by_title("Stammdaten");
        
        Stammdaten data;
        data.steuernummer = ws.cell("B2").to_string();
        data.finanzamtsNummer = ws.cell("B4").to_string();
        data.firmenname = ws.cell("B5").to_string();
        // ... etc
        
        // Bundeseinheitliche Steuernummer berechnen/lesen
        data.steuernummerBund = ws.cell("B3").to_string();
        if (data.steuernummerBund.empty()) {
            data.steuernummerBund = convertSteuernummer(
                data.steuernummer, data.finanzamtsNummer);
        }
        
        return data;
    }
    
    BilanzData readBilanz(const std::string& filepath) {
        auto ws = wb.sheet_by_title("Bilanz");
        
        BilanzData bilanz;
        bilanz.immaterielleVG_konzessionen = ws.cell("C7").value<double>();
        bilanz.sachanlagen_bga = ws.cell("C16").value<double>();
        bilanz.finanzanlagen_verbundenU = ws.cell("C22").value<double>();
        // ... etc
        
        bilanz.summeAktiva = ws.cell("C61").value<double>();
        bilanz.summePassiva = ws.cell("C100").value<double>();
        
        // Vorjahreswerte (Spalte D)
        bilanz.vorjahr_summeAktiva = ws.cell("D61").value<double>();
        bilanz.vorjahr_summePassiva = ws.cell("D100").value<double>();
        // ... etc
        
        return bilanz;
    }
    
private:
    std::string convertSteuernummer(const std::string& vereinfacht, 
                                     const std::string& bufa) {
        // Konvertierung XX/XXX/XXXXX + BuFa → 13-stellig bundeseinheitlich
        // Implementierung nach ELSTER-Dokumentation
        // ...
    }
};
```

#### 2.3.2 Validierung

```cpp
// HINWEIS: Pseudocode — illustriert Validierungslogik

class Validator {
public:
    struct ValidationResult {
        bool isValid;
        std::vector<std::string> errors;
        std::vector<std::string> warnings;
    };
    
    ValidationResult validate(const Stammdaten& stamm, 
                             const BilanzData& bilanz,
                             const GuVData& guv) {
        ValidationResult result;
        result.isValid = true;
        
        // 1. Bilanz-Mathematik
        double diff = std::abs(bilanz.summeAktiva - bilanz.summePassiva);
        if (diff > 0.01) {
            result.errors.push_back("Bilanz nicht ausgeglichen! Differenz: " 
                                   + std::to_string(diff) + " EUR");
            result.isValid = false;
        }
        
        // 2. Jahresüberschuss konsistent?
        diff = std::abs(bilanz.jahresueberschuss - guv.jahresueberschuss);
        if (diff > 0.01) {
            result.errors.push_back("Jahresüberschuss in Bilanz und GuV unterschiedlich!");
            result.isValid = false;
        }
        
        // 3. Gezeichnetes Kapital (GmbH) — ERROR, nicht Warning!
        //    Tool unterstützt nur GmbH, daher muss Kapital >= 25.000 EUR sein
        if (bilanz.gezeichnetesKapital < 25000.0) {
            result.errors.push_back(
                "Gezeichnetes Kapital < 25.000 EUR. "
                "Dieses Tool unterstützt nur GmbH (nicht UG).");
            result.isValid = false;
        }
        
        // 4. Formale Checks
        if (!validateSteuernummer(stamm.steuernummer)) {
            result.errors.push_back("Steuernummer ungültiges Format (erwartet: XX/XXX/XXXXX)");
            result.isValid = false;
        }
        
        if (!validateBufaNummer(stamm.finanzamtsNummer)) {
            result.errors.push_back("BuFa-Nummer ungültig (erwartet: 4-stellig)");
            result.isValid = false;
        }
        
        if (!validateBundesSteuernummer(stamm.steuernummerBund)) {
            result.errors.push_back("Bundeseinheitliche Steuernummer ungültig (13-stellig)");
            result.isValid = false;
        }
        
        if (!validateHandelsregister(stamm.handelsregisterNr)) {
            result.errors.push_back("Handelsregisternummer ungültig (erwartet: HRB XXXXX)");
            result.isValid = false;
        }
        
        // 5. Pflichtfelder
        if (stamm.steuernummer.empty() || stamm.firmenname.empty() || 
            stamm.geschaeftsfuehrer.empty() || stamm.finanzamtsNummer.empty()) {
            result.errors.push_back("Pflichtfelder nicht ausgefüllt!");
            result.isValid = false;
        }
        
        // 6. Erstjahr-Konsistenz
        if (stamm.erstjahr) {
            // Alle Vorjahreswerte müssen 0 sein
            if (bilanz.vorjahr_summeAktiva != 0.0 || bilanz.vorjahr_summePassiva != 0.0) {
                result.warnings.push_back(
                    "Erstjahr markiert, aber Vorjahreswerte != 0. Werden auf 0 gesetzt.");
            }
        }
        
        // 7. Ergebnisverwendung
        if (stamm.ergebnisverwendung != "thesaurierung" && 
            stamm.ausschuettungsbetrag <= 0.0) {
            result.errors.push_back(
                "Ergebnisverwendung ist Ausschüttung, aber kein Betrag angegeben.");
            result.isValid = false;
        }
        
        return result;
    }
    
private:
    bool validateSteuernummer(const std::string& nr) {
        std::regex pattern(R"(\d{2}/\d{3}/\d{5})");
        return std::regex_match(nr, pattern);
    }
    
    bool validateBufaNummer(const std::string& nr) {
        std::regex pattern(R"(\d{4})");
        return std::regex_match(nr, pattern);
    }
    
    bool validateBundesSteuernummer(const std::string& nr) {
        // 13-stellige bundeseinheitliche Steuernummer
        std::regex pattern(R"(\d{13})");
        return std::regex_match(nr, pattern);
    }
    
    bool validateHandelsregister(const std::string& nr) {
        std::regex pattern(R"(HRB\s*\d+)");
        return std::regex_match(nr, pattern);
    }
};
```

#### 2.3.3 XBRL-Generierung

**Library:** [pugixml](https://pugixml.org/) (Lightweight XML, MIT License)

**Taxonomie-Version:** Konfigurierbar über `config.json` (Default: HGB 6.8)

```cpp
// HINWEIS: Pseudocode — illustriert XBRL-Struktur, exakte Tag-Namen nach Taxonomie-Doku

#include "pugixml.hpp"

// Taxonomie-Konfiguration (extern ladbar)
struct TaxonomieConfig {
    std::string version;           // z.B. "6.8"
    std::string namespaceUri;      // z.B. "http://www.xbrl.de/taxonomies/de-gaap-ci-2024-..."
    std::string schemaRef;         // Link zur .xsd
    // Mapping: interne Position-ID → XBRL-Tag
    std::map<std::string, std::string> mapping;
};

class XBRLGenerator {
public:
    XBRLGenerator(const TaxonomieConfig& config) : config_(config) {}
    
    std::string generate(const Stammdaten& stamm,
                        const BilanzData& bilanz,
                        const GuVData& guv) {
        pugi::xml_document doc;
        
        // Root-Element mit Namespaces
        auto root = doc.append_child("xbrli:xbrl");
        root.append_attribute("xmlns:xbrli") = "http://www.xbrl.org/2003/instance";
        root.append_attribute("xmlns:de-gaap-ci") = config_.namespaceUri.c_str();
        root.append_attribute("xmlns:iso4217") = "http://www.xbrl.org/2003/iso4217";
        
        // Schema-Referenz
        auto schemaRef = root.append_child("link:schemaRef");
        schemaRef.append_attribute("xlink:type") = "simple";
        schemaRef.append_attribute("xlink:href") = config_.schemaRef.c_str();
        
        // Context: Aktueller Berichtszeitraum
        createContext(root, "current", stamm.wirtschaftsjahrVon, stamm.wirtschaftsjahrBis, stamm);
        
        // Context: Stichtag (instant)
        createInstantContext(root, "current_instant", stamm.bilanzstichtag, stamm);
        
        // Context: Vorjahr (auch bei Erstjahr — mit leeren Werten)
        std::string vorjahrVon = calculateVorjahrVon(stamm);
        std::string vorjahrBis = calculateVorjahrBis(stamm);
        createContext(root, "prior", vorjahrVon, vorjahrBis, stamm);
        createInstantContext(root, "prior_instant", vorjahrBis, stamm);
        
        // Unit (EUR)
        auto unit = root.append_child("xbrli:unit");
        unit.append_attribute("id") = "EUR";
        unit.append_child("xbrli:measure").text().set("iso4217:EUR");
        
        // Bilanzdaten mit Mapping aus Config
        for (const auto& [posId, value] : bilanz.positionen) {
            auto xbrlTag = config_.mapping.at(posId);
            addMonetaryFact(root, xbrlTag, value, "current_instant", "EUR");
        }
        
        // Auffangpositionen: Werte die nicht detailliert sind,
        // werden in die jeweilige Auffangposition gemappt
        handleAuffangpositionen(root, bilanz);
        
        // Vorjahreswerte
        for (const auto& [posId, value] : bilanz.vorjahrPositionen) {
            auto xbrlTag = config_.mapping.at(posId);
            addMonetaryFact(root, xbrlTag, value, "prior_instant", "EUR");
        }
        
        // XML zu String
        std::stringstream ss;
        doc.save(ss, "  ");
        return ss.str();
    }
    
private:
    TaxonomieConfig config_;
    
    void handleAuffangpositionen(pugi::xml_node& root, const BilanzData& bilanz) {
        // Taxonomie-Mussfelder prüfen:
        // Wenn Detailpositionen leer, Summe in Auffangposition schreiben
        // Beispiel: Wenn keine Einzelpositionen bei Immateriellen VG,
        //           den Gesamtwert in die Auffangposition
        // Implementierung nach Taxonomie-Leitfaden
    }
    
    void addMonetaryFact(pugi::xml_node& parent, 
                        const std::string& concept,
                        double value,
                        const std::string& contextRef,
                        const std::string& unitRef) {
        auto fact = parent.append_child(concept.c_str());
        fact.append_attribute("contextRef") = contextRef.c_str();
        fact.append_attribute("unitRef") = unitRef.c_str();
        fact.append_attribute("decimals") = "2";
        fact.text().set(std::to_string(value).c_str());
    }
    
    void createContext(pugi::xml_node& root, const std::string& id,
                      const std::string& startDate, const std::string& endDate,
                      const Stammdaten& stamm) {
        auto context = root.append_child("xbrli:context");
        context.append_attribute("id") = id.c_str();
        
        auto entity = context.append_child("xbrli:entity");
        auto identifier = entity.append_child("xbrli:identifier");
        identifier.append_attribute("scheme") = "http://www.rzf-nrw.de/Steuernummer";
        identifier.text().set(stamm.steuernummerBund.c_str());
        
        auto period = context.append_child("xbrli:period");
        period.append_child("xbrli:startDate").text().set(startDate.c_str());
        period.append_child("xbrli:endDate").text().set(endDate.c_str());
    }
    
    void createInstantContext(pugi::xml_node& root, const std::string& id,
                             const std::string& date, const Stammdaten& stamm) {
        auto context = root.append_child("xbrli:context");
        context.append_attribute("id") = id.c_str();
        
        auto entity = context.append_child("xbrli:entity");
        auto identifier = entity.append_child("xbrli:identifier");
        identifier.append_attribute("scheme") = "http://www.rzf-nrw.de/Steuernummer";
        identifier.text().set(stamm.steuernummerBund.c_str());
        
        auto period = context.append_child("xbrli:period");
        period.append_child("xbrli:instant").text().set(date.c_str());
    }
};
```

**Mapping HGB → XBRL (Auszug):**
```cpp
// Mapping wird aus config/taxonomie-6.8.json geladen
// Hier nur zur Illustration:

// Aktiva
"immaterielleVG_konzessionen"  → "de-gaap-ci:bs.ass.fixAss.intangAss.concessionsPIR..."
"sachanlagen_bga"              → "de-gaap-ci:bs.ass.fixAss.tan.otherEq..."
"finanzanlagen_verbundenU"     → "de-gaap-ci:bs.ass.fixAss.fin.worksAffiliated..."
"forderungen_lul"              → "de-gaap-ci:bs.ass.currAss.worksDebtorsTA..."
"kassenbestand"                → "de-gaap-ci:bs.ass.currAss.worksChequeCash..."

// Passiva
"gezeichnetesKapital"          → "de-gaap-ci:bs.eqLiab.worksIssuedCapital..."
"jahresueberschuss"            → "de-gaap-ci:bs.eqLiab.equity.netIncome..."
"sonstigeVerbindlichkeiten"    → "de-gaap-ci:bs.eqLiab.worksOtherLiab..."
```

**Hinweis:** Die exakten XBRL-Tag-Namen müssen aus der offiziellen HGB-Taxonomie-Dokumentation entnommen werden. Die hier gezeigten sind vereinfachte Platzhalter. Das vollständige Mapping wird in `config/taxonomie-mapping.json` gepflegt und bei Taxonomie-Updates ausgetauscht.

#### 2.3.4 ERiC-Integration

**ERiC-SDK:** Download von [elster.de](https://www.elster.de/elsterweb/entwickler)

```cpp
// HINWEIS: Pseudocode — exakte Funktionssignaturen und Parameter nach ERiC-SDK-Dokumentation.
// Die echte ERiC-API nutzt EricBearbeiteVorgang() statt vereinfachter Funktionsnamen.
// Rückgabetypen und Handle-Verwaltung nach aktueller ERiC-Doku anpassen.

#include "ericapi.h"

class ERiCConnector {
private:
    // ERiC-Handle-Verwaltung nach SDK-Doku
    
public:
    ERiCConnector() {
        // ERiC initialisieren
        // EricInitialisiere(pluginPfad, logPfad)
    }
    
    ~ERiCConnector() {
        // EricBeende()
        // Zertifikat-Handle freigeben
    }
    
    bool loadCertificate(const std::string& pfxPath, 
                        const std::string& password) {
        // EricGetHandleToCertificate(pfxPath, pin, &certHandle)
        // Rückgabe: ERIC_OK bei Erfolg
        return true;
    }
    
    struct ValidationResult {
        bool isValid;
        std::string message;
        int errorCode;
    };
    
    ValidationResult validate(const std::string& xbrlContent) {
        // EricBearbeiteVorgang() mit Prüf-Flag
        // Rückgabe enthält Validierungsergebnis + Fehlermeldungen
        ValidationResult vr;
        // ...
        return vr;
    }
    
    struct SubmitResult {
        bool success;
        std::string transferTicket;
        std::string message;
        int errorCode;
    };
    
    SubmitResult submit(const std::string& xbrlContent, 
                       bool testMode = false) {
        // EricBearbeiteVorgang() mit Send-Flag
        // testMode: Vorproduktions-Server
        
        SubmitResult sr;
        
        // Bei Erfolg: Transfer-Ticket aus Rückgabe extrahieren
        // Bei Fehler: Error-Code + Nachricht auswerten
        
        return sr;
    }
    
    // Retry-Unterstützung: XBRL direkt senden ohne erneutes Parsen
    SubmitResult submitXbrlFile(const std::string& xbrlFilePath,
                               bool testMode = false) {
        // Datei laden und an submit() delegieren
        std::ifstream file(xbrlFilePath);
        std::string content((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
        return submit(content, testMode);
    }
};
```

---

## 3. Technischer Stack

### 3.1 Entwicklungsumgebung

**Compiler:** 
- macOS: Clang++ (via Xcode Command Line Tools)
- Windows: MSVC oder MinGW-w64

**C++ Standard:** C++17 oder höher

**Build-System:** CMake 3.20+

### 3.2 Dependencies

**Core:**
```cmake
# xlnt - Excel-Library
find_package(xlnt REQUIRED)

# pugixml - XML-Library
find_package(pugixml REQUIRED)

# ERiC-SDK
# Manual linking (proprietary, nicht über Paketmanager)
```

**CLI & Logging:**
```cmake
# CLI11 - Command-Line-Parser
find_package(CLI11 REQUIRED)

# spdlog - Logging (NICHT optional, wird benötigt)
find_package(spdlog REQUIRED)

# nlohmann/json - Taxonomie-Config
find_package(nlohmann_json REQUIRED)
```

**Testing:**
```cmake
# Catch2 - Unit-Testing
find_package(Catch2 REQUIRED)
```

### 3.3 Projekt-Struktur

```
ebilanz-tool/
├── CMakeLists.txt
├── README.md
├── LICENSE
├── CLAUDE.md                        # Claude Code Projektanweisungen
├── config/
│   ├── taxonomie-6.8.json           # XBRL-Mapping (austauschbar)
│   └── bufa-verzeichnis.json        # BuFa-Nummern → Bundesland-Mapping
├── templates/
│   └── ebilanz-template.xlsx        # Excel-Vorlage
├── external/
│   └── eric-sdk/                    # ERiC-SDK (lokal, nicht im Git)
│       ├── include/
│       │   └── ericapi.h
│       └── lib/
│           ├── libericapi.dylib (macOS)
│           └── ericapi.dll (Windows)
├── src/
│   ├── main.cpp                     # CLI Entry Point
│   ├── excel_parser.h/cpp           # Excel-Parsing
│   ├── validator.h/cpp              # Validierung
│   ├── xbrl_generator.h/cpp         # XBRL-Generierung
│   ├── eric_connector.h/cpp         # ERiC-Integration
│   ├── steuernummer.h/cpp           # Steuernummer-Konvertierung
│   ├── taxonomie_config.h/cpp       # Taxonomie-Konfiguration laden
│   ├── logger.h/cpp                 # Logging-Setup
│   ├── models.h                     # Datenstrukturen
│   └── utils.h/cpp                  # Hilfsfunktionen
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
    ├── MANUAL.md                    # Benutzerhandbuch
    └── XBRL_MAPPING.md              # HGB → XBRL Mapping
```

---

## 4. Implementierungs-Phasen

### Phase 1: Foundation (Woche 1-2)
- [ ] Projekt-Setup (CMake, Dependencies, spdlog, CLI11)
- [ ] Steuernummer-Konvertierung implementieren + Tests
- [ ] Excel-Template erstellen (alle 5 Sheets)
- [ ] Excel-Parser implementieren (inkl. Vorjahreswerte)
- [ ] Basis-Validierung (alle Checks inkl. GmbH-Kapital als Error)
- [ ] Unit-Tests für Parser + Validator

**Deliverable:** Excel einlesen + vollständige Validierung

### Phase 2: XBRL-Generation (Woche 3-4)
- [ ] Taxonomie-Config-Loader (JSON-basiert)
- [ ] XBRL-Generator implementieren (current + prior Context)
- [ ] Auffangpositionen-Logik
- [ ] HGB → XBRL Mapping (alle Positionen inkl. Mussfelder)
- [ ] Erstjahr-Handling (prior-Context mit Nullwerten)
- [ ] XBRL-Output-Validierung (lokal gegen Schema)
- [ ] Integration-Tests

**Deliverable:** Valide XBRL-Datei generieren

### Phase 3: ERiC-Integration (Woche 5-6)
- [ ] ERiC-SDK einbinden (macOS primär)
- [ ] Zertifikat-Handling (plattformübergreifend)
- [ ] Test-Versand implementieren (Vorproduktion)
- [ ] Error-Handling + ERiC-Error-Code-Mapping
- [ ] --xbrl Retry-Flag implementieren

**Deliverable:** Testversand funktioniert

### Phase 4: Produktiv-Betrieb (Woche 7-8)
- [ ] Echtversand implementieren
- [ ] CLI-Interface polieren (--verbose, --quiet, --version)
- [ ] Logging implementieren (Datei + Console, Level-basiert)
- [ ] Transfer-History (lokale SQLite oder JSON)
- [ ] Dokumentation schreiben (README, MANUAL)
- [ ] End-to-End-Tests
- [ ] Windows-Build testen

**Deliverable:** Produktions-reifes Tool

---

## 5. Testing-Strategie

### 5.1 Unit-Tests

```cpp
// test_validator.cpp
TEST_CASE("Validator: Bilanz ausgeglichen") {
    Stammdaten stamm = createTestStammdaten();
    BilanzData bilanz;
    bilanz.summeAktiva = 17908.66;
    bilanz.summePassiva = 17908.66;
    bilanz.gezeichnetesKapital = 25000.0;
    
    Validator validator;
    auto result = validator.validate(stamm, bilanz, GuVData{});
    
    REQUIRE(result.isValid == true);
    REQUIRE(result.errors.empty());
}

TEST_CASE("Validator: Bilanz nicht ausgeglichen") {
    BilanzData bilanz;
    bilanz.summeAktiva = 17908.66;
    bilanz.summePassiva = 17000.00;
    bilanz.gezeichnetesKapital = 25000.0;
    
    Validator validator;
    auto result = validator.validate(createTestStammdaten(), bilanz, GuVData{});
    
    REQUIRE(result.isValid == false);
    REQUIRE(result.errors.size() > 0);
}

TEST_CASE("Validator: GmbH-Kapital unter 25.000 EUR ist ERROR") {
    BilanzData bilanz = createBalancedTestBilanz();
    bilanz.gezeichnetesKapital = 12500.0;  // UG-Kapital
    
    Validator validator;
    auto result = validator.validate(createTestStammdaten(), bilanz, createTestGuV());
    
    REQUIRE(result.isValid == false);  // Error, nicht Warning!
    REQUIRE_THAT(result.errors[0], Catch::Matchers::ContainsSubstring("25.000"));
}

// test_steuernummer.cpp
TEST_CASE("Steuernummer-Konvertierung: NRW") {
    // Beispiel: Finanzamt Köln-Altstadt
    auto result = convertSteuernummer("212/5770/1234", "5214");
    REQUIRE(result == "5214021577012340");  // Beispielformat
}

TEST_CASE("Steuernummer-Konvertierung: Bayern") {
    auto result = convertSteuernummer("143/234/56789", "9143");
    // Prüfe Bundesland-spezifische Konvertierung
    REQUIRE(result.length() == 13);
}
```

### 5.2 Integration-Tests

```cpp
TEST_CASE("End-to-End: Excel → XBRL → ERiC-Validierung") {
    // 1. Excel einlesen
    ExcelParser parser;
    auto stamm = parser.readStammdaten("testdata/test-bilanz.xlsx");
    auto bilanz = parser.readBilanz("testdata/test-bilanz.xlsx");
    auto guv = parser.readGuV("testdata/test-bilanz.xlsx");
    
    // 2. Validieren
    Validator validator;
    auto vr = validator.validate(stamm, bilanz, guv);
    REQUIRE(vr.isValid);
    
    // 3. XBRL generieren
    TaxonomieConfig config = loadConfig("config/taxonomie-6.8.json");
    XBRLGenerator generator(config);
    std::string xbrl = generator.generate(stamm, bilanz, guv);
    REQUIRE(!xbrl.empty());
    
    // 4. Mit ERiC validieren (nur wenn SDK verfügbar)
    if (ericSdkAvailable()) {
        ERiCConnector eric;
        auto ericResult = eric.validate(xbrl);
        REQUIRE(ericResult.isValid);
    }
}

TEST_CASE("Erstjahr: Prior-Context mit Nullwerten") {
    ExcelParser parser;
    auto stamm = parser.readStammdaten("testdata/test-bilanz-erstjahr.xlsx");
    REQUIRE(stamm.erstjahr == true);
    
    auto bilanz = parser.readBilanz("testdata/test-bilanz-erstjahr.xlsx");
    
    TaxonomieConfig config = loadConfig("config/taxonomie-6.8.json");
    XBRLGenerator generator(config);
    std::string xbrl = generator.generate(stamm, bilanz, GuVData{});
    
    // XBRL muss prior-Context enthalten, auch bei Erstjahr
    REQUIRE(xbrl.find("prior") != std::string::npos);
}
```

### 5.3 Manuelle Tests

**Checkliste:**
- [ ] Excel-Template mit Beispieldaten füllen
- [ ] Tool ausführen (validate-only)
- [ ] Generierte XBRL-Datei auf elster.de hochladen (Validator)
- [ ] Test-Versand ans Finanzamt (Vorproduktion)
- [ ] Transfer-Ticket verifizieren
- [ ] Retry mit --xbrl Flag testen
- [ ] Erstjahr-Bilanz testen (alle Vorjahre = 0)
- [ ] Echtversand mit echten Daten (VORSICHT!)

---

## 6. Error-Handling

### 6.1 Error-Codes

```cpp
enum class ErrorCode {
    SUCCESS = 0,
    EXCEL_FILE_NOT_FOUND = 1,
    EXCEL_PARSE_ERROR = 2,
    VALIDATION_FAILED = 3,
    XBRL_GENERATION_FAILED = 4,
    XBRL_FILE_NOT_FOUND = 5,      // für --xbrl Flag
    CONFIG_LOAD_FAILED = 6,        // Taxonomie-Config
    ERIC_INIT_FAILED = 10,
    ERIC_CERT_LOAD_FAILED = 11,
    ERIC_VALIDATION_FAILED = 12,
    ERIC_SUBMIT_FAILED = 13,
    ERIC_TIMEOUT = 14,
    STEUERNUMMER_CONVERT_FAILED = 20,
    UNKNOWN_ERROR = 99
};
```

### 6.2 User-Friendly Messages

```cpp
void printError(ErrorCode code, const std::string& details) {
    std::cerr << "❌ FEHLER: ";
    
    switch(code) {
        case ErrorCode::EXCEL_FILE_NOT_FOUND:
            std::cerr << "Excel-Datei nicht gefunden.\n";
            std::cerr << "Details: " << details << "\n";
            break;
            
        case ErrorCode::VALIDATION_FAILED:
            std::cerr << "Validierung fehlgeschlagen!\n";
            std::cerr << details << "\n";
            std::cerr << "Bitte prüfen Sie Ihre Excel-Datei.\n";
            break;
            
        case ErrorCode::ERIC_CERT_LOAD_FAILED:
            std::cerr << "ELSTER-Zertifikat konnte nicht geladen werden.\n";
            std::cerr << "Bitte prüfen Sie Pfad und Passwort.\n";
            break;
            
        case ErrorCode::ERIC_TIMEOUT:
            std::cerr << "Zeitüberschreitung bei der Übermittlung.\n";
            std::cerr << "Tipp: Nutzen Sie --xbrl um die generierte Datei erneut zu senden.\n";
            break;
            
        case ErrorCode::STEUERNUMMER_CONVERT_FAILED:
            std::cerr << "Steuernummer konnte nicht konvertiert werden.\n";
            std::cerr << "Prüfen Sie Steuernummer und BuFa-Nummer.\n";
            break;
            
        // ... etc
    }
    
    // Immer auf Log-Datei verweisen
    spdlog::error("ErrorCode={}: {}", static_cast<int>(code), details);
    std::cerr << "Details in Log-Datei: ./ebilanz.log\n";
}
```

---

## 7. Logging

### 7.1 Log-Konfiguration

```cpp
// logger.h
#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

void setupLogger(const std::string& logFile, bool verbose, bool quiet) {
    auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    auto fileSink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        logFile, 5 * 1024 * 1024, 3);  // 5 MB, 3 Dateien
    
    // Console: nur Errors (default) oder alles (verbose)
    consoleSink->set_level(verbose ? spdlog::level::debug : 
                          quiet ? spdlog::level::err : spdlog::level::info);
    
    // File: immer alles loggen
    fileSink->set_level(spdlog::level::debug);
    
    auto logger = std::make_shared<spdlog::logger>("ebilanz", 
        spdlog::sinks_init_list{consoleSink, fileSink});
    spdlog::set_default_logger(logger);
}
```

### 7.2 Was wird geloggt

- Programmstart mit Version + Zeitstempel
- Excel-Datei: Pfad, Dateigröße, Sheet-Namen
- Alle Validierungsergebnisse (Errors + Warnings)
- XBRL-Generierung: Anzahl Positionen, Dateigröße
- ERiC-Kommunikation: Request/Response (ohne Zertifikat-Passwort!)
- Transfer-Ticket + Zeitstempel (für Nachweise)
- Fehler mit Stack-Context

### 7.3 Transfer-History

Nach jedem erfolgreichen Versand wird ein Eintrag in `~/.ebilanz/history.json` gespeichert:

```json
{
  "transfers": [
    {
      "timestamp": "2026-02-08T14:32:15Z",
      "ticket": "12345-67890-ABCDE-FGHIJ",
      "steuernummer": "12/345/67890",
      "wirtschaftsjahr": "2024",
      "testMode": false,
      "xbrlFile": "./output/ebilanz-2024.xbrl"
    }
  ]
}
```

---

## 8. Sicherheit

### 8.1 Zertifikat-Handling

**Anforderungen:**
- Zertifikat-Passwort **NIE** im Code, Log oder History
- Passwort über CLI-Option oder interaktive Eingabe (stdin)
- Zertifikat nach Nutzung aus Speicher löschen (secure_zero)

```cpp
#include <cstring>

std::string readPassword() {
    std::cout << "Zertifikat-Passwort: ";
    
    #ifdef _WIN32
    // Windows: _getch() für jedes Zeichen
    std::string password;
    char ch;
    while ((ch = _getch()) != '\r') {
        if (ch == '\b' && !password.empty()) {
            password.pop_back();
        } else {
            password.push_back(ch);
        }
    }
    std::cout << "\n";
    #else
    // POSIX: Terminal echo ausschalten
    termios oldt;
    tcgetattr(STDIN_FILENO, &oldt);
    termios newt = oldt;
    newt.c_lflag &= ~ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    
    std::string password;
    std::getline(std::cin, password);
    
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    std::cout << "\n";
    #endif
    
    return password;
}

// Passwort sicher aus dem Speicher löschen
void secureZero(std::string& s) {
    volatile char* p = const_cast<volatile char*>(s.data());
    std::memset(const_cast<char*>(s.data()), 0, s.size());
    // Compiler darf das nicht wegoptimieren wegen volatile
}
```

### 8.2 Daten-Schutz

**Wichtig:**
- Excel-Datei enthält sensible Firmendaten
- Keine Telemetrie, keine Cloud-Uploads, keine Analytics
- XBRL nur lokal speichern (außer Finanzamts-Versand)
- Log-Dateien enthalten keine Passwörter oder Zertifikatsinhalte
- Transfer-History enthält keine Bilanzzahlen

---

## 9. Dokumentation

### 9.1 README.md

```markdown
# E-Bilanz Excel Submitter

Einfaches Tool zur elektronischen Übermittlung von GmbH-Bilanzen ans Finanzamt.

## Installation

### Voraussetzungen
- macOS 12+ oder Windows 10+
- ELSTER-Zertifikat (.pfx)

### Build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

### Download Binary
Alternativ: [Release herunterladen](releases/)

## Verwendung

1. Excel-Template ausfüllen: `ebilanz-template.xlsx`
2. Validieren: `./ebilanz-submit --validate bilanz.xlsx`
3. Testversand: `./ebilanz-submit --test --cert zertifikat.pfx bilanz.xlsx`
4. Echtversand: `./ebilanz-submit --cert zertifikat.pfx bilanz.xlsx`

## Troubleshooting

**Fehler: Bilanz nicht ausgeglichen**
→ Prüfen Sie Sheet "Validierung" in Excel

**Fehler: Zertifikat ungültig**
→ Überprüfen Sie Pfad und Passwort

**Fehler: ERiC-Validierung fehlgeschlagen**
→ Siehe Log-Datei für Details

**Fehler: Steuernummer-Konvertierung**
→ Prüfen Sie Steuernummer (XX/XXX/XXXXX) und BuFa-Nummer (4-stellig)

**Versand abgebrochen — Retry**
→ `./ebilanz-submit --xbrl output/ebilanz-2024.xbrl --cert zertifikat.pfx`
```

### 9.2 MANUAL.md

Ausführliches Benutzerhandbuch mit:
- Screenshots des Excel-Templates
- Schritt-für-Schritt-Anleitung
- Erklärung aller CLI-Optionen
- Steuernummer-Format-Hilfe
- FAQ
- Kontakt/Support

---

## 10. Deployment

### 10.1 Binary-Distribution

**macOS:**
```bash
# Universal Binary (Intel + ARM)
cmake -B build-universal \
  -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build-universal --config Release

# DMG erstellen (optional)
create-dmg build-universal/ebilanz-submit.app
```

**Windows:**
```bash
# MSVC Build
cmake -B build -G "Visual Studio 17 2022"
cmake --build build --config Release

# Installer erstellen (NSIS)
makensis installer.nsi
```

### 10.2 Dependencies Bundling

**macOS:** ERiC-Library einbetten
```bash
install_name_tool -change \
  /usr/local/lib/libericapi.dylib \
  @executable_path/../Frameworks/libericapi.dylib \
  ebilanz-submit
```

**Windows:** DLLs im gleichen Verzeichnis
```
ebilanz-submit.exe
ericapi.dll
xerces-c.dll  (falls benötigt)
```

### 10.3 Taxonomie-Updates

Bei jährlichen Taxonomie-Updates:
1. Neue `config/taxonomie-X.Y.json` erstellen
2. Mapping aktualisieren
3. Neues Release erstellen
4. Optional: Auto-Update-Check via GitHub Releases API

---

## 11. Offene Punkte & Risiken

### 11.1 Offene Fragen

✅ **Beantwortet:**
- Bilanzierungsgröße: Klein
- Rechtsform: Nur GmbH
- Anhang/Lagebericht: Nein (explizit Out of Scope)
- Ergebnisverwendung: Vereinfacht über Stammdaten-Feld
- Deployment: Lokal, Desktop-only
- Umsatzkostenverfahren: Nicht unterstützt

❓ **Offen:**
- Genaue ERiC-API-Signaturen (nach SDK-Download klären)
- Vollständiges XBRL-Mapping (nach Taxonomie-Download)
- Auffangpositionen-Mapping (nach Taxonomie-Leitfaden)
- Steuernummer-Konvertierungsregeln pro Bundesland

### 11.2 Risiken

**Technisch:**
- ERiC-SDK Dokumentation ist schlecht → Viel Trial & Error nötig
- XBRL-Taxonomie-Updates → Tool muss jährlich aktualisiert werden (Config-basiert mitigiert)
- Excel-Format-Änderungen durch User → Robustes Parsing mit klaren Fehlermeldungen
- Auffangpositionen-Logik komplex → Intensive Tests nötig

**Rechtlich:**
- Keine Garantie für Akzeptanz durch Finanzamt → **Disclaimer nötig**
- Tool ist Hilfsmittel, keine Steuerberatung → Nutzer trägt Verantwortung

**Betrieblich:**
- ELSTER-Zertifikat läuft ab (3 Jahre) → User muss verlängern
- ERiC-SDK-Updates erforderlich → Wartung nötig

### 11.3 Mitigation

- **Umfangreiche Tests** mit Finanzamts-Vorproduktion
- **Klarer Disclaimer** im Tool und Doku
- **Konfigurierbare Taxonomie** statt hard-coded
- **Automatische Update-Checks** (optional, via GitHub Releases)
- **Transfer-History** als Nachweis für Nutzer
- **Retry-Mechanismus** bei Übertragungsfehlern

---

## 12. Lizenz & Disclaimer

### 12.1 Software-Lizenz

**MIT License** (oder nach Wahl)

### 12.2 Disclaimer

```
WICHTIGER HINWEIS:

Dieses Tool dient als technische Hilfe zur Übermittlung von E-Bilanzen
an das Finanzamt. Es ersetzt KEINE professionelle Steuerberatung!

- Keine Garantie für Richtigkeit der XBRL-Generierung
- Keine Haftung für fehlerhafte Übermittlungen
- Nutzer trägt volle Verantwortung für eingereichte Daten
- Bei Unsicherheiten: Steuerberater konsultieren

Die Verwendung erfolgt auf eigenes Risiko.
```

---

## Anhang

### A1: HGB → XBRL Mapping (vollständig)

*Siehe separate Datei: `XBRL_MAPPING.md` und `config/taxonomie-6.8.json`*

### A2: ERiC-Error-Codes

| Code | Bedeutung | Lösung |
|------|-----------|--------|
| 610001001 | Zertifikat ungültig | Neues Zertifikat generieren |
| 610001013 | Zertifikat abgelaufen | Zertifikat verlängern |
| 610001201 | XBRL-Validierung fehlgeschlagen | XBRL prüfen (siehe Log) |
| 610001214 | Steuernummer unbekannt | Steuernummer + BuFa-Nr korrigieren |

*(Vollständige Liste in ERiC-Dokumentation)*

### A3: Steuernummer-Konvertierung

Die Konvertierung von vereinfachter → bundeseinheitlicher Steuernummer ist Bundesland-spezifisch. Die Konvertierungsregeln werden aus `config/bufa-verzeichnis.json` geladen. Quelle: ELSTER-Entwickler-Dokumentation.

### A4: Referenzen

- [ELSTER Entwickler-Portal](https://www.elster.de/elsterweb/entwickler)
- [HGB-Taxonomie](https://www.xbrl.de/taxonomie/)
- [xlnt Library](https://github.com/tfussell/xlnt)
- [pugixml Library](https://pugixml.org/)
- [CLI11](https://github.com/CLIUtils/CLI11)
- [spdlog](https://github.com/gabime/spdlog)
- [nlohmann/json](https://github.com/nlohmann/json)

---

**Ende Lastenheft**

---

## Changelog

| Version | Datum | Änderungen |
|---------|-------|------------|
| 1.0 | 08.02.2026 | Initiale Version basierend auf echter Bilanz |
| 1.1 | 08.02.2026 | Bundeseinheitliche Steuernummer + BuFa-Nr; Taxonomie konfigurierbar; ERiC-Code als Pseudocode markiert; Erstjahr-Handling + prior-Context; Ergebnisverwendung (Sheet 5: Info); Auffangpositionen-Logik; CLI: --version, --verbose, --quiet, --log, --xbrl; Scope-Abgrenzung verschärft (explizites Out of Scope); Retry-Mechanismus (--xbrl); Logging-Konzept mit spdlog; GmbH-Kapital-Check als Error statt Warning; Windows readPassword(); Transfer-History; Phasen-Checkmarks zurückgesetzt |
