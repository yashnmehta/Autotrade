#include "repository/NSEFORepository.h"
#include "repository/MasterFileParser.h"
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <iostream>

// Helper function to remove surrounding quotes from CSV field values
static QString trimQuotes(const QString &str) {
    QString trimmed = str.trimmed();
    if (trimmed.startsWith('"') && trimmed.endsWith('"')) {
        return trimmed.mid(1, trimmed.length() - 2);
    }
    return trimmed;
}

// Helper function to get underlying asset token for indices
// XTS master file has -1 for indices, but we need actual index tokens
// For stocks, XTS provides tokens with prefix (e.g., 1100100002885 for RELIANCE)
// We need to extract the last 4-5 digits (2885 for RELIANCE)
static int64_t getUnderlyingAssetToken(const QString& symbolName, int64_t masterFileToken) {
    // For indices with -1, map to actual index tokens
    if (masterFileToken == -1) {
        // Map index names to their underlying tokens
        // These are the spot index tokens used for options/futures settlement
        static const QHash<QString, int64_t> indexTokens = {
            {"BANKNIFTY", 26009},    // Nifty Bank index
            {"NIFTY", 26000},        // Nifty 50 index
            {"FINNIFTY", 26037},     // Nifty Financial Services
            {"MIDCPNIFTY", 26074},   // Nifty Midcap Select
            {"NIFTYNXT50", 26013},   // Nifty Next 50
            {"SENSEX", -1},          // BSE Sensex (placeholder)
            {"BANKEX", -1}           // BSE Bankex (placeholder)
        };
        
        return indexTokens.value(symbolName, -1);
    }
    
    // For stocks, XTS provides tokens like 1100100002885
    // The actual stock token is the last part after removing segment prefix
    // Format: [segment_prefix][stock_token]
    // NSE CM: 11001000xxxxx -> xxxxx
    // BSE CM: similar pattern
    if (masterFileToken > 1000000000) {
        // Extract last 5 digits (stock token)
        // Example: 1100100002885 -> 2885
        int64_t stockToken = masterFileToken % 100000;
        return stockToken;
    }
    
    return masterFileToken;
}

NSEFORepository::NSEFORepository()
    : m_regularCount(0)
    , m_spreadCount(0)
    , m_loaded(false)
{
    allocateArrays();
}

NSEFORepository::~NSEFORepository() = default;

void NSEFORepository::allocateArrays() {
    // Allocate all arrays with ARRAY_SIZE capacity
    m_valid.resize(ARRAY_SIZE);
    m_valid.fill(false);
    
    // Security Master Data (6 fields)
    m_name.resize(ARRAY_SIZE);
    m_displayName.resize(ARRAY_SIZE);
    m_description.resize(ARRAY_SIZE);
    m_series.resize(ARRAY_SIZE);
    m_lotSize.resize(ARRAY_SIZE);
    m_tickSize.resize(ARRAY_SIZE);
    
    // Trading Parameters (1 field)
    m_freezeQty.resize(ARRAY_SIZE);
    
    // Price Bands (2 fields)
    m_priceBandHigh.resize(ARRAY_SIZE);
    m_priceBandLow.resize(ARRAY_SIZE);
    
    // Live Market Data (9 fields)
    m_ltp.resize(ARRAY_SIZE);
    m_open.resize(ARRAY_SIZE);
    m_high.resize(ARRAY_SIZE);
    m_low.resize(ARRAY_SIZE);
    m_close.resize(ARRAY_SIZE);
    m_prevClose.resize(ARRAY_SIZE);
    m_volume.resize(ARRAY_SIZE);
    m_bidPrice.resize(ARRAY_SIZE);
    m_askPrice.resize(ARRAY_SIZE);
    
    // F&O Specific (4 fields)
    m_expiryDate.resize(ARRAY_SIZE);
    m_strikePrice.resize(ARRAY_SIZE);
    m_optionType.resize(ARRAY_SIZE);
    m_assetToken.resize(ARRAY_SIZE);
    m_instrumentType.resize(ARRAY_SIZE);
    
    // Greeks (6 fields)
    m_iv.resize(ARRAY_SIZE);
    m_delta.resize(ARRAY_SIZE);
    m_gamma.resize(ARRAY_SIZE);
    m_vega.resize(ARRAY_SIZE);
    m_theta.resize(ARRAY_SIZE);
    m_rho.resize(ARRAY_SIZE);
    
    // Margin Data (2 fields)
    m_spanMargin.resize(ARRAY_SIZE);
    m_aelMargin.resize(ARRAY_SIZE);
    
    // Reserve space for spread contracts
    m_spreadContracts.reserve(500);
}

