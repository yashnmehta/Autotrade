/**
 * @file master_file_analyzer.cpp
 * @brief Comprehensive analyzer for master_contracts_latest.txt
 * 
 * This utility parses the combined master file and generates a detailed
 * analysis report covering:
 * - Data format variations across exchanges (NSEFO, NSECM, BSEFO, BSECM)
 * - Expiry date formats and inconsistencies
 * - Asset token patterns and extraction logic
 * - Strike price ranges and precision
 * - Option type encoding
 * - Series distribution
 * - Exchange-specific quirks and anomalies
 * 
 * Usage: ./master_file_analyzer <path_to_master_contracts_latest.txt>
 */

#include <QCoreApplication>
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QDateTime>
#include <QMap>
#include <QSet>
#include <QRegularExpression>
#include <algorithm>
#include <cmath>

// ============================================================================
// Data Structures
// ============================================================================

struct MasterRecord {
    // Raw fields from file (20 pipe-separated fields)
    QString exchangeSegment;        // Field 0: NSEFO, NSECM, BSEFO, BSECM
    QString exchangeToken;          // Field 1: Token ID
    QString instrumentType;         // Field 2: 1=Future, 2=Option, etc.
    QString name;                   // Field 3: Underlying symbol
    QString tradingSymbol;          // Field 4: Full trading symbol
    QString series;                 // Field 5: FUTSTK, OPTSTK, FUTIDX, OPTIDX, EQ, INDEX
    QString description;            // Field 6: Display description
    QString compoundToken;          // Field 7: Composite token with prefix
    QString freezeQty;              // Field 8
    QString tickSize;               // Field 9
    QString lotSize;                // Field 10
    QString multiplier;             // Field 11
    QString underlyingInstrumentId; // Field 12
    QString underlyingIndexName;    // Field 13: Empty for stocks, index name for options
    QString assetToken;             // Field 14: Critical for Greeks calculation
    QString strikePriceRaw;         // Field 15: Empty for futures
    QString expiryDate;             // Field 16: ISO format or empty
    QString displayName;            // Field 17
    QString instrumentId;           // Field 18
    QString priceBandLow;           // Field 19
    QString priceBandHigh;          // Field 20
    QString optionType;             // Field 21: 3=CE, 4=PE
    QString displaySymbol;          // Field 22
    QString canBeScripless;         // Field 23
    QString canBeOrdered;           // Field 24
    QString actualSymbol;           // Field 25
    
    // Parsed/derived fields
    int64_t tokenInt = 0;
    int64_t assetTokenInt = 0;
    double strikePrice = 0.0;
    QDateTime expiryDateTime;
    bool hasExpiry = false;
    
    int lineNumber = 0;
    bool parseError = false;
    QString errorMessage;
};

struct AnalysisReport {
    // Overall statistics
    int totalRecords = 0;
    int parseErrors = 0;
    
    // Exchange breakdown
    QMap<QString, int> exchangeSegmentCount;        // NSEFO: 85000, etc.
    QMap<QString, int> seriesCount;                 // OPTSTK: 75000, etc.
    QMap<QString, int> instrumentTypeCount;         // 1: 5000, 2: 80000
    
    // Token analysis
    int64_t minToken = INT64_MAX;
    int64_t maxToken = 0;
    QMap<QString, QPair<int64_t, int64_t>> tokenRangeByExchange;
    QSet<int64_t> duplicateTokens;
    
    // Asset token analysis (CRITICAL for Greeks)
    int recordsWithAssetToken = 0;
    int recordsWithoutAssetToken = 0;
    int recordsWithNegativeOne = 0;                 // asset token = -1
    QMap<QString, int> assetTokenPatterns;          // "composite": 5000, "direct": 3000
    QMap<int64_t, int> assetTokenFrequency;         // How many times each asset token appears
    
    // Expiry date analysis
    int recordsWithExpiry = 0;
    int recordsWithoutExpiry = 0;
    QMap<QString, int> expiryDateFormats;           // "ISO": 80000, "Empty": 2000
    QSet<QString> uniqueExpiries;
    QMap<QString, int> expiryParseErrors;
    
    // Strike price analysis
    int recordsWithStrike = 0;
    int recordsWithoutStrike = 0;
    double minStrike = 1e9;
    double maxStrike = 0;
    QMap<double, int> strikeFrequency;              // Strike distribution
    QMap<QString, QMap<double, int>> strikePrecisionBySymbol; // NIFTY: {0.05: 500}
    
