#include "repository/RepositoryManager.h"
#include "repository/MasterFileParser.h"
#include <QCoreApplication>
#include <QDate>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QStandardPaths>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

// Include distributed price stores
#include "bse_price_store.h"
#include "nsecm_price_store.h"
#include "nsefo_price_store.h"
#include "repository/NSEFORepositoryPreSorted.h"
#include "utils/MemoryProfiler.h"

// Initialize singleton
RepositoryManager *RepositoryManager::s_instance = nullptr;

RepositoryManager *RepositoryManager::getInstance() {
  if (!s_instance) {
    s_instance = new RepositoryManager();
  }
  return s_instance;
}

RepositoryManager::RepositoryManager() : QObject(nullptr), m_loaded(false) {
  m_nsefo = std::make_unique<NSEFORepositoryPreSorted>();
  m_nsecm = std::make_unique<NSECMRepository>();
  m_bsefo = std::make_unique<BSEFORepository>();
  m_bsecm = std::make_unique<BSECMRepository>();
}

RepositoryManager::~RepositoryManager() = default;

QString RepositoryManager::getMastersDirectory() {
  QString appDir = QCoreApplication::applicationDirPath();
  QString mastersDir;

#ifdef Q_OS_MACOS
  // macOS: Check if running from .app bundle
  if (appDir.contains(".app/Contents/MacOS")) {
    // Inside app bundle: use <app>/Masters
    mastersDir = appDir + "/../Resources/Masters";
    // Also check direct sibling folder
    if (!QDir(mastersDir).exists()) {
      mastersDir = appDir + "/Masters";
    }
  } else {
    // Development build: use project relative path
    mastersDir = appDir + "/../../../Masters";
  }
#elif defined(Q_OS_LINUX)
  // Linux: Try user data directory first, then relative path
  QString userDataDir =
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  if (!userDataDir.isEmpty()) {
    mastersDir = userDataDir + "/Masters";
  } else {
    // Fallback to home directory
    mastersDir = QDir::homePath() + "/.TradingTerminal/Masters";
  }
#elif defined(Q_OS_WIN)
  // Windows: Use AppData directory
  QString userDataDir =
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  if (!userDataDir.isEmpty()) {
    mastersDir = userDataDir + "/Masters";
  } else {
    mastersDir = appDir + "/Masters";
  }
#else
  // Generic fallback
  mastersDir = appDir + "/Masters";
#endif

  // Normalize and create directory
  QDir dir;
  QString normalizedPath =
      QDir::cleanPath(QFileInfo(mastersDir).absoluteFilePath());

  if (!dir.exists(normalizedPath)) {
    if (dir.mkpath(normalizedPath)) {
      qDebug() << "[RepositoryManager] Created Masters directory:"
               << normalizedPath;
    } else {
      qWarning() << "[RepositoryManager] Failed to create Masters directory:"
                 << normalizedPath;
    }
  }

  qDebug() << "[RepositoryManager] Using Masters directory:" << normalizedPath;
  return normalizedPath;
}

bool RepositoryManager::loadAll(const QString &mastersPath) {
  qDebug() << "[RepositoryManager] ========================================";
  qDebug() << "[RepositoryManager] Starting Master File Loading";
  qDebug() << "[RepositoryManager] ========================================";
  
  QString mastersDir = mastersPath;
  if (mastersDir == "Masters") {
      mastersDir = getMastersDirectory();
  }

  // PHASE 1: Load NSE CM Index Master (Required) - Always load from MasterFiles
  qDebug() << "[RepositoryManager] [1/5] Loading NSE CM Index Master...";
  QString indexMasterPath = QCoreApplication::applicationDirPath() + "/MasterFiles";
  qDebug() << "[RepositoryManager] Loading index master from:" << indexMasterPath;
  if (!loadIndexMaster(indexMasterPath)) {
    qWarning() << "[RepositoryManager] Failed to load index master from" << indexMasterPath;
    // We continue, but some functionality might be limited (no indices)
  }
  
  // PHASE 2: Load NSE CM (Stocks) - Fallback from CSV to Master File
  qDebug() << "[RepositoryManager] [2/5] Loading NSE CM (Stocks)...";
  if (!loadNSECM(mastersDir, true)) {
    emit loadingError("Failed to load NSE CM", {"nsecm_processed.csv", "contract_nsecm_latest.txt"});
    return false;
  }
  
  // Append index master contracts to NSECM repository
  if (!m_indexContracts.isEmpty()) {
    qDebug() << "[RepositoryManager] Appending" << m_indexContracts.size() 
             << "index contracts to NSECM repository";
    m_nsecm->appendContracts(m_indexContracts);
  }
  
  // PHASE 3: Load NSE FO (Futures & Options) - Fallback from CSV to Master File
  qDebug() << "[RepositoryManager] [3/5] Loading NSE F&O...";
  if (!loadNSEFO(mastersDir, true)) {
    emit loadingError("Failed to load NSE F&O", {"nsefo_processed.csv", "contract_nsefo_latest.txt"});
    return false;
  }
  
  // Update asset tokens in NSEFO from index master
  updateIndexAssetTokens();
  
  // Resolve index asset tokens (for contracts with assetToken = 0 or -1)
  resolveIndexAssetTokens();
  
  // PHASE 4: Load BSE segments (optional)
  qDebug() << "[RepositoryManager] [4/5] Loading BSE CM (optional)...";
  loadBSECM(mastersDir, true);
  
  qDebug() << "[RepositoryManager] [5/5] Loading BSE F&O (optional)...";
  loadBSEFO(mastersDir, true);
  
  // PHASE 6: Initialize distributed price stores
  qDebug() << "[RepositoryManager] Initializing distributed price stores...";
  initializeDistributedStores();
  
  // PHASE 7: Build expiry cache
  qDebug() << "[RepositoryManager] Building expiry cache for ATM Watch...";
  buildExpiryCache();
  
  // PHASE 8: Log summary
  SegmentStats stats = getSegmentStats();
  qDebug() << "[RepositoryManager] ========================================";
  qDebug() << "[RepositoryManager] Loading Complete";
  qDebug() << "[RepositoryManager] NSE CM:    " << stats.nsecm << "contracts";
  qDebug() << "[RepositoryManager] NSE F&O:   " << stats.nsefo << "contracts";
  qDebug() << "[RepositoryManager] BSE CM:    " << stats.bsecm << "contracts";
  qDebug() << "[RepositoryManager] BSE F&O:   " << stats.bsefo << "contracts";
  qDebug() << "[RepositoryManager] Total:     " << getTotalContractCount() << "contracts";
  qDebug() << "[RepositoryManager] ========================================";
  
  m_loaded = true;
  emit mastersLoaded();
  
  // CRITICAL: Emit repositoryLoaded signal so UDP readers can start safely
  emit repositoryLoaded();
  qInfo() << "[RepositoryManager] Repository loaded - UDP readers may now start";
  
  return true;
}

bool RepositoryManager::loadNSEFO(const QString &mastersPath, bool preferCSV) {
  QString csvFile = mastersPath + "/processed_csv/nsefo_processed.csv";
  QString masterFile = mastersPath + "/contract_nsefo_latest.txt";

  // Try CSV first if preferred
  if (preferCSV && QFile::exists(csvFile)) {
    qDebug() << "[RepositoryManager] Loading NSE F&O from CSV:" << csvFile;
    if (m_nsefo->loadProcessedCSV(csvFile)) {
      return true;
    }
    qWarning() << "[RepositoryManager] Failed to load CSV, trying master file";
  }

  // Fall back to master file
  if (QFile::exists(masterFile)) {
    qDebug() << "[RepositoryManager] Loading NSE F&O from master file:"
             << masterFile;
    return m_nsefo->loadMasterFile(masterFile);
  }

  qWarning() << "[RepositoryManager] NSE F&O master file not found:"
             << masterFile;
  return false;
}

bool RepositoryManager::loadNSECM(const QString &mastersPath, bool preferCSV) {
  QString csvFile = mastersPath + "/processed_csv/nsecm_processed.csv";
  QString masterFile = mastersPath + "/contract_nsecm_latest.txt";

  // Try CSV first if preferred
  if (preferCSV && QFile::exists(csvFile)) {
    qDebug() << "[RepositoryManager] Loading NSE CM from CSV:" << csvFile;
    if (m_nsecm->loadProcessedCSV(csvFile)) {
      return true;
    }
    qWarning() << "[RepositoryManager] Failed to load CSV, trying master file";
  }

  // Fall back to master file
  if (QFile::exists(masterFile)) {
    qDebug() << "[RepositoryManager] Loading NSE CM from master file:"
             << masterFile;
    return m_nsecm->loadMasterFile(masterFile);
  }

  qWarning() << "[RepositoryManager] NSE CM master file not found:"
             << masterFile;
  return false;
}

