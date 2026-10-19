#include "xbrl_generator.h"

#include <pugixml.hpp>
#include <spdlog/spdlog.h>

#include <chrono>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <regex>
#include <set>
#include <sstream>

namespace {

constexpr double EPSILON = 0.01;

// ponytail: the template carries book values only (no AK, no accumulated
// depreciation), so any Anlagenspiegel built from it mislabels depreciation as
// disposals. Until the template carries gross cost + accDepr, do not assert one.
constexpr bool kEmitAnlagenspiegel = true;  // ERiC: mandatory for fiscal years from 2025

std::string formatDecimal(double value) {
    // ERiC rejects "-0.00" (zahlNullEnthaeltVorzeichen): normalise negative zero.
    if (std::abs(value) < 0.005) value = 0.0;
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << value;
    return oss.str();
}

void createContext(pugi::xml_node& root, const std::string& id,
                   const std::string& startDate, const std::string& endDate,
                   const std::string& steuernummerBund) {
    // Use unprefixed elements (default xbrli namespace)
    auto context = root.append_child("context");
    context.append_attribute("id") = id.c_str();

    auto entity = context.append_child("entity");
    auto identifier = entity.append_child("identifier");
    identifier.append_attribute("scheme") = "http://www.rzf-nrw.de/Steuernummer";
    identifier.text().set(steuernummerBund.c_str());

    auto period = context.append_child("period");
    period.append_child("startDate").text().set(startDate.c_str());
    period.append_child("endDate").text().set(endDate.c_str());
}

void createInstantContext(pugi::xml_node& root, const std::string& id,
                          const std::string& date,
                          const std::string& steuernummerBund) {
    // Use unprefixed elements (default xbrli namespace)
    auto context = root.append_child("context");
    context.append_attribute("id") = id.c_str();

    auto entity = context.append_child("entity");
    auto identifier = entity.append_child("identifier");
    identifier.append_attribute("scheme") = "http://www.rzf-nrw.de/Steuernummer";
    identifier.text().set(steuernummerBund.c_str());

    auto period = context.append_child("period");
    period.append_child("instant").text().set(date.c_str());
}

void addMonetaryFact(pugi::xml_node& root, const std::string& tag,
                     double value, const std::string& contextRef,
                     const std::string& unitRef) {
    auto fact = root.append_child(tag.c_str());
    fact.append_attribute("contextRef") = contextRef.c_str();
    fact.append_attribute("unitRef") = unitRef.c_str();
    fact.append_attribute("decimals") = "2";
    fact.text().set(formatDecimal(value).c_str());
}

void addStringFact(pugi::xml_node& root, const std::string& tag,
                   const std::string& value, const std::string& contextRef) {
    if (value.empty()) return;
    auto fact = root.append_child(tag.c_str());
    fact.append_attribute("contextRef") = contextRef.c_str();
    fact.text().set(value.c_str());
}

} // anonymous namespace

XBRLGenerator::XBRLGenerator(const TaxonomieConfig& config)
    : config_(config) {}

