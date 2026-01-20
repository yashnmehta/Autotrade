#include "repository/RepositoryManager.h"
#include "repository/MasterFileParser.h"
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
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
#include "utils/MemoryProfiler.h"

// Initialize singleton
RepositoryManager *RepositoryManager::s_instance = nullptr;

RepositoryManager *RepositoryManager::getInstance() {
  if (!s_instance) {
    s_instance = new RepositoryManager();
  }
  return s_instance;
}

RepositoryManager::RepositoryManager() : m_loaded(false) {
  m_nsefo = std::make_unique<NSEFORepository>();
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
  qDebug() << "[RepositoryManager] Loading all master contracts from:"
           << mastersPath;

  bool anyLoaded = false;

  // FAST PATH DISABLED: CSV loading bypasses string interning optimization
  // Force streaming path for memory efficiency
  QString nsefoCSV = mastersPath + "/processed_csv/nsefo_processed.csv";
  QString nsecmCSV = mastersPath + "/processed_csv/nsecm_processed.csv";
  QString bsefoCSV = mastersPath + "/processed_csv/bsefo_processed.csv";
  QString bsecmCSV = mastersPath + "/processed_csv/bsecm_processed.csv";

  // Check if at least NSE files exist (BSE is optional)
  if (false && QFile::exists(nsefoCSV) && QFile::exists(nsecmCSV)) {
    qDebug() << "[RepositoryManager] Found processed CSV files, loading from "
                "cache...";

    bool nsefoLoaded = false;
    bool nsecmLoaded = false;
    bool bsefoLoaded = false;
    bool bsecmLoaded = false;

    // Load NSE segments
    if (m_nsefo->loadProcessedCSV(nsefoCSV)) {
      nsefoLoaded = true;
      anyLoaded = true;
    } else {
      qWarning() << "[RepositoryManager] Failed to load NSE F&O CSV";
    }

    if (m_nsecm->loadProcessedCSV(nsecmCSV)) {
      nsecmLoaded = true;
      anyLoaded = true;
    } else {
      qWarning() << "[RepositoryManager] Failed to load NSE CM CSV";
    }

    // Load BSE segments (optional - don't fail if missing)
    if (QFile::exists(bsefoCSV)) {
      if (m_bsefo->loadProcessedCSV(bsefoCSV)) {
        bsefoLoaded = true;
        anyLoaded = true;
        qDebug() << "[RepositoryManager] BSE F&O CSV loaded successfully";
      } else {
        qWarning() << "[RepositoryManager] Failed to load BSE F&O CSV";
      }
    }

    if (QFile::exists(bsecmCSV)) {
      if (m_bsecm->loadProcessedCSV(bsecmCSV)) {
        bsecmLoaded = true;
        anyLoaded = true;
        qDebug() << "[RepositoryManager] BSE CM CSV loaded successfully";
      } else {
        qWarning() << "[RepositoryManager] Failed to load BSE CM CSV";
      }
    }

    if (nsefoLoaded && nsecmLoaded) {
      m_loaded = true;

      // Initialize distributed price stores
      initializeDistributedStores();

      // Log summary
      SegmentStats stats = getSegmentStats();
      qDebug() << "[RepositoryManager] CSV loading complete (FAST PATH):";
      qDebug() << "  NSE F&O:" << stats.nsefo << "contracts";
      qDebug() << "  NSE CM:" << stats.nsecm << "contracts";
      qDebug() << "  BSE F&O:" << stats.bsefo << "contracts";
      qDebug() << "  BSE CM:" << stats.bsecm << "contracts";
      qDebug() << "  Total:" << getTotalContractCount() << "contracts";

      MemoryProfiler::logSnapshot("Post-Repository Load (CSV)");

      return true;
    }
    // If CSV loading partially failed, fall through to master file loading
    qDebug() << "[RepositoryManager] CSV loading incomplete, falling back to "
                "master file...";
  }

  // SLOW PATH: Check if there's a combined master file (from XTS download)
  QString combinedFile = mastersPath + "/master_contracts_latest.txt";
  if (QFile::exists(combinedFile)) {
    qDebug() << "[RepositoryManager] Found combined master file, parsing "
                "segments...";
    if (loadCombinedMasterFile(combinedFile)) {
      anyLoaded = true;
      m_loaded = true;

      // Initialize distributed price stores
      initializeDistributedStores();

      // CSV saving disabled to preserve string interning benefits
      // saveProcessedCSVs(mastersPath);

      // Log summary
      SegmentStats stats = getSegmentStats();
      qDebug() << "[RepositoryManager] Loading complete:";
      qDebug() << "  NSE F&O:" << stats.nsefo << "contracts";
      qDebug() << "  NSE CM:" << stats.nsecm << "contracts";
      qDebug() << "  Total:" << getTotalContractCount() << "contracts";

      return true;
    }
  }

  // Fall back to individual segment files
  qDebug() << "[RepositoryManager] No combined file, trying individual segment "
              "files...";

  // Load NSE F&O (try CSV first, fall back to master file)
  if (loadNSEFO(mastersPath, true)) {
    anyLoaded = true;
  }

  // Load NSE CM (try CSV first, fall back to master file)
  if (loadNSECM(mastersPath, true)) {
    anyLoaded = true;
  }

  // Load BSE F&O (try CSV first, fall back to master file)
  if (loadBSEFO(mastersPath, true)) {
    anyLoaded = true;
  }

  // Load BSE CM (try CSV first, fall back to master file)
  if (loadBSECM(mastersPath, true)) {
    anyLoaded = true;
  }

  if (anyLoaded) {
    m_loaded = true;

    // Initialize distributed price stores
    initializeDistributedStores();

    // Log summary
    SegmentStats stats = getSegmentStats();
    qDebug() << "[RepositoryManager] Loading complete:";
    qDebug() << "  NSE CM:" << stats.nsecm << "contracts";
    qDebug() << "  BSE F&O:" << stats.bsefo << "contracts";
    qDebug() << "  BSE CM:" << stats.bsecm << "contracts";
    qDebug() << "  Total:" << getTotalContractCount() << "contracts";
  } else {
    qWarning() << "[RepositoryManager] Failed to load any master contracts";
  }

  return anyLoaded;
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
    if (pipeIdx == -1) continue;
    
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
    if (pipeIdx == -1) continue;
    
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
