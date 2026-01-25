#include "repository/NSECMRepository.h"
#include "repository/MasterFileParser.h"
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <fstream>
#include <sstream>
#include <string>
#include <iostream>

// Helper function to remove surrounding quotes from field values
static QString trimQuotes(const QStringRef &str) {
    QStringRef trimmed = str.trimmed();
    if (trimmed.startsWith('"') && trimmed.endsWith('"') && trimmed.length() >= 2) {
        return trimmed.mid(1, trimmed.length() - 2).toString();
    }
    return trimmed.toString();
}

NSECMRepository::NSECMRepository()
    : m_contractCount(0)
    , m_loaded(false)
{
}

NSECMRepository::~NSECMRepository() = default;

void NSECMRepository::allocateArrays(int32_t count) {
    // Token tracking
    m_token.resize(count);
    
    // Security Master Data (4 fields)
    m_name.resize(count);
    m_displayName.resize(count);
    m_description.resize(count);
    m_series.resize(count);
    
    // Trading Parameters (3 fields)
    m_lotSize.resize(count);
    m_tickSize.resize(count);
    m_freezeQty.resize(count);
    
    // Price Bands (2 fields)
    m_priceBandHigh.resize(count);
    m_priceBandLow.resize(count);
    
    // Live Market Data (7 fields)
    m_ltp.resize(count);
    m_open.resize(count);
    m_high.resize(count);
    m_low.resize(count);
    m_close.resize(count);
    m_prevClose.resize(count);
    m_volume.resize(count);
    
    // WebSocket-only fields
    m_bidPrice.resize(count);
    m_askPrice.resize(count);
}

void NSECMRepository::addContractInternal(const MasterContract &contract, 
                                        std::function<QString(const QString &)> intern) {
  int32_t idx = m_contractCount;
  m_tokenToIndex.insert(contract.exchangeInstrumentID, idx);

  m_token.append(contract.exchangeInstrumentID);
  m_name.append(intern(contract.name));
  m_displayName.append(contract.displayName);
  m_description.append(contract.description);
  m_series.append(intern(contract.series));

  m_lotSize.append(contract.lotSize);
  m_tickSize.append(contract.tickSize);
  m_freezeQty.append(contract.freezeQty);

  m_priceBandHigh.append(contract.priceBandHigh);
  m_priceBandLow.append(contract.priceBandLow);

  // Initialize dynamic fields to 0
  m_ltp.append(0.0);
  m_open.append(0.0);
  m_high.append(0.0);
  m_low.append(0.0);
  m_close.append(0.0);
  m_prevClose.append(0.0);
  m_volume.append(0);

  m_bidPrice.append(0.0);
  m_askPrice.append(0.0);

  m_contractCount++;
}

bool NSECMRepository::loadMasterFile(const QString &filename) {
  QWriteLocker locker(&m_mutex);

  // Native C++ file I/O (5-10x faster than QFile)
  std::ifstream file(filename.toStdString(), std::ios::binary);
  if (!file.is_open()) {
    std::cerr << "Failed to open NSECM master file: " << filename.toStdString()
              << std::endl;
    return false;
  }

  prepareForLoad();

  std::string line;
  line.reserve(1024);
  MasterContract contract;
  QString qLine;

  while (std::getline(file, line)) {
    if (line.empty() || line[0] == '\r') {
      continue;
    }

    // Remove trailing whitespace
    while (!line.empty() &&
           (line.back() == ' ' || line.back() == '\r' || line.back() == '\n')) {
      line.pop_back();
    }

    if (line.empty()) {
      continue;
    }

    qLine = QString::fromStdString(line);
    if (MasterFileParser::parseLine(QStringRef(&qLine), "NSECM", contract)) {
      addContractInternal(contract, [](const QString &s) { return s; });
    }
  }

  file.close();
  finalizeLoad();
  return m_loaded;
}

