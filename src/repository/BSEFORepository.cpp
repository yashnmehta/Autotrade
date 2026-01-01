#include "repository/BSEFORepository.h"
#include "repository/MasterFileParser.h"
#include <QDebug>
#include <QFile>
#include <QTextStream>

// Helper function to remove surrounding quotes from CSV field values
static QString trimQuotes(const QString &str) {
    QString trimmed = str.trimmed();
    if (trimmed.startsWith('"') && trimmed.endsWith('"')) {
        return trimmed.mid(1, trimmed.length() - 2);
    }
    return trimmed;
}

BSEFORepository::BSEFORepository()
    : m_contractCount(0)
    , m_loaded(false)
{
}

BSEFORepository::~BSEFORepository() = default;

bool BSEFORepository::loadMasterFile(const QString& filename) {
    QWriteLocker locker(&m_mutex);
    
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to open BSE FO master file:" << filename;
        return false;
    }
    
    QVector<MasterContract> contracts;
    QTextStream in(&file);
    
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;
        
        MasterContract contract;
        if (MasterFileParser::parseLine(line, "BSEFO", contract)) {
            contracts.append(contract);
        }
    }
    
    file.close();
    
    // Release lock and load from parsed contracts
    locker.unlock();
    return loadFromContracts(contracts);
}

bool BSEFORepository::loadProcessedCSV(const QString& filename) {
    QWriteLocker locker(&m_mutex);
    
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }
    
    QTextStream in(&file);
    QString header = in.readLine();  // Skip header
    
    QVector<MasterContract> contracts;
    
    while (!in.atEnd()) {
        QString line = in.readLine();
        QStringList fields = line.split(',');
        
        if (fields.size() < 16) continue;
        
        MasterContract contract;
        contract.exchange = "BSEFO";
        contract.exchangeInstrumentID = fields[0].toLongLong();
        contract.name = trimQuotes(fields[1]);
        contract.displayName = trimQuotes(fields[2]);
        contract.description = trimQuotes(fields[3]);
        contract.series = trimQuotes(fields[4]);
        contract.lotSize = fields[5].toInt();
        contract.tickSize = fields[6].toDouble();
        contract.expiryDate = trimQuotes(fields[7]);
        contract.strikePrice = fields[8].toDouble();
        contract.optionType = fields[9].toInt();
        
        // Detect spread contracts and mark them with a special series
        // Spreads have "SPD" in their display name (e.g., "BANKEX 29JAN25MAR SPD")
        if (contract.displayName.contains("SPD", Qt::CaseInsensitive)) {
            contract.series = "SPREAD";  // Mark as spread for easy filtering
        }
        
        contracts.append(contract);
    }
    
    file.close();
    
    // Release write lock before calling loadFromContracts
    locker.unlock();
    
    bool result = loadFromContracts(contracts);
    
    if (result) {
        qDebug() << "BSE FO Repository loaded from CSV:" << filename << m_contractCount << "contracts";
    }
    
    return result;
}