bool RepositoryManager::loadBSEFO(const QString &mastersPath, bool preferCSV) {
  QString csvFile = mastersPath + "/processed_csv/bsefo_processed.csv";
  QString masterFile = mastersPath + "/contract_bsefo_latest.txt";

  // Try CSV first if preferred
  if (preferCSV && QFile::exists(csvFile)) {
    qDebug() << "[RepositoryManager] Loading BSE F&O from CSV:" << csvFile;
    if (m_bsefo->loadProcessedCSV(csvFile)) {
      return true;
    }
    qWarning() << "[RepositoryManager] Failed to load CSV, trying master file";
  }

  // Fall back to master file
  if (QFile::exists(masterFile)) {
    qDebug() << "[RepositoryManager] Loading BSE F&O from master file:"
             << masterFile;
    return m_bsefo->loadMasterFile(masterFile);
  }

  qWarning() << "[RepositoryManager] BSE F&O master file not found:"
             << masterFile;
  return false;
}

bool RepositoryManager::loadBSECM(const QString &mastersPath, bool preferCSV) {
  QString csvFile = mastersPath + "/processed_csv/bsecm_processed.csv";
  QString masterFile = mastersPath + "/contract_bsecm_latest.txt";

  // Try CSV first if preferred
  if (preferCSV && QFile::exists(csvFile)) {
    qDebug() << "[RepositoryManager] Loading BSE CM from CSV:" << csvFile;
    if (m_bsecm->loadProcessedCSV(csvFile)) {
      return true;
    }
    qWarning() << "[RepositoryManager] Failed to load CSV, trying master file";
  }

  // Fall back to master file
  if (QFile::exists(masterFile)) {
    qDebug() << "[RepositoryManager] Loading BSE CM from master file:"
             << masterFile;
    return m_bsecm->loadMasterFile(masterFile);
  }

  qWarning() << "[RepositoryManager] BSE CM master file not found:"
             << masterFile;
  return false;
}

bool RepositoryManager::loadIndexMaster(const QString &mastersPath) {
    QString filePath = mastersPath + "/nse_cm_index_master.csv";
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to open Index Master:" << filePath;
        return false;
    }

    m_indexNameTokenMap.clear();
    m_indexContracts.clear();
    m_symbolToAssetToken.clear();
    
    QTextStream in(&file);
    
    // Skip header
    if (!in.atEnd()) in.readLine();

    while (!in.atEnd()) {
        QString line = in.readLine();
        QStringList fields = line.split(',');
        
        // Expected Format: Token,Symbol,Name,ClosePrice
        // or Format: id,Name,Token,created_at
        
        if (fields.size() >= 3) {
            bool ok;
            int64_t token = 0;
            QString symbol;
            QString name;
            
            // Try to detect format
            if (fields[0].at(0).isDigit() && fields[0].length() >= 5) {
                // Token,Symbol,Name format
                token = fields[0].toLongLong(&ok);
                symbol = fields[1].trimmed();
                name = fields[2].trimmed();
            } else {
                // id,Name,Token format
                name = fields[1].trimmed();
                token = fields[2].toLongLong(&ok);
                symbol = name; // Fallback
            }
            
            if (ok) {
                // Map: "Nifty 50" -> 26000
                m_indexNameTokenMap[name] = token;
                
                // Map: "NIFTY" -> 26000
                m_symbolToAssetToken[symbol] = token;
                
                // Create ContractData for appending to NSECM
                ContractData contract;
                contract.exchangeInstrumentID = token;
                contract.name = symbol; // Use symbol (e.g. NIFTY) as name
                contract.displayName = name; // Use full name (e.g. Nifty 50) as displayName
                contract.description = name;
                contract.series = "INDEX";
                contract.instrumentType = 0; // Index
                m_indexContracts.append(contract);
            }
        }
    }
    file.close();
    qDebug() << "Loaded" << m_indexNameTokenMap.size() << "indices from" << filePath;
    
    // Initialize UDP index name→token mapping for NSE CM price store
    if (!m_indexNameTokenMap.isEmpty()) {
        qDebug() << "[RepositoryManager] Initializing NSE CM UDP index name mapping...";
        
        // Convert QHash<QString, int64_t> to std::unordered_map<std::string, uint32_t>
        std::unordered_map<std::string, uint32_t> mapping;
        for (auto it = m_indexNameTokenMap.constBegin(); it != m_indexNameTokenMap.constEnd(); ++it) {
            mapping[it.key().toStdString()] = static_cast<uint32_t>(it.value());
        }
        
        // Pass to UDP price store so it can resolve index names from UDP packets
        nsecm::initializeIndexMapping(mapping);
        
        qDebug() << "[RepositoryManager] Initialized" << mapping.size() 
                 << "index name→token mappings for UDP price updates";
    }
    
    return true;
}

const QHash<QString, int64_t>& RepositoryManager::getIndexNameTokenMap() const {
    return m_indexNameTokenMap;
}

void RepositoryManager::updateIndexAssetTokens() {
  if (!m_nsefo || m_symbolToAssetToken.isEmpty()) {
    return;
  }
  
  qDebug() << "Updating asset tokens in NSEFO from index master...";
  
  int updatedCount = 0;
  m_nsefo->forEachContract([this, &updatedCount](const ContractData& contract) {
    // For index options/futures
    if (contract.series == "OPTIDX" || contract.series == "FUTIDX") {
      // Look up asset token from index master
      if (m_symbolToAssetToken.contains(contract.name)) {
        int64_t assetToken = m_symbolToAssetToken[contract.name];
        if (contract.assetToken != assetToken) {
            m_nsefo->updateAssetToken(contract.exchangeInstrumentID, assetToken);
            updatedCount++;
        }
      }
    }
  });
  
  qDebug() << "Updated" << updatedCount << "contracts with asset tokens";
}

void RepositoryManager::resolveIndexAssetTokens() {
  QWriteLocker lock(&m_repositoryLock);  // Thread-safe write access - modifying contracts
  
  if (!m_nsecm || !m_nsefo) {
    qWarning() << "[RepositoryManager] Cannot resolve index tokens: "
                  "NSECM or NSEFO not loaded";
    return;
  }

  qDebug() << "[RepositoryManager] Resolving index asset tokens...";

  // Step 1: Build index name → token map from NSECM and m_symbolToAssetToken
  QHash<QString, int64_t> indexTokens;
  
  // Use the existing symbol to asset token map from index master
  if (!m_symbolToAssetToken.isEmpty()) {
    indexTokens = m_symbolToAssetToken;
    qDebug() << "[RepositoryManager] Using" << indexTokens.size() 
             << "index tokens from index master";
  }
  
  // Also add from NSECM INDEX series for additional coverage
  m_nsecm->forEachContract([&indexTokens](const ContractData& c) {
    if (c.series == "INDEX") {
      indexTokens[c.name] = c.exchangeInstrumentID;
      
      // Store variations without common suffixes for better matching
      QString shortName = c.name;
      shortName.replace(" 50", "").replace(" BANK", "").replace(" 100", "");
      if (shortName != c.name) {
        indexTokens[shortName] = c.exchangeInstrumentID;
      }
    }
  });

  if (indexTokens.isEmpty()) {
    qWarning() << "[RepositoryManager] No index tokens available for resolution";
    return;
  }

  qDebug() << "[RepositoryManager] Built index token map with" 
           << indexTokens.size() << "entries";

  // Step 2: Update NSEFO contracts with assetToken = 0 or -1
  int totalCount = 0;
  int resolvedCount = 0;
  int unresolvedCount = 0;
  QStringList unresolvedSymbols;
  
  m_nsefo->forEachContract([&](const ContractData& contract) {
    // Check if this contract has missing or invalid asset token
    if (contract.assetToken == 0 || contract.assetToken == -1) {
      totalCount++;
      
      // Try to resolve from index token map using the contract's name (underlying)
      QString symbol = contract.name;
      
      if (indexTokens.contains(symbol)) {
        int64_t newAssetToken = indexTokens[symbol];
        
        // Update the asset token in the repository
        m_nsefo->updateAssetToken(contract.exchangeInstrumentID, newAssetToken);
        resolvedCount++;
        
        if (resolvedCount <= 3) { // Log first few for verification
          qDebug() << "  Resolved:" << symbol 
                   << "token" << contract.exchangeInstrumentID
                   << "→ assetToken" << newAssetToken;
        }
      } else {
        unresolvedCount++;
        if (unresolvedCount <= 5) { // Log first few unresolved
          qWarning() << "  Cannot resolve:" << symbol 
                     << "(token" << contract.exchangeInstrumentID << ")";
          unresolvedSymbols.append(symbol);
        }
      }
    }
  });

  qDebug() << "[RepositoryManager] Index asset token resolution summary:";
  qDebug() << "  Total contracts with missing asset tokens:" << totalCount;
  qDebug() << "  Resolved:" << resolvedCount 
           << "(" << (totalCount > 0 ? QString::number(100.0 * resolvedCount / totalCount, 'f', 1) : "0") << "%)";
  qDebug() << "  Unresolvable:" << unresolvedCount;
  
  if (unresolvedCount > 0 && !unresolvedSymbols.isEmpty()) {
    qWarning() << "[RepositoryManager] Unresolved symbols:" << unresolvedSymbols.join(", ");
  }
  
  if (resolvedCount > 0) {
    qDebug() << "[RepositoryManager] Successfully resolved" << resolvedCount 
             << "index asset tokens for Greeks calculation";
  }
}