    // Option type analysis
    QMap<QString, int> optionTypeCount;             // 3: 40000, 4: 40000
    QMap<QString, int> ceVsPeBySymbol;              // NIFTY_CE: 250, NIFTY_PE: 250
    
    // Symbol analysis
    QSet<QString> uniqueSymbols;
    QMap<QString, int> symbolFrequency;
    QMap<QString, int> symbolContractCount;         // How many contracts per symbol
    
    // Data quality issues
    QStringList validationErrors;
    QMap<QString, int> errorCategories;
    
    // Exchange-specific quirks
    struct ExchangeQuirks {
        bool hasCompositeTokens = false;
        bool hasIndexUnderlying = false;
        bool hasFractionalStrikes = false;
        QString assetTokenFormat;
        QString expiryFormat;
        QStringList notes;
    };
    QMap<QString, ExchangeQuirks> quirks;
};

// ============================================================================
// Parser Functions
// ============================================================================

MasterRecord parseLine(const QString& line, int lineNumber) {
    MasterRecord record;
    record.lineNumber = lineNumber;
    
    QStringList fields = line.split('|');
    
    if (fields.size() < 20) {
        record.parseError = true;
        record.errorMessage = QString("Insufficient fields: %1 (expected >= 20)").arg(fields.size());
        return record;
    }
    
    // Parse all fields
    record.exchangeSegment = fields[0].trimmed();
    record.exchangeToken = fields[1].trimmed();
    record.instrumentType = fields[2].trimmed();
    record.name = fields[3].trimmed();
    record.tradingSymbol = fields[4].trimmed();
    record.series = fields[5].trimmed();
    record.description = fields[6].trimmed();
    record.compoundToken = fields[7].trimmed();
    record.freezeQty = fields[8].trimmed();
    record.tickSize = fields[9].trimmed();
    record.lotSize = fields[10].trimmed();
    record.multiplier = fields[11].trimmed();
    record.underlyingInstrumentId = fields[12].trimmed();
    record.underlyingIndexName = fields[13].trimmed();
    record.assetToken = fields[14].trimmed();
    record.strikePriceRaw = fields[15].trimmed();
    record.expiryDate = fields[16].trimmed();
    record.displayName = fields[17].trimmed();
    record.instrumentId = fields[18].trimmed();
    record.priceBandLow = fields[19].trimmed();
    
    if (fields.size() > 20) record.priceBandHigh = fields[20].trimmed();
    if (fields.size() > 21) record.optionType = fields[21].trimmed();
    if (fields.size() > 22) record.displaySymbol = fields[22].trimmed();
    if (fields.size() > 23) record.canBeScripless = fields[23].trimmed();
    if (fields.size() > 24) record.canBeOrdered = fields[24].trimmed();
    if (fields.size() > 25) record.actualSymbol = fields[25].trimmed();
    
    // Parse token
    bool ok;
    record.tokenInt = record.exchangeToken.toLongLong(&ok);
    if (!ok) {
        record.parseError = true;
        record.errorMessage = "Invalid token format: " + record.exchangeToken;
    }
    
    // Parse asset token
    if (!record.assetToken.isEmpty()) {
        record.assetTokenInt = record.assetToken.toLongLong(&ok);
        if (!ok && record.assetToken != "-1") {
            record.parseError = true;
            record.errorMessage = "Invalid asset token format: " + record.assetToken;
        }
    }
    
    // Parse strike price
    if (!record.strikePriceRaw.isEmpty()) {
        record.strikePrice = record.strikePriceRaw.toDouble(&ok);
        if (!ok) {
            record.parseError = true;
            record.errorMessage = "Invalid strike price: " + record.strikePriceRaw;
        }
    }
    
    // Parse expiry date
    if (!record.expiryDate.isEmpty()) {
        record.hasExpiry = true;
        
        // Try ISO format: 2026-01-27T14:30:00
        record.expiryDateTime = QDateTime::fromString(record.expiryDate, Qt::ISODate);
        
        if (!record.expiryDateTime.isValid()) {
            // Try other formats if needed
            record.parseError = true;
            record.errorMessage = "Invalid expiry date format: " + record.expiryDate;
        }
    }
    
    return record;
}

