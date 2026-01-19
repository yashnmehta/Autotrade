#include "repository/BSECMRepository.h"
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

BSECMRepository::BSECMRepository()
    : m_contractCount(0)
    , m_loaded(false)
{
}

BSECMRepository::~BSECMRepository() = default;

bool BSECMRepository::loadMasterFile(const QString& filename) {
    QWriteLocker locker(&m_mutex);
    
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to open BSE CM master file:" << filename;
        return false;
    }
    
    QVector<MasterContract> contracts;
    QTextStream in(&file);
    
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;
        
        MasterContract contract;
        if (MasterFileParser::parseLine(line, "BSECM", contract)) {
            contracts.append(contract);
        }
    }
    
    file.close();
    
    // Load from parsed contracts
    return loadFromContracts(contracts);
}

bool BSECMRepository::loadProcessedCSV(const QString& filename) {
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
        
        if (fields.size() < 11) continue;
        
        MasterContract contract;
        contract.exchange = "BSECM";
        contract.exchangeInstrumentID = fields[0].toLongLong();
        contract.name = trimQuotes(fields[1]);
        contract.displayName = trimQuotes(fields[2]);
        contract.description = trimQuotes(fields[3]);
        contract.series = trimQuotes(fields[4]);
        contract.lotSize = fields[5].toInt();
        contract.tickSize = fields[6].toDouble();
        contract.priceBandHigh = fields[7].toDouble();
        contract.priceBandLow = fields[8].toDouble();
        
        contracts.append(contract);
    }
    
    file.close();
    
    // Release write lock before calling loadFromContracts
    locker.unlock();
    
    bool result = loadFromContracts(contracts);
    
    if (result) {
        qDebug() << "BSE CM Repository loaded from CSV:" << filename << m_contractCount << "contracts";
    }
    
    return result;
}

bool BSECMRepository::loadFromContracts(const QVector<MasterContract>& contracts) {
    if (contracts.isEmpty()) {
        qWarning() << "BSE CM Repository: No contracts to load";
        return false;
    }

    QWriteLocker locker(&m_mutex);
    
    // Clear existing data - MUST clear all arrays!
    m_tokenToIndex.clear();
    m_contractCount = 0;
    
    // Clear all parallel arrays to avoid stale data
    m_token.clear();
    m_name.clear();
    m_displayName.clear();
    m_description.clear();
    m_series.clear();
    m_lotSize.clear();
    m_tickSize.clear();
    m_priceBandHigh.clear();
    m_priceBandLow.clear();
    m_ltp.clear();
    m_open.clear();
    m_high.clear();
    m_low.clear();
    m_close.clear();
    m_prevClose.clear();
    m_volume.clear();
    
    // Pre-allocate for efficiency
    const int32_t expectedSize = contracts.size();
    m_token.reserve(expectedSize);
    m_name.reserve(expectedSize);
    m_displayName.reserve(expectedSize);
    m_description.reserve(expectedSize);
    m_series.reserve(expectedSize);
    m_lotSize.reserve(expectedSize);
    m_tickSize.reserve(expectedSize);
    m_priceBandHigh.reserve(expectedSize);
    m_priceBandLow.reserve(expectedSize);
    m_tokenToIndex.reserve(expectedSize);
    
    // Load contracts directly from QVector
    for (const MasterContract& contract : contracts) {
        if (contract.exchange != "BSECM") {
            continue;  // Skip non-BSECM contracts
        }
        
        // Add to parallel arrays
        m_token.push_back(contract.exchangeInstrumentID);
        m_name.push_back(contract.name);
        m_displayName.push_back(contract.displayName);
        m_description.push_back(contract.description);
        m_series.push_back(contract.series);
        m_lotSize.push_back(contract.lotSize);
        m_tickSize.push_back(contract.tickSize);
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
        
        // Add to token index
        m_tokenToIndex.insert(contract.exchangeInstrumentID, m_contractCount);
        m_contractCount++;
    }
    
    m_loaded = (m_contractCount > 0);
    
    qDebug() << "BSE CM Repository loaded from contracts:" << m_contractCount << "contracts";
    return m_contractCount > 0;
}