bool NSEFORepository::loadMasterFile(const QString& filename) {
    // Lock stripe 0 for initialization
    QWriteLocker locker(&m_mutexes[0]);
    
    // Native C++ file I/O (5-10x faster than QFile)
    std::ifstream file(filename.toStdString(), std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open NSEFO master file: " << filename.toStdString() << std::endl;
        return false;
    }
    
    std::string line;
    line.reserve(1024);  // Pre-allocate to avoid reallocations
    
    int32_t loadedCount = 0;
    int32_t spreadLoadedCount = 0;
    
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '\r') {
            continue;
        }
        
        // Remove trailing whitespace
        while (!line.empty() && (line.back() == ' ' || line.back() == '\r' || line.back() == '\n')) {
            line.pop_back();
        }
        
        if (line.empty()) {
            continue;
        }
        
        // Parse master line
        QString qLine = QString::fromStdString(line);
        MasterContract contract;
        if (!MasterFileParser::parseLine(qLine, "NSEFO", contract)) {
            continue;
        }
        
        // Debug: Show first 3 parsed contracts to check for quotes
        static int debugCount = 0;
        if (debugCount < 3 && contract.series == "FUTIDX") {
            qDebug() << "[NSEFORepo] Parsed contract:" << debugCount 
                     << "Token:" << contract.exchangeInstrumentID
                     << "Name bytes:" << contract.name.toUtf8().toHex()
                     << "Name:'" << contract.name << "'"
                     << "Series:'" << contract.series << "'";
            debugCount++;
        }
        
        int64_t token = contract.exchangeInstrumentID;
        
        if (isRegularContract(token)) {
            // Store in cached indexed array
            int32_t idx = getArrayIndex(token);
            
            m_valid[idx] = true;
            m_name[idx] = contract.name;
            m_displayName[idx] = contract.displayName;
            m_description[idx] = contract.description;
            m_series[idx] = contract.series;
            m_lotSize[idx] = contract.lotSize;
            m_tickSize[idx] = contract.tickSize;
            m_freezeQty[idx] = contract.freezeQty;
            m_priceBandHigh[idx] = contract.priceBandHigh;
            m_priceBandLow[idx] = contract.priceBandLow;
            m_expiryDate[idx] = contract.expiryDate;
            m_strikePrice[idx] = contract.strikePrice;
            m_assetToken[idx] = getUnderlyingAssetToken(contract.name, contract.assetToken);
            m_instrumentType[idx] = contract.instrumentType;
            
            // Convert optionType from int to string
            // Based on actual data: 3=CE, 4=PE, others=XX
            if (loadedCount < 10 && contract.series == "OPTIDX") {
                qDebug() << "  [DEBUG] Token:" << token << "Symbol:" << contract.name 
                         << "Raw optionType:" << contract.optionType;
            }
            
            if (contract.instrumentType == 4) {
                m_optionType[idx] = "SPD";
            } else if (contract.optionType == 3) {
                m_optionType[idx] = "CE";
            } else if (contract.optionType == 4) {
                m_optionType[idx] = "PE";
            } else {
                m_optionType[idx] = "XX";
            }
            
            // Initialize live data to zero
            m_ltp[idx] = 0.0;
            m_open[idx] = 0.0;
            m_high[idx] = 0.0;
            m_low[idx] = 0.0;
            m_close[idx] = 0.0;
            m_prevClose[idx] = 0.0;
            m_volume[idx] = 0;
            m_bidPrice[idx] = 0.0;
            m_askPrice[idx] = 0.0;
            
            // Initialize Greeks to zero
            m_iv[idx] = 0.0;
            m_delta[idx] = 0.0;
            m_gamma[idx] = 0.0;
            m_vega[idx] = 0.0;
            m_theta[idx] = 0.0;
            m_rho[idx] = 0.0;
            
            // Initialize Margins to zero
            m_spanMargin[idx] = 0.0;
            m_aelMargin[idx] = 0.0;
            
            loadedCount++;
        } else if (token >= SPREAD_THRESHOLD) {
            // Store spread contract in map
            auto contractData = std::make_shared<ContractData>(contract.toContractData());
            m_spreadContracts[token] = contractData;
            spreadLoadedCount++;
        }
    }
    
    file.close();
    
    m_regularCount = loadedCount;
    m_spreadCount = spreadLoadedCount;
    m_loaded = true;
    
    qDebug() << "NSE FO Repository loaded:"
             << "Regular:" << m_regularCount
             << "Spread:" << m_spreadCount
             << "Total:" << getTotalCount();
    
    // Debug: Show first 5 valid contracts to verify expiry date format
    qDebug() << "[NSE FO] First 5 contracts with expiry dates:";
    int shown = 0;
    for (int32_t idx = 0; idx < ARRAY_SIZE && shown < 5; ++idx) {
        if (m_valid[idx] && !m_expiryDate[idx].isEmpty()) {
            qDebug() << "  Token:" << (MIN_TOKEN + idx)
                     << "Symbol:" << m_name[idx]
                     << "Expiry:" << m_expiryDate[idx]
                     << "Strike:" << m_strikePrice[idx]
                     << "Type:" << m_optionType[idx];
            shown++;
        }
    }
    
    return true;
}

