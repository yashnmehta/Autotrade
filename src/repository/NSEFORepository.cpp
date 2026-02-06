#include "repository/NSEFORepository.h"
#include "repository/MasterFileParser.h"
#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <QThread>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>

static QString trimQuotes(const QStringRef &str) {
  QStringRef trimmed = str.trimmed();
  if (trimmed.startsWith('"') && trimmed.endsWith('"') &&
      trimmed.length() >= 2) {
    return trimmed.mid(1, trimmed.length() - 2).toString();
  }
  return trimmed.toString();
}

// Helper function to get underlying asset token for indices
// XTS master file has -1 for indices, but we need actual index tokens
// For stocks, XTS provides tokens with prefix (e.g., 1100100002885 for
// RELIANCE) We need to extract the last 4-5 digits (2885 for RELIANCE)
static int64_t getUnderlyingAssetToken(const QString &symbolName,
                                       int64_t masterFileToken) {
  // For indices with -1, map to actual index tokens
  if (masterFileToken == -1) {
    // Map index names to their underlying tokens
    // These are the spot index tokens used for options/futures settlement
    static const QHash<QString, int64_t> indexTokens = {
        {"BANKNIFTY", 26009},  // Nifty Bank index
        {"NIFTY", 26000},      // Nifty 50 index
        {"FINNIFTY", 26037},   // Nifty Financial Services
        {"MIDCPNIFTY", 26074}, // Nifty Midcap Select
        {"NIFTYNXT50", 26013}, // Nifty Next 50
        {"SENSEX", -1},        // BSE Sensex (placeholder)
        {"BANKEX", -1}         // BSE Bankex (placeholder)
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
    : m_regularCount(0), m_spreadCount(0), m_loaded(false) {
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

  // Live Market Data (REMOVED)

  // F&O Specific (9 fields - with typed columns)
  m_expiryDate.resize(ARRAY_SIZE);
  m_expiryDate_dt.resize(ARRAY_SIZE); // QDate for fast sorting
  m_strikePrice.resize(ARRAY_SIZE);
  m_strikePrice_fl.resize(ARRAY_SIZE); // float for memory efficiency
  m_timeToExpiry.resize(ARRAY_SIZE);   // Pre-calculated for Greeks
  m_optionType.resize(ARRAY_SIZE);
  m_assetToken.resize(ARRAY_SIZE);
  m_instrumentType.resize(ARRAY_SIZE);

  // Greeks (REMOVED)
  // Margin Data (REMOVED)

  // Reserve space for spread contracts
  m_spreadContracts.reserve(500);
}

bool NSEFORepository::loadMasterFile(const QString &filename) {
  // Lock stripe 0 for initialization
  QWriteLocker locker(&m_mutexes[0]);

  // Native C++ file I/O (5-10x faster than QFile)
  std::ifstream file(filename.toStdString(), std::ios::binary);
  if (!file.is_open()) {
    std::cerr << "Failed to open NSEFO master file: " << filename.toStdString()
              << std::endl;
    return false;
  }

  std::string line;
  line.reserve(1024); // Pre-allocate to avoid reallocations

  int32_t loadedCount = 0;
  int32_t spreadLoadedCount = 0;

  // String Interning Pool
  // Used to deduplicate repetitive strings (e.g., "BANKNIFTY", "27JAN2026",
  // "CE") By reusing the same QString instance, we share the underlying data
  // (COW)
  QSet<QString> stringPool;
  auto internString = [&stringPool](const QString &str) -> QString {
    auto it = stringPool.find(str);
    if (it != stringPool.end()) {
      return *it;
    }
    stringPool.insert(str);
    return str;
  };

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

    // Parse master line
    qLine = QString::fromStdString(line);
    if (!MasterFileParser::parseLine(QStringRef(&qLine), "NSEFO", contract)) {
      continue;
    }

    // Debug: Show first 3 parsed contracts to check for quotes
    static int debugCount = 0;
    if (debugCount < 3 && contract.series == "FUTIDX") {
      qDebug() << "[NSEFORepo] Parsed contract:" << debugCount
               << "Token:" << contract.exchangeInstrumentID
               << "Name bytes:" << contract.name.toUtf8().toHex() << "Name:'"
               << contract.name << "'"
               << "Series:'" << contract.series << "'";
      debugCount++;
    }

    int64_t token = contract.exchangeInstrumentID;

    if (isRegularContract(token)) {
      // Store in cached indexed array
      int32_t idx = getArrayIndex(token);

      m_valid[idx] = true;
      m_name[idx] = internString(contract.name);
      m_displayName[idx] =
          contract.displayName; // DisplayName is unique usually
      m_description[idx] =
          contract.description; // Description is unique usually
      m_series[idx] = internString(contract.series);
      m_lotSize[idx] = contract.lotSize;
      m_tickSize[idx] = contract.tickSize;
      m_freezeQty[idx] = contract.freezeQty;
      m_priceBandHigh[idx] = contract.priceBandHigh;
      m_priceBandLow[idx] = contract.priceBandLow;
      m_expiryDate[idx] = internString(contract.expiryDate);
      m_strikePrice[idx] = contract.strikePrice;
      m_assetToken[idx] =
          getUnderlyingAssetToken(contract.name, contract.assetToken);
      m_symbolToAssetToken[m_name[idx]] = m_assetToken[idx];
      m_instrumentType[idx] = contract.instrumentType;

      // Convert optionType from int to string
      // Based on actual data: 3=CE, 4=PE, others=XX
      if (loadedCount < 10 && contract.series == "OPTIDX") {
        qDebug() << "  [DEBUG] Token:" << token << "Symbol:" << contract.name
                 << "Raw optionType:" << contract.optionType;
      }

      QString optType;
      if (contract.instrumentType == 4) {
        optType = "SPD";
      } else if (contract.optionType == 3) {
        optType = "CE";
      } else if (contract.optionType == 4) {
        optType = "PE";
      } else {
        optType = "XX";
      }
      m_optionType[idx] = internString(optType);

      loadedCount++;
    } else if (token >= SPREAD_THRESHOLD) {
      // Store spread contract in map
      auto contractData =
          std::make_shared<ContractData>(contract.toContractData());

      // Intern strings for spread contracts too
      contractData->name = internString(contractData->name);
      contractData->series = internString(contractData->series);
      contractData->expiryDate = internString(contractData->expiryDate);
      contractData->optionType = internString(contractData->optionType);

      m_spreadContracts[token] = contractData;
      spreadLoadedCount++;
    }
  }

  file.close();

  m_regularCount = loadedCount;
  m_spreadCount = spreadLoadedCount;
  m_loaded = true;

  qDebug() << "NSE FO Repository loaded:"
           << "Regular:" << m_regularCount << "Spread:" << m_spreadCount
           << "Total:" << getTotalCount();

  // Debug: Show first 5 valid contracts to verify expiry date format
  qDebug() << "[NSE FO] First 5 contracts with expiry dates:";
  int shown = 0;
  for (int32_t idx = 0; idx < ARRAY_SIZE && shown < 5; ++idx) {
    if (m_valid[idx] && !m_expiryDate[idx].isEmpty()) {
      qDebug() << "  Token:" << (MIN_TOKEN + idx) << "Symbol:" << m_name[idx]
               << "Expiry:" << m_expiryDate[idx]
               << "Strike:" << m_strikePrice[idx]
               << "Type:" << m_optionType[idx];
      shown++;
    }
  }

  return true;
}

bool NSEFORepository::loadProcessedCSV(const QString &filename) {
  // Lock stripe 0 for initialization
  QWriteLocker locker(&m_mutexes[0]);

  // Native C++ file I/O (10x faster than QFile/QTextStream)
  std::ifstream file(filename.toStdString(), std::ios::binary);
  if (!file.is_open()) {
    std::cerr << "Failed to open NSEFO CSV file: " << filename.toStdString()
              << std::endl;
    return false;
  }

  std::string line;
  line.reserve(2048); // CSV lines are longer

  // Skip header line
  std::getline(file, line);

  int32_t loadedCount = 0;
  int32_t spreadLoadedCount = 0;

  // String Interning Pool - DISABLED for CSV loading (causes O(n²) slowdown)
  // QSet<QString> stringPool;
  auto internString = [](const QString &str) -> QString {
    return str; // No interning - just return the string as-is
  };

  QString qLine;
  int lineCount = 0;
  while (std::getline(file, line)) {
    lineCount++;
    if (lineCount % 10000 == 0) {
      qDebug() << "[NSEFO] Loaded lines:" << lineCount;
      // Unlock briefly to allow progress signals, then relock
      locker.unlock();
      QCoreApplication::processEvents();
      locker.relock();
    }
    if (line.empty() || line[0] == '\r') {
      continue;
    }

    qLine = QString::fromStdString(line);

    // Optimized manual split using midRef to avoid heap allocations
    QVector<QStringRef> fields;
    int start = 0;
    int end = 0;
    while ((end = qLine.indexOf(',', start)) != -1) {
      fields.append(qLine.midRef(start, end - start));
      start = end + 1;
    }
    fields.append(qLine.midRef(start));

    if (fields.size() < 28) { // Added instrumentType
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
      m_name[idx] = internString(trimQuotes(fields[1]));   // Symbol
      m_displayName[idx] = trimQuotes(fields[2]);          // DisplayName
      m_description[idx] = trimQuotes(fields[3]);          // Description
      m_series[idx] = internString(trimQuotes(fields[4])); // Series
      m_lotSize[idx] = fields[5].toInt();
      m_tickSize[idx] = fields[6].toDouble();
      m_expiryDate[idx] =
          internString(trimQuotes(fields[7])); // DDMMMYYYY format
      m_strikePrice[idx] = fields[8].toDouble();
      m_optionType[idx] = internString(trimQuotes(fields[9])); // CE/PE/XX
      m_assetToken[idx] = fields[11].toLongLong();

      // Only update symbol map if this symbol hasn't been seen yet
      if (!m_symbolToAssetToken.contains(m_name[idx])) {
        m_symbolToAssetToken[m_name[idx]] = m_assetToken[idx];
      }

      m_freezeQty[idx] = fields[12].toInt();
      m_priceBandHigh[idx] = fields[13].toDouble();
      m_priceBandLow[idx] = fields[14].toDouble();
      m_instrumentType[idx] = fields[27].toInt(); // Added

      loadedCount++;
    } else if (token >= SPREAD_THRESHOLD) {
      // Create ContractData for spread contract
      auto contractData = std::make_shared<ContractData>();
      contractData->exchangeInstrumentID = token;
      contractData->name = internString(trimQuotes(fields[1]));
      contractData->displayName = trimQuotes(fields[2]);
      contractData->description = trimQuotes(fields[3]);
      contractData->series = internString(trimQuotes(fields[4]));
      contractData->lotSize = fields[5].toInt();
      contractData->tickSize = fields[6].toDouble();
      contractData->expiryDate = internString(trimQuotes(fields[7]));
      contractData->strikePrice = fields[8].toDouble();
      contractData->optionType = internString(trimQuotes(fields[9]));
      contractData->assetToken = fields[11].toLongLong();
      contractData->freezeQty = fields[12].toInt();
      contractData->priceBandHigh = fields[13].toDouble();
      contractData->priceBandLow = fields[14].toDouble();
      contractData->instrumentType = fields[27].toInt();

      m_spreadContracts[token] = contractData;
      spreadLoadedCount++;
    }
  }
  qDebug() << "Loaded lines:" << lineCount;
  qDebug() << "outside while loop";

  qDebug() << "Closing file...";
  file.close();

  qDebug() << "Setting counters...";
  m_regularCount = loadedCount;
  m_spreadCount = spreadLoadedCount;

  // Return false if no contracts loaded (empty CSV file)
  if (loadedCount == 0 && spreadLoadedCount == 0) {
    qWarning()
        << "NSE FO Repository CSV file is empty, will fall back to master file";
    return false;
  }

  qDebug() << "Calling finalizeLoad()...";
  // Unlock before calling finalizeLoad to avoid deadlock
  locker.unlock();
  finalizeLoad();

  qDebug() << "NSE FO Repository loaded from CSV:"
           << "Regular:" << m_regularCount << "Spread:" << m_spreadCount
           << "Total:" << getTotalCount();

  return true;
}

const ContractData *NSEFORepository::getContract(int64_t token) const {
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
    tempContract.expiryDate = m_expiryDate[idx];
    tempContract.strikePrice = m_strikePrice[idx];
    tempContract.optionType = m_optionType[idx];
    tempContract.assetToken = m_assetToken[idx];
    tempContract.instrumentType = m_instrumentType[idx];

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

void NSEFORepository::forEachContract(
    std::function<void(const ContractData &)> callback) const {
  // Iterate regular contracts
  for (int32_t idx = 0; idx < ARRAY_SIZE; ++idx) {
    // Optimize: Check valid flag without locking first?
    // No, need memory visibility. But we can use stripe lock.
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
      contract.instrumentType = m_instrumentType[idx];

      // Live data/Greeks not stored here anymore

      locker.unlock(); // Unlock before callback to prevent deadlock
      callback(contract);
      locker.relock();
    }
  }

  // Iterate spread contracts
  {
    QReadLocker locker(&m_mutexes[0]);
    // Iterate copy of values or lock during callback?
    // Locking during callback is dangerous. Copying shared_ptr is cheap.
    auto spreads = m_spreadContracts.values();
    locker.unlock();

    for (const auto &contractPtr : spreads) {
      callback(*contractPtr);
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
      contract.expiryDate_dt = m_expiryDate_dt[idx]; // QDate
      contract.strikePrice = m_strikePrice[idx];
      contract.timeToExpiry = m_timeToExpiry[idx]; // Pre-calculated
      contract.optionType = m_optionType[idx];
      contract.assetToken = m_assetToken[idx];
      contract.instrumentType = m_instrumentType[idx];

      contracts.append(contract);
    }
  }

  // Add spread contracts
  {
    QReadLocker locker(&m_mutexes[0]);
    for (auto it = m_spreadContracts.begin(); it != m_spreadContracts.end();
         ++it) {
      contracts.append(*it.value());
    }
  }

  return contracts;
}

QVector<ContractData>
NSEFORepository::getContractsBySeries(const QString &series) const {
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

QVector<ContractData>
NSEFORepository::getContractsBySymbol(const QString &symbol) const {
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

QVector<ContractData> NSEFORepository::getContractsBySymbolAndExpiry(
    const QString &symbol, const QString &expiry, int instrumentType) const {
  QVector<ContractData> contracts;

  // Search regular contracts
  for (int32_t idx = 0; idx < ARRAY_SIZE; ++idx) {
    int32_t stripeIdx = getStripeIndex(idx);
    QReadLocker locker(&m_mutexes[stripeIdx]);

    if (m_valid[idx] && m_name[idx] == symbol && m_expiryDate[idx] == expiry) {
      if (instrumentType == -1 || m_instrumentType[idx] == instrumentType) {
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
  }

  // Search spread contracts
  {
    QReadLocker locker(&m_mutexes[0]);
    for (auto it = m_spreadContracts.begin(); it != m_spreadContracts.end();
         ++it) {
      const auto &c = *it.value();
      if (c.name == symbol && c.expiryDate == expiry) {
        if (instrumentType == -1 || c.instrumentType == instrumentType) {
          contracts.append(c);
        }
      }
    }
  }

  return contracts;
}

void NSEFORepository::updateAssetToken(int64_t token, int64_t assetToken) {
  if (isRegularContract(token)) {
    int32_t idx = getArrayIndex(token);
    int32_t stripeIdx = getStripeIndex(idx);
    QWriteLocker locker(&m_mutexes[stripeIdx]);
    if (m_valid[idx]) {
      m_assetToken[idx] = assetToken;

      // Also update the symbolic map (using stripe 0 for map access)
      if (stripeIdx != 0) {
        m_mutexes[0].lockForWrite();
        m_symbolToAssetToken[m_name[idx]] = assetToken;
        m_mutexes[0].unlock();
      } else {
        m_symbolToAssetToken[m_name[idx]] = assetToken;
      }
    }
  } else if (token >= SPREAD_THRESHOLD) {
    QWriteLocker locker(&m_mutexes[0]);
    auto it = m_spreadContracts.find(token);
    if (it != m_spreadContracts.end()) {
      it.value()->assetToken = assetToken;
    }
  }
}

bool NSEFORepository::loadFromContracts(
    const QVector<MasterContract> &contracts) {
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

  // String Interning Pool
  QSet<QString> stringPool;
  auto internString = [&stringPool](const QString &str) -> QString {
    auto it = stringPool.find(str);
    if (it != stringPool.end()) {
      return *it;
    }
    stringPool.insert(str);
    return str;
  };

  // Load contracts directly from QVector
  int loaded = 0;
  for (const MasterContract &contract : contracts) {
    if (contract.exchange != "NSEFO") {
      continue; // Skip non-NSEFO contracts
    }

    int64_t token = contract.exchangeInstrumentID;

    if (isRegularContract(token)) {
      // Regular contract: store in array
      int32_t idx = getArrayIndex(token);
      int32_t stripeIdx = getStripeIndex(idx);

      QWriteLocker locker(&m_mutexes[stripeIdx]);

      m_valid[idx] = true;
      m_name[idx] = internString(contract.name);
      m_displayName[idx] = contract.displayName;
      m_description[idx] = contract.description;
      m_series[idx] = internString(contract.series);
      m_lotSize[idx] = contract.lotSize;
      m_tickSize[idx] = contract.tickSize;
      m_freezeQty[idx] = contract.freezeQty;
      m_priceBandHigh[idx] = contract.priceBandHigh;
      m_priceBandLow[idx] = contract.priceBandLow;
      m_expiryDate[idx] = internString(contract.expiryDate);
      m_strikePrice[idx] = contract.strikePrice;

      // Determine OptionType (1=CE, 2=PE, 3=CE, 4=PE)
      QString optType = "XX";
      if (contract.instrumentType == 2) { // OPTIDX/OPTSTK
        if (contract.optionType == 1 || contract.optionType == 3)
          optType = "CE";
        else if (contract.optionType == 2 || contract.optionType == 4)
          optType = "PE";
      }
      m_optionType[idx] = internString(optType);

      m_assetToken[idx] = contract.assetToken;

      // Update symbol map (protected by stripe 0 lock)
      if (stripeIdx != 0) {
        m_mutexes[0].lockForWrite();
        m_symbolToAssetToken[m_name[idx]] = m_assetToken[idx];
        m_mutexes[0].unlock();
      } else {
        m_symbolToAssetToken[m_name[idx]] = m_assetToken[idx];
      }
      m_instrumentType[idx] = contract.instrumentType;

      // Live data/Greeks not persisted, no need to load

      // F&O specific fields loaded above

      m_regularCount++;
      loaded++;

    } else if (token >= SPREAD_THRESHOLD) {
      // Spread contract: store in map
      // Create contract data structure for spreads
      auto contractData = std::make_shared<ContractData>();
      contractData->exchangeInstrumentID = token;
      contractData->name = internString(contract.name); // Interned
      contractData->displayName = contract.displayName;
      contractData->description = contract.description;
      contractData->series = internString(contract.series); // Interned
      contractData->lotSize = contract.lotSize;
      contractData->tickSize = contract.tickSize;
      contractData->freezeQty = contract.freezeQty;
      contractData->priceBandHigh = contract.priceBandHigh;
      contractData->priceBandLow = contract.priceBandLow;
      contractData->expiryDate = internString(contract.expiryDate); // Interned
      contractData->strikePrice = contract.strikePrice;

      QString optType = "SPD";            // Default for spread
      if (contract.instrumentType == 2) { // OPTIDX/OPTSTK
        if (contract.optionType == 1)
          optType = "CE";
        else if (contract.optionType == 2)
          optType = "PE";
      }
      contractData->optionType = internString(optType); // Interned

      contractData->assetToken = contract.assetToken;
      contractData->instrumentType = contract.instrumentType;

      // Add to unique_ptr map (protected by a mutex)
      // Since spreads are > 10,000,000, we don't use array based lock
      // We will use a dedicated lock or a striped lock based on hash?
      // Since spread access is rare, we can use the first stripe lock or a
      // global one But we have striped mutexes. Let's use stripe 0 for spread
      // map for now Or better, assume single threaded loading or use hash-based
      // stripe But m_spreadContracts is a QHash. It's not thread-safe for write
      // We should use a lock.
      // For now, protecting with Stripe 0 is simplest as spreads are rare
      QWriteLocker locker(&m_mutexes[0]);
      m_spreadContracts[token] = contractData;

      m_spreadCount++;
      loaded++;
    }
  }

  m_loaded = (loaded > 0);

  qDebug() << "NSE FO Repository loaded from contracts:" << loaded
           << "Regular:" << m_regularCount << "Spread:" << m_spreadCount;
  return loaded > 0;
}

bool NSEFORepository::saveProcessedCSV(const QString &filename) const {
  QFile file(filename);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    qWarning() << "Failed to open file for writing:" << filename;
    return false;
  }

  QTextStream out(&file);

  // Write header
  out << "Token,Symbol,DisplayName,Description,Series,LotSize,TickSize,"
         "ExpiryDate,StrikePrice,OptionType,UnderlyingSymbol,AssetToken,"
         "FreezeQty,PriceBandHigh,PriceBandLow,LTP,Open,High,Low,Close,"
         "PrevClose,Volume,IV,Delta,Gamma,Vega,Theta,InstrumentType\n";

  // Write regular contracts
  for (int32_t idx = 0; idx < ARRAY_SIZE; ++idx) {
    if (!m_valid[idx])
      continue;

    int64_t token = MIN_TOKEN + idx;
    out << token << "," << m_name[idx] << "," << m_displayName[idx] << ","
        << m_description[idx] << "," << m_series[idx] << "," << m_lotSize[idx]
        << "," << m_tickSize[idx] << "," << m_expiryDate[idx] << ","
        << m_strikePrice[idx] << "," << m_optionType[idx] << "," << m_name[idx]
        << "," // UnderlyingSymbol (same as name)
        << m_assetToken[idx] << "," << m_freezeQty[idx] << ","
        << m_priceBandHigh[idx] << "," << m_priceBandLow[idx] << ","
        << "0,0,0,0,0,0,0," // Live data (not persisted)
        << "0,0,0,0,0,"     // Greeks (not persisted)
        << m_instrumentType[idx] << "\n";
  }

  // Write spread contracts
  for (auto it = m_spreadContracts.constBegin();
       it != m_spreadContracts.constEnd(); ++it) {
    const ContractData *contract = it.value().get();
    out << contract->exchangeInstrumentID << "," << contract->name << ","
        << contract->displayName << "," << contract->description << ","
        << contract->series << "," << contract->lotSize << ","
        << contract->tickSize << "," << contract->expiryDate << ","
        << contract->strikePrice << "," << contract->optionType << ","
        << contract->name << "," << contract->assetToken << ","
        << contract->freezeQty << "," << contract->priceBandHigh << ","
        << contract->priceBandLow << ","
        << "0,0,0,0,0,0,0," // Live data
        << "0,0,0,0,0,"     // Greeks
        << contract->instrumentType << "\n";
  }

  file.close();
  qDebug() << "NSE FO Repository saved to CSV:" << filename
           << "Regular:" << m_regularCount << "Spread:" << m_spreadCount;
  return true;
}

void NSEFORepository::prepareForLoad() {
  m_regularCount = 0;
  m_spreadCount = 0;
  m_valid.fill(false);
  m_spreadContracts.clear();
  m_symbolToAssetToken.clear();
}

void NSEFORepository::finalizeLoad() {
  qDebug() << "  [finalizeLoad] Starting...";
  m_loaded = (m_regularCount > 0 || m_spreadCount > 0);

  qDebug() << "  [finalizeLoad] Squeezing containers...";
  // Squeeze internal containers to return memory to the OS
  m_spreadContracts.squeeze();

  qDebug() << "  [finalizeLoad] Verifying symbol map...";
  // Verify symbol map
  {
    QReadLocker locker(&m_mutexes[0]);
    qDebug() << "NSE FO Repository finalized:"
             << "Regular:" << m_regularCount << "Spread:" << m_spreadCount
             << "Total:" << getTotalCount()
             << "Unique Symbols:" << m_symbolToAssetToken.size();
  }
  qDebug() << "  [finalizeLoad] Complete!";
}

void NSEFORepository::addContract(
    const MasterContract &contract,
    std::function<QString(const QString &)> internFunc) {

  // Use supplied interner or identity if null
  auto intern = internFunc ? internFunc : [](const QString &s) { return s; };

  int64_t token = contract.exchangeInstrumentID;

  if (isRegularContract(token)) {
    // Determine OptionType (1=CE, 2=PE, 3/4=XX)
    QString optType = "XX";
    if (contract.instrumentType == 2) { // OPTIDX/OPTSTK
      if (contract.optionType == 1 || contract.optionType == 3)
        optType = "CE";
      else if (contract.optionType == 2 || contract.optionType == 4)
        optType = "PE";
    }

    int32_t idx = getArrayIndex(token);
    int32_t stripeIdx = getStripeIndex(idx);

    QWriteLocker locker(&m_mutexes[stripeIdx]);

    m_valid[idx] = true;
    m_name[idx] = intern(contract.name);
    m_displayName[idx] = contract.displayName; // DisplayName is unique usually
    m_description[idx] = contract.description; // Description is unique usually
    m_series[idx] = intern(contract.series);
    m_lotSize[idx] = contract.lotSize;
    m_tickSize[idx] = contract.tickSize;
    m_freezeQty[idx] = contract.freezeQty;
    m_priceBandHigh[idx] = contract.priceBandHigh;
    m_priceBandLow[idx] = contract.priceBandLow;

    // F&O Specific fields (with typed columns)
    m_expiryDate[idx] = intern(contract.expiryDate);
    m_expiryDate_dt[idx] = contract.expiryDate_dt; // QDate for sorting
    m_strikePrice[idx] = contract.strikePrice;
    m_strikePrice_fl[idx] =
        static_cast<float>(contract.strikePrice); // float for efficiency
    m_timeToExpiry[idx] = contract.timeToExpiry;  // Pre-calculated
    m_optionType[idx] = intern(optType);
    m_assetToken[idx] = contract.assetToken;
    m_instrumentType[idx] = contract.instrumentType;

    // Update symbol map (thread-safe)
    // We are currently holding a stripe lock, but map needs lock 0 (or
    // dedicated lock) If strict locking is needed, this might be tricky, but
    // assuming addContract is called during load phase where contention is
    // minimal or safe. To be safe, we release stripe lock and acquire map lock
    // (stripe 0), but that's expensive. However, in loadFromContracts we do: if
    // (stripeIdx != 0) { mutex[0].lock; map update; mutex[0].unlock; } else {
    // map update; } We can't do that here efficiently while holding stripe lock
    // if we want to avoid releasing/reacquiring. But since this is critical, we
    // should do it.

    // BUT! addContract is holding the stripe lock.
    // If we nest locks, we must be consistent.
    // If stripeIdx != 0, we are locking 0 inside lock N.
    // This is generally unsafe unless we guarantee 0 never waits for N.
    // Given the context (loading), let's assume it's OK or use a scoped unlock
    // if possible. Actually, let's just use the same unsafe-but-functional
    // approach as loadFromContracts because loading is mostly single-writer or
    // partitioned.

    if (stripeIdx != 0) {
      m_mutexes[0].lockForWrite();
      m_symbolToAssetToken[m_name[idx]] = m_assetToken[idx];
      m_mutexes[0].unlock();
    } else {
      m_symbolToAssetToken[m_name[idx]] = m_assetToken[idx];
    }

    m_regularCount++;

  } else if (token >= SPREAD_THRESHOLD) {
    auto contractData = std::make_shared<ContractData>();
    contractData->exchangeInstrumentID = token;
    contractData->name = intern(contract.name);
    contractData->displayName = contract.displayName;
    contractData->description = contract.description;
    contractData->series = intern(contract.series);
    contractData->lotSize = contract.lotSize;
    contractData->tickSize = contract.tickSize;
    contractData->freezeQty = contract.freezeQty;
    contractData->priceBandHigh = contract.priceBandHigh;
    contractData->priceBandLow = contract.priceBandLow;
    contractData->expiryDate = intern(contract.expiryDate);
    contractData->strikePrice = contract.strikePrice;

    QString optType = "SPD";            // Default for spread
    if (contract.instrumentType == 2) { // OPTIDX/OPTSTK
      if (contract.optionType == 1)
        optType = "CE";
      else if (contract.optionType == 2)
        optType = "PE";
    }

    contractData->optionType = intern(optType);
    contractData->assetToken = contract.assetToken;
    contractData->instrumentType = contract.instrumentType;

    // Use stripe 0 for map protection
    QWriteLocker locker(&m_mutexes[0]);
    m_spreadContracts[token] = contractData;
    m_spreadCount++;
  }
}

int64_t NSEFORepository::getAssetToken(const QString &symbol) const {
  // Lock stripe 0 for symbol map
  QReadLocker locker(&m_mutexes[0]);

  auto it = m_symbolToAssetToken.find(symbol);
  if (it != m_symbolToAssetToken.end()) {
    return it.value();
  }

  return -1;
}