// ============================================================================
// Analysis Functions
// ============================================================================

QString detectExpiryFormat(const QString& expiry) {
    if (expiry.isEmpty()) {
        return "EMPTY";
    }
    
    // ISO format: 2026-01-27T14:30:00
    QRegularExpression isoPattern("^\\d{4}-\\d{2}-\\d{2}T\\d{2}:\\d{2}:\\d{2}$");
    if (isoPattern.match(expiry).hasMatch()) {
        return "ISO_DATETIME";
    }
    
    // DDMMMYYYY format: 27JAN2026
    QRegularExpression ddmmmyyyyPattern("^\\d{2}[A-Z]{3}\\d{4}$");
    if (ddmmmyyyyPattern.match(expiry).hasMatch()) {
        return "DDMMMYYYY";
    }
    
    // YYYYMMDD format: 20260127
    QRegularExpression yyyymmddPattern("^\\d{8}$");
    if (yyyymmddPattern.match(expiry).hasMatch()) {
        return "YYYYMMDD";
    }
    
    return "UNKNOWN";
}

QString detectAssetTokenPattern(int64_t assetToken, const QString& symbolName) {
    if (assetToken == 0) {
        return "ZERO";
    }
    
    if (assetToken == -1) {
        return "MINUS_ONE (Index underlying - needs mapping)";
    }
    
    if (assetToken > 10000000000LL) {
        // Composite token: 1100100002885
        // Pattern: [SEGMENT_PREFIX][ACTUAL_TOKEN]
        int64_t extracted = assetToken % 100000;
        return QString("COMPOSITE (Prefix: %1, Extracted: %2)")
            .arg(assetToken / 100000)
            .arg(extracted);
    }
    
    if (assetToken > 0 && assetToken < 10000000) {
        return QString("DIRECT (%1)").arg(assetToken);
    }
    
    return "UNKNOWN_PATTERN";
}

void analyzeRecord(const MasterRecord& record, AnalysisReport& report) {
    report.totalRecords++;
    
    if (record.parseError) {
        report.parseErrors++;
        report.validationErrors.append(
            QString("Line %1: %2").arg(record.lineNumber).arg(record.errorMessage)
        );
        report.errorCategories[record.errorMessage.left(30)]++;
        return;
    }
    
    // Exchange segment analysis
    report.exchangeSegmentCount[record.exchangeSegment]++;
    report.seriesCount[record.series]++;
    report.instrumentTypeCount[record.instrumentType]++;
    
    // Token analysis
    if (record.tokenInt > 0) {
        report.minToken = qMin(report.minToken, record.tokenInt);
        report.maxToken = qMax(report.maxToken, record.tokenInt);
        
        auto& range = report.tokenRangeByExchange[record.exchangeSegment];
        if (range.first == 0) {
            range.first = record.tokenInt;
            range.second = record.tokenInt;
        } else {
            range.first = qMin(range.first, record.tokenInt);
            range.second = qMax(range.second, record.tokenInt);
        }
    }
    
    // Asset token analysis
    if (!record.assetToken.isEmpty()) {
        report.recordsWithAssetToken++;
        
        if (record.assetTokenInt == -1) {
            report.recordsWithNegativeOne++;
        }
        
        QString pattern = detectAssetTokenPattern(record.assetTokenInt, record.name);
        report.assetTokenPatterns[pattern]++;
        
        if (record.assetTokenInt > 0) {
            report.assetTokenFrequency[record.assetTokenInt]++;
        }
    } else {
        report.recordsWithoutAssetToken++;
    }
    
    // Expiry date analysis
    if (record.hasExpiry) {
        report.recordsWithExpiry++;
        QString format = detectExpiryFormat(record.expiryDate);
        report.expiryDateFormats[format]++;
        
        if (record.expiryDateTime.isValid()) {
            report.uniqueExpiries.insert(record.expiryDateTime.date().toString("ddMMMyyyy"));
        } else {
            report.expiryParseErrors[record.expiryDate]++;
        }
    } else {
        report.recordsWithoutExpiry++;
    }
    
    // Strike price analysis
    if (record.strikePrice > 0) {
        report.recordsWithStrike++;
        report.minStrike = qMin(report.minStrike, record.strikePrice);
        report.maxStrike = qMax(report.maxStrike, record.strikePrice);
        report.strikeFrequency[record.strikePrice]++;
        
        // Analyze strike precision
        double fractionalPart = record.strikePrice - std::floor(record.strikePrice);
        if (fractionalPart > 0.001) {
            report.strikePrecisionBySymbol[record.name][fractionalPart * 100]++;
        }
    } else if (!record.strikePriceRaw.isEmpty()) {
        report.recordsWithStrike++;
    } else {
        report.recordsWithoutStrike++;
    }
    
    // Option type analysis
    if (!record.optionType.isEmpty()) {
        report.optionTypeCount[record.optionType]++;
        
        QString key = record.name + "_" + (record.optionType == "3" ? "CE" : "PE");
        report.ceVsPeBySymbol[key]++;
    }
    
    // Symbol analysis
    if (!record.name.isEmpty()) {
        report.uniqueSymbols.insert(record.name);
        report.symbolFrequency[record.name]++;
        report.symbolContractCount[record.name]++;
    }
}

