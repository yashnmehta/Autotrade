#include "repository/BSEFORepository.h"
#include "repository/MasterFileParser.h"
#include "utils/DateUtils.h"
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <fstream>
#include <iostream>
#include <string>

// Helper function to remove surrounding quotes from CSV field values
static QString trimQuotes(const QStringView &str) {
  QStringView trimmed = str.trimmed();
  if (trimmed.startsWith('"') && trimmed.endsWith('"') &&
      trimmed.length() >= 2) {
    return trimmed.mid(1, trimmed.length() - 2).toString();
  }
  return trimmed.toString();
}

BSEFORepository::BSEFORepository() : m_contractCount(0), m_loaded(false) {}

BSEFORepository::~BSEFORepository() = default;
bool BSEFORepository::loadMasterFile(const QString &filename) {
  QWriteLocker locker(&m_mutex);

  // Native C++ file I/O (5-10x faster than QFile)
  std::ifstream file(filename.toStdString(), std::ios::binary);
  if (!file.is_open()) {
    qWarning() << "Failed to open BSE FO master file:" << filename;
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
    if (MasterFileParser::parseLine(QStringView(qLine), "BSEFO", contract)) {
      addContractInternal(contract, [](const QString &s) { return s; });
    }
  }

  file.close();
  finalizeLoad();
  return m_loaded;
}

bool BSEFORepository::loadProcessedCSV(const QString &filename) {
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
      qDebug() << "[BSEFO] Loaded lines:" << lineCount;

    // Optimized split without QStringList
    QVector<QStringView> fields;
    int start = 0;
    int end = 0;
    while ((end = qLine.indexOf(',', start)) != -1) {
      fields.append(QStringView(qLine).mid(start, end - start));
      start = end + 1;
    }
    fields.append(QStringView(qLine).mid(start));

    if (fields.size() < 16)
      continue;

    contract.exchange = "BSEFO";
    contract.exchangeInstrumentID = fields[0].toLongLong();
    contract.name = trimQuotes(fields[1]);
    contract.displayName = trimQuotes(fields[2]);
    contract.description = trimQuotes(fields[3]);
    contract.series = trimQuotes(fields[4]);
    contract.lotSize = fields[5].toInt();
    contract.tickSize = fields[6].toDouble();

    // ✅ Parse expiry date to DDMMMYYYY format + QDate
    QString rawExpiryDate = trimQuotes(fields[7]);
    QString parsedDate;
    QDate parsedQDate;
    double timeToExpiry;

    if (DateUtils::parseExpiryDate(rawExpiryDate, parsedDate, parsedQDate,
                                   timeToExpiry)) {
      contract.expiryDate = parsedDate;     // DDMMMYYYY format
      contract.expiryDate_dt = parsedQDate; // QDate for sorting
      contract.timeToExpiry = timeToExpiry; // For Greeks calculation
    } else {
      // Parsing failed - use raw value as fallback
      contract.expiryDate = rawExpiryDate;
      contract.expiryDate_dt = QDate();
      contract.timeToExpiry = 0.0;
    }

    contract.strikePrice = fields[8].toDouble();
    contract.instrumentType = fields[fields.size() - 1].toInt();

    // Read OptionType from field 9 (CE/PE/FUT/SPD)
    QString optionTypeStr = trimQuotes(fields[9]);
    if (optionTypeStr == "CE") {
      contract.optionType = 3; // CE
    } else if (optionTypeStr == "PE") {
      contract.optionType = 4; // PE
    } else if (optionTypeStr == "FUT") {
      contract.optionType = 0; // Future
    } else if (optionTypeStr == "SPD") {
      contract.optionType = 0; // Spread
    } else {
      contract.optionType = 0;
    }

    if (contract.instrumentType == 2 && contract.optionType == 0) {
      qWarning()
          << "[BSEFORepo] Detected corrupted CSV. Forcing reload from master.";
      file.close();
      return false;
    }

    addContractInternal(contract, [](const QString &s) { return s; });
  }

  file.close();
  finalizeLoad();

  qDebug() << "BSE FO Repository loaded from CSV:" << filename
           << m_contractCount << "contracts";
  return m_loaded;
}

void BSEFORepository::finalizeLoad() {
  m_loaded = (m_contractCount > 0);

  // Squeeze all parallel arrays
  m_token.squeeze();
  m_name.squeeze();
  m_displayName.squeeze();
  m_description.squeeze();
  m_series.squeeze();
  m_lotSize.squeeze();
  m_tickSize.squeeze();
  m_expiryDate.squeeze();
  m_strikePrice.squeeze();
  m_optionType.squeeze();
  m_assetToken.squeeze();
  m_instrumentType.squeeze();
  m_priceBandHigh.squeeze();
  m_priceBandLow.squeeze();
  m_freezeQty.squeeze();
  m_ltp.squeeze();
  m_open.squeeze();
  m_high.squeeze();
  m_low.squeeze();
  m_close.squeeze();
  m_prevClose.squeeze();
  m_volume.squeeze();
  m_bidPrice.squeeze();
  m_askPrice.squeeze();
  m_iv.squeeze();
  m_delta.squeeze();
  m_gamma.squeeze();
  m_vega.squeeze();
  m_theta.squeeze();
}

