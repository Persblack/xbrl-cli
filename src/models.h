#pragma once

#include <map>
#include <string>
#include <vector>

// ── Error Codes ──────────────────────────────────────────────────────────────

enum class ErrorCode {
    SUCCESS = 0,
    EXCEL_FILE_NOT_FOUND = 1,
    EXCEL_PARSE_ERROR = 2,
    VALIDATION_FAILED = 3,
    XBRL_GENERATION_FAILED = 4,
    XBRL_FILE_NOT_FOUND = 5,
    CONFIG_LOAD_FAILED = 6,
    ERIC_INIT_FAILED = 10,
    ERIC_CERT_LOAD_FAILED = 11,
    ERIC_VALIDATION_FAILED = 12,
    ERIC_SUBMIT_FAILED = 13,
    ERIC_TIMEOUT = 14,
    STEUERNUMMER_CONVERT_FAILED = 20,
    UNKNOWN_ERROR = 99
};

// ── Stammdaten ───────────────────────────────────────────────────────────────

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
    std::string telefon;               // optional
    std::string email;                 // optional
    std::string uebermittlungsanlass;  // "erstmalig" | "korrigiert"
    std::string bilanzaufstellung;     // "handelsrecht"
    bool erstjahr = false;             // Steuert Vorjahr-Handling
    std::string ergebnisverwendung;    // "thesaurierung" | "ausschuettung" | "teilausschuettung"
    double ausschuettungsbetrag = 0.0;
    std::string herstellerID;          // ELSTER HerstellerID (required for submission)
    std::string datenLieferant;        // Optional: override for DatenLieferant field
};

// ── BilanzData ───────────────────────────────────────────────────────────────

struct BilanzData {
    // positionId -> Betrag (current year)
    std::map<std::string, double> positionen;
    // positionId -> Vorjahres-Betrag
    std::map<std::string, double> vorjahrPositionen;

    // Summary fields (computed from Excel formulas / sum cells)
    double summeAktiva = 0.0;
    double summePassiva = 0.0;
    double gezeichnetesKapital = 0.0;
    double jahresueberschuss = 0.0;

    // Vorjahr summaries
    double vorjahr_summeAktiva = 0.0;
    double vorjahr_summePassiva = 0.0;
};

// ── GuVData ──────────────────────────────────────────────────────────────────

struct GuVData {
    // positionId -> Betrag (current year)
    std::map<std::string, double> positionen;
    // positionId -> Vorjahres-Betrag
    std::map<std::string, double> vorjahrPositionen;

    double jahresueberschuss = 0.0;
};

// ── Validation ───────────────────────────────────────────────────────────────

struct ValidationMessage {
    std::string rule;       // e.g. "bilanz_ausgeglichen"
    std::string message;    // German user-facing message
};

struct ValidationResult {
    bool isValid = true;
    std::vector<ValidationMessage> errors;
    std::vector<ValidationMessage> warnings;
};

// ── Submit ───────────────────────────────────────────────────────────────────

struct SubmitResult {
    bool success = false;
    std::string transferTicket;
    std::string message;
    ErrorCode errorCode = ErrorCode::UNKNOWN_ERROR;
};

// ── Transfer History Entry ───────────────────────────────────────────────────

struct TransferHistoryEntry {
    std::string timestamp;          // ISO 8601
    std::string transferTicket;
    std::string steuernummer;       // vereinfacht
    std::string wirtschaftsjahr;    // YYYY
    bool testMode = false;
    std::string xbrlFile;
};