void analyzeExchangeQuirks(const QList<MasterRecord>& records, AnalysisReport& report) {
    QMap<QString, QList<MasterRecord>> recordsByExchange;
    
    for (const auto& rec : records) {
        if (!rec.parseError) {
            recordsByExchange[rec.exchangeSegment].append(rec);
        }
    }
    
    for (auto it = recordsByExchange.begin(); it != recordsByExchange.end(); ++it) {
        QString exchange = it.key();
        const auto& exchangeRecords = it.value();
        
        AnalysisReport::ExchangeQuirks quirks;
        
        // Check for composite tokens
        for (const auto& rec : exchangeRecords) {
            if (rec.assetTokenInt > 10000000000LL) {
                quirks.hasCompositeTokens = true;
                quirks.assetTokenFormat = "Composite (Prefix + Token)";
                break;
            }
        }
        
        // Check for index underlying
        for (const auto& rec : exchangeRecords) {
            if (!rec.underlyingIndexName.isEmpty()) {
                quirks.hasIndexUnderlying = true;
                quirks.notes.append("Uses underlyingIndexName field for index options");
                break;
            }
        }
        
        // Check for fractional strikes
        for (const auto& rec : exchangeRecords) {
            if (rec.strikePrice > 0) {
                double fractional = rec.strikePrice - std::floor(rec.strikePrice);
                if (fractional > 0.001) {
                    quirks.hasFractionalStrikes = true;
                    quirks.notes.append(QString("Fractional strikes found (e.g., %.2f)").arg(rec.strikePrice));
                    break;
                }
            }
        }
        
        // Expiry format
        for (const auto& rec : exchangeRecords) {
            if (rec.hasExpiry) {
                quirks.expiryFormat = detectExpiryFormat(rec.expiryDate);
                break;
            }
        }
        
        report.quirks[exchange] = quirks;
    }
}

// ============================================================================
// Report Generation
// ============================================================================