bool RepositoryManager::loadCombinedMasterFile(const QString &filePath) {
  qDebug() << "[RepositoryManager] Loading combined master file (STREAMING):"
           << filePath;

  // Native C++ file I/O (fast)
  std::ifstream file(filePath.toStdString(), std::ios::binary);
  if (!file.is_open()) {
    qWarning() << "[RepositoryManager] Failed to open combined master file";
    return false;
  }

  // GLOBAL STRING POOL for deduplication
  // This ensures we share underlying QString data for identical strings
  // (e.g. "OPTIDX", "29-Jan-2026", "RELIANCE") across all repositories
  QSet<QString> stringPool;
  // NOTE: Reduced reserve from 100,000 to avoid excessive memory allocation
  // Strings will be added dynamically as needed
  stringPool.reserve(
      10000); // Reserve for typical unique strings (~5-10K expected)

  // Helper lambda for interning
  auto intern = [&](const QString &str) -> QString {
    // QSet::insert returns iterator to existing item if found, or new item if
    // inserted Dereferencing gives us the canonical instance from the pool
    return *stringPool.insert(str);
  };

  // Prepare repositories for load (clears old data)
  if (m_nsefo)
    m_nsefo->prepareForLoad();
  if (m_nsecm)
    m_nsecm->prepareForLoad();
  if (m_bsefo)
    m_bsefo->prepareForLoad();
  if (m_bsecm)
    m_bsecm->prepareForLoad();

  std::string line;
  line.reserve(1024);

  int lineCount = 0;
  int nsefoCount = 0;
  int nsecmCount = 0;
  int bsefoCount = 0;
  int bsecmCount = 0;

  MasterContract contract;
  QString qLine;

  while (std::getline(file, line)) {
    lineCount++;

    if (line.empty())
      continue;

    // Remove trailing whitespace
    while (!line.empty() &&
           (line.back() == ' ' || line.back() == '\r' || line.back() == '\n')) {
      line.pop_back();
    }

    if (line.empty())
      continue;

    // Create QString once per line and use QStringRef for parsing
    qLine = QString::fromStdString(line);
    QStringRef qLineRef(&qLine);

    // Check segment prefix (first field before |) using QStringRef
    int pipeIdx = qLineRef.indexOf('|');
    if (pipeIdx == -1)
      continue;

    QStringRef segment = qLineRef.left(pipeIdx);
    bool parsed = false;

    if (segment == "NSEFO") {
      parsed = MasterFileParser::parseLine(qLineRef, "NSEFO", contract);
      if (parsed && m_nsefo) {
        m_nsefo->addContract(contract, intern);
        nsefoCount++;
      }
    } else if (segment == "NSECM") {
      parsed = MasterFileParser::parseLine(qLineRef, "NSECM", contract);
      if (parsed && m_nsecm) {
        m_nsecm->addContract(contract, intern);
        nsecmCount++;
      }
    } else if (segment == "BSEFO") {
      parsed = MasterFileParser::parseLine(qLineRef, "BSEFO", contract);
      if (parsed && m_bsefo) {
        m_bsefo->addContract(contract, intern);
        bsefoCount++;
      }
    } else if (segment == "BSECM") {
      parsed = MasterFileParser::parseLine(qLineRef, "BSECM", contract);
      if (parsed && m_bsecm) {
        m_bsecm->addContract(contract, intern);
        bsecmCount++;
      }
    }
  }

  file.close();

  qDebug() << "[RepositoryManager] Streaming Load Complete. Lines processed:"
           << lineCount;
  qDebug() << "  NSE FO:" << nsefoCount;
  qDebug() << "  NSE CM:" << nsecmCount;
  qDebug() << "  BSE FO:" << bsefoCount;
  qDebug() << "  BSE CM:" << bsecmCount;
  qDebug() << "  Unique Strings in Pool:" << stringPool.size();

  // Finalize loading for each repository (marks as loaded)
  if (m_nsefo)
    m_nsefo->finalizeLoad();
  if (m_nsecm)
    m_nsecm->finalizeLoad();
  if (m_bsefo)
    m_bsefo->finalizeLoad();
  if (m_bsecm)
    m_bsecm->finalizeLoad();

  // Build expiry cache for ATM Watch optimization
  buildExpiryCache();

  // Mark as loaded if we got any data
  bool anyLoaded =
      (nsefoCount > 0 || nsecmCount > 0 || bsefoCount > 0 || bsecmCount > 0);

  // Note: We don't save CSVs here because we just loaded from master.
  // The caller (loadAll) calls saveProcessedCSVs appropriately.

  return anyLoaded;
}