bool NSEFORepository::loadProcessedCSV(const QString& filename) {
    // Lock stripe 0 for initialization
    QWriteLocker locker(&m_mutexes[0]);
    
    // Native C++ file I/O (10x faster than QFile/QTextStream)
    std::ifstream file(filename.toStdString(), std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open NSEFO CSV file: " << filename.toStdString() << std::endl;
        return false;
    }
    
    std::string line;
    line.reserve(2048);  // CSV lines are longer
    
    // Skip header line
    std::getline(file, line);
    
    int32_t loadedCount = 0;
    int32_t spreadLoadedCount = 0;
    
    while (std::getline(file, line)) {
        if (line.empty()) {
            continue;
        }
        
        // Convert to QString for compatibility with existing code
        QString qLine = QString::fromStdString(line);
        QStringList fields = qLine.split(',');
        
        if (fields.size() < 28) {  // Added instrumentType
            continue;
        }
        
        bool ok;
        int64_t token = fields[0].toLongLong(&ok);
        if (!ok) {
            continue;
        }
        
        if (isRegularContract(token)) {
            // Store in cached indexed array
            int32_t idx = getArrayIndex(token);
            
            m_valid[idx] = true;
            m_name[idx] = trimQuotes(fields[1]);             // Symbol
            m_displayName[idx] = trimQuotes(fields[2]);      // DisplayName
            m_description[idx] = trimQuotes(fields[3]);      // Description
            m_series[idx] = trimQuotes(fields[4]);           // Series
            m_lotSize[idx] = fields[5].toInt();
            m_tickSize[idx] = fields[6].toDouble();
            m_expiryDate[idx] = trimQuotes(fields[7]);       // DDMMMYYYY format
            m_strikePrice[idx] = fields[8].toDouble();
            m_optionType[idx] = trimQuotes(fields[9]);       // CE/PE/XX
            // fields[10] = UnderlyingSymbol (not stored separately)
            m_assetToken[idx] = fields[11].toLongLong();
            m_freezeQty[idx] = fields[12].toInt();
            m_priceBandHigh[idx] = fields[13].toDouble();
            m_priceBandLow[idx] = fields[14].toDouble();
            m_instrumentType[idx] = fields[27].toInt();      // Added
            
            // Live market data (initialized to 0 in CSV)
            m_ltp[idx] = fields[15].toDouble();
            m_open[idx] = fields[16].toDouble();
            m_high[idx] = fields[17].toDouble();
            m_low[idx] = fields[18].toDouble();
            m_close[idx] = fields[19].toDouble();
            m_prevClose[idx] = fields[20].toDouble();
            m_volume[idx] = fields[21].toLongLong();
            
            // Greeks (initialized to 0 in CSV)
            m_iv[idx] = fields[22].toDouble();
            m_delta[idx] = fields[23].toDouble();
            m_gamma[idx] = fields[24].toDouble();
            m_vega[idx] = fields[25].toDouble();
            m_theta[idx] = fields[26].toDouble();
            
            // Initialize bid/ask and margins to zero
            m_bidPrice[idx] = 0.0;
            m_askPrice[idx] = 0.0;
            m_rho[idx] = 0.0;
            m_spanMargin[idx] = 0.0;
            m_aelMargin[idx] = 0.0;
            
            loadedCount++;
        } else if (token >= SPREAD_THRESHOLD) {
            // Create ContractData for spread contract
            auto contractData = std::make_shared<ContractData>();
            contractData->exchangeInstrumentID = token;
            contractData->name = trimQuotes(fields[1]);
            contractData->displayName = trimQuotes(fields[2]);
            contractData->description = trimQuotes(fields[3]);
            contractData->series = trimQuotes(fields[4]);
            contractData->lotSize = fields[5].toInt();
            contractData->tickSize = fields[6].toDouble();
            contractData->expiryDate = trimQuotes(fields[7]);
            contractData->strikePrice = fields[8].toDouble();
            contractData->optionType = trimQuotes(fields[9]);
            contractData->assetToken = fields[11].toLongLong();
            contractData->freezeQty = fields[12].toInt();
            contractData->priceBandHigh = fields[13].toDouble();
            contractData->priceBandLow = fields[14].toDouble();
            contractData->ltp = fields[15].toDouble();
            contractData->open = fields[16].toDouble();
            contractData->high = fields[17].toDouble();
            contractData->low = fields[18].toDouble();
            contractData->close = fields[19].toDouble();
            contractData->prevClose = fields[20].toDouble();
            contractData->volume = fields[21].toLongLong();
            contractData->iv = fields[22].toDouble();
            contractData->delta = fields[23].toDouble();
            contractData->gamma = fields[24].toDouble();
            contractData->vega = fields[25].toDouble();
            contractData->theta = fields[26].toDouble();
            contractData->instrumentType = fields[27].toInt();
            
            m_spreadContracts[token] = contractData;
            spreadLoadedCount++;
        }
    }
    
    file.close();
    
    m_regularCount = loadedCount;
    m_spreadCount = spreadLoadedCount;
    
    // Return false if no contracts loaded (empty CSV file)
    if (loadedCount == 0 && spreadLoadedCount == 0) {
        qWarning() << "NSE FO Repository CSV file is empty, will fall back to master file";
        return false;
    }
    
    m_loaded = true;
    
    qDebug() << "NSE FO Repository loaded from CSV:"
             << "Regular:" << m_regularCount
             << "Spread:" << m_spreadCount
             << "Total:" << getTotalCount();
    
    return true;
}