void generateMarkdownReport(const AnalysisReport& report, const QString& outputPath) {
    QFile file(outputPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qCritical() << "Failed to open output file:" << outputPath;
        return;
    }
    
    QTextStream out(&file);
    out.setCodec("UTF-8");
    
    // Header
    out << "# Master Contracts File - Comprehensive Analysis\n\n";
    out << "**Generated:** " << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n";
    out << "**Total Records:** " << report.totalRecords << "\n";
    out << "**Parse Errors:** " << report.parseErrors << "\n\n";
    
    out << "---\n\n";
    
    // ========================================================================
    // 1. Executive Summary
    // ========================================================================
    out << "## 1. Executive Summary\n\n";
    
    out << "### Key Findings\n\n";
    out << "| Metric | Value | Status |\n";
    out << "|--------|-------|--------|\n";
    out << "| **Total Records** | " << report.totalRecords << " | ✅ |\n";
    out << "| **Parse Errors** | " << report.parseErrors << " | " 
        << (report.parseErrors == 0 ? "✅" : "⚠️") << " |\n";
    out << "| **Unique Symbols** | " << report.uniqueSymbols.size() << " | ✅ |\n";
    out << "| **Unique Expiries** | " << report.uniqueExpiries.size() << " | ✅ |\n";
    out << "| **Records with Asset Token** | " << report.recordsWithAssetToken 
        << " (" << (100.0 * report.recordsWithAssetToken / report.totalRecords) << "%) | "
        << (report.recordsWithAssetToken * 100 / report.totalRecords > 80 ? "✅" : "❌") << " |\n";
    out << "| **Records with Expiry** | " << report.recordsWithExpiry 
        << " (" << (100.0 * report.recordsWithExpiry / report.totalRecords) << "%) | ✅ |\n\n";
    
    // ========================================================================
    // 2. Exchange Segment Analysis
    // ========================================================================
    out << "## 2. Exchange Segment Breakdown\n\n";
    out << "| Exchange Segment | Count | Percentage |\n";
    out << "|-----------------|-------|------------|\n";
    
    for (auto it = report.exchangeSegmentCount.begin(); 
         it != report.exchangeSegmentCount.end(); ++it) {
        double pct = 100.0 * it.value() / report.totalRecords;
        out << "| " << it.key() << " | " << it.value() << " | " 
            << QString::number(pct, 'f', 2) << "% |\n";
    }
    out << "\n";
    
    // ========================================================================
    // 3. Series Distribution
    // ========================================================================
    out << "## 3. Series Distribution\n\n";
    out << "| Series | Count | Description |\n";
    out << "|--------|-------|-------------|\n";
    
    QMap<QString, QString> seriesDesc = {
        {"OPTSTK", "Stock Options"},
        {"OPTIDX", "Index Options"},
        {"FUTSTK", "Stock Futures"},
        {"FUTIDX", "Index Futures"},
        {"EQ", "Equity (Cash Market)"},
        {"INDEX", "Indices"}
    };
    
    for (auto it = report.seriesCount.begin(); it != report.seriesCount.end(); ++it) {
        QString desc = seriesDesc.value(it.key(), "Unknown");
        out << "| " << it.key() << " | " << it.value() << " | " << desc << " |\n";
    }
    out << "\n";
    
    // ========================================================================
    // 4. Token Range Analysis
    // ========================================================================
    out << "## 4. Token Range Analysis\n\n";
    out << "**Overall Token Range:** " << report.minToken << " - " << report.maxToken << "\n\n";
    
    out << "| Exchange | Min Token | Max Token | Range | Recommended Storage |\n";
    out << "|----------|-----------|-----------|-------|---------------------|\n";
    
    for (auto it = report.tokenRangeByExchange.begin(); 
         it != report.tokenRangeByExchange.end(); ++it) {
        int64_t range = it.value().second - it.value().first;
        QString storage = (range < 200000) ? "Array (indexed)" : "Hash (sparse)";
        
        out << "| " << it.key() << " | " << it.value().first << " | " 
            << it.value().second << " | " << range << " | " << storage << " |\n";
    }
    out << "\n";
    
    // ========================================================================
    // 5. Asset Token Analysis (CRITICAL)
    // ========================================================================
    out << "## 5. Asset Token Analysis (CRITICAL for Greeks Calculation)\n\n";
    
    out << "### Summary\n\n";
    out << "| Metric | Value |\n";
    out << "|--------|-------|\n";
    out << "| Records with Asset Token | " << report.recordsWithAssetToken << " |\n";
    out << "| Records without Asset Token | " << report.recordsWithoutAssetToken << " |\n";
    out << "| Records with asset_token = -1 | " << report.recordsWithNegativeOne 
        << " (Index options) |\n\n";
    
    out << "### Asset Token Patterns\n\n";
    out << "| Pattern | Count | Note |\n";
    out << "|---------|-------|------|\n";
    
    for (auto it = report.assetTokenPatterns.begin(); 
         it != report.assetTokenPatterns.end(); ++it) {
        QString note = it.key().contains("MINUS_ONE") 
            ? "❌ **Needs index master mapping**" 
            : "✅";
        out << "| " << it.key() << " | " << it.value() << " | " << note << " |\n";
    }
    out << "\n";
    
    out << "### Most Common Asset Tokens\n\n";
    out << "| Asset Token | Frequency | Note |\n";
    out << "|-------------|-----------|------|\n";
    
    QList<QPair<int64_t, int>> sortedTokens;
    for (auto it = report.assetTokenFrequency.begin(); 
         it != report.assetTokenFrequency.end(); ++it) {
        sortedTokens.append(qMakePair(it.key(), it.value()));
    }
    std::sort(sortedTokens.begin(), sortedTokens.end(), 
              [](const auto& a, const auto& b) { return a.second > b.second; });
    
    for (int i = 0; i < qMin(20, sortedTokens.size()); ++i) {
        out << "| " << sortedTokens[i].first << " | " << sortedTokens[i].second 
            << " | Options/Futures for this underlying |\n";
    }
    out << "\n";
    
    // ========================================================================
    // 6. Expiry Date Analysis
    // ========================================================================
    out << "## 6. Expiry Date Format Analysis\n\n";
    
    out << "### Format Distribution\n\n";
    out << "| Format | Count | Percentage | Recommended Parser |\n";
    out << "|--------|-------|------------|--------------------|\n";
    
    for (auto it = report.expiryDateFormats.begin(); 
         it != report.expiryDateFormats.end(); ++it) {
        double pct = 100.0 * it.value() / report.recordsWithExpiry;
        QString parser = (it.key() == "ISO_DATETIME") 
            ? "QDateTime::fromString(expiry, Qt::ISODate)"
            : "QDate::fromString(expiry, \"ddMMMyyyy\")";
        
        out << "| " << it.key() << " | " << it.value() << " | " 
            << QString::number(pct, 'f', 2) << "% | " << parser << " |\n";
    }
    out << "\n";
    
    out << "### Expiry Date Parsing Errors\n\n";
    if (report.expiryParseErrors.isEmpty()) {
        out << "✅ **No expiry date parsing errors found!**\n\n";
    } else {
        out << "❌ **Found " << report.expiryParseErrors.size() << " unparseable dates**\n\n";
        out << "| Expiry String | Occurrences |\n";
        out << "|---------------|-------------|\n";
        
        for (auto it = report.expiryParseErrors.begin(); 
             it != report.expiryParseErrors.end(); ++it) {
            out << "| `" << it.key() << "` | " << it.value() << " |\n";
        }
        out << "\n";
    }
    
    // ========================================================================
    // 7. Strike Price Analysis
    // ========================================================================
    out << "## 7. Strike Price Analysis\n\n";
    
    out << "### Summary\n\n";
    out << "| Metric | Value |\n";
    out << "|--------|-------|\n";
    out << "| Records with Strike | " << report.recordsWithStrike << " |\n";
    out << "| Records without Strike | " << report.recordsWithoutStrike 
        << " (Futures, Cash) |\n";
    out << "| Min Strike | " << report.minStrike << " |\n";
    out << "| Max Strike | " << report.maxStrike << " |\n\n";
    
    out << "### Strike Precision by Symbol (Top 10)\n\n";
    out << "| Symbol | Fractional Values | Note |\n";
    out << "|--------|------------------|------|\n";
    
    for (auto it = report.strikePrecisionBySymbol.begin(); 
         it != report.strikePrecisionBySymbol.end(); ++it) {
        if (it == report.strikePrecisionBySymbol.begin() + qMin(10, report.strikePrecisionBySymbol.size())) {
            break;
        }
        
        QStringList fractions;
        for (auto jt = it.value().begin(); jt != it.value().end(); ++jt) {
            fractions.append(QString::number(jt.key() / 100.0, 'f', 2));
        }
        
        out << "| " << it.key() << " | " << fractions.join(", ") 
            << " | Uses fractional strikes |\n";
    }
    out << "\n";
    
    // ========================================================================
    // 8. Option Type Distribution
    // ========================================================================
    out << "## 8. Option Type Distribution\n\n";
    
    out << "| Option Type | Count | Description |\n";
    out << "|-------------|-------|-------------|\n";
    
    for (auto it = report.optionTypeCount.begin(); 
         it != report.optionTypeCount.end(); ++it) {
        QString desc = (it.key() == "3") ? "Call (CE)" : 
                      (it.key() == "4") ? "Put (PE)" : "Unknown";
        out << "| " << it.key() << " | " << it.value() << " | " << desc << " |\n";
    }
    out << "\n";
    
    out << "### CE vs PE Balance (Top 10 Symbols)\n\n";
    out << "| Symbol | CE Count | PE Count | Ratio | Balance |\n";
    out << "|--------|----------|----------|-------|----------|\n";
    
    QMap<QString, QPair<int, int>> ceVsPe;
    for (auto it = report.ceVsPeBySymbol.begin(); 
         it != report.ceVsPeBySymbol.end(); ++it) {
        QStringList parts = it.key().split('_');
        if (parts.size() == 2) {
            QString symbol = parts[0];
            if (parts[1] == "CE") {
                ceVsPe[symbol].first += it.value();
            } else {
                ceVsPe[symbol].second += it.value();
            }
        }
    }
    
    QList<QString> sortedSymbols = ceVsPe.keys();
    std::sort(sortedSymbols.begin(), sortedSymbols.end(), 
              [&ceVsPe](const QString& a, const QString& b) {
                  return ceVsPe[a].first + ceVsPe[a].second > 
                         ceVsPe[b].first + ceVsPe[b].second;
              });
    
    for (int i = 0; i < qMin(10, sortedSymbols.size()); ++i) {
        QString symbol = sortedSymbols[i];
        int ce = ceVsPe[symbol].first;
        int pe = ceVsPe[symbol].second;
        double ratio = (pe > 0) ? (double)ce / pe : 0;
        QString balance = (std::abs(ratio - 1.0) < 0.05) ? "✅ Balanced" : "⚠️ Imbalanced";
        
        out << "| " << symbol << " | " << ce << " | " << pe << " | " 
            << QString::number(ratio, 'f', 2) << " | " << balance << " |\n";
    }
    out << "\n";
    
    // ========================================================================
    // 9. Exchange-Specific Quirks
    // ========================================================================
    out << "## 9. Exchange-Specific Quirks & Recommendations\n\n";
    
    for (auto it = report.quirks.begin(); it != report.quirks.end(); ++it) {
        out << "### " << it.key() << "\n\n";
        
        const auto& quirk = it.value();
        
        out << "| Property | Value |\n";
        out << "|----------|-------|\n";
        out << "| Composite Tokens | " << (quirk.hasCompositeTokens ? "✅ Yes" : "❌ No") << " |\n";
        out << "| Index Underlying | " << (quirk.hasIndexUnderlying ? "✅ Yes" : "❌ No") << " |\n";
        out << "| Fractional Strikes | " << (quirk.hasFractionalStrikes ? "✅ Yes" : "❌ No") << " |\n";
        out << "| Asset Token Format | " << quirk.assetTokenFormat << " |\n";
        out << "| Expiry Format | " << quirk.expiryFormat << " |\n\n";
        
        if (!quirk.notes.isEmpty()) {
            out << "**Notes:**\n\n";
            for (const QString& note : quirk.notes) {
                out << "- " << note << "\n";
            }
            out << "\n";
        }
    }
    
    // ========================================================================
    // 10. Data Quality Issues
    // ========================================================================
    out << "## 10. Data Quality Issues\n\n";
    
    if (report.validationErrors.isEmpty()) {
        out << "✅ **No data quality issues found!**\n\n";
    } else {
        out << "❌ **Found " << report.validationErrors.size() << " validation errors**\n\n";
        
        out << "### Error Categories\n\n";
        out << "| Error Type | Count |\n";
        out << "|------------|-------|\n";
        
        for (auto it = report.errorCategories.begin(); 
             it != report.errorCategories.end(); ++it) {
            out << "| " << it.key() << " | " << it.value() << " |\n";
        }
        out << "\n";
        
        out << "### First 20 Errors\n\n";
        out << "```\n";
        for (int i = 0; i < qMin(20, report.validationErrors.size()); ++i) {
            out << report.validationErrors[i] << "\n";
        }
        out << "```\n\n";
    }
    
    // ========================================================================
    // 11. Recommendations
    // ========================================================================
    out << "## 11. Implementation Recommendations\n\n";
    
    out << "### Critical Changes Needed\n\n";
    
    out << "#### 1. Asset Token Extraction\n\n";
    out << "```cpp\n";
    out << "// Current logic has issues with composite tokens and -1 values\n";
    out << "int64_t extractAssetToken(int64_t rawToken, const QString& symbol) {\n";
    out << "    if (rawToken == -1) {\n";
    out << "        // Index option - lookup from index master\n";
    out << "        return m_indexNameTokenMap.value(symbol, 0);\n";
    out << "    }\n";
    out << "    \n";
    out << "    if (rawToken > 10000000000LL) {\n";
    out << "        // Composite format: extract last 5 digits\n";
    out << "        return rawToken % 100000;\n";
    out << "    }\n";
    out << "    \n";
    out << "    return rawToken;\n";
    out << "}\n";
    out << "```\n\n";
    
    out << "#### 2. Expiry Date Parsing\n\n";
    out << "```cpp\n";
    out << "// All expiry dates are in ISO format: 2026-01-27T14:30:00\n";
    out << "QDateTime expiryDateTime = QDateTime::fromString(expiryStr, Qt::ISODate);\n";
    out << "if (expiryDateTime.isValid()) {\n";
    out << "    contract.expiryDate_dt = expiryDateTime.date();\n";
    out << "    contract.expiryDate = expiryDateTime.date().toString(\"ddMMMyyyy\").toUpper();\n";
    out << "}\n";
    out << "```\n\n";
    
    out << "#### 3. Repository Storage Strategy\n\n";
    out << "Based on token range analysis:\n\n";
    out << "| Exchange | Strategy | Reason |\n";
    out << "|----------|----------|--------|\n";
    
    for (auto it = report.tokenRangeByExchange.begin(); 
         it != report.tokenRangeByExchange.end(); ++it) {
        int64_t range = it.value().second - it.value().first;
        QString strategy = (range < 200000) 
            ? "Array (cached indexed)" 
            : "Hash (token → index)";
        QString reason = (range < 200000)
            ? "Tokens are dense, direct indexing efficient"
            : "Tokens are sparse, hash lookup better";
        
        out << "| " << it.key() << " | " << strategy << " | " << reason << " |\n";
    }
    out << "\n";
    
    out << "#### 4. Index Master Integration\n\n";
    out << "**Critical:** " << report.recordsWithNegativeOne 
        << " records have asset_token = -1 (index options)\n\n";
    out << "**Action Required:**\n";
    out << "1. Load index master FIRST before F&O\n";
    out << "2. Build symbol → token mapping\n";
    out << "3. Update asset tokens in NSEFO/BSEFO during parsing\n";
    out << "4. Export index name → token map to UDP reader\n\n";
    
    // ========================================================================
    // Footer
    // ========================================================================
    out << "---\n\n";
    out << "**Analysis Complete**\n\n";
    out << "Next Steps:\n";
    out << "1. Review exchange-specific quirks\n";
    out << "2. Implement recommended parsers\n";
    out << "3. Update asset token extraction logic\n";
    out << "4. Integrate index master BEFORE F&O loading\n";
    out << "5. Add validation for all parsed fields\n";
    
    file.close();
    qInfo() << "Report generated:" << outputPath;
}