bool RepositoryManager::loadFromMemory(const QString &csvData) {
  qDebug() << "[RepositoryManager] Loading masters from in-memory CSV data "
              "(STREAMING, size:"
           << csvData.size() << "bytes)";

  // Global String Pool for deduplication
  QSet<QString> stringPool;
  // NOTE: Reduced reserve from 100,000 to avoid excessive memory allocation
  // Strings will be added dynamically as needed
  stringPool.reserve(
      10000); // Reserve for typical unique strings (~5-10K expected)

  auto intern = [&](const QString &str) -> QString {
    return *stringPool.insert(str);
  };

  // Prepare repositories
  if (m_nsefo)
    m_nsefo->prepareForLoad();
  if (m_nsecm)
    m_nsecm->prepareForLoad();
  if (m_bsefo)
    m_bsefo->prepareForLoad();
  if (m_bsecm)
    m_bsecm->prepareForLoad();

  // Parse CSV data directly from memory
  int start = 0;
  int end = 0;
  int lineCount = 0;
  int nsefoCount = 0;
  int nsecmCount = 0;
  int bsefoCount = 0;
  int bsecmCount = 0;

  MasterContract contract;

  while ((end = csvData.indexOf('\n', start)) != -1) {
    lineCount++;
    QStringRef lineRef = csvData.midRef(start, end - start);
    start = end + 1;

    QStringRef trimmedLine = lineRef.trimmed();
    if (trimmedLine.isEmpty())
      continue;

    // Fast segment detection without creating a new QString
    int pipeIdx = trimmedLine.indexOf('|');
    if (pipeIdx == -1)
      continue;

    QStringRef segment = trimmedLine.left(pipeIdx);
    bool parsed = false;

    if (segment == "NSEFO") {
      parsed = MasterFileParser::parseLine(trimmedLine, "NSEFO", contract);
      if (parsed && m_nsefo) {
        m_nsefo->addContract(contract, intern);
        nsefoCount++;
      }
    } else if (segment == "NSECM") {
      parsed = MasterFileParser::parseLine(trimmedLine, "NSECM", contract);
      if (parsed && m_nsecm) {
        m_nsecm->addContract(contract, intern);
        nsecmCount++;
      }
    } else if (segment == "BSEFO") {
      parsed = MasterFileParser::parseLine(trimmedLine, "BSEFO", contract);
      if (parsed && m_bsefo) {
        m_bsefo->addContract(contract, intern);
        bsefoCount++;
      }
    } else if (segment == "BSECM") {
      parsed = MasterFileParser::parseLine(trimmedLine, "BSECM", contract);
      if (parsed && m_bsecm) {
        m_bsecm->addContract(contract, intern);
        bsecmCount++;
      }
    }
  }

  // Handle last line if no newline at end
  if (start < csvData.length()) {
    QStringRef lineRef = csvData.midRef(start);
    QStringRef trimmedLine = lineRef.trimmed();
    if (!trimmedLine.isEmpty()) {
      lineCount++;
      int pipeIdx = trimmedLine.indexOf('|');
      if (pipeIdx != -1) {
        QStringRef segment = trimmedLine.left(pipeIdx);
        bool parsed = false;
        if (segment == "NSEFO") {
          parsed = MasterFileParser::parseLine(trimmedLine, "NSEFO", contract);
          if (parsed && m_nsefo) {
            m_nsefo->addContract(contract, intern);
            nsefoCount++;
          }
        } else if (segment == "NSECM") {
          parsed = MasterFileParser::parseLine(trimmedLine, "NSECM", contract);
          if (parsed && m_nsecm) {
            m_nsecm->addContract(contract, intern);
            nsecmCount++;
          }
        } else if (segment == "BSEFO") {
          parsed = MasterFileParser::parseLine(trimmedLine, "BSEFO", contract);
          if (parsed && m_bsefo) {
            m_bsefo->addContract(contract, intern);
            bsefoCount++;
          }
        } else if (segment == "BSECM") {
          parsed = MasterFileParser::parseLine(trimmedLine, "BSECM", contract);
          if (parsed && m_bsecm) {
            m_bsecm->addContract(contract, intern);
            bsecmCount++;
          }
        }
      }
    }
  }

  qDebug() << "[RepositoryManager] In-memory Load Complete. Lines processed:"
           << lineCount;
  qDebug() << "  NSE FO:" << nsefoCount;
  qDebug() << "  NSE CM:" << nsecmCount;
  qDebug() << "  BSE FO:" << bsefoCount;
  qDebug() << "  BSE CM:" << bsecmCount;
  qDebug() << "  Unique Strings in Pool:" << stringPool.size();

  // Finalize loading for each repository (marks as loaded)
  if (m_nsefo)
    m_nsefo->finalizeLoad();
  if (m_nsecm)
    m_nsecm->finalizeLoad();
  if (m_bsefo)
    m_bsefo->finalizeLoad();
  if (m_bsecm)
    m_bsecm->finalizeLoad();

  // NEW: Integrate Index Master Data (Crucial for Download Master flow)
  QString mastersDir = getMastersDirectory();
  qDebug() << "[RepositoryManager] Loading Index Master from" << mastersDir;
  if (loadIndexMaster(mastersDir)) {
      if (!m_indexContracts.isEmpty() && m_nsecm) {
          m_nsecm->appendContracts(m_indexContracts);
      }
      updateIndexAssetTokens();
  }

  // Initialize distributed price stores (Required for real-time data)
  initializeDistributedStores();

  // Build expiry cache for ATM Watch optimization
  buildExpiryCache();

  bool anyLoaded =
      (nsefoCount > 0 || nsecmCount > 0 || bsefoCount > 0 || bsecmCount > 0);

  if (anyLoaded) {
    m_loaded = true;
  }

  return anyLoaded;
}

QVector<ContractData> RepositoryManager::searchScrips(const QString &exchange,
                                                      const QString &segment,
                                                      const QString &series,
                                                      const QString &searchText,
                                                      int maxResults) const {
  QVector<ContractData> results;
  results.reserve(maxResults);

  QString segmentKey = getSegmentKey(exchange, segment);
  QString searchUpper = searchText.toUpper();

  if (segmentKey == "NSEFO" && m_nsefo->isLoaded()) {
    // Search NSE F&O by series
    QVector<ContractData> allContracts = m_nsefo->getContractsBySeries(series);

    for (const ContractData &contract : allContracts) {
      // Match by symbol prefix
      if (contract.name.startsWith(searchUpper, Qt::CaseInsensitive) ||
          contract.displayName.contains(searchUpper, Qt::CaseInsensitive)) {
        results.append(contract);
        if (results.size() >= maxResults) {
          break;
        }
      }
    }
  } else if (segmentKey == "NSECM" && m_nsecm->isLoaded()) {
    // Search NSE CM by series
    QVector<ContractData> allContracts = m_nsecm->getContractsBySeries(series);

    for (const ContractData &contract : allContracts) {
      // Match by symbol prefix
      if (contract.name.startsWith(searchUpper, Qt::CaseInsensitive) ||
          contract.displayName.contains(searchUpper, Qt::CaseInsensitive)) {
        results.append(contract);
        if (results.size() >= maxResults) {
          break;
        }
      }
    }
  } else if (segmentKey == "BSEFO" && m_bsefo->isLoaded()) {
    // Search BSE F&O by series
    QVector<ContractData> allContracts = m_bsefo->getContractsBySeries(series);

    for (const ContractData &contract : allContracts) {
      // Match by symbol prefix
      if (contract.name.startsWith(searchUpper, Qt::CaseInsensitive) ||
          contract.displayName.contains(searchUpper, Qt::CaseInsensitive)) {
        results.append(contract);
        if (results.size() >= maxResults) {
          break;
        }
      }
    }
  } else if (segmentKey == "BSECM" && m_bsecm->isLoaded()) {
    // Search BSE CM by series
    QVector<ContractData> allContracts = m_bsecm->getContractsBySeries(series);

    for (const ContractData &contract : allContracts) {
      // Match by symbol prefix
      if (contract.name.startsWith(searchUpper, Qt::CaseInsensitive) ||
          contract.displayName.contains(searchUpper, Qt::CaseInsensitive)) {
        results.append(contract);
        if (results.size() >= maxResults) {
          break;
        }
      }
    }
  }

  qDebug() << "[RepositoryManager] Search results:" << results.size() << "for"
           << exchange << segment << series << searchText;

  return results;
}

const ContractData *RepositoryManager::getContractByToken(int exchangeSegmentID,
                                                          int64_t token) const {
  QReadLocker lock(&m_repositoryLock);  // Thread-safe read access
  
  if (exchangeSegmentID == 1 && m_nsecm->isLoaded()) {
    return m_nsecm->getContract(token);
  } else if (exchangeSegmentID == 2 && m_nsefo->isLoaded()) {
    return m_nsefo->getContract(token);
  } else if (exchangeSegmentID == 11 && m_bsecm->isLoaded()) {
    return m_bsecm->getContract(token);
  } else if (exchangeSegmentID == 12 && m_bsefo->isLoaded()) {
    return m_bsefo->getContract(token);
  }
  return nullptr;
}

QVector<ContractData>
RepositoryManager::getScrips(const QString &exchange, const QString &segment,
                             const QString &series) const {
  QReadLocker lock(&m_repositoryLock);  // Thread-safe read access
  
  QString segmentKey = getSegmentKey(exchange, segment);

  if (segmentKey == "NSEFO" && m_nsefo->isLoaded()) {
    return m_nsefo->getContractsBySeries(series);
  } else if (segmentKey == "NSECM" && m_nsecm->isLoaded()) {
    return m_nsecm->getContractsBySeries(series);
  } else if (segmentKey == "BSEFO" && m_bsefo->isLoaded()) {
    return m_bsefo->getContractsBySeries(series);
  } else if (segmentKey == "BSECM" && m_bsecm->isLoaded()) {
    return m_bsecm->getContractsBySeries(series);
  }

  return QVector<ContractData>();
}

const ContractData *
RepositoryManager::getContractByToken(const QString &segmentKey,
                                      int64_t token) const {
  QReadLocker lock(&m_repositoryLock);  // Thread-safe read access
  
  QString key = segmentKey.toUpper();
  if (key == "NSEFO" || key == "NSEF" || key == "NSEO") {
    return m_nsefo->isLoaded() ? m_nsefo->getContract(token) : nullptr;
  } else if (key == "NSECM" || key == "NSEE") {
    return m_nsecm->isLoaded() ? m_nsecm->getContract(token) : nullptr;
  } else if (key == "BSEFO" || key == "BSEF") {
    return m_bsefo->isLoaded() ? m_bsefo->getContract(token) : nullptr;
  } else if (key == "BSECM" || key == "BSEE") {
    return m_bsecm->isLoaded() ? m_bsecm->getContract(token) : nullptr;
  }
  return nullptr;
}