bool BSEFORepository::loadFromContracts(const QVector<MasterContract>& contracts) {
    if (contracts.isEmpty()) {
        qWarning() << "BSE FO Repository: No contracts to load";
        return false;
    }

    QWriteLocker locker(&m_mutex);
    
    // Clear existing data
    m_tokenToIndex.clear();
    m_contractCount = 0;
    
    // Pre-allocate for efficiency
    const int32_t expectedSize = contracts.size();
    m_token.reserve(expectedSize);
    m_name.reserve(expectedSize);
    m_displayName.reserve(expectedSize);
    m_description.reserve(expectedSize);
    m_series.reserve(expectedSize);
    m_lotSize.reserve(expectedSize);
    m_tickSize.reserve(expectedSize);
    m_expiryDate.reserve(expectedSize);
    m_strikePrice.reserve(expectedSize);
    m_optionType.reserve(expectedSize);
    m_assetToken.reserve(expectedSize);
    m_priceBandHigh.reserve(expectedSize);
    m_priceBandLow.reserve(expectedSize);
    m_tokenToIndex.reserve(expectedSize);
    
    // Load contracts directly from QVector
    for (const MasterContract& contract : contracts) {
        if (contract.exchange != "BSEFO") {
            continue;  // Skip non-BSEFO contracts
        }
        
        // Add to parallel arrays
        m_token.push_back(contract.exchangeInstrumentID);
        m_name.push_back(contract.name);
        m_displayName.push_back(contract.displayName);
        m_description.push_back(contract.description);
        m_series.push_back(contract.series);
        m_lotSize.push_back(contract.lotSize);
        m_tickSize.push_back(contract.tickSize);
        m_expiryDate.push_back(contract.expiryDate);
        m_strikePrice.push_back(contract.strikePrice);
        m_optionType.push_back(contract.optionType);
        m_assetToken.push_back(0);
        m_priceBandHigh.push_back(contract.freezeQty);
        m_priceBandLow.push_back(0);
        
        // Initialize live data arrays
        m_ltp.push_back(0.0);
        m_open.push_back(0.0);
        m_high.push_back(0.0);
        m_low.push_back(0.0);
        m_close.push_back(0.0);
        m_prevClose.push_back(0.0);
        m_volume.push_back(0);
        m_bidPrice.push_back(0.0);
        m_askPrice.push_back(0.0);
        
        // Initialize Greeks
        m_iv.push_back(0.0);
        m_delta.push_back(0.0);
        m_gamma.push_back(0.0);
        m_vega.push_back(0.0);
        m_theta.push_back(0.0);
        
        // Add to token index
        m_tokenToIndex.insert(contract.exchangeInstrumentID, m_contractCount);
        m_contractCount++;
    }
    
    m_loaded = (m_contractCount > 0);
    
    qDebug() << "BSE FO Repository loaded from contracts:" << m_contractCount << "contracts";
    return m_contractCount > 0;
}

bool BSEFORepository::saveProcessedCSV(const QString& filename) const {
    QReadLocker locker(&m_mutex);
    
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Failed to open file for writing:" << filename;
        return false;
    }
    
    QTextStream out(&file);
    
    // Write header
    out << "Token,Symbol,DisplayName,Description,Series,LotSize,TickSize,ExpiryDate,StrikePrice,OptionType,UnderlyingSymbol,AssetToken,PriceBandHigh,PriceBandLow,LTP,Open,High,Low,Close,PrevClose,Volume,IV,Delta,Gamma,Vega,Theta\n";
    
    // Write all contracts
    for (int32_t idx = 0; idx < m_contractCount; ++idx) {
        out << m_token[idx] << ","
            << m_name[idx] << ","
            << m_displayName[idx] << ","
            << m_description[idx] << ","
            << m_series[idx] << ","
            << m_lotSize[idx] << ","
            << m_tickSize[idx] << ","
            << m_expiryDate[idx] << ","
            << m_strikePrice[idx] << ","
            << m_optionType[idx] << ","
            << m_name[idx] << ","  // UnderlyingSymbol
            << m_assetToken[idx] << ","
            << m_priceBandHigh[idx] << ","
            << m_priceBandLow[idx] << ","
            << "0,0,0,0,0,0,0,"  // Live data (not persisted)
            << "0,0,0,0,0\n";    // Greeks (not persisted)
    }
    
    file.close();
    qDebug() << "BSE FO Repository saved to CSV:" << filename << m_contractCount << "contracts";
    return true;
}

const ContractData* BSEFORepository::getContract(int64_t token) const {
    QReadLocker locker(&m_mutex);
    
    auto it = m_tokenToIndex.constFind(token);
    if (it == m_tokenToIndex.constEnd()) {
        return nullptr;
    }
    
    int32_t idx = it.value();
    
    // Create temporary ContractData from arrays
    static thread_local ContractData tempContract;
    tempContract.exchangeInstrumentID = token;
    tempContract.name = m_name[idx];
    tempContract.displayName = m_displayName[idx];
    tempContract.description = m_description[idx];
    tempContract.series = m_series[idx];
    tempContract.lotSize = m_lotSize[idx];
    tempContract.tickSize = m_tickSize[idx];
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
    
    // BSE F&O Option Type Mapping
    if (m_optionType[idx] == 3) tempContract.optionType = "CE";
    else if (m_optionType[idx] == 4) tempContract.optionType = "PE";
    else tempContract.optionType = "XX";
    
    tempContract.iv = m_iv[idx];
    tempContract.delta = m_delta[idx];
    tempContract.gamma = m_gamma[idx];
    tempContract.vega = m_vega[idx];
    tempContract.theta = m_theta[idx];
    
    return &tempContract;
}