bool BSEFORepository::loadFromContracts(
    const QVector<MasterContract> &contracts) {
  if (contracts.isEmpty()) {
    qWarning() << "BSE FO Repository: No contracts to load";
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
  m_expiryDate.clear();
  m_strikePrice.clear();
  m_optionType.clear();
  m_assetToken.clear();
  m_instrumentType.clear();
  m_priceBandHigh.clear();
  m_priceBandLow.clear();
  m_freezeQty.clear();
  m_ltp.clear();
  m_open.clear();
  m_high.clear();
  m_low.clear();
  m_close.clear();
  m_prevClose.clear();
  m_volume.clear();
  m_bidPrice.clear();
  m_askPrice.clear();
  m_iv.clear();
  m_delta.clear();
  m_gamma.clear();
  m_vega.clear();
  m_theta.clear();

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
  for (const MasterContract &contract : contracts) {
    if (contract.exchange != "BSEFO") {
      continue; // Skip non-BSEFO contracts
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

    // Convert optionType - use integer value if set, otherwise fallback to
    // string detection
    if (contract.instrumentType == 4) {
      m_optionType.push_back("SPD");
    } else if (contract.instrumentType == 2) {
      // FACT-BASED: Use optionType numeric field
      // BSE encoding: 1=CE, 2=PE (different from NSE's 3=CE, 4=PE)
      // Also support NSE format for compatibility
      if (contract.optionType == 1 || contract.optionType == 3) {
        m_optionType.push_back("CE");
      } else if (contract.optionType == 2 || contract.optionType == 4) {
        m_optionType.push_back("PE");
      } else {
        // Unknown optionType - this should not happen with valid data
        // qWarning() << "[BSEFORepo] Unknown optionType:" <<
        // contract.optionType
        //            << "for token" << contract.exchangeInstrumentID;
        m_optionType.push_back("XX");
      }
    } else if (contract.instrumentType == 1) {
      m_optionType.push_back("FUT");
    } else {
      m_optionType.push_back("XX");
    }

    m_assetToken.push_back(contract.assetToken);
    m_instrumentType.push_back(contract.instrumentType);
    m_priceBandHigh.push_back(contract.priceBandHigh);
    m_priceBandLow.push_back(contract.priceBandLow);
    m_freezeQty.push_back(contract.freezeQty);

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

  qDebug() << "BSE FO Repository loaded from contracts:" << m_contractCount
           << "contracts";
  return m_contractCount > 0;
}

bool BSEFORepository::saveProcessedCSV(const QString &filename) const {
  QReadLocker locker(&m_mutex);

  QFile file(filename);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    qWarning() << "Failed to open file for writing:" << filename;
    return false;
  }

  QTextStream out(&file);

  // Write header
  out << "Token,Symbol,DisplayName,Description,Series,LotSize,TickSize,"
         "ExpiryDate,StrikePrice,OptionType,UnderlyingSymbol,AssetToken,"
         "PriceBandHigh,PriceBandLow,LTP,Open,High,Low,Close,PrevClose,Volume,"
         "IV,Delta,Gamma,Vega,Theta,InstrumentType\n";

  // Write all contracts
  for (int32_t idx = 0; idx < m_contractCount; ++idx) {
    out << m_token[idx] << "," << m_name[idx] << "," << m_displayName[idx]
        << "," << m_description[idx] << "," << m_series[idx] << ","
        << m_lotSize[idx] << "," << m_tickSize[idx] << "," << m_expiryDate[idx]
        << "," << m_strikePrice[idx] << "," << m_optionType[idx] << ","
        << m_name[idx] << "," // UnderlyingSymbol
        << m_assetToken[idx] << "," << m_priceBandHigh[idx] << ","
        << m_priceBandLow[idx] << ","
        << "0,0,0,0,0,0,0," // Live data (not persisted)
        << "0,0,0,0,0,"     // Greeks (not persisted)
        << m_instrumentType[idx] << "\n";
  }

  file.close();
  qDebug() << "BSE FO Repository saved to CSV:" << filename << m_contractCount
           << "contracts";
  return true;
}

const ContractData *BSEFORepository::getContract(int64_t token) const {
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

  // Use the stored string value directly
  tempContract.optionType = m_optionType[idx];

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
    contract.description = m_description[idx];
    contract.series = m_series[idx];
    contract.lotSize = m_lotSize[idx];
    contract.tickSize = m_tickSize[idx];
    contract.expiryDate = m_expiryDate[idx];
    contract.strikePrice = m_strikePrice[idx];
    contract.optionType = m_optionType[idx];
    contract.instrumentType = m_instrumentType[idx];

    contracts.append(contract);
  }

  return contracts;
}

QVector<ContractData>
BSEFORepository::getContractsBySeries(const QString &series) const {
  QReadLocker locker(&m_mutex);

  QVector<ContractData> contracts;

  for (int32_t idx = 0; idx < m_contractCount; ++idx) {
    // If series is empty, return all contracts (used for symbol search across all series)
    // Otherwise, match exact series
    if (series.isEmpty() || m_series[idx] == series) {
      ContractData contract;
      contract.exchangeInstrumentID = m_token[idx];
      contract.name = m_name[idx];
      contract.displayName = m_displayName[idx];
      contract.description = m_description[idx];
      contract.series = m_series[idx];
      contract.lotSize = m_lotSize[idx];
      contract.tickSize = m_tickSize[idx];
      contract.expiryDate = m_expiryDate[idx];
      contract.strikePrice = m_strikePrice[idx];
      contract.optionType = m_optionType[idx];
      contract.instrumentType = m_instrumentType[idx];

      contracts.append(contract);
    }
  }

  return contracts;
}

QVector<ContractData>
BSEFORepository::getContractsBySymbol(const QString &symbol) const {
  QReadLocker locker(&m_mutex);

  QVector<ContractData> contracts;

  for (int32_t idx = 0; idx < m_contractCount; ++idx) {
    if (m_name[idx].contains(symbol, Qt::CaseInsensitive)) {
      ContractData contract;
      contract.exchangeInstrumentID = m_token[idx];
      contract.name = m_name[idx];
      contract.displayName = m_displayName[idx];
      contract.description = m_description[idx];
      contract.series = m_series[idx];
      contract.lotSize = m_lotSize[idx];
      contract.tickSize = m_tickSize[idx];
      contract.expiryDate = m_expiryDate[idx];
      contract.strikePrice = m_strikePrice[idx];
      contract.optionType = m_optionType[idx];
      contract.instrumentType = m_instrumentType[idx];

      contracts.append(contract);
    }
  }

  return contracts;
}

QStringList BSEFORepository::getUniqueSymbols(const QString &series) const {
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

void BSEFORepository::updateLiveData(int64_t token, double ltp, double open,
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

void BSEFORepository::updateGreeks(int64_t token, double iv, double delta,
                                   double gamma, double vega, double theta) {
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

void BSEFORepository::prepareForLoad() {
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

  m_expiryDate.clear();
  m_strikePrice.clear();
  m_optionType.clear();
  m_assetToken.clear();
  m_instrumentType.clear();

  m_ltp.clear();
  m_open.clear();
  m_high.clear();
  m_low.clear();
  m_close.clear();
  m_prevClose.clear();
  m_volume.clear();

  m_bidPrice.clear();
  m_askPrice.clear();

  // Invalidate cached symbols (lazy cache)
  m_symbolsCached = false;
  m_cachedUniqueSymbols.clear();

  m_loaded = false;
}

void BSEFORepository::addContract(
    const MasterContract &contract,
    std::function<QString(const QString &)> internFunc) {
  QWriteLocker locker(&m_mutex);
  auto intern = internFunc ? internFunc : [](const QString &s) { return s; };
  addContractInternal(contract, intern);
}

void BSEFORepository::addContractInternal(
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
  m_freezeQty.append(contract.freezeQty);

  m_priceBandHigh.append(contract.priceBandHigh);
  m_priceBandLow.append(contract.priceBandLow);

  m_expiryDate.append(intern(contract.expiryDate));
  m_strikePrice.append(contract.strikePrice);

  // Determine OptionType
  QString optType = "XX";
  if (contract.instrumentType == 2) { // OPTIDX/OPTSTK
    if (contract.optionType == 1 || contract.optionType == 3)
      optType = "CE";
    else if (contract.optionType == 2 || contract.optionType == 4)
      optType = "PE";
  } else if (contract.instrumentType == 4) {
    optType = "SPD";
  } else if (contract.instrumentType == 1) {
    optType = "FUT";
  }
  m_optionType.append(intern(optType));

  m_assetToken.append(contract.assetToken);
  m_instrumentType.append(contract.instrumentType);

  // Initialize dynamic fields
  m_ltp.append(0.0);
  m_open.append(0.0);
  m_high.append(0.0);
  m_low.append(0.0);
  m_close.append(0.0);
  m_prevClose.append(0.0);
  m_volume.append(0);
  m_bidPrice.append(0.0);
  m_askPrice.append(0.0);
  m_iv.append(0.0);
  m_delta.append(0.0);
  m_gamma.append(0.0);
  m_vega.append(0.0);
  m_theta.append(0.0);

  m_contractCount++;
}

void BSEFORepository::forEachContract(
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
    data.expiryDate = m_expiryDate[i];
    data.strikePrice = m_strikePrice[i];
    data.optionType = m_optionType[i];
    data.assetToken = m_assetToken[i];
    data.instrumentType = m_instrumentType[i];
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

    callback(data);
  }
}