const ContractData* NSEFORepository::getContract(int64_t token) const {
    if (isRegularContract(token)) {
        int32_t idx = getArrayIndex(token);
        int32_t stripeIdx = getStripeIndex(idx);
        
        QReadLocker locker(&m_mutexes[stripeIdx]);
        
        if (!m_valid[idx]) {
            return nullptr;
        }
        
        // Create temporary ContractData from arrays
        // Note: This is not optimal for frequent access; consider caching
        static thread_local ContractData tempContract;
        tempContract.exchangeInstrumentID = token;
        tempContract.name = m_name[idx];
        tempContract.displayName = m_displayName[idx];
        tempContract.description = m_description[idx];
        tempContract.series = m_series[idx];
        tempContract.lotSize = m_lotSize[idx];
        tempContract.tickSize = m_tickSize[idx];
        tempContract.freezeQty = m_freezeQty[idx];
        tempContract.priceBandHigh = m_priceBandHigh[idx];
        tempContract.priceBandLow = m_priceBandLow[idx];
        tempContract.ltp = m_ltp[idx];
        tempContract.open = m_open[idx];
        tempContract.high = m_high[idx];
        tempContract.low = m_low[idx];
        tempContract.close = m_close[idx];
        tempContract.prevClose = m_prevClose[idx];
        tempContract.volume = m_volume[idx];
        tempContract.bidPrice = m_bidPrice[idx];
        tempContract.askPrice = m_askPrice[idx];
        tempContract.expiryDate = m_expiryDate[idx];
        tempContract.strikePrice = m_strikePrice[idx];
        tempContract.optionType = m_optionType[idx];
        tempContract.assetToken = m_assetToken[idx];
        tempContract.instrumentType = m_instrumentType[idx];
        tempContract.iv = m_iv[idx];
        tempContract.delta = m_delta[idx];
        tempContract.gamma = m_gamma[idx];
        tempContract.vega = m_vega[idx];
        tempContract.theta = m_theta[idx];
        tempContract.rho = m_rho[idx];
        tempContract.spanMargin = m_spanMargin[idx];
        tempContract.aelMargin = m_aelMargin[idx];
        
        return &tempContract;
    } else if (token >= SPREAD_THRESHOLD) {
        // Lock stripe 0 for spread contracts map
        QReadLocker locker(&m_mutexes[0]);
        
        auto it = m_spreadContracts.find(token);
        if (it != m_spreadContracts.end()) {
            return it.value().get();
        }
    }
    
    return nullptr;
}