// ============================================================================
// Main Function
// ============================================================================

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    
    if (argc < 2) {
        qCritical() << "Usage: master_file_analyzer <path_to_master_contracts_latest.txt>";
        return 1;
    }
    
    QString inputPath = argv[1];
    QString outputPath = "master_file_analysis_report.md";
    
    if (argc >= 3) {
        outputPath = argv[2];
    }
    
    qInfo() << "Starting analysis...";
    qInfo() << "Input file:" << inputPath;
    qInfo() << "Output file:" << outputPath;
    
    // Open input file
    QFile file(inputPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qCritical() << "Failed to open input file:" << inputPath;
        return 1;
    }
    
    QTextStream in(&file);
    in.setCodec("UTF-8");
    
    // Parse all records
    QList<MasterRecord> records;
    AnalysisReport report;
    
    int lineNumber = 0;
    while (!in.atEnd()) {
        QString line = in.readLine();
        lineNumber++;
        
        if (line.trimmed().isEmpty()) {
            continue;
        }
        
        MasterRecord record = parseLine(line, lineNumber);
        records.append(record);
        analyzeRecord(record, report);
        
        if (lineNumber % 10000 == 0) {
            qInfo() << "Processed" << lineNumber << "lines...";
        }
    }
    
    file.close();
    
    qInfo() << "Analysis complete. Analyzing exchange quirks...";
    analyzeExchangeQuirks(records, report);
    
    qInfo() << "Generating report...";
    generateMarkdownReport(report, outputPath);
    
    qInfo() << "Done!";
    qInfo() << "Total records:" << report.totalRecords;
    qInfo() << "Parse errors:" << report.parseErrors;
    qInfo() << "Report saved to:" << outputPath;
    
    return 0;
}
