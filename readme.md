  ---
  ## Setup: ERiC SDK Installation (macOS)

  The ERiC SDK is required for ELSTER submission. Download from:
  https://www.elster.de/elsterweb/entwickler

  1. Download ERiC-43.3.2.0-Darwin-universal.jar (or latest version)

  2. Extract the JAR file (it's a ZIP):
     ```bash
     cd ~/Downloads
     unzip -q ERiC-43.3.2.0-Darwin-universal.jar -d eric-extracted
     ```

  3. Copy to project:
     ```bash
     cd /path/to/ebilanz
     cp -r ~/Downloads/eric-extracted/ERiC-43.3.2.0/Darwin-universal/include external/eric-sdk/
     cp -r ~/Downloads/eric-extracted/ERiC-43.3.2.0/Darwin-universal/lib external/eric-sdk/
     ```

  4. **IMPORTANT - macOS Security Fix:**
     ```bash
     # Remove quarantine attribute from all libraries
     xattr -d com.apple.quarantine external/eric-sdk/lib/*.dylib
     xattr -d com.apple.quarantine external/eric-sdk/lib/plugins/*.dylib

     # Ad-hoc sign all libraries
     for lib in external/eric-sdk/lib/*.dylib; do codesign -s - "$lib"; done
     for lib in external/eric-sdk/lib/plugins/*.dylib; do codesign -s - "$lib"; done
     ```

  5. Rebuild the project:
     ```bash
     cmake -B build -DWITH_ERIC=ON
     cmake --build build
     ```

  ---
  ## Step 1: Fill the Excel Template

  Use templates/ebilanz-template.xlsx with 5 sheets:

  Sheet: Stammdaten
  What to enter: Company info: tax number (XX/XXX/XXXXX),
  BuFa-Nr, address, fiscal year dates, first year?
  ────────────────────────────────────────
  Sheet: Bilanz
  What to enter: Balance sheet per HGB §266: assets (Aktiva),
    liabilities (Passiva), current + prior year
  ────────────────────────────────────────
  Sheet: GuV
  What to enter: Income statement (Gesamtkostenverfahren):
    revenue, costs, annual result
  ────────────────────────────────────────
  Sheet: Validierung
  What to enter: Auto-checks (built-in formulas show OK/ERROR)
  ────────────────────────────────────────
  Sheet: Info
  What to enter: Help & instructions

  Key rules:
  - Aktiva must equal Passiva
  - Jahresüberschuss in Bilanz must match GuV
  - Registered capital ≥ 25,000 EUR (GmbH only, no UG)

  ---
  ## Step 2: Validate Your Data

  **IMPORTANT: Run from the build directory!**

  ```bash
  cd build
  ./ebilanz-submit --validate ../tests/testdata/test-bilanz.xlsx
  ```

  Checks all 12 validation rules. Fix any ERRORs before proceeding.

  ---
  ## Step 3: Generate XBRL (No ERiC Required)

  ```bash
  cd build
  ./ebilanz-submit --output /tmp ../tests/testdata/test-bilanz.xlsx
  ```

  ✅ **This works perfectly!** Generates HGB-compliant XBRL file.

  Saves the XBRL file locally so you can inspect it before submission.

  ---
  ## Step 4: Test Submission (Requires ERiC SDK + Certificate)

  ```bash
  cd build
  ./ebilanz-submit --test --cert zertifikat.pfx ../bilanz-2024.xlsx
  ```

  ⚠️ **Status**: ERiC SDK initializes and validates XBRL. May show
  schema validation errors - XBRL generation is being fine-tuned.

  Submits to the test server (Vorproduktion). You'll be prompted
  for your certificate password.

  ---
  ## Step 5: Production Submission

  ```bash
  cd build
  ./ebilanz-submit --cert zertifikat.pfx ../bilanz-2024.xlsx
  ```

  Submits to the live tax office. You'll receive a
  Transfer-Ticket — save this for your records.

  ---
  ## Quick Reference

  ⚠️ **Always run from build directory**: `cd build` first!

  ┌────────────────┬─────────────────────────────────────┐
  │    Command     │          What it does               │
  ├────────────────┼─────────────────────────────────────┤
  │ --validate     │ Check data only, no submission      │
  ├────────────────┼─────────────────────────────────────┤
  │ --output <dir> │ Save XBRL file locally (✅ Works!)  │
  ├────────────────┼─────────────────────────────────────┤
  │ --test         │ Use test server (Vorproduktion)     │
  ├────────────────┼─────────────────────────────────────┤
  │ --cert <file>  │ ELSTER certificate (.pfx)           │
  ├────────────────┼─────────────────────────────────────┤
  │ --verbose      │ Detailed debug output               │
  ├────────────────┼─────────────────────────────────────┤
  │ --log <file>   │ Save logs to file                   │
  └────────────────┴─────────────────────────────────────┘

  ---
  ## Requirements

  - ELSTER certificate (.pfx) from elster.de
  - ERiC-SDK in external/eric-sdk/ (see Setup above)
  - Filled Excel template with valid GmbH data

  ---
  ## What's Working

  ✅ **Excel parsing** - All 40 tests pass
  ✅ **Data validation** - 12 validation rules implemented
  ✅ **XBRL generation** - HGB Taxonomie 6.8 support
  ✅ **ERiC SDK integration** - Initializes, loads plugins
  ⚠️ **XBRL schema validation** - Being fine-tuned for full compliance
  🔧 **ELSTER submission** - Needs real certificate for testing

  ---
  ## Troubleshooting

  **"Library not loaded" errors on macOS:**
  → Run the security fix commands in Setup step 4

  **"ERiC Initialisierung fehlgeschlagen":**
  → Make sure you're running from the build directory: `cd build`

  **"Plugin not found" errors:**
  → Verify plugins copied: `ls build/lib/plugins | wc -l` should show 212

  **"No such file" when running:**
  → Use relative paths from build dir: `./ebilanz-submit ../file.xlsx`