bool NSEFORepository::hasContract(int64_t token) const {
    if (isRegularContract(token)) {
        int32_t idx = getArrayIndex(token);
        int32_t stripeIdx = getStripeIndex(idx);
        
        QReadLocker locker(&m_mutexes[stripeIdx]);
        return m_valid[idx];
    } else if (token >= SPREAD_THRESHOLD) {
        QReadLocker locker(&m_mutexes[0]);
        return m_spreadContracts.contains(token);
    }
    
    return false;
}

void NSEFORepository::updateLiveData(int64_t token, double ltp, int64_t volume) {
    if (isRegularContract(token)) {
        int32_t idx = getArrayIndex(token);
        int32_t stripeIdx = getStripeIndex(idx);
        
        QWriteLocker locker(&m_mutexes[stripeIdx]);
        
        if (m_valid[idx]) {
            m_ltp[idx] = ltp;
            m_volume[idx] = volume;
        }
    } else if (token >= SPREAD_THRESHOLD) {
        QWriteLocker locker(&m_mutexes[0]);
        
        auto it = m_spreadContracts.find(token);
        if (it != m_spreadContracts.end()) {
            it.value()->ltp = ltp;
            it.value()->volume = volume;
        }
    }
}

void NSEFORepository::updateBidAsk(int64_t token, double bidPrice, double askPrice) {
    if (isRegularContract(token)) {
        int32_t idx = getArrayIndex(token);
        int32_t stripeIdx = getStripeIndex(idx);
        
        QWriteLocker locker(&m_mutexes[stripeIdx]);
        
        if (m_valid[idx]) {
            m_bidPrice[idx] = bidPrice;
            m_askPrice[idx] = askPrice;
        }
    } else if (token >= SPREAD_THRESHOLD) {
        QWriteLocker locker(&m_mutexes[0]);
        
        auto it = m_spreadContracts.find(token);
        if (it != m_spreadContracts.end()) {
            it.value()->bidPrice = bidPrice;
            it.value()->askPrice = askPrice;
        }
    }
}

