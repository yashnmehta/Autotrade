#include "repository/NSEFORepository.h"
#include "repository/MasterFileParser.h"
#include <QFile>
#include <QTextStream>
#include <QDebug>

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
    
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to open NSEFO master file:" << filename;
        return false;
    }
    
    QTextStream in(&file);
    int32_t loadedCount = 0;
    int32_t spreadLoadedCount = 0;
    
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty()) {
            continue;
        }
        
        // Parse master line
        MasterContract contract;
        if (!MasterFileParser::parseLine(line, "NSEFO", contract)) {
            continue;
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
            m_assetToken[idx] = contract.assetToken;
            
            // Convert optionType from int to string
            // Based on actual data: 3=CE, 4=PE, others=XX
            if (loadedCount < 10 && contract.series == "OPTIDX") {
                qDebug() << "  [DEBUG] Token:" << token << "Symbol:" << contract.name 
                         << "Raw optionType:" << contract.optionType;
            }
            
            if (contract.optionType == 3) {
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
    
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to open NSEFO CSV file:" << filename;
        return false;
    }
    
    QTextStream in(&file);
    
    // Skip header line
    if (!in.atEnd()) {
        in.readLine();
    }
    
    int32_t loadedCount = 0;
    int32_t spreadLoadedCount = 0;
    
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty()) {
            continue;
        }
        
        QStringList fields = line.split(',');
        if (fields.size() < 26) {  // NSEFO CSV has 26 columns
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
            m_name[idx] = fields[1];             // Symbol
            m_displayName[idx] = fields[2];      // DisplayName
            m_description[idx] = fields[3];      // Description
            m_series[idx] = fields[4];           // Series
            m_lotSize[idx] = fields[5].toInt();
            m_tickSize[idx] = fields[6].toDouble();
            m_expiryDate[idx] = fields[7];       // DDMMMYYYY format
            m_strikePrice[idx] = fields[8].toDouble();
            m_optionType[idx] = fields[9];       // CE/PE/XX
            // fields[10] = UnderlyingSymbol (not stored separately)
            m_assetToken[idx] = fields[11].toLongLong();
            m_priceBandHigh[idx] = fields[12].toDouble();
            m_priceBandLow[idx] = fields[13].toDouble();
            
            // Live market data (initialized to 0 in CSV)
            m_ltp[idx] = fields[14].toDouble();
            m_open[idx] = fields[15].toDouble();
            m_high[idx] = fields[16].toDouble();
            m_low[idx] = fields[17].toDouble();
            m_close[idx] = fields[18].toDouble();
            m_prevClose[idx] = fields[19].toDouble();
            m_volume[idx] = fields[20].toLongLong();
            
            // Greeks (initialized to 0 in CSV)
            m_iv[idx] = fields[21].toDouble();
            m_delta[idx] = fields[22].toDouble();
            m_gamma[idx] = fields[23].toDouble();
            m_vega[idx] = fields[24].toDouble();
            m_theta[idx] = fields[25].toDouble();
            
            // Initialize bid/ask and margins to zero
            m_bidPrice[idx] = 0.0;
            m_askPrice[idx] = 0.0;
            m_rho[idx] = 0.0;
            m_spanMargin[idx] = 0.0;
            m_aelMargin[idx] = 0.0;
            m_freezeQty[idx] = 0;  // Not in CSV
            
            loadedCount++;
        } else if (token >= SPREAD_THRESHOLD) {
            // Create ContractData for spread contract
            auto contractData = std::make_shared<ContractData>();
            contractData->exchangeInstrumentID = token;
            contractData->name = fields[1];
            contractData->displayName = fields[2];
            contractData->description = fields[3];
            contractData->series = fields[4];
            contractData->lotSize = fields[5].toInt();
            contractData->tickSize = fields[6].toDouble();
            contractData->expiryDate = fields[7];
            contractData->strikePrice = fields[8].toDouble();
            contractData->optionType = fields[9];
            contractData->assetToken = fields[11].toLongLong();
            contractData->priceBandHigh = fields[12].toDouble();
            contractData->priceBandLow = fields[13].toDouble();
            contractData->ltp = fields[14].toDouble();
            contractData->open = fields[15].toDouble();
            contractData->high = fields[16].toDouble();
            contractData->low = fields[17].toDouble();
            contractData->close = fields[18].toDouble();
            contractData->prevClose = fields[19].toDouble();
            contractData->volume = fields[20].toLongLong();
            contractData->iv = fields[21].toDouble();
            contractData->delta = fields[22].toDouble();
            contractData->gamma = fields[23].toDouble();
            contractData->vega = fields[24].toDouble();
            contractData->theta = fields[25].toDouble();
            
            m_spreadContracts[token] = contractData;
            spreadLoadedCount++;
        }
    }
    
    file.close();
    
    m_regularCount = loadedCount;
    m_spreadCount = spreadLoadedCount;
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
            
            contracts.append(contract);
        }
    }
    
    return contracts;
}

bool NSEFORepository::saveProcessedCSV(const QString& filename) const {
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Failed to open file for writing:" << filename;
        return false;
    }
    
    QTextStream out(&file);
    
    // Write header
    out << "Token,Symbol,DisplayName,Description,Series,LotSize,TickSize,ExpiryDate,StrikePrice,OptionType,UnderlyingSymbol,AssetToken,PriceBandHigh,PriceBandLow,LTP,Open,High,Low,Close,PrevClose,Volume,IV,Delta,Gamma,Vega,Theta\n";
    
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
            << m_priceBandHigh[idx] << ","
            << m_priceBandLow[idx] << ","
            << "0,0,0,0,0,0,0,"  // Live data (not persisted)
            << "0,0,0,0,0\n";    // Greeks (not persisted)
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
            << contract->priceBandHigh << ","
            << contract->priceBandLow << ","
            << "0,0,0,0,0,0,0,"  // Live data
            << "0,0,0,0,0\n";    // Greeks
    }
    
    file.close();
    qDebug() << "NSE FO Repository saved to CSV:" << filename << "Regular:" << m_regularCount << "Spread:" << m_spreadCount;
    return true;
}