std::string XBRLGenerator::generate(const Stammdaten& stamm,
                                     const BilanzData& bilanz,
                                     const GuVData& guv) const {
    spdlog::debug("Generiere XBRL (Taxonomie v{})", config_.version);

    pugi::xml_document doc;

    // XML declaration
    auto decl = doc.prepend_child(pugi::node_declaration);
    decl.append_attribute("version") = "1.0";
    decl.append_attribute("encoding") = "UTF-8";

    // Root element with namespaces
    // NOTE: ERiC expects xbrli as the DEFAULT namespace (xmlns), not prefixed
    auto root = doc.append_child("xbrl");
    root.append_attribute("xmlns") = "http://www.xbrl.org/2003/instance";
    root.append_attribute("xmlns:link") = config_.linkNamespace.c_str();
    root.append_attribute("xmlns:xlink") = config_.xlinkNamespace.c_str();
    root.append_attribute("xmlns:de-gaap-ci") = config_.namespaceUri.c_str();
    if (!config_.gcdNamespace.empty()) {
        root.append_attribute("xmlns:de-gcd") = config_.gcdNamespace.c_str();
    }
    root.append_attribute("xmlns:iso4217") = "http://www.xbrl.org/2003/iso4217";
    root.append_attribute("xmlns:xbrldi") = "http://xbrl.org/2006/xbrldi";
    root.append_attribute("xmlns:xsi") = "http://www.w3.org/2001/XMLSchema-instance";

    // Schema references — GCD + GAAP entry points (both required per Technischer Leitfaden B.3)
    if (!config_.gcdSchemaRef.empty()) {
        auto gcdRef = root.append_child("link:schemaRef");
        gcdRef.append_attribute("xlink:type") = "simple";
        gcdRef.append_attribute("xlink:href") = config_.gcdSchemaRef.c_str();
    }
    auto schemaRef = root.append_child("link:schemaRef");
    schemaRef.append_attribute("xlink:type") = "simple";
    schemaRef.append_attribute("xlink:href") = config_.schemaRef.c_str();

    // ── Contexts ──

    // Current period (duration) — for GuV and Stammdaten
    createContext(root, "current", stamm.wirtschaftsjahrVon,
                  stamm.wirtschaftsjahrBis, stamm.steuernummerBund);

    // Current instant — for Bilanz positions
    createInstantContext(root, "current_instant", stamm.bilanzstichtag,
                         stamm.steuernummerBund);

    // Prior year period dates (needed for GCD even without prior contexts)
    std::string vorjahrVon = calculateVorjahrVon(stamm);
    std::string vorjahrBis = calculateVorjahrBis(stamm);

    // HAOE (Einheitsbilanz): no prior period contexts.
    // ERiC rejects prior contexts for HAOE: "keine Handelsbilanz vor" (170205105)
    // Prior year data is not expected for Einheitsbilanz.
    // createContext(root, "prior", vorjahrVon, vorjahrBis, stamm.steuernummerBund);
    // createInstantContext(root, "prior_instant", vorjahrBis, stamm.steuernummerBund);

    // Dimensional contexts for full Anlagenspiegel (BAL)
    // Hypercube cube_.nt.ass.gross requires both dimensions:
    //   dim_taxTrans (with domain dim_taxBal) and dim_changes.nt.ass.gross
    // Note: condensed form (cube_.nt.ass.gross_short) is FORBIDDEN by ERiC rule 170405047
    // ERiC requires sub-categories (not just total bs.ass.fixAss), so create contexts for:
    //   intan, tan, fin (the three main fixed asset categories)
    auto createDimContext = [&](const char* ctxId, const char* dimMember) {
        auto ctx = root.append_child("context");
        ctx.append_attribute("id") = ctxId;
        auto entity = ctx.append_child("entity");
        auto id = entity.append_child("identifier");
        id.append_attribute("scheme") = "http://www.rzf-nrw.de/Steuernummer";
        id.text().set(stamm.steuernummerBund.c_str());
        auto period = ctx.append_child("period");
        period.append_child("startDate").text().set(stamm.wirtschaftsjahrVon.c_str());
        period.append_child("endDate").text().set(stamm.wirtschaftsjahrBis.c_str());
        auto scenario = ctx.append_child("scenario");
        auto taxMember = scenario.append_child("xbrldi:explicitMember");
        taxMember.append_attribute("dimension") = "de-gaap-ci:dim_taxTrans";
        taxMember.text().set("de-gaap-ci:dim_comBal");
        auto assMember = scenario.append_child("xbrldi:explicitMember");
        assMember.append_attribute("dimension") = "de-gaap-ci:dim_changes.nt.ass.gross";
        assMember.text().set(dimMember);
    };
    if (kEmitAnlagenspiegel) {
    createDimContext("current_d_fixAss", "de-gaap-ci:bs.ass.fixAss");        // total (parent)
    createDimContext("current_d_fixAss_intan", "de-gaap-ci:bs.ass.fixAss.intan");
    createDimContext("current_d_fixAss_tan", "de-gaap-ci:bs.ass.fixAss.tan");
    createDimContext("current_d_fixAss_fin", "de-gaap-ci:bs.ass.fixAss.fin");
    }

    // ── Units ──
    auto unit = root.append_child("unit");
    unit.append_attribute("id") = "EUR";
    unit.append_child("measure").text().set("iso4217:EUR");

    // Pure unit for dimensionless numeric items (e.g., currentnumber)
    // Uses default namespace (xmlns = xbrli), so no prefix needed
    auto pureUnit = root.append_child("unit");
    pureUnit.append_attribute("id") = "pure";
    pureUnit.append_child("measure").text().set("pure");

    // ── GCD Stammdaten (genInfo elements in de-gcd namespace) ──

    // ── Document identification (genInfo.doc) ──
    {
        // Generation date — current date in ISO format
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::tm tm = *std::localtime(&time);
        std::ostringstream dateOss;
        dateOss << std::put_time(&tm, "%Y-%m-%d");
        addStringFact(root, "de-gcd:genInfo.doc.id.generationDate", dateOss.str(), "current");
    }

    // Document author — tuple with name (required)
    {
        auto author = root.append_child("de-gcd:genInfo.doc.author");
        auto name = author.append_child("de-gcd:genInfo.doc.author.name");
        name.append_attribute("contextRef") = "current";
        name.text().set(stamm.firmenname.c_str());
    }

    // ── Company identification ──
    addStringFact(root, "de-gcd:genInfo.company.id.name", stamm.firmenname, "current");

    // Location — split street into street name + house number
    {
        std::string streetName = stamm.strasse;
        std::string houseNo;
        // Try to extract house number from end of street string (e.g., "Musterstr. 10" → "Musterstr.", "10")
        std::regex houseNoPattern(R"(^(.+?)\s+(\d+\s*\w?)$)");
        std::smatch match;
        if (std::regex_match(stamm.strasse, match, houseNoPattern)) {
            streetName = match[1].str();
            houseNo = match[2].str();
        }
        addStringFact(root, "de-gcd:genInfo.company.id.location.street", streetName, "current");
        addStringFact(root, "de-gcd:genInfo.company.id.location.houseNo", houseNo, "current");
    }
    addStringFact(root, "de-gcd:genInfo.company.id.location.zipCode", stamm.plz, "current");
    addStringFact(root, "de-gcd:genInfo.company.id.location.city", stamm.ort, "current");
    addStringFact(root, "de-gcd:genInfo.company.id.location.country", "Deutschland", "current");
    addStringFact(root, "de-gcd:genInfo.company.id.location.country.isoCode", "DE", "current");

    // Legal status — tuple element with substitution group child.
    // GmbH and UG (haftungsbeschränkt) are both legal-form type "KST"
    // (Technischer Leitfaden zur HGB-Taxonomie, Kap. B.4.1 "Rechtsformen"),
    // so only this enumeration child differs: every other KSt-specific
    // emission in this file (KKE nil, statementType.tax nil, the
    // rechtsformfremd nil-handling, sizeClass KK, incomeClassification)
    // is identical for a UG.
    {
        auto legalStatus = root.append_child("de-gcd:genInfo.company.id.legalStatus");
        const char* legalStatusChild = (stamm.rechtsform == "ug")
            ? "de-gcd:genInfo.company.id.legalStatus.legalStatus.UG"
            : "de-gcd:genInfo.company.id.legalStatus.legalStatus.GMBH";
        auto child = legalStatus.append_child(legalStatusChild);
        child.append_attribute("contextRef") = "current";
    }

    // Company registration (Handelsregister)
    addStringFact(root, "de-gcd:genInfo.company.id.Incorporation.court", stamm.registergericht, "current");
    // Parse HRB number from handelsregisterNr (e.g., "HRB 12345" -> prefix=HRB, number=12345)
    {
        auto hrType = root.append_child("de-gcd:genInfo.company.id.Incorporation.Type");
        auto child = hrType.append_child("de-gcd:genInfo.company.id.Incorporation.Type.Type.HR");
        child.append_attribute("contextRef") = "current";
    }
    // Extract just the number part from "HRB 12345"
    {
        std::string hrNr = stamm.handelsregisterNr;
        std::string prefix, number;
        auto spacePos = hrNr.find(' ');
        if (spacePos != std::string::npos) {
            prefix = hrNr.substr(0, spacePos);
            number = hrNr.substr(spacePos + 1);
        } else {
            number = hrNr;
        }
        addStringFact(root, "de-gcd:genInfo.company.id.Incorporation.prefix", prefix, "current");
        addStringFact(root, "de-gcd:genInfo.company.id.Incorporation.number", number, "current");
    }

    // Company ID numbers — tuple with children in XSD sequence order:
    // HRN?, UID?, ST13?, STID?, STWID?, BF4?, BKN?, BUN?, ...
    {
        auto idNo = root.append_child("de-gcd:genInfo.company.id.idNo");
        // ST13 (13-digit Steuernummer)
        auto st13 = idNo.append_child("de-gcd:genInfo.company.id.idNo.type.companyId.ST13");
        st13.append_attribute("contextRef") = "current";
        st13.text().set(stamm.steuernummerBund.c_str());
        // STID (Steuer-ID) — nil for Kapitalgesellschaften (only for natural persons)
        auto stid = idNo.append_child("de-gcd:genInfo.company.id.idNo.type.companyId.STID");
        stid.append_attribute("contextRef") = "current";
        stid.append_attribute("xsi:nil") = "true";
        // STWID (Wirtschafts-ID) — nil (not yet assigned to most companies)
        auto stwid = idNo.append_child("de-gcd:genInfo.company.id.idNo.type.companyId.STWID");
        stwid.append_attribute("contextRef") = "current";
        stwid.append_attribute("xsi:nil") = "true";
        // BF4 (4-digit Finanzamt number)
        auto bf4 = idNo.append_child("de-gcd:genInfo.company.id.idNo.type.companyId.BF4");
        bf4.append_attribute("contextRef") = "current";
        bf4.text().set(stamm.finanzamtsNummer.c_str());
    }

    // Business description (Geschäftszweck)
    addStringFact(root, "de-gcd:genInfo.company.id.business", "Holding", "current");

    // Not a domestic branch of foreign company — nil child item
    {
        auto dbfc = root.append_child("de-gcd:genInfo.company.id.domesticBranchOfForeignCompany");
        auto child = dbfc.append_child("de-gcd:genInfo.company.id.domesticBranchOfForeignCompany.HGB");
        child.append_attribute("contextRef") = "current";
        child.append_attribute("xsi:nil") = "true";
    }

    // Entity with taxable purpose — normal business (Mussfeld for KSt)
    // .normal declared as nil to satisfy Mussfeld without triggering transmissionOption rule
    {
        auto ewtp = root.append_child("de-gcd:genInfo.company.id.entityWithTaxablePurposeBusiness");
        auto child = ewtp.append_child("de-gcd:genInfo.company.id.entityWithTaxablePurposeBusiness.normal");
        child.append_attribute("contextRef") = "current";
        child.append_attribute("xsi:nil") = "true";
    }

    // optionKStGs1a — rechtsformfremd for KSt, but Mussfeld → nil
    {
        auto opt = root.append_child("de-gcd:genInfo.company.id.optionKStGs1a");
        opt.append_attribute("contextRef") = "current";
        opt.append_attribute("xsi:nil") = "true";
    }

    // Shareholder (Gesellschafter) — tuple with required fields
    {
        auto sh = root.append_child("de-gcd:genInfo.company.id.shareholder");
        // Name (required)
        auto name = sh.append_child("de-gcd:genInfo.company.id.shareholder.name");
        name.append_attribute("contextRef") = "current";
        name.text().set(stamm.geschaeftsfuehrer.c_str());
        // ID
        auto id = sh.append_child("de-gcd:genInfo.company.id.shareholder.id");
        id.append_attribute("contextRef") = "current";
        id.text().set("1");
        // Group — nil child (not applicable for Kapitalgesellschaft)
        auto group = sh.append_child("de-gcd:genInfo.company.id.shareholder.group");
        auto groupChild = group.append_child("de-gcd:genInfo.company.id.shareholder.group.limPartner");
        groupChild.append_attribute("contextRef") = "current";
        groupChild.append_attribute("xsi:nil") = "true";
        // currentnumber — rechtsformfremd for KSt, but Mussfeld → nil (pureIntegerItemType needs unitRef)
        auto cn = sh.append_child("de-gcd:genInfo.company.id.shareholder.currentnumber");
        cn.append_attribute("contextRef") = "current";
        cn.append_attribute("unitRef") = "pure";
        cn.append_attribute("xsi:nil") = "true";
        // Tax number — nil (shareholder's personal Steuernummer)
        auto taxNum = sh.append_child("de-gcd:genInfo.company.id.shareholder.taxnumber");
        taxNum.append_attribute("contextRef") = "current";
        taxNum.append_attribute("xsi:nil") = "true";
        // Tax ID — nil
        auto taxId = sh.append_child("de-gcd:genInfo.company.id.shareholder.taxid");
        taxId.append_attribute("contextRef") = "current";
        taxId.append_attribute("xsi:nil") = "true";
        // WID — nil
        auto wid = sh.append_child("de-gcd:genInfo.company.id.shareholder.WID");
        wid.append_attribute("contextRef") = "current";
        wid.append_attribute("xsi:nil") = "true";
        // Legal status — Kapitalgesellschaft (KOER)
        auto ls = sh.append_child("de-gcd:genInfo.company.id.shareholder.legalStatus");
        auto lsChild = ls.append_child("de-gcd:genInfo.company.id.shareholder.legalStatus.legalStatus.NPP");
        lsChild.append_attribute("contextRef") = "current";
        // ShareDivideKey — 100/100 for sole shareholder
        auto sdk = sh.append_child("de-gcd:genInfo.company.id.shareholder.ShareDivideKey");
        auto num = sdk.append_child("de-gcd:genInfo.company.id.shareholder.ShareDivideKey.numerator");
        num.append_attribute("contextRef") = "current";
        num.text().set("100");
        auto den = sdk.append_child("de-gcd:genInfo.company.id.shareholder.ShareDivideKey.denominator");
        den.append_attribute("contextRef") = "current";
        den.text().set("100");
        // SpecialBalanceRequired — rechtsformfremd for KSt → nil
        auto sbr = sh.append_child("de-gcd:genInfo.company.id.shareholder.SpecialBalanceRequired");
        sbr.append_attribute("contextRef") = "current";
        sbr.append_attribute("xsi:nil") = "true";
        // extensionRequired — rechtsformfremd for KSt → nil
        auto er = sh.append_child("de-gcd:genInfo.company.id.shareholder.extensionRequired");
        er.append_attribute("contextRef") = "current";
        er.append_attribute("xsi:nil") = "true";
    }

    // Size class — KK (Kleinstkapitalgesellschaft) for small Kapitalgesellschaften (§267a HGB)
    {
        auto sc = root.append_child("de-gcd:genInfo.company.id.sizeClass");
        auto child = sc.append_child("de-gcd:genInfo.company.id.sizeClass.sizeClass.KK");
        child.append_attribute("contextRef") = "current";
    }

    // Report period dates
    addStringFact(root, "de-gcd:genInfo.report.period.fiscalYearBegin", stamm.wirtschaftsjahrVon, "current");
    addStringFact(root, "de-gcd:genInfo.report.period.fiscalYearEnd", stamm.wirtschaftsjahrBis, "current");
    addStringFact(root, "de-gcd:genInfo.report.period.balSheetClosingDate", stamm.bilanzstichtag, "current");
    // Report period (usually same as fiscal year for annual reports)
    addStringFact(root, "de-gcd:genInfo.report.period.reportPeriodBegin", stamm.wirtschaftsjahrVon, "current");
    addStringFact(root, "de-gcd:genInfo.report.period.reportPeriodEnd", stamm.wirtschaftsjahrBis, "current");
    // Prior year dates — Mussfeld (required even for HAOE)
    addStringFact(root, "de-gcd:genInfo.report.period.fiscalPreviousYearBegin", vorjahrVon, "current");
    addStringFact(root, "de-gcd:genInfo.report.period.fiscalPreviousYearEnd", vorjahrBis, "current");
    addStringFact(root, "de-gcd:genInfo.report.period.balSheetClosingDatePreviousYear", vorjahrBis, "current");

    // The submitted figures are the HGB financial statements. Tax-only partnership
    // classifications are supplied through the structured reconciliation below.
    {
        auto accStd = root.append_child("de-gcd:genInfo.report.id.accountingStandard");
        auto child = accStd.append_child("de-gcd:genInfo.report.id.accountingStandard.accountingStandard.HGBM");
        child.append_attribute("contextRef") = "current";
    }

    // Statement type — "E" = Einzelabschluss (individual financial statement)
    {
        auto stmtType = root.append_child("de-gcd:genInfo.report.id.statementType");
        auto child = stmtType.append_child("de-gcd:genInfo.report.id.statementType.statementType.E");
        child.append_attribute("contextRef") = "current";
    }

    // Statement type tax — rechtsformfremd for KSt, but Mussfeld → tuple with nil child
    {
        auto stmtTax = root.append_child("de-gcd:genInfo.report.id.statementType.tax");
        auto child = stmtTax.append_child("de-gcd:genInfo.report.id.statementType.tax.statementTypeTax.GHB");
        child.append_attribute("contextRef") = "current";
        child.append_attribute("xsi:nil") = "true";
    }

    // Report status — "E" = endgültig (final)
    {
        auto reportStatus = root.append_child("de-gcd:genInfo.report.id.reportStatus");
        auto child = reportStatus.append_child("de-gcd:genInfo.report.id.reportStatus.reportStatus.E");
        child.append_attribute("contextRef") = "current";
    }

    // Revision status — "E" = erstmalig
    {
        auto revStatus = root.append_child("de-gcd:genInfo.report.id.revisionStatus");
        std::string revChild = (stamm.uebermittlungsanlass == "korrigiert")
            ? "de-gcd:genInfo.report.id.revisionStatus.revisionStatus.K"
            : "de-gcd:genInfo.report.id.revisionStatus.revisionStatus.E";
        auto child = revStatus.append_child(revChild.c_str());
        child.append_attribute("contextRef") = "current";
    }

    // Report elements — declare which parts are included
    // Each reportElement is a separate tuple instance
    auto addReportElement = [&](const char* suffix) {
        auto re = root.append_child("de-gcd:genInfo.report.id.reportElement");
        std::string childName = std::string("de-gcd:genInfo.report.id.reportElement.reportElements.") + suffix;
        auto child = re.append_child(childName.c_str());
        child.append_attribute("contextRef") = "current";
    };
    addReportElement("B");     // Bilanz
    addReportElement("GuV");   // Gewinn- und Verlustrechnung
    addReportElement("EV");    // Ergebnisverwendung (income appropriation)

    // Report elements not included — must be declared as nil (Mussfeld requirement)
    auto addNilReportElement = [&](const char* suffix) {
        auto re = root.append_child("de-gcd:genInfo.report.id.reportElement");
        std::string childName = std::string("de-gcd:genInfo.report.id.reportElement.reportElements.") + suffix;
        auto child = re.append_child(childName.c_str());
        child.append_attribute("contextRef") = "current";
        child.append_attribute("xsi:nil") = "true";
    };
    addNilReportElement("EB");    // Eröffnungsbilanz — not applicable (regular filing)
    addReportElement("STU");      // Strukturierte Überleitungsrechnung
    addReportElement("BVV");      // Beteiligungsübersicht — required when GuV is declared
    if (kEmitAnlagenspiegel) addReportElement("BAL"); else addNilReportElement("BAL");
    addNilReportElement("KKE");   // Kapitalkontenentwicklung — not applicable (KSt)
    addNilReportElement("SGE");   // Sonderbilanzen — not applicable
    addNilReportElement("SGEP");  // Ergänzungsbilanzen — not applicable

    // Allocation: incomeUse appears as extension of GuV (EG layout)
    {
        auto alloc = root.append_child("de-gcd:genInfo.report.id.reportElement.allocation.incomeUse");
        auto layout = alloc.append_child("de-gcd:genInfo.report.id.reportElement.allocation.reportElementsLayout.EG");
        layout.append_attribute("contextRef") = "current";
    }

    // Income statement ends with Bilanzgewinn (required when bs.eqLiab.equity.profitLoss is reported)
    addStringFact(root, "de-gcd:genInfo.report.id.incomeStatementendswithBalProfit", "true", "current");

    // Income classification — trade (gewerblich)
    {
        auto ic = root.append_child("de-gcd:genInfo.company.id.incomeClassification");
        auto child = ic.append_child("de-gcd:genInfo.company.id.incomeClassification.trade");
        child.append_attribute("contextRef") = "current";
    }

    // GuV format — GKV (Gesamtkostenverfahren) — tuple element with substitution group child
    {
        auto isFormat = root.append_child("de-gcd:genInfo.report.id.incomeStatementFormat");
        auto child = isFormat.append_child("de-gcd:genInfo.report.id.incomeStatementFormat.incomeStatementFormat.GKV");
        child.append_attribute("contextRef") = "current";
    }

    // Report type — JA (Jahresabschluss)
    {
        auto rt = root.append_child("de-gcd:genInfo.report.id.reportType");
        auto child = rt.append_child("de-gcd:genInfo.report.id.reportType.reportType.JA");
        child.append_attribute("contextRef") = "current";
    }

    // Consolidation range — EA (Einzelabschluss)
    {
        auto cr = root.append_child("de-gcd:genInfo.report.id.consolidationRange");
        auto child = cr.append_child("de-gcd:genInfo.report.id.consolidationRange.consolidationRange.EA");
        child.append_attribute("contextRef") = "current";
    }

    // Special accounting standard — K = keine besondere Rechnungslegungsvorschrift
    // Not nil — ERiC requires a "werthaltige Angabe" (empty string is the value for enum choice items)
    {
        auto sas = root.append_child("de-gcd:genInfo.report.id.specialAccountingStandard");
        auto child = sas.append_child("de-gcd:genInfo.report.id.specialAccountingStandard.K");
        child.append_attribute("contextRef") = "current";
    }

    // Audit — no audit required for small Kapitalgesellschaften
    {
        auto noAudit = root.append_child("de-gcd:genInfo.report.audit.noAudit");
        auto child = noAudit.append_child("de-gcd:genInfo.report.audit.noAudit.noAuditRequi");
        child.append_attribute("contextRef") = "current";
    }

    // accordingTo — Mussfeld but only werthaltig for Sonder-/Ergänzungsbilanzen → nil children
    {
        auto at = root.append_child("de-gcd:genInfo.report.id.accordingTo");
        auto name = at.append_child("de-gcd:genInfo.report.id.accordingTo.name");
        name.append_attribute("contextRef") = "current";
        name.append_attribute("xsi:nil") = "true";
        // idNo is a sub-tuple — need nil child inside it
        auto idNo = at.append_child("de-gcd:genInfo.report.id.accordingTo.idNo");
        auto st13 = idNo.append_child("de-gcd:genInfo.company.id.idNo.type.companyId.ST13");
        st13.append_attribute("contextRef") = "current";
        st13.append_attribute("xsi:nil") = "true";
        auto yearEnd = at.append_child("de-gcd:genInfo.report.id.accordingTo.yearEnd");
        yearEnd.append_attribute("contextRef") = "current";
        yearEnd.append_attribute("xsi:nil") = "true";
    }

    // ── Build resolved XBRL fact maps ──
    // Convert position IDs to XBRL tags, apply auffangpositionen, then
    // recompute all summary positions to match the calculation linkbase.

    // Helper to map position IDs to XBRL tags
    auto mapPositions = [&](const std::map<std::string, double>& positions,
                            std::map<std::string, double>& facts) {
        for (const auto& [posId, value] : positions) {
            auto it = config_.mapping.find(posId);
            if (it == config_.mapping.end()) {
                if (posId.find("_comment") == std::string::npos) {
                    spdlog::warn("Keine XBRL-Zuordnung für Position: {}", posId);
                }
                continue;
            }
            facts[it->second] = value;
        }
    };

    // Helper to apply auffangpositionen (catch-all logic)
    auto applyAuffang = [&](const std::map<std::string, double>& positions,
                            std::map<std::string, double>& facts) {
        for (const auto& [groupName, auffang] : config_.auffangpositionen) {
            bool allDetailZero = true;
            for (const auto& detailKey : auffang.detailKeys) {
                auto it = positions.find(detailKey);
                if (it != positions.end() && std::abs(it->second) >= EPSILON) {
                    allDetailZero = false;
                    break;
                }
            }
            auto sumIt = positions.find(auffang.summeKey);
            double sumValue = (sumIt != positions.end()) ? sumIt->second : 0.0;
            if (allDetailZero && std::abs(sumValue) >= EPSILON) {
                facts[auffang.auffangTag] = sumValue;  // Override existing 0
                spdlog::debug("Auffangposition {}: {} = {}", groupName, auffang.auffangTag, sumValue);
            }
        }
    };

    // Helper to get a fact value (0.0 if not present)
    auto getFact = [](const std::map<std::string, double>& facts, const std::string& tag) -> double {
        auto it = facts.find(tag);
        return (it != facts.end()) ? it->second : 0.0;
    };

    // Recompute all balance sheet summaries bottom-up to match calculation linkbase.
    // Each summary position must equal the sum of its children.
    // We compute from deepest level up (leaves → intermediates → totals).
    auto recomputeBilanzSummaries = [&](std::map<std::string, double>& f) {
        const std::string P = "de-gaap-ci:";
        auto get = [&](const std::string& suffix) { return getFact(f, P + suffix); };
        auto set = [&](const std::string& suffix, double v) { f[P + suffix] = v; };

        // ── Deepest children first (leaf → intermediate) ──

        // Cash equivalents: cashEquiv = cashEquiv.cash (only child in calc linkbase)
        set("bs.ass.currAss.cashEquiv",
            get("bs.ass.currAss.cashEquiv.cash"));

        // Loans to affiliated: loansToAffil = corporations + partnerships
        set("bs.ass.fixAss.fin.loansToAffil",
            get("bs.ass.fixAss.fin.loansToAffil.corporations") +
            get("bs.ass.fixAss.fin.loansToAffil.partnerships"));

        // Receivables other: sum of all sub-classifications
        // Note: unpaidCapitalLimited/UnlimitedLiablePartner are rechtsformfremd for KSt
        set("bs.ass.currAss.receiv.other",
            get("bs.ass.currAss.receiv.other.assetManaging") +
            get("bs.ass.currAss.receiv.other.commPart") +
            get("bs.ass.currAss.receiv.other.reInsurClaim") +
            get("bs.ass.currAss.receiv.other.shareholders"));

        // Tangible office/business equipment: sum the detail positions.
        set("bs.ass.fixAss.tan.otherEquipm",
            get("bs.ass.fixAss.tan.otherEquipm.factory") +
            get("bs.ass.fixAss.tan.otherEquipm.office") +
            get("bs.ass.fixAss.tan.otherEquipm.gwg") +
            get("bs.ass.fixAss.tan.otherEquipm.gwgsammelposten") +
            get("bs.ass.fixAss.tan.otherEquipm.otherbga") +
            get("bs.ass.fixAss.tan.otherEquipm.misc") +
            get("bs.ass.fixAss.tan.otherEquipm.passengerCars") +
            get("bs.ass.fixAss.tan.otherEquipm.comVehicle") +
            get("bs.ass.fixAss.tan.otherEquipm.otherTransportMeans"));

        // Participation in a GbR: detail -> commercial partnership -> partnership -> total.
        set("bs.ass.fixAss.fin.particip.partnerships.commPart",
            get("bs.ass.fixAss.fin.particip.partnerships.commPart.other") +
            get("bs.ass.fixAss.fin.particip.partnerships.commPart.silentAtyp"));
        set("bs.ass.fixAss.fin.particip.partnerships",
            get("bs.ass.fixAss.fin.particip.partnerships.commPart") +
            get("bs.ass.fixAss.fin.particip.partnerships.assetManaging"));
        set("bs.ass.fixAss.fin.particip",
            get("bs.ass.fixAss.fin.particip.corporations") +
            get("bs.ass.fixAss.fin.particip.partnerships") +
            get("bs.ass.fixAss.fin.particip.assoc") +
            get("bs.ass.fixAss.fin.particip.generalPartners") +
            get("bs.ass.fixAss.fin.particip.jointVent") +
            get("bs.ass.fixAss.fin.particip.silent") +
            get("bs.ass.fixAss.fin.particip.other"));

        // Liabilities other: sum of sub-classifications
        set("bs.eqLiab.liab.other",
            get("bs.eqLiab.liab.other.shareholders"));

        // ── Standard summaries ──

        // Intangible assets
        set("bs.ass.fixAss.intan",
            get("bs.ass.fixAss.intan.selfmade") + get("bs.ass.fixAss.intan.concessionBrands") +
            get("bs.ass.fixAss.intan.goodwill") + get("bs.ass.fixAss.intan.advPaym") +
            get("bs.ass.fixAss.intan.other"));
        // Tangible assets
        set("bs.ass.fixAss.tan",
            get("bs.ass.fixAss.tan.landBuildings") + get("bs.ass.fixAss.tan.machinery") +
            get("bs.ass.fixAss.tan.otherEquipm") + get("bs.ass.fixAss.tan.inConstrAdvPaym") +
            get("bs.ass.fixAss.tan.other"));
        // Financial assets
        set("bs.ass.fixAss.fin",
            get("bs.ass.fixAss.fin.sharesInAffil") + get("bs.ass.fixAss.fin.loansToAffil") +
            get("bs.ass.fixAss.fin.particip") + get("bs.ass.fixAss.fin.loansToParticip") +
            get("bs.ass.fixAss.fin.securities") + get("bs.ass.fixAss.fin.otherLoans") +
            get("bs.ass.fixAss.fin.otherFinAss"));
        // Total fixed assets
        set("bs.ass.fixAss",
            get("bs.ass.fixAss.intan") + get("bs.ass.fixAss.tan") + get("bs.ass.fixAss.fin"));
        // Inventory
        set("bs.ass.currAss.inventory",
            get("bs.ass.currAss.inventory.material") + get("bs.ass.currAss.inventory.inProgress") +
            get("bs.ass.currAss.inventory.finishedAndMerch") + get("bs.ass.currAss.inventory.advPaymPaid"));
        // Receivables
        set("bs.ass.currAss.receiv",
            get("bs.ass.currAss.receiv.trade") + get("bs.ass.currAss.receiv.affil") +
            get("bs.ass.currAss.receiv.particip") + get("bs.ass.currAss.receiv.other") +
            get("bs.ass.currAss.receiv.other.unpaidCapital"));
        // Securities (current)
        set("bs.ass.currAss.securities",
            get("bs.ass.currAss.securities.affil") + get("bs.ass.currAss.securities.other"));
        // Total current assets
        set("bs.ass.currAss",
            get("bs.ass.currAss.inventory") + get("bs.ass.currAss.receiv") +
            get("bs.ass.currAss.securities") + get("bs.ass.currAss.cashEquiv"));
        // Total assets
        set("bs.ass",
            get("bs.ass.fixAss") + get("bs.ass.currAss") + get("bs.ass.prepaidExp") +
            get("bs.ass.defTax") + get("bs.ass.SurplusFromOffsetting") +
            get("bs.ass.deficitNotCoveredByCapital"));

        // Subscribed capital: taxonomy calc = corp(+1) - unpaidCap(-1)
        // corp = the full registered capital (Stammkapital)
        set("bs.eqLiab.equity.subscribed",
            get("bs.eqLiab.equity.subscribed.corp") - get("bs.eqLiab.equity.subscribed.unpaidCap"));
        // Revenue reserves
        set("bs.eqLiab.equity.revenueRes",
            get("bs.eqLiab.equity.revenueRes.legal") + get("bs.eqLiab.equity.revenueRes.sharesParentComp") +
            get("bs.eqLiab.equity.revenueRes.statutory") + get("bs.eqLiab.equity.revenueRes.other"));
        // Total equity
        set("bs.eqLiab.equity",
            get("bs.eqLiab.equity.subscribed") + get("bs.eqLiab.equity.capRes") +
            get("bs.eqLiab.equity.revenueRes") + get("bs.eqLiab.equity.profitLoss") +
            get("bs.eqLiab.equity.netIncome") +
            get("bs.eqLiab.equity.deficitNotCoveredByCapital"));
        // Accruals
        set("bs.eqLiab.accruals",
            get("bs.eqLiab.accruals.pensions") + get("bs.eqLiab.accruals.tax") +
            get("bs.eqLiab.accruals.other"));
        // Liabilities
        set("bs.eqLiab.liab",
            get("bs.eqLiab.liab.securities") + get("bs.eqLiab.liab.bank") +
            get("bs.eqLiab.liab.advPaym") + get("bs.eqLiab.liab.trade") +
            get("bs.eqLiab.liab.notes") + get("bs.eqLiab.liab.assocComp") +
            get("bs.eqLiab.liab.particip") + get("bs.eqLiab.liab.other"));
        // Total equity + liabilities
        set("bs.eqLiab",
            get("bs.eqLiab.equity") + get("bs.eqLiab.accruals") + get("bs.eqLiab.liab") +
            get("bs.eqLiab.defIncome") + get("bs.eqLiab.defTax"));
    };

    // Add GuV intermediate summary elements required by the calculation linkbase
    auto addGuVIntermediates = [&](std::map<std::string, double>& f) {
        const std::string P = "de-gaap-ci:";
        auto get = [&](const std::string& suffix) { return getFact(f, P + suffix); };
        auto set = [&](const std::string& suffix, double v) { f[P + suffix] = v; };

        // materialServices.material = rawMatConsSup + purchased
        set("is.netIncome.regular.operatingTC.grossTradingProfit.materialServices.material",
            get("is.netIncome.regular.operatingTC.grossTradingProfit.materialServices.material.rawMatConsSup") +
            get("is.netIncome.regular.operatingTC.grossTradingProfit.materialServices.material.purchased"));
        // materialServices = material + services
        set("is.netIncome.regular.operatingTC.grossTradingProfit.materialServices",
            get("is.netIncome.regular.operatingTC.grossTradingProfit.materialServices.material") +
            get("is.netIncome.regular.operatingTC.grossTradingProfit.materialServices.services"));
        // totalOutput = netSales + inventoryChange + ownWork
        set("is.netIncome.regular.operatingTC.grossTradingProfit.totalOutput",
            get("is.netIncome.regular.operatingTC.grossTradingProfit.totalOutput.netSales") +
            get("is.netIncome.regular.operatingTC.grossTradingProfit.totalOutput.inventoryChange") +
            get("is.netIncome.regular.operatingTC.grossTradingProfit.totalOutput.ownWork"));
        // grossTradingProfit = totalOutput - materialServices
        set("is.netIncome.regular.operatingTC.grossTradingProfit",
            get("is.netIncome.regular.operatingTC.grossTradingProfit.totalOutput") -
            get("is.netIncome.regular.operatingTC.grossTradingProfit.materialServices"));
        // staff (recompute from children)
        set("is.netIncome.regular.operatingTC.staff",
            get("is.netIncome.regular.operatingTC.staff.salaries") +
            get("is.netIncome.regular.operatingTC.staff.social"));
        // deprAmort.fixAss.tan = sum of children (buildings + lowValueAs + lowValueAsCollItem)
        set("is.netIncome.regular.operatingTC.deprAmort.fixAss.tan",
            get("is.netIncome.regular.operatingTC.deprAmort.fixAss.tan.buildings") +
            get("is.netIncome.regular.operatingTC.deprAmort.fixAss.tan.lowValueAs") +
            get("is.netIncome.regular.operatingTC.deprAmort.fixAss.tan.lowValueAsCollItem") +
            get("is.netIncome.regular.operatingTC.deprAmort.fixAss.tan.other") +
            get("is.netIncome.regular.operatingTC.deprAmort.fixAss.tan.otherMisc"));
        // deprAmort.fixAss = sum of children (goodwill + otherIntan + specific + tan)
        set("is.netIncome.regular.operatingTC.deprAmort.fixAss",
            get("is.netIncome.regular.operatingTC.deprAmort.fixAss.goodwill") +
            get("is.netIncome.regular.operatingTC.deprAmort.fixAss.otherIntan") +
            get("is.netIncome.regular.operatingTC.deprAmort.fixAss.specific") +
            get("is.netIncome.regular.operatingTC.deprAmort.fixAss.tan"));
        // deprAmort (recompute from children)
        set("is.netIncome.regular.operatingTC.deprAmort",
            get("is.netIncome.regular.operatingTC.deprAmort.fixAss") +
            get("is.netIncome.regular.operatingTC.deprAmort.currAss"));
        // otherCost = sum of all sub-positions (miscellaneous is the catch-all)
        set("is.netIncome.regular.operatingTC.otherCost",
            get("is.netIncome.regular.operatingTC.otherCost.miscellaneous") +
            get("is.netIncome.regular.operatingTC.otherCost.vehicles") +
            get("is.netIncome.regular.operatingTC.otherCost.insurance") +
            get("is.netIncome.regular.operatingTC.otherCost.communication") +
            get("is.netIncome.regular.operatingTC.otherCost.energyCost") +
            get("is.netIncome.regular.operatingTC.otherCost.fees") +
            get("is.netIncome.regular.operatingTC.otherCost.freight") +
            get("is.netIncome.regular.operatingTC.otherCost.marketing") +
            get("is.netIncome.regular.operatingTC.otherCost.training") +
            get("is.netIncome.regular.operatingTC.otherCost.provisions") +
            get("is.netIncome.regular.operatingTC.otherCost.legalConsulting") +
            get("is.netIncome.regular.operatingTC.otherCost.concessLicenses") +
            get("is.netIncome.regular.operatingTC.otherCost.otherTaxes") +
            get("is.netIncome.regular.operatingTC.otherCost.businessTravelOwner") +
            get("is.netIncome.regular.operatingTC.otherCost.merger") +
            get("is.netIncome.regular.operatingTC.otherCost.disposalCorp") +
            get("is.netIncome.regular.operatingTC.otherCost.liabilityRemunerationPartners") +
            get("is.netIncome.regular.operatingTC.otherCost.otherRemunerationPartners") +
            get("is.netIncome.regular.operatingTC.otherCost.regAllowance") +
            get("is.netIncome.regular.operatingTC.otherCost.deductValueEStG7g_2") +
            get("is.netIncome.regular.operatingTC.otherCost.disposFixAss") +
            get("is.netIncome.regular.operatingTC.otherCost.addPreTaxRes") +
            get("is.netIncome.regular.operatingTC.otherCost.limitedDeductible") +
            get("is.netIncome.regular.operatingTC.otherCost.leaseFix") +
            get("is.netIncome.regular.operatingTC.otherCost.leaseMovable") +
            get("is.netIncome.regular.operatingTC.otherCost.leasingAll") +
            get("is.netIncome.regular.operatingTC.otherCost.transferValuatonPresentYear"));
        // operatingTC = grossTradingProfit + otherOpRevenue - staff - deprAmort - otherCost
        set("is.netIncome.regular.operatingTC",
            get("is.netIncome.regular.operatingTC.grossTradingProfit") +
            get("is.netIncome.regular.operatingTC.otherOpRevenue") -
            get("is.netIncome.regular.operatingTC.staff") -
            get("is.netIncome.regular.operatingTC.deprAmort") -
            get("is.netIncome.regular.operatingTC.otherCost"));
        set("is.netIncome.regular.fin.netParticipation.earnings",
            get("is.netIncome.regular.fin.netParticipation.earnings.corporations") +
            get("is.netIncome.regular.fin.netParticipation.earnings.partnerships") +
            get("is.netIncome.regular.fin.netParticipation.earnings.silentPart"));
        set("is.netIncome.regular.fin.netParticipation.amortFinanc",
            get("is.netIncome.regular.fin.netParticipation.amortFinanc.partnerships") +
            get("is.netIncome.regular.fin.netParticipation.amortFinanc.financialsExcept") +
            get("is.netIncome.regular.fin.netParticipation.amortFinanc.longTermRecAllowance") +
            get("is.netIncome.regular.fin.netParticipation.amortFinanc.longTermRecLumSumAllow") +
            get("is.netIncome.regular.fin.netParticipation.amortFinanc.secCurrAss"));
        // netParticipation = earnings from participations + earnings from
        // securities - amortisation of financial assets.
        set("is.netIncome.regular.fin.netParticipation",
            get("is.netIncome.regular.fin.netParticipation.earnings") +
            get("is.netIncome.regular.fin.netParticipation.earningSecurities") -
            get("is.netIncome.regular.fin.netParticipation.amortFinanc"));
        // netInterest = income - expenses
        set("is.netIncome.regular.fin.netInterest",
            get("is.netIncome.regular.fin.netInterest.income") -
            get("is.netIncome.regular.fin.netInterest.expenses"));
        // fin = netParticipation + netInterest
        set("is.netIncome.regular.fin",
            get("is.netIncome.regular.fin.netParticipation") +
            get("is.netIncome.regular.fin.netInterest"));
        // eat = operatingTC + fin - tax (note: is.netIncome.regular does NOT exist as an element)
        set("is.netIncome.eat",
            get("is.netIncome.regular.operatingTC") +
            get("is.netIncome.regular.fin") -
            get("is.netIncome.tax"));
        // netIncome = eat - otherTaxes
        set("is.netIncome",
            get("is.netIncome.eat") - get("is.netIncome.otherTaxes"));
    };

    // ── Current year Bilanz ──
    std::map<std::string, double> bilanzFacts;
    mapPositions(bilanz.positionen, bilanzFacts);
    applyAuffang(bilanz.positionen, bilanzFacts);
    // Add subscribed.corp (Stammkapital) for calculation linkbase
    bilanzFacts["de-gaap-ci:bs.eqLiab.equity.subscribed.corp"] = bilanz.gezeichnetesKapital;

    // incomeStatementendswithBalProfit=true: BS shows profitLoss (Bilanzgewinn) instead of
    // separate retainedEarnings (Gewinnvortrag) + netIncome (JÜ).
    // Excel maps: gewinnvortrag → profitLoss, jahresueberschuss → netIncome
    // Combine into profitLoss = Gewinnvortrag + JÜ and erase netIncome (not allowed for KSt).
    double originalGewinnvortrag = getFact(bilanzFacts, "de-gaap-ci:bs.eqLiab.equity.profitLoss");
    {
        double netInc = getFact(bilanzFacts, "de-gaap-ci:bs.eqLiab.equity.netIncome");
        bilanzFacts["de-gaap-ci:bs.eqLiab.equity.profitLoss"] = originalGewinnvortrag + netInc;
        bilanzFacts.erase("de-gaap-ci:bs.eqLiab.equity.netIncome");
        bilanzFacts.erase("de-gaap-ci:bs.eqLiab.equity.retainedEarnings");
        spdlog::debug("Bilanzgewinn: {} (Gewinnvortrag {} + JÜ {})",
                      originalGewinnvortrag + netInc, originalGewinnvortrag, netInc);
    }

    // § 268 Abs. 3 HGB: mirror the asset-side deficit in the equity calculation.
    // This makes reported equity zero while retaining the subscribed capital and loss.
    bilanzFacts["de-gaap-ci:bs.eqLiab.equity.deficitNotCoveredByCapital"] =
        getFact(bilanzFacts, "de-gaap-ci:bs.ass.deficitNotCoveredByCapital");

    recomputeBilanzSummaries(bilanzFacts);

    // HAOE (Einheitsbilanz): remove steuerlich-unzulässige positions.
    // These are HGB-only positions that must not appear in a Steuerbilanz/Einheitsbilanz.
    // Even 0.00 is considered "werthaltig" and triggers ERiC error 170165093.
    static const std::set<std::string> steuerlichUnzulaessig = {
        "de-gaap-ci:bs.ass.defTax",
        "de-gaap-ci:bs.eqLiab.defTax",
        "de-gaap-ci:bs.ass.SurplusFromOffsetting",
        "de-gaap-ci:bs.ass.fixAss.intan.selfmade",
        "de-gaap-ci:bs.ass.fixAss.intan.selfmade.finished",
        "de-gaap-ci:bs.ass.fixAss.intan.selfmade.underConstr",
        "de-gaap-ci:bs.ass.fixAss.intan.development",
        "de-gaap-ci:bs.ass.accountingConvenience",
    };
    for (const auto& tag : steuerlichUnzulaessig) {
        bilanzFacts.erase(tag);
    }

    std::set<std::string> writtenTags;
    for (const auto& [tag, value] : bilanzFacts) {
        addMonetaryFact(root, tag, value, "current_instant", "EUR");
        writtenTags.insert(tag);
    }

    // ── Current year GuV ──
    std::map<std::string, double> guvFacts;
    for (const auto& [posId, value] : guv.positionen) {
        auto it = config_.mapping.find(posId);
        if (it == config_.mapping.end()) {
            if (posId.find("_comment") == std::string::npos) {
                spdlog::warn("Keine XBRL-Zuordnung für GuV-Position: {}", posId);
            }
            continue;
        }
        guvFacts[it->second] = value;
    }
    addGuVIntermediates(guvFacts);
    for (const auto& [tag, value] : guvFacts) {
        addMonetaryFact(root, tag, value, "current", "EUR");
        writtenTags.insert(tag);
    }

    // HAOE (Einheitsbilanz): No prior year data.
    // ERiC rejects prior contexts for HAOE: "keine Handelsbilanz vor" (170205105)
    // Prior year bilanz/GuV facts still computed for BVV but not output.
    std::map<std::string, double> priorBilanzFacts;
    mapPositions(bilanz.vorjahrPositionen, priorBilanzFacts);
    applyAuffang(bilanz.vorjahrPositionen, priorBilanzFacts);
    double vorjahrGezeichnet = getFact(priorBilanzFacts, "de-gaap-ci:bs.eqLiab.equity.subscribed");
    priorBilanzFacts["de-gaap-ci:bs.eqLiab.equity.subscribed.corp"] = vorjahrGezeichnet;
    {
        double priorGewinnvortrag = getFact(priorBilanzFacts, "de-gaap-ci:bs.eqLiab.equity.profitLoss");
        double priorNetInc = getFact(priorBilanzFacts, "de-gaap-ci:bs.eqLiab.equity.netIncome");
        priorBilanzFacts["de-gaap-ci:bs.eqLiab.equity.profitLoss"] = priorGewinnvortrag + priorNetInc;
        priorBilanzFacts.erase("de-gaap-ci:bs.eqLiab.equity.netIncome");
        priorBilanzFacts.erase("de-gaap-ci:bs.eqLiab.equity.retainedEarnings");
    }
    recomputeBilanzSummaries(priorBilanzFacts);

    // ── BVV (Betriebsvermögensvergleich) ──
    // BVV.profitLoss must equal is.netIncome
    // BVV.profitLoss.assetsCurrentYear must equal bs.eqLiab.equity - bs.ass.deficitNotCoveredByCapital
    {
        double bvvProfitLoss = guv.jahresueberschuss;
        double equityCurrent = getFact(bilanzFacts, "de-gaap-ci:bs.eqLiab.equity");
        double deficitCurrent = getFact(bilanzFacts, "de-gaap-ci:bs.ass.deficitNotCoveredByCapital");
        double assetsCurrentYear = equityCurrent - deficitCurrent;
        double equityPrior = getFact(priorBilanzFacts, "de-gaap-ci:bs.eqLiab.equity");
        double deficitPrior = getFact(priorBilanzFacts, "de-gaap-ci:bs.ass.deficitNotCoveredByCapital");
        double assetsPreviousYear = equityPrior - deficitPrior;

        // BVV calculation linkbase weights:
        // profitLoss = assetsCurrentYear(+1) + assetsPreviousYear(-1) + withdrawalDistrib(+1) + contribution(-1) + EStG6b(-1)
        // For Erstjahr: equity change includes initial capital which is NOT profit → needs contribution correction
        double equityChange = assetsCurrentYear - assetsPreviousYear;
        double contribution = equityChange - bvvProfitLoss;  // e.g. 9451.36 - (-3048.64) = 12500
        double withdrawalDistrib = 0.0;

        addMonetaryFact(root, "de-gaap-ci:BVV.profitLoss", bvvProfitLoss, "current", "EUR");
        // BVV positions are periodType="duration" in the taxonomy
        addMonetaryFact(root, "de-gaap-ci:BVV.profitLoss.assetsCurrentYear", assetsCurrentYear, "current", "EUR");
        addMonetaryFact(root, "de-gaap-ci:BVV.profitLoss.assetsPreviousYear", assetsPreviousYear, "current", "EUR");
        addMonetaryFact(root, "de-gaap-ci:BVV.profitLoss.assetsPreviousYear.assets", assetsPreviousYear, "current", "EUR");
        addMonetaryFact(root, "de-gaap-ci:BVV.profitLoss.contribution", contribution, "current", "EUR");
        addMonetaryFact(root, "de-gaap-ci:BVV.profitLoss.withdrawalDistrib", withdrawalDistrib, "current", "EUR");
        // EStG6b is rechtsformfremd for KSt — must be nil
        {
            auto fact = root.append_child("de-gaap-ci:BVV.profitLoss.EStG6b");
            fact.append_attribute("contextRef") = "current";
            fact.append_attribute("unitRef") = "EUR";
            fact.append_attribute("xsi:nil") = "true";
        }

        writtenTags.insert("de-gaap-ci:BVV.profitLoss");
        writtenTags.insert("de-gaap-ci:BVV.profitLoss.assetsCurrentYear");
        writtenTags.insert("de-gaap-ci:BVV.profitLoss.assetsPreviousYear");
        writtenTags.insert("de-gaap-ci:BVV.profitLoss.assetsPreviousYear.assets");
        writtenTags.insert("de-gaap-ci:BVV.profitLoss.contribution");
        writtenTags.insert("de-gaap-ci:BVV.profitLoss.withdrawalDistrib");
        writtenTags.insert("de-gaap-ci:BVV.profitLoss.EStG6b");

        spdlog::debug("BVV: profitLoss={}, assetsCurrentYear={}, assetsPreviousYear={}, contribution={}",
                      bvvProfitLoss, assetsCurrentYear, assetsPreviousYear, contribution);
    }

    // ── Ergebnisverwendung (incomeUse) — since we declare EV ──
    {
        double netIncome = guv.jahresueberschuss;
        // Gewinnvortrag/Verlustvortrag from previous year (saved before profitLoss combination)
        double gewinnvortrag = originalGewinnvortrag;
        double gainLoss = netIncome + gewinnvortrag;

        // Determine dividend/retained split based on ergebnisverwendung
        double dividends = 0.0;
        double retainedEarnings = gainLoss;
        if (stamm.ergebnisverwendung != "thesaurierung") {
            dividends = stamm.ausschuettungsbetrag;
            retainedEarnings = gainLoss - dividends;
        }

        // incomeUse facts — duration context (periodType="duration" in XSD)
        addMonetaryFact(root, "de-gaap-ci:incomeUse.gainLoss.netIncome", netIncome, "current", "EUR");
        addMonetaryFact(root, "de-gaap-ci:incomeUse.gainLoss.retainedEarningsPrevYear", gewinnvortrag, "current", "EUR");
        addMonetaryFact(root, "de-gaap-ci:incomeUse.gainLoss.accumLossPrevYear", 0.0, "current", "EUR");
        addMonetaryFact(root, "de-gaap-ci:incomeUse.gainLoss.dividensPlanned", dividends, "current", "EUR");
        addMonetaryFact(root, "de-gaap-ci:incomeUse.gainLoss", gainLoss, "current", "EUR");
        addMonetaryFact(root, "de-gaap-ci:incomeUse.retainedEarnings", retainedEarnings, "current", "EUR");
        addMonetaryFact(root, "de-gaap-ci:incomeUse.accumLoss", 0.0, "current", "EUR");
        addMonetaryFact(root, "de-gaap-ci:incomeUse.dividends", dividends, "current", "EUR");

        // Mark written tags
        writtenTags.insert("de-gaap-ci:incomeUse.gainLoss.netIncome");
        writtenTags.insert("de-gaap-ci:incomeUse.gainLoss.retainedEarningsPrevYear");
        writtenTags.insert("de-gaap-ci:incomeUse.gainLoss.accumLossPrevYear");
        writtenTags.insert("de-gaap-ci:incomeUse.gainLoss.dividensPlanned");
        writtenTags.insert("de-gaap-ci:incomeUse.gainLoss");
        writtenTags.insert("de-gaap-ci:incomeUse.retainedEarnings");
        writtenTags.insert("de-gaap-ci:incomeUse.accumLoss");
        writtenTags.insert("de-gaap-ci:incomeUse.dividends");
    }

    // STU: Reclassify the HGB partnership positions to the tax-only balance-sheet
    // classification and neutralise tax-prohibited GuV detail positions. The
    // collective GuV item preserves the unchanged taxable net result.
    {
        auto transfer = root.append_child("de-gaap-ci:hbst.transfer");
        auto kind = transfer.append_child("de-gaap-ci:hbst.transfer.kind");
        auto kindValue = kind.append_child("de-gaap-ci:hbst.transfer.kind.reclassificationChangeValue");
        kindValue.append_attribute("contextRef") = "current";

        auto addGuVTransfer = [&](const char* position, double change) {
            auto tuple = transfer.append_child("de-gaap-ci:hbst.transfer.isChangeNetIncome");
            auto name = tuple.append_child("de-gaap-ci:hbst.transfer.isChangeNetIncome.name");
            name.append_attribute("contextRef") = "current";
            name.text().set(position);
            auto value = tuple.append_child("de-gaap-ci:hbst.transfer.isChangeNetIncome.changeValueActualPeriod");
            value.append_attribute("contextRef") = "current";
            value.append_attribute("unitRef") = "EUR";
            value.append_attribute("decimals") = "2";
            value.text().set(formatDecimal(change).c_str());
        };
        auto addAssetTransfer = [&](const char* position, double change) {
            auto tuple = transfer.append_child("de-gaap-ci:hbst.transfer.bsAss");
            auto name = tuple.append_child("de-gaap-ci:hbst.transfer.bsAss.name");
            name.append_attribute("contextRef") = "current";
            name.text().set(position);
            auto actual = tuple.append_child("de-gaap-ci:hbst.transfer.bsAss.changeValueActualPeriod");
            actual.append_attribute("contextRef") = "current";
            actual.append_attribute("unitRef") = "EUR";
            actual.append_attribute("decimals") = "2";
            actual.text().set(formatDecimal(change).c_str());
            auto previous = tuple.append_child("de-gaap-ci:hbst.transfer.bsAss.changeValuePreviousPeriod");
            previous.append_attribute("contextRef") = "current";
            previous.append_attribute("unitRef") = "EUR";
            previous.append_attribute("decimals") = "2";
            previous.text().set("0.00");
        };

        const double partnershipProfit =
            getFact(guvFacts, "de-gaap-ci:is.netIncome.regular.fin.netParticipation.earnings.partnerships");
        const double partnershipLoss =
            getFact(guvFacts, "de-gaap-ci:is.netIncome.regular.fin.netParticipation.amortFinanc.partnerships");
        const double partnershipNet = partnershipProfit - partnershipLoss;

        const double partnershipBookValue =
            getFact(bilanzFacts, "de-gaap-ci:bs.ass.fixAss.fin.particip.other");
        addAssetTransfer("de-gaap-ci:bs.ass.fixAss.fin.particip.other", -partnershipBookValue);
        addAssetTransfer("de-gaap-ci:bs.ass.fixAss.fin.particip.partnerships.commPart.other", partnershipBookValue);
        addAssetTransfer("de-gaap-ci:bs.ass.fixAss.fin.particip.partnerships.commPart", partnershipBookValue);
        addAssetTransfer("de-gaap-ci:bs.ass.fixAss.fin.particip.partnerships", partnershipBookValue);

        addGuVTransfer("de-gaap-ci:is.netIncome.regular.fin.netParticipation.earnings.partnerships", -partnershipProfit);
        addGuVTransfer("de-gaap-ci:is.netIncome.regular.fin.netParticipation.earnings", -partnershipProfit);
        addGuVTransfer("de-gaap-ci:is.netIncome.regular.fin.netParticipation.amortFinanc.partnerships", -partnershipLoss);
        addGuVTransfer("de-gaap-ci:is.netIncome.regular.fin.netParticipation.amortFinanc", -partnershipLoss);
        addGuVTransfer("de-gaap-ci:is.netIncome.regular.fin.netParticipation", -partnershipNet);
        addGuVTransfer("de-gaap-ci:is.netIncome.regular.fin", -partnershipNet);
        addGuVTransfer("de-gaap-ci:is.netIncome.eat", -partnershipNet);
        addGuVTransfer("de-gaap-ci:is.netIncome.collItemChangeProfitHbst", partnershipNet);
    }

    if (kEmitAnlagenspiegel) // ── BAL: Anlagenspiegel (full/brutto) ──
    // Dimensional table using dim_changes.nt.ass.gross with sub-category rows.
    // Note: condensed form (cube_.nt.ass.gross_short) is forbidden by ERiC rule 170405047.
    // ERiC requires reporting at sub-category level (intan, tan, fin), not just total.
    {
        // Helper to write all Anlagenspiegel columns for a given sub-category
        // ponytail: the template carries book values only (Nettomethode), so
        // gross cost = opening book value + additions and accDepr.beginning = 0.
        // A decrease is depreciation as far as the GuV reports depreciation for
        // that asset class (deprAmort for intan/tan, amortFinanc for fin) and a
        // disposal only beyond that — never the other way round.
        const double deprBudgetFix =
            getFact(guvFacts, "de-gaap-ci:is.netIncome.regular.operatingTC.deprAmort") -
            getFact(guvFacts, "de-gaap-ci:is.netIncome.regular.operatingTC.deprAmort.currAss");
        const double deprBudgetFin =
            getFact(guvFacts, "de-gaap-ci:is.netIncome.regular.fin.netParticipation.amortFinanc");
        auto writeAnlageColumns = [&](const char* ctxId, double currentVal, double priorVal) {
            double beginning = stamm.erstjahr ? 0.0 : priorVal;
            double additions = currentVal - beginning;
            if (additions < 0) additions = 0.0;
            double decrease = (currentVal < beginning) ? (beginning - currentVal) : 0.0;
            const std::string ctx(ctxId);
            double budget = (ctx.find("_fin") != std::string::npos) ? deprBudgetFin
                          : (ctx == "current_d_fixAss")              ? deprBudgetFix + deprBudgetFin
                                                                     : deprBudgetFix;
            double depreciation = std::min(decrease, std::max(budget, 0.0));
            double releases = decrease - depreciation;
            double grossEnd = beginning + additions - releases;

            auto addDimFact = [&](const char* element, double value) {
                auto fact = root.append_child(element);
                fact.append_attribute("contextRef") = ctxId;
                fact.append_attribute("unitRef") = "EUR";
                fact.append_attribute("decimals") = "2";
                fact.text().set(formatDecimal(value).c_str());
            };

            // Gross cost columns
            addDimFact("de-gaap-ci:grossCost.beginning", beginning);
            addDimFact("de-gaap-ci:gross.addition", additions);
            addDimFact("de-gaap-ci:gross.release", releases);
            addDimFact("de-gaap-ci:gross.movements", 0.0);
            addDimFact("de-gaap-ci:gross.diff", 0.0);
            addDimFact("de-gaap-ci:gross", grossEnd);  // ending gross cost
            addDimFact("de-gaap-ci:revaluation", 0.0);

            // Accumulated depreciation columns
            addDimFact("de-gaap-ci:accDepr.beginning", 0.0);
            addDimFact("de-gaap-ci:accDepr.addition", 0.0);  // calc: accDepr = beginning + addition + DeprPeriod
            addDimFact("de-gaap-ci:accDepr.release", 0.0);
            addDimFact("de-gaap-ci:accDepr.movements", 0.0);
            addDimFact("de-gaap-ci:accDepr.diff", 0.0);
            addDimFact("de-gaap-ci:accDepr", depreciation);  // ending accumulated depreciation
            addDimFact("de-gaap-ci:accDepr.DeprPeriod", depreciation);
            addDimFact("de-gaap-ci:accDepr.DeprPeriod.regular", depreciation);
            addDimFact("de-gaap-ci:accDepr.DeprPeriod.extraordinary", 0.0);
            addDimFact("de-gaap-ci:accDepr.DeprPeriod.impairment", 0.0);
            addDimFact("de-gaap-ci:accDepr.DeprPeriod.EStG7g_2", 0.0);
            addDimFact("de-gaap-ci:accDepr.DeprPeriod.EStG6b_3", 0.0);

            // Totals
            addDimFact("de-gaap-ci:all", currentVal);
            addDimFact("de-gaap-ci:all_Prev_period", priorVal);
        };

        // Report total (parent) row first
        double fixAssTotal = getFact(bilanzFacts, "de-gaap-ci:bs.ass.fixAss");
        double fixAssTotalPrior = getFact(priorBilanzFacts, "de-gaap-ci:bs.ass.fixAss");
        writeAnlageColumns("current_d_fixAss", fixAssTotal, fixAssTotalPrior);

        // Report for each main sub-category
        struct AnlageRow {
            std::string ctxId;
            std::string tag;
        };
        std::vector<AnlageRow> rows = {
            {"current_d_fixAss_intan", "de-gaap-ci:bs.ass.fixAss.intan"},
            {"current_d_fixAss_tan", "de-gaap-ci:bs.ass.fixAss.tan"},
            {"current_d_fixAss_fin", "de-gaap-ci:bs.ass.fixAss.fin"},
        };

        for (const auto& row : rows) {
            double cur = getFact(bilanzFacts, row.tag);
            double pri = getFact(priorBilanzFacts, row.tag);
            writeAnlageColumns(row.ctxId.c_str(), cur, pri);
        }

        // ERiC also requires sub-sub-categories for non-zero items.
        // Check balance sheet facts for specific fixAss detail positions and add them.
        // This satisfies "Summenmussfeldposition needs werthaltig Unterposition" rule.
        std::vector<std::pair<std::string, std::string>> detailPositions = {
            // Tangible sub-types
            {"de-gaap-ci:bs.ass.fixAss.tan.landBuildings", "fixAss_tan_land"},
            {"de-gaap-ci:bs.ass.fixAss.tan.machinery", "fixAss_tan_mach"},
            {"de-gaap-ci:bs.ass.fixAss.tan.otherEquipm", "fixAss_tan_equip"},
            {"de-gaap-ci:bs.ass.fixAss.tan.otherEquipm.office", "fixAss_tan_equip_office"},
            {"de-gaap-ci:bs.ass.fixAss.tan.inConstrAdvPaym", "fixAss_tan_constr"},
            {"de-gaap-ci:bs.ass.fixAss.tan.other", "fixAss_tan_other"},
            // Financial sub-types
            {"de-gaap-ci:bs.ass.fixAss.fin.sharesInAffil", "fixAss_fin_shares"},
            {"de-gaap-ci:bs.ass.fixAss.fin.loansToAffil", "fixAss_fin_loans"},
            {"de-gaap-ci:bs.ass.fixAss.fin.particip", "fixAss_fin_partic"},
            {"de-gaap-ci:bs.ass.fixAss.fin.particip.other", "fixAss_fin_partic_other"},
            {"de-gaap-ci:bs.ass.fixAss.fin.loansToParticip", "fixAss_fin_loansp"},
            {"de-gaap-ci:bs.ass.fixAss.fin.securities", "fixAss_fin_sec"},
            {"de-gaap-ci:bs.ass.fixAss.fin.otherLoans", "fixAss_fin_othloans"},
            {"de-gaap-ci:bs.ass.fixAss.fin.otherFinAss", "fixAss_fin_other"},
            // Sub-sub-types for sum positions (loansToAffil, sharesInAffil, etc. are themselves sum positions)
            {"de-gaap-ci:bs.ass.fixAss.fin.loansToAffil.corporations", "fixAss_fin_loans_corp"},
            {"de-gaap-ci:bs.ass.fixAss.fin.loansToAffil.partnerships", "fixAss_fin_loans_part"},
            {"de-gaap-ci:bs.ass.fixAss.fin.loansToAffil.soleProprietor", "fixAss_fin_loans_sole"},
            {"de-gaap-ci:bs.ass.fixAss.fin.loansToAffil.other", "fixAss_fin_loans_oth"},
            {"de-gaap-ci:bs.ass.fixAss.fin.sharesInAffil.corporations", "fixAss_fin_shares_corp"},
            {"de-gaap-ci:bs.ass.fixAss.fin.sharesInAffil.partnerships", "fixAss_fin_shares_part"},
            {"de-gaap-ci:bs.ass.fixAss.fin.particip.corporations", "fixAss_fin_partic_corp"},
            {"de-gaap-ci:bs.ass.fixAss.fin.particip.partnerships", "fixAss_fin_partic_part"},
            {"de-gaap-ci:bs.ass.fixAss.fin.loansToParticip.corporations", "fixAss_fin_loansp_corp"},
            {"de-gaap-ci:bs.ass.fixAss.fin.loansToParticip.partnerships", "fixAss_fin_loansp_part"},
            // Intangible sub-types
            {"de-gaap-ci:bs.ass.fixAss.intan.selfmade", "fixAss_int_self"},
            {"de-gaap-ci:bs.ass.fixAss.intan.concessionBrands", "fixAss_int_conc"},
            {"de-gaap-ci:bs.ass.fixAss.intan.goodwill", "fixAss_int_gw"},
            {"de-gaap-ci:bs.ass.fixAss.intan.advPaym", "fixAss_int_adv"},
            {"de-gaap-ci:bs.ass.fixAss.intan.other", "fixAss_int_other"},
        };

        for (const auto& [tag, suffix] : detailPositions) {
            double cur = getFact(bilanzFacts, tag);
            double pri = getFact(priorBilanzFacts, tag);
            if (std::abs(cur) >= EPSILON || std::abs(pri) >= EPSILON) {
                std::string ctxId = "current_d_" + suffix;
                createDimContext(ctxId.c_str(), tag.c_str());
                writeAnlageColumns(ctxId.c_str(), cur, pri);
                spdlog::debug("BAL detail: {} = {}", tag, cur);
            }
        }

        spdlog::debug("BAL: Anlagenspiegel total fixAss={}", fixAssTotal);
    }

    // ── Mussfelder: ensure mandatory fields are present ──
    for (const auto& mussfeld : config_.mussfelder) {
        if (writtenTags.find(mussfeld) == writtenTags.end()) {
            // Positions belonging to nil report elements or inapplicable sections must be xsi:nil
            // (0.00 is considered "werthaltig" and triggers validation errors)
            // :fpl matches both fpl and fplgm (both are nil sections)
            // operatingCOGS is UKV — must be nil since we declared GKV
            bool isNilSection = (mussfeld.find(":kke") != std::string::npos) ||
                                (mussfeld.find(":fpl") != std::string::npos) ||
                                (mussfeld.find(".operatingCOGS") != std::string::npos);
            if (isNilSection) {
                bool isInstant = (mussfeld.find(":kke") != std::string::npos);
                const char* ctx = isInstant ? "current_instant" : "current";
                auto fact = root.append_child(mussfeld.c_str());
                fact.append_attribute("contextRef") = ctx;
                // kke is stringItemType (no unitRef); BVV/fpl/fplgm are monetary (need unitRef but NOT decimals for nil)
                bool isString = (mussfeld.find(":kke") != std::string::npos);
                if (!isString) {
                    fact.append_attribute("unitRef") = "EUR";
                }
                fact.append_attribute("xsi:nil") = "true";
                spdlog::debug("Mussfeld nil befüllt: {}", mussfeld);
                continue;
            }
            // Positions not allowed with incomeStatementendswithBalProfit=true (KSt):
            // netIncome and retainedEarnings must be nil on BS (absorbed into profitLoss)
            bool isNotAllowedWithBalProfit =
                (mussfeld.find("equity.netIncome") != std::string::npos) ||
                (mussfeld.find("equity.retainedEarnings") != std::string::npos);
            if (isNotAllowedWithBalProfit) {
                auto fact = root.append_child(mussfeld.c_str());
                fact.append_attribute("contextRef") = "current_instant";
                fact.append_attribute("unitRef") = "EUR";
                fact.append_attribute("xsi:nil") = "true";
                spdlog::debug("Mussfeld nil (not allowed with BalProfit): {}", mussfeld);
                continue;
            }
            // Rechtsformfremd positions for KSt: must be reported but as nil, not 0.00
            // Covers all Personengesellschaft-specific positions
            bool isRechtsformfremd =
                (mussfeld.find("partner") != std::string::npos) ||  // partner, partners
                (mussfeld.find("Partner") != std::string::npos) ||  // *Partners*, Partnership, LiablePartner
                (mussfeld.find("limitedLiable") != std::string::npos) ||
                (mussfeld.find("colEntity") != std::string::npos) ||
                (mussfeld.find("privateAccountSP") != std::string::npos) ||
                (mussfeld.find("costAllocItem") != std::string::npos) ||
                (mussfeld.find("EStG6b") != std::string::npos) ||
                (mussfeld.find("EStGs6b") != std::string::npos) ||
                (mussfeld.find("EStG4_4a") != std::string::npos) ||
                (mussfeld.find("EStG15") != std::string::npos) ||
                (mussfeld.find("businessTravelOwner") != std::string::npos) ||
                (mussfeld.find("ownConsumption") != std::string::npos);
            if (isRechtsformfremd) {
                bool isInstant = (mussfeld.find("bs.") != std::string::npos);
                const char* ctx = isInstant ? "current_instant" : "current";
                auto fact = root.append_child(mussfeld.c_str());
                fact.append_attribute("contextRef") = ctx;
                fact.append_attribute("unitRef") = "EUR";
                fact.append_attribute("xsi:nil") = "true";
                spdlog::debug("Mussfeld nil (rechtsformfremd): {}", mussfeld);
                continue;
            }
            // Determine correct context based on XBRL periodType:
            // - bs.* = instant (balance sheet date)
            // - is.*, incomeUse.* = duration (reporting period)
            bool isInstant = (mussfeld.find("bs.") != std::string::npos);
            const char* ctx = isInstant ? "current_instant" : "current";
            addMonetaryFact(root, mussfeld, 0.0, ctx, "EUR");
            spdlog::debug("Mussfeld mit 0 befüllt: {}", mussfeld);
        }
    }

    // ── Serialize to string ──
    std::ostringstream oss;
    doc.save(oss, "  ");
    std::string result = oss.str();

    spdlog::info("XBRL generiert: {} Bytes, {} Positionen", result.size(), writtenTags.size());
    return result;
}

std::string XBRLGenerator::calculateVorjahrVon(const Stammdaten& stamm) const {
    // Prior year start: one year before wirtschaftsjahrVon
    if (stamm.wirtschaftsjahrVon.size() < 10) return "";

    int year = std::stoi(stamm.wirtschaftsjahrVon.substr(0, 4));
    return std::to_string(year - 1) + stamm.wirtschaftsjahrVon.substr(4);
}

std::string XBRLGenerator::calculateVorjahrBis(const Stammdaten& stamm) const {
    // Prior year end: one year before wirtschaftsjahrBis
    if (stamm.wirtschaftsjahrBis.size() < 10) return "";

    int year = std::stoi(stamm.wirtschaftsjahrBis.substr(0, 4));
    return std::to_string(year - 1) + stamm.wirtschaftsjahrBis.substr(4);
}