void NSEFORepository::updateGreeks(int64_t token, double iv, double delta, 
                                  double gamma, double vega, double theta) {
    if (isRegularContract(token)) {
        int32_t idx = getArrayIndex(token);
        int32_t stripeIdx = getStripeIndex(idx);
        
        QWriteLocker locker(&m_mutexes[stripeIdx]);
        
        if (m_valid[idx]) {
            m_iv[idx] = iv;
            m_delta[idx] = delta;
            m_gamma[idx] = gamma;
            m_vega[idx] = vega;
            m_theta[idx] = theta;
        }
    } else if (token >= SPREAD_THRESHOLD) {
        QWriteLocker locker(&m_mutexes[0]);
        
        auto it = m_spreadContracts.find(token);
        if (it != m_spreadContracts.end()) {
            it.value()->iv = iv;
            it.value()->delta = delta;
            it.value()->gamma = gamma;
            it.value()->vega = vega;
            it.value()->theta = theta;
        }
    }
}

QVector<ContractData> NSEFORepository::getAllContracts() const {
    QVector<ContractData> contracts;
    contracts.reserve(getTotalCount());
    
    // Add regular contracts
    for (int32_t idx = 0; idx < ARRAY_SIZE; ++idx) {
        int32_t stripeIdx = getStripeIndex(idx);
        QReadLocker locker(&m_mutexes[stripeIdx]);
        
        if (m_valid[idx]) {
            ContractData contract;
            contract.exchangeInstrumentID = MIN_TOKEN + idx;
            contract.name = m_name[idx];
            contract.displayName = m_displayName[idx];
            contract.description = m_description[idx];
            contract.series = m_series[idx];
            contract.lotSize = m_lotSize[idx];
            contract.tickSize = m_tickSize[idx];
            contract.freezeQty = m_freezeQty[idx];
            contract.priceBandHigh = m_priceBandHigh[idx];
            contract.priceBandLow = m_priceBandLow[idx];
            contract.expiryDate = m_expiryDate[idx];
            contract.strikePrice = m_strikePrice[idx];
            contract.optionType = m_optionType[idx];
            contract.assetToken = m_assetToken[idx];
            contract.ltp = m_ltp[idx];
            contract.volume = m_volume[idx];
            contract.iv = m_iv[idx];
            contract.delta = m_delta[idx];
            contract.instrumentType = m_instrumentType[idx];
            
            contracts.append(contract);
        }
    }
    
    // Add spread contracts
    {
        QReadLocker locker(&m_mutexes[0]);
        for (auto it = m_spreadContracts.begin(); it != m_spreadContracts.end(); ++it) {
            contracts.append(*it.value());
        }
    }
    
    return contracts;
}

QVector<ContractData> NSEFORepository::getContractsBySeries(const QString& series) const {
    QVector<ContractData> contracts;
    
    for (int32_t idx = 0; idx < ARRAY_SIZE; ++idx) {
        int32_t stripeIdx = getStripeIndex(idx);
        QReadLocker locker(&m_mutexes[stripeIdx]);
        
        if (m_valid[idx] && m_series[idx] == series) {
            ContractData contract;
            contract.exchangeInstrumentID = MIN_TOKEN + idx;
            contract.name = m_name[idx];
            contract.displayName = m_displayName[idx];
            contract.series = m_series[idx];
            contract.lotSize = m_lotSize[idx];
            contract.expiryDate = m_expiryDate[idx];
            contract.strikePrice = m_strikePrice[idx];
            contract.optionType = m_optionType[idx];
            contract.instrumentType = m_instrumentType[idx];
            
            contracts.append(contract);
        }
    }
    
    return contracts;
}