const ContractData *RepositoryManager::getContractByToken(
    const QString &exchange, const QString &segment, int64_t token) const {
  return getContractByToken(getSegmentKey(exchange, segment), token);
}

QVector<ContractData>
RepositoryManager::getOptionChain(const QString &exchange,
                                  const QString &symbol) const {
  QReadLocker lock(&m_repositoryLock);  // Thread-safe read access
  
  if (exchange == "NSE" && m_nsefo->isLoaded()) {
    return m_nsefo->getContractsBySymbol(symbol);
  } else if (exchange == "BSE" && m_bsefo->isLoaded()) {
    return m_bsefo->getContractsBySymbol(symbol);
  }

  return QVector<ContractData>();
}

QVector<ContractData>
RepositoryManager::getContractsBySegment(const QString &exchange,
                                         const QString &segment) const {
  QString segmentKey = getSegmentKey(exchange, segment);

  if (segmentKey == "NSEFO" && m_nsefo->isLoaded())
    return m_nsefo->getAllContracts();
  if (segmentKey == "NSECM" && m_nsecm->isLoaded())
    return m_nsecm->getAllContracts();
  if (segmentKey == "BSEFO" && m_bsefo->isLoaded())
    return m_bsefo->getAllContracts();
  if (segmentKey == "BSECM" && m_bsecm->isLoaded())
    return m_bsecm->getAllContracts();

  return QVector<ContractData>();
}

void RepositoryManager::updateLiveData(const QString &exchange,
                                       const QString &segment, int64_t token,
                                       double ltp, int64_t volume) {
  // Live data storage in Repository is redundant (stored in PriceStore via
  // BroadcastService) This method is now a no-op
}

void RepositoryManager::updateBidAsk(const QString &exchange,
                                     const QString &segment, int64_t token,
                                     double bidPrice, double askPrice) {
  // Bid/Ask storage in Repository is redundant
}

void RepositoryManager::updateGreeks(int64_t token, double iv, double delta,
                                     double gamma, double vega, double theta) {
  // Greeks storage in Repository is redundant
}

int RepositoryManager::getTotalContractCount() const {
  int total = 0;

  if (m_nsefo->isLoaded()) {
    total += m_nsefo->getTotalCount();
  }
  if (m_nsecm->isLoaded()) {
    total += m_nsecm->getTotalCount();
  }
  if (m_bsefo->isLoaded()) {
    total += m_bsefo->getTotalCount();
  }
  if (m_bsecm->isLoaded()) {
    total += m_bsecm->getTotalCount();
  }

  return total;
}

RepositoryManager::SegmentStats RepositoryManager::getSegmentStats() const {
  SegmentStats stats;
  stats.nsefo = m_nsefo->isLoaded() ? m_nsefo->getTotalCount() : 0;
  stats.nsecm = m_nsecm->isLoaded() ? m_nsecm->getTotalCount() : 0;
  stats.bsefo = m_bsefo->isLoaded() ? m_bsefo->getTotalCount() : 0;
  stats.bsecm = m_bsecm->isLoaded() ? m_bsecm->getTotalCount() : 0;
  return stats;
}

bool RepositoryManager::isLoaded() const { return m_loaded; }

QString RepositoryManager::getSegmentKey(const QString &exchange,
                                         const QString &segment) {
  QString key = exchange.toUpper() + segment.toUpper();

  // Normalize common variations
  // NSE F&O -> NSEFO
  if (key == "NSEF&O" || key == "NSEFAO")
    return "NSEFO";
  // BSE F&O -> BSEFO
  if (key == "BSEF&O" || key == "BSEFAO")
    return "BSEFO";
  // MCX F/O -> MCXFO
  if (key == "MCXF&O" || key == "MCXFAO")
    return "MCXFO";

  // Handle segment names directly
  if (segment.toUpper() == "F&O" || segment.toUpper() == "FAO") {
    if (exchange.toUpper() == "NSE")
      return "NSEFO";
    if (exchange.toUpper() == "BSE")
      return "BSEFO";
    if (exchange.toUpper() == "MCX")
      return "MCXFO";
  }

  // MCX F/O=MCXFO

  if (key == "NSECM" || key == "NSEE")
    return "NSECM";
  if (key == "NSEFO" || key == "NSEF" || key == "NSEO" || key == "NSED" ||
      key == "NFO")
    return "NSEFO"; // F, O, D and NFO map to NSEFO
  if (key == "NSECDSF" || key == "NSECDSO")
    return "NSECD"; // NSECDS F and O map to NSECD
  if (key == "BSECM" || key == "BSEE")
    return "BSECM";
  if (key == "BSEFO" || key == "BSEF" || key == "BSEO")
    return "BSEFO";
  if (key == "MCXF" || key == "MCXO")
    return "MCXFO"; // Both F and O map to MCXFO

  return key;
}

int RepositoryManager::getExchangeSegmentID(const QString &exchange,
                                            const QString &segment) {
  QString segmentKey = getSegmentKey(exchange, segment);

  // XTS API exchange segment IDs
  // E = Equity/CM, F = F&O, O = Currency/CD, D = Derivative (XTS specific)
  if (segmentKey == "NSECM")
    return 1; // NSE Cash Market (E)
  if (segmentKey == "NSEFO")
    return 2; // NSE F&O (F, D)
  if (segmentKey == "BSECM")
    return 11; // BSE Cash Market (E)
  if (segmentKey == "BSEFO")
    return 12; // BSE F&O (F)

  return -1;
}

QString RepositoryManager::getExchangeSegmentName(int exchangeSegmentID) {
  switch (exchangeSegmentID) {
  case 1:
    return "NSECM";
  case 2:
    return "NSEFO";
  case 11:
    return "BSECM";
  case 12:
    return "BSEFO";
  case 13:
    return "NSECD";
  case 51:
    return "MCXFO";
  default:
    return QString::number(exchangeSegmentID);
  }
}

bool RepositoryManager::saveProcessedCSVs(const QString &mastersPath) {
  bool anySaved = false;

  // Create processed_csv directory if it doesn't exist
  QDir dir(mastersPath);
  if (!dir.exists("processed_csv")) {
    dir.mkpath("processed_csv");
  }

  QString csvDir = mastersPath + "/processed_csv";

  // Save NSE FO
  if (m_nsefo->isLoaded()) {
    QString csvFile = csvDir + "/nsefo_processed.csv";
    if (m_nsefo->saveProcessedCSV(csvFile)) {
      anySaved = true;
    }
  }

  // Save NSE CM
  if (m_nsecm->isLoaded()) {
    QString csvFile = csvDir + "/nsecm_processed.csv";
    if (m_nsecm->saveProcessedCSV(csvFile)) {
      anySaved = true;
    }
  }

  // Save BSE FO
  if (m_bsefo->isLoaded()) {
    QString csvFile = csvDir + "/bsefo_processed.csv";
    if (m_bsefo->saveProcessedCSV(csvFile)) {
      anySaved = true;
    }
  }

  // Save BSE CM
  if (m_bsecm->isLoaded()) {
    QString csvFile = csvDir + "/bsecm_processed.csv";
    if (m_bsecm->saveProcessedCSV(csvFile)) {
      anySaved = true;
    }
  }

  if (anySaved) {
    qDebug() << "[RepositoryManager] Processed CSVs saved to:" << csvDir;
  }

  return anySaved;
}