bool BSEFORepository::hasContract(int64_t token) const {
    QReadLocker locker(&m_mutex);
    return m_tokenToIndex.contains(token);
}

QVector<ContractData> BSEFORepository::getAllContracts() const {
    QReadLocker locker(&m_mutex);
    
    QVector<ContractData> contracts;
    contracts.reserve(m_contractCount);
    
    for (int32_t idx = 0; idx < m_contractCount; ++idx) {
        ContractData contract;
        contract.exchangeInstrumentID = m_token[idx];
        contract.name = m_name[idx];
        contract.displayName = m_displayName[idx];
        contract.series = m_series[idx];
        contract.lotSize = m_lotSize[idx];
        contract.expiryDate = m_expiryDate[idx];
        contract.strikePrice = m_strikePrice[idx];
        
        if (m_optionType[idx] == 3) contract.optionType = "CE";
        else if (m_optionType[idx] == 4) contract.optionType = "PE";
        else contract.optionType = "XX";
        
        contracts.append(contract);
    }
    
    return contracts;
}

QVector<ContractData> BSEFORepository::getContractsBySeries(const QString& series) const {
    QReadLocker locker(&m_mutex);
    
    QVector<ContractData> contracts;
    
    for (int32_t idx = 0; idx < m_contractCount; ++idx) {
        if (m_series[idx] == series) {
            ContractData contract;
            contract.exchangeInstrumentID = m_token[idx];
            contract.name = m_name[idx];
            contract.displayName = m_displayName[idx];
            contract.series = m_series[idx];
            contract.lotSize = m_lotSize[idx];
            contract.expiryDate = m_expiryDate[idx];
            contract.strikePrice = m_strikePrice[idx];
            
            if (m_optionType[idx] == 3) contract.optionType = "CE";
            else if (m_optionType[idx] == 4) contract.optionType = "PE";
            else contract.optionType = "XX";
            
            contracts.append(contract);
        }
    }
    
    return contracts;
}

QVector<ContractData> BSEFORepository::getContractsBySymbol(const QString& symbol) const {
    QReadLocker locker(&m_mutex);
    
    QVector<ContractData> contracts;
    
    for (int32_t idx = 0; idx < m_contractCount; ++idx) {
        if (m_name[idx].contains(symbol, Qt::CaseInsensitive)) {
            ContractData contract;
            contract.exchangeInstrumentID = m_token[idx];
            contract.name = m_name[idx];
            contract.displayName = m_displayName[idx];
            contract.series = m_series[idx];
            contract.lotSize = m_lotSize[idx];
            contract.expiryDate = m_expiryDate[idx];
            contract.strikePrice = m_strikePrice[idx];
            
            if (m_optionType[idx] == 3) contract.optionType = "CE";
            else if (m_optionType[idx] == 4) contract.optionType = "PE";
            else contract.optionType = "XX";
            
            contracts.append(contract);
        }
    }
    
    return contracts;
}

void BSEFORepository::updateLiveData(int64_t token, double ltp, double open, double high, 
                                     double low, double close, double prevClose, int64_t volume) {
    QWriteLocker locker(&m_mutex);
    
    auto it = m_tokenToIndex.constFind(token);
    if (it == m_tokenToIndex.constEnd()) {
        return;
    }
    
    int32_t idx = it.value();
    m_ltp[idx] = ltp;
    m_open[idx] = open;
    m_high[idx] = high;
    m_low[idx] = low;
    m_close[idx] = close;
    m_prevClose[idx] = prevClose;
    m_volume[idx] = volume;
}

void BSEFORepository::updateGreeks(int64_t token, double iv, double delta, double gamma, 
                                   double vega, double theta) {
    QWriteLocker locker(&m_mutex);
    
    auto it = m_tokenToIndex.constFind(token);
    if (it == m_tokenToIndex.constEnd()) {
        return;
    }
    
    int32_t idx = it.value();
    m_iv[idx] = iv;
    m_delta[idx] = delta;
    m_gamma[idx] = gamma;
    m_vega[idx] = vega;
    m_theta[idx] = theta;
}