QVector<ContractData> NSEFORepository::getContractsBySymbol(const QString& symbol) const {
    QVector<ContractData> contracts;
    
    for (int32_t idx = 0; idx < ARRAY_SIZE; ++idx) {
        int32_t stripeIdx = getStripeIndex(idx);
        QReadLocker locker(&m_mutexes[stripeIdx]);
        
        if (m_valid[idx] && m_name[idx] == symbol) {
            ContractData contract;
            contract.exchangeInstrumentID = MIN_TOKEN + idx;
            contract.name = m_name[idx];
            contract.displayName = m_displayName[idx];
            contract.series = m_series[idx];
            contract.lotSize = m_lotSize[idx];
            contract.expiryDate = m_expiryDate[idx];
            contract.strikePrice = m_strikePrice[idx];
            contract.optionType = m_optionType[idx];
            contract.instrumentType = m_instrumentType[idx];
            
            contracts.append(contract);
        }
    }
    
    return contracts;
}

bool NSEFORepository::loadFromContracts(const QVector<MasterContract>& contracts) {
    if (contracts.isEmpty()) {
        qWarning() << "NSE FO Repository: No contracts to load";
        return false;
    }

    // Clear existing data (reset counters)
    m_regularCount = 0;
    m_spreadCount = 0;
    
    // Clear valid flags and spread contracts
    for (int32_t i = 0; i < ARRAY_SIZE; ++i) {
        m_valid[i] = false;
    }
    m_spreadContracts.clear();
    
    // Load contracts directly from QVector
    int loaded = 0;
    for (const MasterContract& contract : contracts) {
        if (contract.exchange != "NSEFO") {
            continue;  // Skip non-NSEFO contracts
        }
        
        int64_t token = contract.exchangeInstrumentID;
        
        if (isRegularContract(token)) {
            // Regular contract: store in array
            int32_t idx = getArrayIndex(token);
            int32_t stripeIdx = getStripeIndex(idx);
            QWriteLocker locker(&m_mutexes[stripeIdx]);
            
            m_valid[idx] = true;
            m_name[idx] = contract.name;
            m_displayName[idx] = contract.displayName;
            m_description[idx] = contract.description;
            m_series[idx] = contract.series;
            m_lotSize[idx] = contract.lotSize;
            m_tickSize[idx] = contract.tickSize;
            m_expiryDate[idx] = contract.expiryDate;
            m_strikePrice[idx] = contract.strikePrice;
            
            // Convert optionType from int to string
            if (contract.instrumentType == 4) {
                m_optionType[idx] = "SPD";
            } else if (contract.optionType == 3) {
                m_optionType[idx] = "CE";
            } else if (contract.optionType == 4) {
                m_optionType[idx] = "PE";
            } else {
                m_optionType[idx] = "XX";
            }
            
            m_instrumentType[idx] = contract.instrumentType;
            
            m_assetToken[idx] = getUnderlyingAssetToken(contract.name, contract.assetToken);
            // why we are using freezeQty for priceBandHigh???
            // freezeQty is one of the most important trading parameter why arent we saving it properly???
            // It seems like a bug in the original code, correcting it here
            // also we missed tickSize 
            //we also missed lotSize
            m_lotSize[idx] = contract.lotSize;
            m_tickSize[idx] = contract.tickSize;
            m_freezeQty[idx] = contract.freezeQty;

            m_priceBandHigh[idx] = contract.priceBandHigh;
            m_priceBandLow[idx] = contract.priceBandLow;
            
            // Initialize live data
            m_ltp[idx] = 0.0;
            m_open[idx] = 0.0;
            m_high[idx] = 0.0;
            m_low[idx] = 0.0;
            m_close[idx] = 0.0;
            m_prevClose[idx] = 0.0;
            m_volume[idx] = 0;
            m_bidPrice[idx] = 0.0;
            m_askPrice[idx] = 0.0;
            
            // Initialize Greeks
            m_iv[idx] = 0.0;
            m_delta[idx] = 0.0;
            m_gamma[idx] = 0.0;
            m_vega[idx] = 0.0;
            m_theta[idx] = 0.0;
            
            m_regularCount++;
            loaded++;
            
        } else if (token >= SPREAD_THRESHOLD) {
            // Spread contract: store in map
            QWriteLocker locker(&m_mutexes[0]);
            
            auto contractData = std::make_shared<ContractData>(contract.toContractData());
            
            m_spreadContracts.insert(token, std::move(contractData));
            m_spreadCount++;
            loaded++;
        }
    }
    
    m_loaded = (loaded > 0);
    
    qDebug() << "NSE FO Repository loaded from contracts:" << loaded 
             << "Regular:" << m_regularCount << "Spread:" << m_spreadCount;
    return loaded > 0;
}