bool NSECMRepository::loadProcessedCSV(const QString &filename) {
  QWriteLocker locker(&m_mutex);

  // Native C++ file I/O (10x faster than QFile/QTextStream)
  std::ifstream file(filename.toStdString(), std::ios::binary);
  if (!file.is_open()) {
    std::cerr << "Failed to open NSECM CSV file: " << filename.toStdString()
              << std::endl;
    return false;
  }

  prepareForLoad();

  std::string line;
  line.reserve(2048);
  MasterContract contract;
  QString qLine;

  // Skip header line
  std::getline(file, line);

  int lineCount = 0;
  while (std::getline(file, line)) {
    lineCount++;
    if (lineCount % 1000 == 0) qDebug() << "[NSECM] Loaded lines:" << lineCount;
    if (line.empty()) {
      continue;
    }

    qLine = QString::fromStdString(line);

    // Optimized split without QStringList
    QVector<QStringRef> fields;
    int start = 0;
    int end = 0;
    while ((end = qLine.indexOf(',', start)) != -1) {
      fields.append(qLine.midRef(start, end - start));
      start = end + 1;
    }
    fields.append(qLine.midRef(start));

    if (fields.size() < 17) {
      continue;
    }

    bool ok;
    int64_t token = fields[0].toLongLong(&ok);
    if (!ok) {
      continue;
    }

    contract.exchange = "NSECM";
    contract.exchangeInstrumentID = token;
    contract.name = trimQuotes(fields[1]);
    contract.displayName = trimQuotes(fields[2]);
    contract.description = trimQuotes(fields[3]);
    contract.series = trimQuotes(fields[4]);
    contract.lotSize = fields[5].toInt();
    contract.tickSize = fields[6].toDouble();
    contract.priceBandHigh = fields[8].toDouble();
    contract.priceBandLow = fields[9].toDouble();
    contract.instrumentType = fields[fields.size() - 1].toInt();

    addContractInternal(contract, [](const QString &s) { return s; });
  }

  file.close();
  finalizeLoad();

  qDebug() << "NSE CM Repository loaded from CSV:" << m_contractCount
           << "contracts";
  return m_loaded;
}

int32_t NSECMRepository::getIndex(int64_t token) const {
    auto it = m_tokenToIndex.find(token);
    if (it != m_tokenToIndex.end()) {
        return it.value();
    }
    return -1;
}

const ContractData* NSECMRepository::getContract(int64_t token) const {
    QReadLocker locker(&m_mutex);
    
    int32_t idx = getIndex(token);
    if (idx < 0) {
        return nullptr;
    }
    
    // Create temporary ContractData from arrays
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
    
    return &tempContract;
}

bool NSECMRepository::hasContract(int64_t token) const {
    QReadLocker locker(&m_mutex);
    return m_tokenToIndex.contains(token);
}

void NSECMRepository::updateLiveData(int64_t token, double ltp, int64_t volume) {
    QWriteLocker locker(&m_mutex);
    
    int32_t idx = getIndex(token);
    if (idx >= 0) {
        m_ltp[idx] = ltp;
        m_volume[idx] = volume;
    }
}

void NSECMRepository::updateBidAsk(int64_t token, double bidPrice, double askPrice) {
    QWriteLocker locker(&m_mutex);
    
    int32_t idx = getIndex(token);
    if (idx >= 0) {
        m_bidPrice[idx] = bidPrice;
        m_askPrice[idx] = askPrice;
    }
}

void NSECMRepository::updateOHLC(int64_t token, double open, double high, double low, double close) {
    QWriteLocker locker(&m_mutex);
    
    int32_t idx = getIndex(token);
    if (idx >= 0) {
        m_open[idx] = open;
        m_high[idx] = high;
        m_low[idx] = low;
        m_close[idx] = close;
    }
}

QVector<ContractData> NSECMRepository::getAllContracts() const {
    QReadLocker locker(&m_mutex);
    
    QVector<ContractData> contracts;
    contracts.reserve(m_contractCount);
    
    for (int32_t idx = 0; idx < m_contractCount; ++idx) {
        ContractData contract;
        contract.exchangeInstrumentID = m_token[idx];
        contract.name = m_name[idx];
        contract.displayName = m_displayName[idx];
        contract.description = m_description[idx];
        contract.series = m_series[idx];
        contract.lotSize = m_lotSize[idx];
        contract.tickSize = m_tickSize[idx];
        contract.freezeQty = m_freezeQty[idx];
        contract.priceBandHigh = m_priceBandHigh[idx];
        contract.priceBandLow = m_priceBandLow[idx];
        contract.ltp = m_ltp[idx];
        contract.volume = m_volume[idx];
        
        contracts.append(contract);
    }
    
    return contracts;
}