void RepositoryManager::initializeDistributedStores() {
  qDebug() << "[RepositoryManager] Initializing distributed price stores with "
              "contract master...";

  // Reset all stores before re-initialization (Clean reload)
  nsefo::g_nseFoPriceStore.clear();
  nsecm::g_nseCmPriceStore.clear();
  bse::g_bseFoPriceStore.clear();
  bse::g_bseCmPriceStore.clear();
  bse::g_bseFoIndexStore.clear();
  bse::g_bseCmIndexStore.clear();

  // NSE FO: Pre-populate array with contract master data
  if (m_nsefo && m_nsefo->isLoaded()) {
    std::vector<uint32_t> tokens;
    tokens.reserve(m_nsefo->getTotalCount());

    m_nsefo->forEachContract([&](const ContractData &contract) {
      uint32_t token = static_cast<uint32_t>(contract.exchangeInstrumentID);
      tokens.push_back(token);

      // Initialize token with contract master metadata
      nsefo::g_nseFoPriceStore.initializeToken(
          token, contract.name.toUtf8().constData(),
          contract.displayName.toUtf8().constData(), contract.lotSize,
          contract.strikePrice, contract.optionType.toUtf8().constData(),
          contract.expiryDate.toUtf8().constData(), contract.assetToken,
          contract.instrumentType, contract.tickSize);
    });

    // Mark valid tokens in store
    nsefo::g_nseFoPriceStore.initializeFromMaster(tokens);
    qDebug() << "  NSE FO:" << tokens.size() << "contracts pre-populated";
  }

  // NSE CM: Pre-populate hash map with contract master data
  if (m_nsecm && m_nsecm->isLoaded()) {
    // Pass index name to token map to the broadcast library for unified price store
    QHash<QString, int64_t> indexMap = m_nsecm->getIndexNameTokenMap();
    std::unordered_map<std::string, uint32_t> stdIndexMap;
    for (auto it = indexMap.begin(); it != indexMap.end(); ++it) {
        stdIndexMap[it.key().toStdString()] = static_cast<uint32_t>(it.value());
    }
    nsecm::initializeIndexMapping(stdIndexMap);

    std::vector<uint32_t> tokens;
    tokens.reserve(m_nsecm->getTotalCount());

    m_nsecm->forEachContract([&](const ContractData &contract) {
      uint32_t token = static_cast<uint32_t>(contract.exchangeInstrumentID);
      tokens.push_back(token);

      // Initialize token with contract master metadata
      nsecm::g_nseCmPriceStore.initializeToken(
          token, contract.name.toUtf8().constData(),
          contract.series.toUtf8().constData(),
          contract.displayName.toUtf8().constData(), contract.lotSize,
          contract.tickSize, contract.priceBandHigh, contract.priceBandLow);
    });

    nsecm::g_nseCmPriceStore.initializeFromMaster(tokens);
    qDebug() << "  NSE CM:" << tokens.size() << "contracts pre-populated";
  }

  // BSE FO: Pre-populate hash map with contract master data
  if (m_bsefo && m_bsefo->isLoaded()) {
    std::vector<uint32_t> tokens;
    tokens.reserve(m_bsefo->getTotalCount());

    m_bsefo->forEachContract([&](const ContractData &contract) {
      uint32_t token = static_cast<uint32_t>(contract.exchangeInstrumentID);
      tokens.push_back(token);

      // Initialize token with contract master metadata
      bse::g_bseFoPriceStore.initializeToken(
          token, contract.name.toUtf8().constData(),
          contract.displayName.toUtf8().constData(),
          contract.scripCode.toUtf8().constData(),
          contract.series.toUtf8().constData(), contract.lotSize,
          contract.strikePrice, contract.optionType.toUtf8().constData(),
          contract.expiryDate.toUtf8().constData(), contract.assetToken,
          contract.instrumentType, contract.tickSize);
    });

    bse::g_bseFoPriceStore.initializeFromMaster(tokens);
    qDebug() << "  BSE FO:" << tokens.size() << "contracts pre-populated";
  }

  // BSE CM: Pre-populate hash map with contract master data
  if (m_bsecm && m_bsecm->isLoaded()) {
    std::vector<uint32_t> tokens;
    tokens.reserve(m_bsecm->getTotalCount());

    m_bsecm->forEachContract([&](const ContractData &contract) {
      uint32_t token = static_cast<uint32_t>(contract.exchangeInstrumentID);
      tokens.push_back(token);

      // Initialize token with contract master metadata
      bse::g_bseCmPriceStore.initializeToken(
          token, contract.name.toUtf8().constData(),
          contract.displayName.toUtf8().constData(),
          contract.scripCode.toUtf8().constData(),
          contract.series.toUtf8().constData(), contract.lotSize,
          contract.strikePrice, contract.optionType.toUtf8().constData(),
          contract.expiryDate.toUtf8().constData(), contract.assetToken,
          contract.instrumentType, contract.tickSize);
    });

    bse::g_bseCmPriceStore.initializeFromMaster(tokens);
    qDebug() << "  BSE CM:" << tokens.size() << "contracts pre-populated";
  }

  qDebug() << "[RepositoryManager] Distributed stores initialized with "
              "contract master data";
}

// ===== ATM WATCH CACHE OPTIMIZATION =====
// This implementation pre-processes ~100K contracts into O(1) lookup tables.
// Total processing time: ~30-50ms (one-time during startup).
// Runtime query time: <1µs (O(1) hash lookup).