bool NSEFORepository::saveProcessedCSV(const QString& filename) const {
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Failed to open file for writing:" << filename;
        return false;
    }
    
    QTextStream out(&file);
    
    // Write header
    out << "Token,Symbol,DisplayName,Description,Series,LotSize,TickSize,ExpiryDate,StrikePrice,OptionType,UnderlyingSymbol,AssetToken,FreezeQty,PriceBandHigh,PriceBandLow,LTP,Open,High,Low,Close,PrevClose,Volume,IV,Delta,Gamma,Vega,Theta,InstrumentType\n";
    
    // Write regular contracts
    for (int32_t idx = 0; idx < ARRAY_SIZE; ++idx) {
        if (!m_valid[idx]) continue;
        
        int64_t token = MIN_TOKEN + idx;
        out << token << ","
            << m_name[idx] << ","
            << m_displayName[idx] << ","
            << m_description[idx] << ","
            << m_series[idx] << ","
            << m_lotSize[idx] << ","
            << m_tickSize[idx] << ","
            << m_expiryDate[idx] << ","
            << m_strikePrice[idx] << ","
            << m_optionType[idx] << ","
            << m_name[idx] << ","  // UnderlyingSymbol (same as name)
            << m_assetToken[idx] << ","
            << m_freezeQty[idx] << ","
            << m_priceBandHigh[idx] << ","
            << m_priceBandLow[idx] << ","
            << "0,0,0,0,0,0,0,"  // Live data (not persisted)
            << "0,0,0,0,0,"      // Greeks (not persisted)
            << m_instrumentType[idx] << "\n";
    }
    
    // Write spread contracts
    for (auto it = m_spreadContracts.constBegin(); it != m_spreadContracts.constEnd(); ++it) {
        const ContractData* contract = it.value().get();
        out << contract->exchangeInstrumentID << ","
            << contract->name << ","
            << contract->displayName << ","
            << contract->description << ","
            << contract->series << ","
            << contract->lotSize << ","
            << contract->tickSize << ","
            << contract->expiryDate << ","
            << contract->strikePrice << ","
            << contract->optionType << ","
            << contract->name << ","
            << contract->assetToken << ","
            << contract->freezeQty << ","
            << contract->priceBandHigh << ","
            << contract->priceBandLow << ","
            << "0,0,0,0,0,0,0,"  // Live data
            << "0,0,0,0,0,"      // Greeks
            << contract->instrumentType << "\n";
    }
    
    file.close();
    qDebug() << "NSE FO Repository saved to CSV:" << filename << "Regular:" << m_regularCount << "Spread:" << m_spreadCount;
    return true;
}
