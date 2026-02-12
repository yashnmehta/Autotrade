#include "repository/BSECMRepository.h"
#include "repository/MasterFileParser.h"
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

// Helper function to remove surrounding quotes from field values
static QString trimQuotes(const QStringView &str) {
  QStringView trimmed = str.trimmed();
  if (trimmed.startsWith('"') && trimmed.endsWith('"') &&
      trimmed.length() >= 2) {
    return trimmed.mid(1, trimmed.length() - 2).toString();
  }
  return trimmed.toString();
}

BSECMRepository::BSECMRepository() : m_contractCount(0), m_loaded(false) {}

BSECMRepository::~BSECMRepository() = default;

bool BSECMRepository::loadMasterFile(const QString &filename) {
  QWriteLocker locker(&m_mutex);

  // Native C++ file I/O (5-10x faster than QFile)
  std::ifstream file(filename.toStdString(), std::ios::binary);
  if (!file.is_open()) {
    qWarning() << "Failed to open BSE CM master file:" << filename;
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
    if (MasterFileParser::parseLine(QStringView(qLine), "BSECM", contract)) {
      addContractInternal(contract, [](const QString &s) { return s; });
    }
  }

  file.close();
  finalizeLoad();
  return m_loaded;
}

bool BSECMRepository::loadProcessedCSV(const QString &filename) {
  QWriteLocker locker(&m_mutex);

  QFile file(filename);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return false;
  }

  prepareForLoad();
  MasterContract contract;
  QTextStream in(&file);
  QString header = in.readLine(); // Skip header

  int lineCount = 0;
  while (!in.atEnd()) {
    QString qLine = in.readLine();
    lineCount++;
    if (lineCount % 1000 == 0)
      qDebug() << "[BSECM] Loaded lines:" << lineCount;

    // Optimized split without QStringList
    QVector<QStringView> fields;
    int start = 0;
    int end = 0;
    while ((end = qLine.indexOf(',', start)) != -1) {
      fields.append(QStringView(qLine).mid(start, end - start));
      start = end + 1;
    }
    fields.append(QStringView(qLine).mid(start));

    if (fields.size() < 11)
      continue;

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
    contract.instrumentType = fields[fields.size() - 1].toInt();

    addContractInternal(contract, [](const QString &s) { return s; });
  }

  file.close();
  finalizeLoad();

  qDebug() << "BSE CM Repository loaded from CSV:" << filename
           << m_contractCount << "contracts";
  return m_loaded;
}

void BSECMRepository::finalizeLoad() {
  m_loaded = (m_contractCount > 0);

  // Squeeze all parallel arrays
  m_token.squeeze();
  m_name.squeeze();
  m_displayName.squeeze();
  m_description.squeeze();
  m_series.squeeze();
  m_lotSize.squeeze();
  m_tickSize.squeeze();
  m_priceBandHigh.squeeze();
  m_priceBandLow.squeeze();
  m_ltp.squeeze();
  m_open.squeeze();
  m_high.squeeze();
  m_low.squeeze();
  m_close.squeeze();
  m_prevClose.squeeze();
  m_volume.squeeze();
}