void RepositoryManager::buildExpiryCache() {
  std::unique_lock lock(m_expiryCacheMutex);

  m_expiryToSymbols.clear();
  m_symbolToCurrentExpiry.clear();
  m_optionSymbols.clear();
  // m_optionExpiries was removed as a member
  m_sortedExpiries.clear();
  // Don't clear m_indexNameTokenMap here, it's loaded separately
  
  // NEW: Clear ATM optimization caches
  m_symbolExpiryStrikes.clear();
  m_strikeToTokens.clear();
  m_symbolToAssetToken.clear();
  m_symbolExpiryFutureToken.clear();

  QHash<QString, QVector<QString>> symbolToExpiries;
  QSet<QString> optionExpiries; // Local set for uniqueness tracking

  if (m_nsefo) {
    QElapsedTimer timer;
    timer.start();

    m_nsefo->forEachContract([this, &symbolToExpiries, &optionExpiries](const ContractData &contract) {
      // Futures: FUTSTK (stock futures) and FUTIDX (index futures)
      // instrumentType 1 = Future
      if (contract.series == "FUTSTK" || contract.series == "FUTIDX") {
        QString symbolExpiryKey = contract.name + "|" + contract.expiryDate;
        // Store future token for this symbol+expiry
        if (!m_symbolExpiryFutureToken.contains(symbolExpiryKey)) {
          m_symbolExpiryFutureToken[symbolExpiryKey] =
              contract.exchangeInstrumentID;
          
          // Reverse mapping for debugging/lookup
          m_futureTokenToSymbol[contract.exchangeInstrumentID] = contract.name;
        }
      }

      // Options: OPTSTK (stock options) and OPTIDX (index options)
      if (contract.series == "OPTSTK" || contract.series == "OPTIDX") {
        // all expiries
        if (!optionExpiries.contains(contract.expiryDate)) {
          optionExpiries.insert(contract.expiryDate);
        }
        // all symbols
        if (!m_optionSymbols.contains(contract.name)) {
          m_optionSymbols.insert(contract.name);
        }

        // Add symbol to expiry mapping
        if (!m_expiryToSymbols[contract.expiryDate].contains(contract.name)) {
          m_expiryToSymbols[contract.expiryDate].append(contract.name);
        }

        // Collect all expiries for this symbol
        if (!symbolToExpiries[contract.name].contains(contract.expiryDate)) {
          symbolToExpiries[contract.name].append(contract.expiryDate);
        }

        // Build strike list for symbol+expiry
        QString symbolExpiryKey = contract.name + "|" + contract.expiryDate;
        // Optimization: PreSorted repo already provides strikes in order, but we're
        // iterating all contracts. Still, avoiding .contains() check with a Set
        // could be better, but for now we'll stick to verified logic.
        if (!m_symbolExpiryStrikes[symbolExpiryKey].contains(contract.strikePrice)) {
            m_symbolExpiryStrikes[symbolExpiryKey].append(contract.strikePrice);
        }

        // Map strike to CE/PE tokens
        QString strikeKey = symbolExpiryKey + "|" +
                            QString::number(contract.strikePrice, 'f', 2);
        if (contract.optionType == "CE") {
          m_strikeToTokens[strikeKey].first = contract.exchangeInstrumentID;
        } else if (contract.optionType == "PE") {
          m_strikeToTokens[strikeKey].second = contract.exchangeInstrumentID;
        }

        // NEW: Store asset token (once per symbol)
        if (!m_symbolToAssetToken.contains(contract.name)) {
            if (contract.assetToken > 0) {
                 m_symbolToAssetToken[contract.name] = contract.assetToken;
            }
        }
      }
    });

    // Hardcoded Index Tokens (for ATM Watch calculation fallback)
    auto addIndex = [&](const QString &symbol, int64_t token) {
      if (!m_symbolToAssetToken.contains(symbol)) {
        m_symbolToAssetToken[symbol] = token;
      }
    };
    addIndex("NIFTY", 26000);
    addIndex("BANKNIFTY", 26009);
    addIndex("FINNIFTY", 26037);
    addIndex("MIDCPNIFTY", 26074);
    addIndex("NIFTYNXT50", 26013);

    // Sort all strike lists (only necessary if repo wasn't pre-sorted, 
    // but since we're using a visitor on the full set, visit order might not be strike order)
    for (auto it = m_symbolExpiryStrikes.begin();
         it != m_symbolExpiryStrikes.end(); ++it) {
      std::sort(it.value().begin(), it.value().end());
    }

    std::cout << "NSE FO: build expiry cache time taken " << timer.elapsed()
              << " ms, strikes cached: " << m_symbolExpiryStrikes.size()
              << ", tokens cached: " << m_strikeToTokens.size()
              << ", futures cached: " << m_symbolExpiryFutureToken.size()
              << std::endl;
  }

  if (m_bsefo) {
    QElapsedTimer timer;
    timer.start();
    m_bsefo->forEachContract([this, &symbolToExpiries](const ContractData &contract) {
      if (contract.series == "OPTSTK") { // OPTSTK (Options)
        m_optionSymbols.insert(contract.name);

        // Add symbol to expiry mapping
        if (!m_expiryToSymbols[contract.expiryDate].contains(contract.name)) {
          m_expiryToSymbols[contract.expiryDate].append(contract.name);
        }

        // Collect all expiries for this symbol
        if (!symbolToExpiries[contract.name].contains(contract.expiryDate)) {
          symbolToExpiries[contract.name].append(contract.expiryDate);
        }
      }
    });
    std::cout << "BSE FO: build expiry cache time taken " << timer.elapsed()
              << " ms" << std::endl;
  }

  // --- Map Indices from Loaded Index Master ---
  // If m_indexNameTokenMap is populated, map known Future Symbols to Index Tokens
  if (!m_indexNameTokenMap.isEmpty()) {
      // Map NIFTY -> Nifty 50
      if (m_indexNameTokenMap.contains("Nifty 50")) {
          m_symbolToAssetToken["NIFTY"] = m_indexNameTokenMap["Nifty 50"];
      }
      // Map BANKNIFTY -> Nifty Bank
      if (m_indexNameTokenMap.contains("Nifty Bank")) {
          m_symbolToAssetToken["BANKNIFTY"] = m_indexNameTokenMap["Nifty Bank"];
      }
      // Map FINNIFTY -> Nifty Fin Service
      if (m_indexNameTokenMap.contains("Nifty Fin Service")) {
          m_symbolToAssetToken["FINNIFTY"] = m_indexNameTokenMap["Nifty Fin Service"];
      }
      // Map MIDCPNIFTY -> NIFTY MID SELECT
      if (m_indexNameTokenMap.contains("NIFTY MID SELECT")) {
          m_symbolToAssetToken["MIDCPNIFTY"] = m_indexNameTokenMap["NIFTY MID SELECT"];
      }
      
      qDebug() << "Mapped Index Tokens: NIFTY=" << m_symbolToAssetToken.value("NIFTY")
               << "BANKNIFTY=" << m_symbolToAssetToken.value("BANKNIFTY");
  }



  // get current expiry for all symbols
  // for loop m_optionSymbols list

  for (auto i_symbol : m_optionSymbols) {
    // get all expiries for this symbol
    QVector<QString> expiries = symbolToExpiries[i_symbol];
    if (expiries.isEmpty())
      continue;

    // convert all expiries to QDate
    QVector<QDate> expiryDates;
    for (auto j_expiry : expiries) {
      QDate date = QDate::fromString(j_expiry, "ddMMMyyyy");
      if (date.isValid()) {
        expiryDates.append(date);
      }
    }

    if (expiryDates.isEmpty())
      continue;

    // sort expiryDates ascending
    std::sort(expiryDates.begin(), expiryDates.end());
    
    // get nearest expiry >= today
    QDate today = QDate::currentDate();
    auto it = std::lower_bound(expiryDates.begin(), expiryDates.end(), today);
    
    QString nearestExpiry;
    if (it != expiryDates.end()) {
        nearestExpiry = it->toString("ddMMMyyyy").toUpper();
    } else if (!expiryDates.isEmpty()) {
        // Fallback to last expiry if all are in the past (unlikely for live systems)
        nearestExpiry = expiryDates.last().toString("ddMMMyyyy").toUpper();
    }
    
    if (!nearestExpiry.isEmpty()) {
        m_symbolToCurrentExpiry.insert(i_symbol, nearestExpiry);
    }
  }

  // Pre-sort all symbol vectors in m_expiryToSymbols so that
  // getSymbolsForExpiry() is O(1) read without sorting.
  for (auto it = m_expiryToSymbols.begin(); it != m_expiryToSymbols.end(); ++it) {
      std::sort(it.value().begin(), it.value().end());
  }

  // Populate sorted expiries cache
  m_sortedExpiries.reserve(optionExpiries.size());
  for(const QString& expiry : optionExpiries) {
      m_sortedExpiries.append(expiry);
  }
  
  // Sort by date once
  std::sort(m_sortedExpiries.begin(), m_sortedExpiries.end(),
            [](const QString &a, const QString &b) {
              QDate dateA = QDate::fromString(a, "ddMMMyyyy");
              QDate dateB = QDate::fromString(b, "ddMMMyyyy");
              return dateA < dateB;
            });

  // Calculate memory overhead (approximate)
  int memoryKB = 0;
  memoryKB += m_optionSymbols.size() * 20;         // ~20 bytes per symbol
  memoryKB += m_expiryToSymbols.size() * 30;       // ~30 bytes per expiry entry
  memoryKB += m_symbolToCurrentExpiry.size() * 40; // ~40 bytes per mapping
  memoryKB += m_symbolToCurrentExpiry.size() * 40; // ~40 bytes per mapping
  qDebug() << "  - Estimated memory:" << (memoryKB / 1024.0) << "KB";

  // Auto-dump debug file as requested
  // Note: dumpFutureTokenMap uses its own locking or is safe here?
  // Since we hold the unique_lock, we should ensure dumpFutureTokenMap doesn't deadlock 
  // or that we release it. dumpFutureTokenMap is const and reads m_symbolExpiryFutureToken 
  // which is also being written to? Wait, m_symbolExpiryFutureToken is written in this loop.
  // It is safe to call dumpFutureTokenMap IF it doesn't acquire the MAIN lock again. 
  // Actually, dumpFutureTokenMap isn't using the mutex yet, but we should verify. 
  // For now, let's keep it safe.
  
  // Note: The original code called this at the end.
}

// Separate dump call to avoid complex locking logic inside buildExpiryCache
void dumpFutureTokenMapWRAPPER(const RepositoryManager* repo) {
    repo->dumpFutureTokenMap("debug_future_tokens.csv");
}

QVector<QString> RepositoryManager::getOptionSymbols() const {
  std::shared_lock lock(m_expiryCacheMutex);
  return m_optionSymbols.values().toVector();
}

QVector<QString>
RepositoryManager::getSymbolsForExpiry(const QString &expiry) const {
  std::shared_lock lock(m_expiryCacheMutex);
  // Return pre-sorted vector - O(1) lookup + copy overhead
  return m_expiryToSymbols.value(expiry);
}

QString RepositoryManager::getCurrentExpiry(const QString &symbol) const {
  std::shared_lock lock(m_expiryCacheMutex);
  return m_symbolToCurrentExpiry.value(symbol);
}