bool BSECMRepository::saveProcessedCSV(const QString& filename) const {
    QReadLocker locker(&m_mutex);
    
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Failed to open file for writing:" << filename;
        return false;
    }
    
    QTextStream out(&file);
    
    // Write header
    out << "Token,Symbol,DisplayName,Description,Series,LotSize,TickSize,PriceBandHigh,PriceBandLow,LTP,Open,High,Low,Close,PrevClose,Volume\n";
    
    // Write all contracts
    for (int32_t idx = 0; idx < m_contractCount; ++idx) {
        out << m_token[idx] << ","
            << m_name[idx] << ","
            << m_displayName[idx] << ","
            << m_description[idx] << ","
            << m_series[idx] << ","
            << m_lotSize[idx] << ","
            << m_tickSize[idx] << ","
            << m_priceBandHigh[idx] << ","
            << m_priceBandLow[idx] << ","
            << "0,0,0,0,0,0,0\n";  // Live data not persisted
    }
    
    file.close();
    qDebug() << "BSE CM Repository saved to CSV:" << filename << m_contractCount << "contracts";
    return true;
}

const ContractData* BSECMRepository::getContract(int64_t token) const {
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
    tempContract.scripCode = QString::number(token); 
    
    return &tempContract;
}

bool BSECMRepository::hasContract(int64_t token) const {
    QReadLocker locker(&m_mutex);
    return m_tokenToIndex.contains(token);
}

QVector<ContractData> BSECMRepository::getAllContracts() const {
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
        
        contracts.append(contract);
    }
    
    return contracts;
}

QVector<ContractData> BSECMRepository::getContractsBySeries(const QString& series) const {
    QReadLocker locker(&m_mutex);
    
    QVector<ContractData> contracts;
    
    for (int32_t idx = 0; idx < m_contractCount; ++idx) {
        // If series is empty, return all contracts (used for BSE equity search)
        // Otherwise, match exact series
        if (series.isEmpty() || m_series[idx] == series) {
            ContractData contract;
            contract.exchangeInstrumentID = m_token[idx];
            contract.name = m_name[idx];
            contract.displayName = m_displayName[idx];
            contract.series = m_series[idx];
            contract.lotSize = m_lotSize[idx];
            contract.scripCode = QString::number(m_token[idx]);  // BSE scrip code = token
            
            contracts.append(contract);
        }
    }
    
    return contracts;
}

QVector<ContractData> BSECMRepository::getContractsBySymbol(const QString& symbol) const {
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
            
            contracts.append(contract);
        }
    }
    
    return contracts;
}

void BSECMRepository::updateLiveData(int64_t token, double ltp, double open, double high, 
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

void BSECMRepository::prepareForLoad() {
  QWriteLocker locker(&m_mutex);
  m_contractCount = 0;
  m_tokenToIndex.clear();

  m_token.clear();
  m_name.clear();
  m_displayName.clear();
  m_description.clear();
  m_series.clear();

  m_lotSize.clear();
  m_tickSize.clear();

  m_priceBandHigh.clear();
  m_priceBandLow.clear();

  m_ltp.clear();
  m_open.clear();
  m_high.clear();
  m_low.clear();
  m_close.clear();
  m_prevClose.clear();
  m_volume.clear();

  m_loaded = false;
}

void BSECMRepository::addContract(
    const MasterContract &contract,
    std::function<QString(const QString &)> internFunc) {

  QWriteLocker locker(&m_mutex);

  // Use supplied interner or identity if null
  auto intern = internFunc ? internFunc : [](const QString &s) { return s; };

  int32_t idx = m_contractCount;
  m_tokenToIndex.insert(contract.exchangeInstrumentID, idx);

  m_token.append(contract.exchangeInstrumentID);
  m_name.append(intern(contract.name));
  m_displayName.append(contract.displayName);
  m_description.append(contract.description);
  m_series.append(intern(contract.series));

  m_lotSize.append(contract.lotSize);
  m_tickSize.append(contract.tickSize);

  m_priceBandHigh.append(contract.priceBandHigh);
  m_priceBandLow.append(contract.priceBandLow);

  // Initialize dynamic fields
  m_ltp.append(0.0);
  m_open.append(0.0);
  m_high.append(0.0);
  m_low.append(0.0);
  m_close.append(0.0);
  m_prevClose.append(0.0);
  m_volume.append(0);

  m_contractCount++;
}