bool BSECMRepository::loadFromContracts(
    const QVector<MasterContract> &contracts) {
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
  for (const MasterContract &contract : contracts) {
    if (contract.exchange != "BSECM") {
      continue; // Skip non-BSECM contracts
    }

    // Add to parallel arrays
    m_token.push_back(contract.exchangeInstrumentID);
    m_name.push_back(contract.name);
    m_displayName.push_back(contract.displayName);
    m_description.push_back(contract.description);
    m_series.push_back(contract.series);
    m_lotSize.push_back(contract.lotSize);
    m_tickSize.push_back(contract.tickSize);
    m_priceBandHigh.push_back(contract.priceBandHigh);
    m_priceBandLow.push_back(contract.priceBandLow);

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

  qDebug() << "BSE CM Repository loaded from contracts:" << m_contractCount
           << "contracts";
  return m_contractCount > 0;
}

bool BSECMRepository::saveProcessedCSV(const QString &filename) const {
  QReadLocker locker(&m_mutex);

  QFile file(filename);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    qWarning() << "Failed to open file for writing:" << filename;
    return false;
  }

  QTextStream out(&file);

  // Write header
  out << "Token,Symbol,DisplayName,Description,Series,LotSize,TickSize,"
         "PriceBandHigh,PriceBandLow,LTP,Open,High,Low,Close,PrevClose,"
         "Volume\n";

  // Write all contracts
  for (int32_t idx = 0; idx < m_contractCount; ++idx) {
    out << m_token[idx] << "," << m_name[idx] << "," << m_displayName[idx]
        << "," << m_description[idx] << "," << m_series[idx] << ","
        << m_lotSize[idx] << "," << m_tickSize[idx] << ","
        << m_priceBandHigh[idx] << "," << m_priceBandLow[idx] << ","
        << "0,0,0,0,0,0,0\n"; // Live data not persisted
  }

  file.close();
  qDebug() << "BSE CM Repository saved to CSV:" << filename << m_contractCount
           << "contracts";
  return true;
}

const ContractData *BSECMRepository::getContract(int64_t token) const {
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

QVector<ContractData>
BSECMRepository::getContractsBySeries(const QString &series) const {
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
      contract.scripCode =
          QString::number(m_token[idx]); // BSE scrip code = token

      contracts.append(contract);
    }
  }

  return contracts;
}

QVector<ContractData>
BSECMRepository::getContractsBySymbol(const QString &symbol) const {
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

QStringList BSECMRepository::getUniqueSymbols(const QString &series) const {
  QReadLocker locker(&m_mutex);

  // For series filter, always compute fresh (rare use case)
  if (!series.isEmpty()) {
    QSet<QString> symbolSet;
    for (int32_t idx = 0; idx < m_contractCount; ++idx) {
      if (m_series[idx] == series && !m_name[idx].isEmpty()) {
        symbolSet.insert(m_name[idx]);
      }
    }
    QStringList symbols = symbolSet.values();
    symbols.sort();
    return symbols;
  }

  // For all symbols, use lazy cache (common use case - uniform pattern)
  if (!m_symbolsCached) {
    QSet<QString> symbolSet;
    for (int32_t idx = 0; idx < m_contractCount; ++idx) {
      if (!m_name[idx].isEmpty()) {
        symbolSet.insert(m_name[idx]);
      }
    }
    m_cachedUniqueSymbols = symbolSet.values();
    m_cachedUniqueSymbols.sort();
    m_symbolsCached = true;
  }

  return m_cachedUniqueSymbols;
}

void BSECMRepository::updateLiveData(int64_t token, double ltp, double open,
                                     double high, double low, double close,
                                     double prevClose, int64_t volume) {
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

  // Invalidate cached symbols (lazy cache)
  m_symbolsCached = false;
  m_cachedUniqueSymbols.clear();

  m_loaded = false;
}

void BSECMRepository::addContract(
    const MasterContract &contract,
    std::function<QString(const QString &)> internFunc) {
  QWriteLocker locker(&m_mutex);
  auto intern = internFunc ? internFunc : [](const QString &s) { return s; };
  addContractInternal(contract, intern);
}

void BSECMRepository::addContractInternal(
    const MasterContract &contract,
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

void BSECMRepository::forEachContract(
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
    data.priceBandHigh = m_priceBandHigh[i];
    data.priceBandLow = m_priceBandLow[i];
    data.ltp = m_ltp[i];
    data.open = m_open[i];
    data.high = m_high[i];
    data.low = m_low[i];
    data.close = m_close[i];
    data.prevClose = m_prevClose[i];
    data.volume = m_volume[i];
    data.scripCode = QString::number(data.exchangeInstrumentID);

    // Default F&O fields for EQ
    data.expiryDate = "";
    data.strikePrice = 0.0;
    data.optionType = "EQ";
    data.assetToken = 0;
    data.instrumentType = 0;

    callback(data);
  }
}