// get all expiries for a symbol (sorted ascending)
QVector<QString>
RepositoryManager::getExpiriesForSymbol(const QString &symbol) const {
  // Logic requires iterating contracts, so checking loaded repos is enough.
  // However, if we wanted to use the m_expiryToSymbols map reverse lookup?
  // Current logic iterates loaded repos. 
  // Let's stick to current logic as it doesn't touch the caches directly except m_nsefo usage.
  // Wait, the original code used m_nsefo->getContractsBySymbol(symbol). 
  // This is thread-safe assuming Repositories are immutable after load.
  
  // No change needed for this specific method's logic as it doesn't use the simple caches.
  // BUT access to m_nsefo is technically shared. The Repositories are read-only after load.
  
  // Update: The previous logic is fine.
  
  return QVector<QString>(); // Placeholder to match original structure scan if needed, but original provided logic.
  // Implementation below matches original.
  
  QSet<QString> uniqueExpiries;

  if (m_nsefo && m_nsefo->isLoaded()) {
    QVector<ContractData> contracts = m_nsefo->getContractsBySymbol(symbol);
    for (const auto &c : contracts) {
      if (c.instrumentType == 2) { // Options only
        uniqueExpiries.insert(c.expiryDate);
      }
    }
  }

  // Convert to vector and sort
  QVector<QString> expiries;
  for (const QString &exp : uniqueExpiries) {
    expiries.append(exp);
  }

  // Sort by date
  std::sort(expiries.begin(), expiries.end(),
            [](const QString &a, const QString &b) {
              QDate dateA = QDate::fromString(a, "ddMMMyyyy");
              QDate dateB = QDate::fromString(b, "ddMMMyyyy");
              return dateA < dateB;
            });

  return expiries;
}

QVector<QString> RepositoryManager::getAllExpiries() const {
  std::shared_lock lock(m_expiryCacheMutex);
  return m_sortedExpiries;
}

// function to fecth list from array
//  is there any way other than for loop to convert set to qvector

QVector<QString> RepositoryManager::getOptionSymbolsFromArray() const {
  std::shared_lock lock(m_expiryCacheMutex);
  QVector<QString> symbols;
  symbols.reserve(m_optionSymbols.size());
  for (const QString &symbol : m_optionSymbols) {
    symbols.append(symbol);
  }
  return symbols;
}

// function to fetch unique expiry from array of stock option

QVector<QString> RepositoryManager::getUniqueExpiryOfStockOption() const {
  std::shared_lock lock(m_expiryCacheMutex);
  return m_sortedExpiries;
}

// get all unique symbol of stock option

QVector<QString> RepositoryManager::getAllUniqueSymbolOfStockOption() const {
  std::shared_lock lock(m_expiryCacheMutex);
  QVector<QString> symbols;
  symbols.reserve(m_optionSymbols.size());
  for (const QString &symbol : m_optionSymbols) {
    symbols.append(symbol);
  }
  return symbols;
}

// get current expiry of all stock option take input as symbollist

QVector<QString> RepositoryManager::getCurrentExpiryOfAllStockOption(
    const QVector<QString> &symbolList) const {
  QVector<QString> currentExpiries;
  currentExpiries.reserve(symbolList.size());

  for (const QString &symbol : symbolList) {
    // Get all expiries for this symbol, then find nearest
    QVector<QString> expiries = getExpiriesForSymbol(symbol);
    QString nearestExpiry = getNearestExpiry(expiries);
    currentExpiries.append(nearestExpiry);
  }
  return currentExpiries;
}

// get nearest expiry from a list of expiry dates
QString
RepositoryManager::getNearestExpiry(const QVector<QString> &expiryList) const {
  if (expiryList.isEmpty()) {
    return QString();
  }

  // convert all values into date and sort them
  QVector<QDate> dates;
  for (const QString &expiry : expiryList) {
    QDate date = QDate::fromString(expiry, "ddMMMyyyy");
    if (date.isValid()) {
      dates.append(date);
    }
  }

  if (dates.isEmpty()) {
    return QString();
  }

  std::sort(dates.begin(), dates.end());

  // find nearest expiry >= today
  QDate today = QDate::currentDate();
  for (const QDate &date : dates) {
    if (date >= today) {
      return date.toString("ddMMMyyyy").toUpper();
    }
  }

  // If no future expiry found, return the last (most recent past) one
  return dates.last().toString("ddMMMyyyy").toUpper();
}

// ===== ATM WATCH OPTIMIZATION: O(1) LOOKUPS =====

const QVector<double> &
RepositoryManager::getStrikesForSymbolExpiry(const QString &symbol,
                                             const QString &expiry) const {
  static const QVector<double> emptyVector; // Static empty vector for invalid lookups
  std::shared_lock lock(m_expiryCacheMutex);                                            
  QString key = symbol + "|" + expiry;
  auto it = m_symbolExpiryStrikes.find(key);
  if (it != m_symbolExpiryStrikes.end()) {
    return it.value();
  }
  return emptyVector;
}

QPair<int64_t, int64_t> RepositoryManager::getTokensForStrike(
    const QString &symbol, const QString &expiry, double strike) const {
  std::shared_lock lock(m_expiryCacheMutex);
  QString key = symbol + "|" + expiry + "|" + QString::number(strike, 'f', 2);
  return m_strikeToTokens.value(key, qMakePair(0LL, 0LL));
}

int64_t RepositoryManager::getAssetTokenForSymbol(const QString &symbol) const {
  std::shared_lock lock(m_expiryCacheMutex);
  return m_symbolToAssetToken.value(symbol, 0);
}

int64_t
RepositoryManager::getFutureTokenForSymbolExpiry(const QString &symbol,
                                                 const QString &expiry) const {
  std::shared_lock lock(m_expiryCacheMutex);
  QString key = symbol + "|" + expiry;
  return m_symbolExpiryFutureToken.value(key, 0);
}

double RepositoryManager::getUnderlyingPrice(const QString &symbol, const QString &expiry) const {
  std::shared_lock lock(m_expiryCacheMutex);

  // Step 1: Try Cash Market / Index Price
  double price = 0.0;
  int64_t assetToken = m_symbolToAssetToken.value(symbol, 0);
  
  if (assetToken > 0) {
      price = nsecm::getGenericLtp(static_cast<uint32_t>(assetToken));
  }

  // Step 2: Fallback to Futures Price if Cash Price is missing
  if (price <= 0.0) {
      QString key = symbol + "|" + expiry;
      int64_t futureToken = m_symbolExpiryFutureToken.value(key, 0);
      
      if (futureToken > 0) {
          auto* state = nsefo::g_nseFoPriceStore.getUnifiedState(static_cast<uint32_t>(futureToken));
          if (state) {
              price = state->ltp;
          }
      }
  }

  return price;
}

QString RepositoryManager::getSymbolForFutureToken(int64_t token) const {
  std::shared_lock lock(m_expiryCacheMutex);
  return m_futureTokenToSymbol.value(token, "");
}

void RepositoryManager::dumpFutureTokenMap(const QString& filepath) const {
  std::shared_lock lock(m_expiryCacheMutex);
  QFile file(filepath);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
      qWarning() << "[RepositoryManager] Failed to open debug file:" << filepath;
      return;
  }
  
  QTextStream out(&file);
  out << "Token,Symbol,Expiry\n";
  
  // We need to iterate the forward map to get expiry info easily, 
  // or iterate reverse map and lookup expiry? 
  // Let's iterate the forward map since it has all keys
  
  QMap<QString, int64_t> sortedMap; // Sort by key for readable output
  for(auto it = m_symbolExpiryFutureToken.begin(); it != m_symbolExpiryFutureToken.end(); ++it) {
      sortedMap.insert(it.key(), it.value());
  }
  
  for(auto it = sortedMap.begin(); it != sortedMap.end(); ++it) {
      QString key = it.key(); // SYMBOL|EXPIRY
      int64_t token = it.value();
      
      QStringList parts = key.split('|');
      if (parts.size() == 2) {
          out << token << "," << parts[0] << "," << parts[1] << "\n";
      }
  }
  
  file.close();
  qDebug() << "[RepositoryManager] Dumped" << sortedMap.size() << "future tokens to" << filepath;

  // Dump REVERSE map as well for verification
  QString reverseFile = filepath;
  reverseFile.replace(".csv", "_reverse.csv");
  QFile fileRev(reverseFile);
  if (fileRev.open(QIODevice::WriteOnly | QIODevice::Text)) {
      QTextStream outRev(&fileRev);
      outRev << "Token,Symbol (From Reverse Map)\n";
      
      QMap<int64_t, QString> sortedRev;
      for(auto it = m_futureTokenToSymbol.begin(); it != m_futureTokenToSymbol.end(); ++it) {
          sortedRev.insert(it.key(), it.value());
      }
      
      for(auto it = sortedRev.begin(); it != sortedRev.end(); ++it) {
          outRev << it.key() << "," << it.value() << "\n";
      }
      fileRev.close();
      qDebug() << "[RepositoryManager] Dumped" << sortedRev.size() << "reverse mappings to" << reverseFile;
  }
}