QVector<ContractData> NSECMRepository::getContractsBySeries(const QString& series) const {
    QReadLocker locker(&m_mutex);
    
    QVector<ContractData> contracts;
    
    for (int32_t idx = 0; idx < m_contractCount; ++idx) {
        // If series is empty, return all contracts (used for equity search across all series)
        // Otherwise, match exact series
        if (series.isEmpty() || m_series[idx] == series) {
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

QVector<ContractData> NSECMRepository::getContractsBySymbol(const QString& symbol) const {
    QReadLocker locker(&m_mutex);
    
    QVector<ContractData> contracts;
    
    for (int32_t idx = 0; idx < m_contractCount; ++idx) {
        if (m_name[idx] == symbol) {
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

bool NSECMRepository::loadFromContracts(const QVector<MasterContract>& contracts) {
    if (contracts.isEmpty()) {
        qWarning() << "NSE CM Repository: No contracts to load";
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
    m_freezeQty.clear();
    m_priceBandHigh.clear();
    m_priceBandLow.clear();
    m_ltp.clear();
    m_open.clear();
    m_high.clear();
    m_low.clear();
    m_close.clear();
    m_prevClose.clear();
    m_volume.clear();
    m_bidPrice.clear();
    m_askPrice.clear();
    
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
        if (contract.exchange != "NSECM") {
            continue;  // Skip non-NSECM contracts
        }
        
        // Add to parallel arrays
        m_token.push_back(contract.exchangeInstrumentID);
        m_name.push_back(contract.name);
        m_displayName.push_back(contract.displayName);
        m_description.push_back(contract.description);
        m_series.push_back(contract.series);
        m_lotSize.push_back(contract.lotSize);
        m_tickSize.push_back(contract.tickSize);
        m_priceBandHigh.push_back(contract.freezeQty);  // Map from existing field
        m_priceBandLow.push_back(0);  // Not in master file
        
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
    
    qDebug() << "NSE CM Repository loaded from contracts:" << m_contractCount << "contracts";
    return m_contractCount > 0;
}

bool NSECMRepository::saveProcessedCSV(const QString& filename) const {
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
            << m_priceBandLow[idx] << ",";
        out << "0,0,0,0,0,0,0\n";  // Live data not persisted
    }
    
    file.close();
    qDebug() << "NSE CM Repository saved to CSV:" << filename << m_contractCount << "contracts";
    return true;
}

void NSECMRepository::forEachContract(
    std::function<void(const ContractData &)> callback) const {
  QReadLocker locker(&m_mutex);
  ContractData data;
  for (int32_t i = 0; i < m_contractCount; ++i) {
    data.exchangeInstrumentID = m_token[i];
    data.name = m_name[i];
    data.displayName = m_displayName[i];
    data.description = m_description[i];
    data.series = m_series[i];
    data.lotSize = m_lotSize[i];
    data.tickSize = m_tickSize[i];
    data.freezeQty = m_freezeQty[i];
    data.priceBandHigh = m_priceBandHigh[i];
    data.priceBandLow = m_priceBandLow[i];
    data.ltp = m_ltp[i];
    data.open = m_open[i];
    data.high = m_high[i];
    data.low = m_low[i];
    data.close = m_close[i];
    data.prevClose = m_prevClose[i];
    data.volume = m_volume[i];
    data.bidPrice = m_bidPrice[i];
    data.askPrice = m_askPrice[i];
    
    // Default F&O fields for EQ
    data.expiryDate = "";
    data.strikePrice = 0.0;
    data.optionType = "EQ";
    data.assetToken = 0;
    data.instrumentType = 0;

    callback(data);
  }
}

void NSECMRepository::prepareForLoad() {
  m_contractCount = 0;
  m_tokenToIndex.clear();

  m_token.clear();
  m_name.clear();
  m_displayName.clear();
  m_description.clear();
  m_series.clear();

  m_lotSize.clear();
  m_tickSize.clear();
  m_freezeQty.clear();

  m_priceBandHigh.clear();
  m_priceBandLow.clear();

  m_ltp.clear();
  m_open.clear();
  m_high.clear();
  m_low.clear();
  m_close.clear();
  m_prevClose.clear();
  m_volume.clear();

  m_bidPrice.clear();
  m_askPrice.clear();

  m_loaded = false;
}

void NSECMRepository::finalizeLoad() {
  m_loaded = (m_contractCount > 0);
  
  // Build index name map for indices
  buildIndexNameMap();

  // Squeeze all parallel arrays to return unused capacity to the OS
  m_token.squeeze();
  m_name.squeeze();
  m_displayName.squeeze();
  m_description.squeeze();
  m_series.squeeze();
  m_lotSize.squeeze();
  m_tickSize.squeeze();
  m_freezeQty.squeeze();
  m_priceBandHigh.squeeze();
  m_priceBandLow.squeeze();
  m_ltp.squeeze();
  m_open.squeeze();
  m_high.squeeze();
  m_low.squeeze();
  m_close.squeeze();
  m_prevClose.squeeze();
  m_volume.squeeze();
  m_bidPrice.squeeze();
  m_askPrice.squeeze();

  qDebug() << "NSE CM Repository finalized:" << m_contractCount << "contracts";
}

void NSECMRepository::addContract(
    const MasterContract &contract,
    std::function<QString(const QString &)> internFunc) {

  QWriteLocker locker(&m_mutex);
  auto intern = internFunc ? internFunc : [](const QString &s) { return s; };
  addContractInternal(contract, intern);
}

void NSECMRepository::appendContracts(const QVector<ContractData>& contracts) {
    QWriteLocker lock(&m_mutex);
    
    for (const ContractData& contract : contracts) {
        if (m_tokenToIndex.contains(contract.exchangeInstrumentID)) {
            continue; // Skip duplicates
        }
        
        int32_t idx = m_contractCount;
        
        m_token.append(contract.exchangeInstrumentID);
        m_name.append(contract.name);
        m_displayName.append(contract.displayName);
        m_description.append(contract.description);
        m_series.append(contract.series);
        
        m_lotSize.append(contract.lotSize);
        m_tickSize.append(contract.tickSize);
        m_freezeQty.append(contract.freezeQty);
        
        m_priceBandHigh.append(contract.priceBandHigh);
        m_priceBandLow.append(contract.priceBandLow);
        
        // Initialize dynamic fields to 0
        m_ltp.append(0.0);
        m_open.append(0.0);
        m_high.append(0.0);
        m_low.append(0.0);
        m_close.append(0.0);
        m_prevClose.append(0.0);
        m_volume.append(0);
        
        m_bidPrice.append(0.0);
        m_askPrice.append(0.0);
        
        m_tokenToIndex.insert(contract.exchangeInstrumentID, idx);
        m_contractCount++;
        
        // For indices, also map name -> token
        if (contract.series == "INDEX") {
            m_indexNameToToken[contract.name] = contract.exchangeInstrumentID;
        }
    }
    
    qDebug() << "Appended" << contracts.size() << "contracts to NSECM. Total:" << m_contractCount;
}

void NSECMRepository::buildIndexNameMap() {
    // Note: Mutex is already locked if called from finalizeLoad
    // but we use a locker to be safe if called externally
    QWriteLocker lock(&m_mutex);
    
    m_indexNameToToken.clear();
    for (int32_t i = 0; i < m_contractCount; ++i) {
        if (m_series[i] == "INDEX") {
            m_indexNameToToken[m_name[i]] = m_token[i];
        }
    }
    qDebug() << "Built index name map with" << m_indexNameToToken.size() << "entries";
}

QHash<QString, int64_t> NSECMRepository::getIndexNameTokenMap() const {
    QReadLocker lock(&m_mutex);
    return m_indexNameToToken;
}
