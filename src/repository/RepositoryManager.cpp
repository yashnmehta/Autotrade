#include "repository/RepositoryManager.h"
#include "repository/MasterFileParser.h"
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QTextStream>
#include <QDebug>

// Initialize singleton
RepositoryManager* RepositoryManager::s_instance = nullptr;

RepositoryManager* RepositoryManager::getInstance() {
    if (!s_instance) {
        s_instance = new RepositoryManager();
    }
    return s_instance;
}

RepositoryManager::RepositoryManager()
    : m_loaded(false)
{
    m_nsefo = std::make_unique<NSEFORepository>();
    m_nsecm = std::make_unique<NSECMRepository>();
}

RepositoryManager::~RepositoryManager() = default;

bool RepositoryManager::loadAll(const QString& mastersPath) {
    qDebug() << "[RepositoryManager] Loading all master contracts from:" << mastersPath;
    
    bool anyLoaded = false;
    
    // FAST PATH: Check for processed CSV files first (10x faster loading)
    QString nsefoCSV = mastersPath + "/processed_csv/nsefo_processed.csv";
    QString nsecmCSV = mastersPath + "/processed_csv/nsecm_processed.csv";
    
    if (QFile::exists(nsefoCSV) && QFile::exists(nsecmCSV)) {
        qDebug() << "[RepositoryManager] Found processed CSV files, loading from cache...";
        
        bool nsefoLoaded = false;
        bool nsecmLoaded = false;
        
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
        
        if (nsefoLoaded && nsecmLoaded) {
            m_loaded = true;
            
            // Log summary
            SegmentStats stats = getSegmentStats();
            qDebug() << "[RepositoryManager] CSV loading complete (FAST PATH):";
            qDebug() << "  NSE F&O:" << stats.nsefo << "contracts";
            qDebug() << "  NSE CM:" << stats.nsecm << "contracts";
            qDebug() << "  Total:" << getTotalContractCount() << "contracts";
            
            return true;
        }
        // If CSV loading partially failed, fall through to master file loading
        qDebug() << "[RepositoryManager] CSV loading incomplete, falling back to master file...";
    }
    
    // SLOW PATH: Check if there's a combined master file (from XTS download)
    QString combinedFile = mastersPath + "/master_contracts_latest.txt";
    if (QFile::exists(combinedFile)) {
        qDebug() << "[RepositoryManager] Found combined master file, parsing segments...";
        if (loadCombinedMasterFile(combinedFile)) {
            anyLoaded = true;
            m_loaded = true;
            
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
    qDebug() << "[RepositoryManager] No combined file, trying individual segment files...";
    
    // Load NSE F&O (try CSV first, fall back to master file)
    if (loadNSEFO(mastersPath, true)) {
        anyLoaded = true;
    }
    
    // Load NSE CM (try CSV first, fall back to master file)
    if (loadNSECM(mastersPath, true)) {
        anyLoaded = true;
    }
    
    // TODO: Load BSE F&O and BSE CM
    
    if (anyLoaded) {
        m_loaded = true;
        
        // Log summary
        SegmentStats stats = getSegmentStats();
        qDebug() << "[RepositoryManager] Loading complete:";
        qDebug() << "  NSE F&O:" << stats.nsefo << "contracts";
        qDebug() << "  NSE CM:" << stats.nsecm << "contracts";
        qDebug() << "  Total:" << getTotalContractCount() << "contracts";
    } else {
        qWarning() << "[RepositoryManager] Failed to load any master contracts";
    }
    
    return anyLoaded;
}

bool RepositoryManager::loadNSEFO(const QString& mastersPath, bool preferCSV) {
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
        qDebug() << "[RepositoryManager] Loading NSE F&O from master file:" << masterFile;
        return m_nsefo->loadMasterFile(masterFile);
    }
    
    qWarning() << "[RepositoryManager] NSE F&O master file not found:" << masterFile;
    return false;
}

bool RepositoryManager::loadNSECM(const QString& mastersPath, bool preferCSV) {
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
        qDebug() << "[RepositoryManager] Loading NSE CM from master file:" << masterFile;
        return m_nsecm->loadMasterFile(masterFile);
    }
    
    qWarning() << "[RepositoryManager] NSE CM master file not found:" << masterFile;
    return false;
}

bool RepositoryManager::loadCombinedMasterFile(const QString& filePath) {
    qDebug() << "[RepositoryManager] Loading combined master file:" << filePath;
    
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "[RepositoryManager] Failed to open combined master file";
        return false;
    }
    
    // Parse file and split by segment
    QTextStream in(&file);
    
    QVector<MasterContract> nsefoContracts;
    QVector<MasterContract> nsecmContracts;
    QVector<MasterContract> bsefoContracts;
    QVector<MasterContract> bsecmContracts;
    
    int lineCount = 0;
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        lineCount++;
        
        if (line.isEmpty()) {
            continue;
        }
        
        // Check segment prefix (first field before |)
        QString segment = line.section('|', 0, 0);
        
        MasterContract contract;
        bool parsed = false;
        
        if (segment == "NSEFO") {
            parsed = MasterFileParser::parseLine(line, "NSEFO", contract);
            if (parsed) nsefoContracts.append(contract);
        }
        else if (segment == "NSECM") {
            parsed = MasterFileParser::parseLine(line, "NSECM", contract);
            if (parsed) nsecmContracts.append(contract);
        }
        else if (segment == "BSEFO") {
            parsed = MasterFileParser::parseLine(line, "BSEFO", contract);
            if (parsed) bsefoContracts.append(contract);
        }
        else if (segment == "BSECM") {
            parsed = MasterFileParser::parseLine(line, "BSECM", contract);
            if (parsed) bsecmContracts.append(contract);
        }
    }
    
    file.close();
    
    qDebug() << "[RepositoryManager] Parsed" << lineCount << "lines:";
    qDebug() << "  NSE FO:" << nsefoContracts.size();
    qDebug() << "  NSE CM:" << nsecmContracts.size();
    qDebug() << "  BSE FO:" << bsefoContracts.size();
    qDebug() << "  BSE CM:" << bsecmContracts.size();
    
    // Load into repositories (implement direct loading from vector)
    // For now, we'll create temporary files - TODO: add loadFromVector() methods
    bool anyLoaded = false;
    
    // Write temporary segment files and load them
    QString tempDir = QFileInfo(filePath).absolutePath();
    
    if (!nsefoContracts.isEmpty()) {
        QString tempFile = tempDir + "/temp_nsefo.txt";
        QFile f(tempFile);
        if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&f);
            // Re-read and write NSEFO lines
            file.open(QIODevice::ReadOnly | QIODevice::Text);
            QTextStream in2(&file);
            while (!in2.atEnd()) {
                QString line = in2.readLine();
                if (line.startsWith("NSEFO|")) {
                    out << line << "\n";
                }
            }
            file.close();
            f.close();
            
            if (m_nsefo->loadMasterFile(tempFile)) {
                anyLoaded = true;
                qDebug() << "[RepositoryManager] NSE FO loaded successfully";
            }
            QFile::remove(tempFile);
        }
    }
    
    if (!nsecmContracts.isEmpty()) {
        QString tempFile = tempDir + "/temp_nsecm.txt";
        QFile f(tempFile);
        if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&f);
            // Re-read and write NSECM lines
            file.open(QIODevice::ReadOnly | QIODevice::Text);
            QTextStream in2(&file);
            while (!in2.atEnd()) {
                QString line = in2.readLine();
                if (line.startsWith("NSECM|")) {
                    out << line << "\n";
                }
            }
            file.close();
            f.close();
            
            if (m_nsecm->loadMasterFile(tempFile)) {
                anyLoaded = true;
                qDebug() << "[RepositoryManager] NSE CM loaded successfully";
            }
            QFile::remove(tempFile);
        }
    }
    
    return anyLoaded;
}

QVector<ContractData> RepositoryManager::searchScrips(
    const QString& exchange,
    const QString& segment,
    const QString& series,
    const QString& searchText,
    int maxResults
) const {
    QVector<ContractData> results;
    results.reserve(maxResults);
    
    QString segmentKey = getSegmentKey(exchange, segment);
    QString searchUpper = searchText.toUpper();
    
    if (segmentKey == "NSEFO" && m_nsefo->isLoaded()) {
        // Search NSE F&O by series
        QVector<ContractData> allContracts = m_nsefo->getContractsBySeries(series);
        
        for (const ContractData& contract : allContracts) {
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
    else if (segmentKey == "NSECM" && m_nsecm->isLoaded()) {
        // Search NSE CM by series
        QVector<ContractData> allContracts = m_nsecm->getContractsBySeries(series);
        
        for (const ContractData& contract : allContracts) {
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
    // TODO: Add BSE support
    
    qDebug() << "[RepositoryManager] Search results:" << results.size()
             << "for" << exchange << segment << series << searchText;
    
    return results;
}

QVector<ContractData> RepositoryManager::getScrips(
    const QString& exchange,
    const QString& segment,
    const QString& series
) const {
    QString segmentKey = getSegmentKey(exchange, segment);
    
    if (segmentKey == "NSEFO" && m_nsefo->isLoaded()) {
        return m_nsefo->getContractsBySeries(series);
    }
    else if (segmentKey == "NSECM" && m_nsecm->isLoaded()) {
        return m_nsecm->getContractsBySeries(series);
    }
    // TODO: Add BSE support
    
    return QVector<ContractData>();
}

const ContractData* RepositoryManager::getContractByToken(
    const QString& exchange,
    const QString& segment,
    int64_t token
) const {
    QString segmentKey = getSegmentKey(exchange, segment);
    
    if (segmentKey == "NSEFO" && m_nsefo->isLoaded()) {
        return m_nsefo->getContract(token);
    }
    else if (segmentKey == "NSECM" && m_nsecm->isLoaded()) {
        return m_nsecm->getContract(token);
    }
    // TODO: Add BSE support
    
    return nullptr;
}

QVector<ContractData> RepositoryManager::getOptionChain(
    const QString& exchange,
    const QString& symbol
) const {
    if (exchange == "NSE" && m_nsefo->isLoaded()) {
        return m_nsefo->getContractsBySymbol(symbol);
    }
    // TODO: Add BSE support
    
    return QVector<ContractData>();
}

void RepositoryManager::updateLiveData(
    const QString& exchange,
    const QString& segment,
    int64_t token,
    double ltp,
    int64_t volume
) {
    QString segmentKey = getSegmentKey(exchange, segment);
    
    if (segmentKey == "NSEFO" && m_nsefo->isLoaded()) {
        m_nsefo->updateLiveData(token, ltp, volume);
    }
    else if (segmentKey == "NSECM" && m_nsecm->isLoaded()) {
        m_nsecm->updateLiveData(token, ltp, volume);
    }
    // TODO: Add BSE support
}

void RepositoryManager::updateBidAsk(
    const QString& exchange,
    const QString& segment,
    int64_t token,
    double bidPrice,
    double askPrice
) {
    QString segmentKey = getSegmentKey(exchange, segment);
    
    if (segmentKey == "NSEFO" && m_nsefo->isLoaded()) {
        m_nsefo->updateBidAsk(token, bidPrice, askPrice);
    }
    else if (segmentKey == "NSECM" && m_nsecm->isLoaded()) {
        m_nsecm->updateBidAsk(token, bidPrice, askPrice);
    }
    // TODO: Add BSE support
}

void RepositoryManager::updateGreeks(
    int64_t token,
    double iv,
    double delta,
    double gamma,
    double vega,
    double theta
) {
    // Greeks are only for F&O contracts
    // Try NSE F&O first (most common)
    if (m_nsefo->isLoaded() && m_nsefo->hasContract(token)) {
        m_nsefo->updateGreeks(token, iv, delta, gamma, vega, theta);
        return;
    }
    
    // TODO: Try BSE F&O
}

int RepositoryManager::getTotalContractCount() const {
    int total = 0;
    
    if (m_nsefo->isLoaded()) {
        total += m_nsefo->getTotalCount();
    }
    if (m_nsecm->isLoaded()) {
        total += m_nsecm->getTotalCount();
    }
    // TODO: Add BSE counts
    
    return total;
}

RepositoryManager::SegmentStats RepositoryManager::getSegmentStats() const {
    SegmentStats stats;
    stats.nsefo = m_nsefo->isLoaded() ? m_nsefo->getTotalCount() : 0;
    stats.nsecm = m_nsecm->isLoaded() ? m_nsecm->getTotalCount() : 0;
    stats.bsefo = 0;  // TODO
    stats.bsecm = 0;  // TODO
    return stats;
}

bool RepositoryManager::isLoaded() const {
    return m_loaded;
}

QString RepositoryManager::getSegmentKey(const QString& exchange, const QString& segment) {
    QString key = exchange.toUpper() + segment.toUpper();
    
    // Normalize segment names per reference image mapping:
    // NSE E=NSECM, NSE F/O=NSEFO
    // NSECDS F/O=NSECD
    // BSE E=BSECM, BSE F=BSEFO
    // MCX F/O=MCXFO
    
    if (key == "NSECM" || key == "NSEE") return "NSECM";
    if (key == "NSEFO" || key == "NSEF" || key == "NSEO") return "NSEFO";  // Both F and O map to NSEFO
    if (key == "NSECDSF" || key == "NSECDSO") return "NSECD";  // NSECDS F and O map to NSECD
    if (key == "BSECM" || key == "BSEE") return "BSECM";
    if (key == "BSEFO" || key == "BSEF") return "BSEFO";
    if (key == "MCXF" || key == "MCXO") return "MCXFO";  // Both F and O map to MCXFO
    
    return key;
}

int RepositoryManager::getExchangeSegmentID(const QString& exchange, const QString& segment) {
    QString segmentKey = getSegmentKey(exchange, segment);
    
    // XTS API exchange segment IDs
    // E = Equity/CM, F = F&O, O = Currency/CD
    if (segmentKey == "NSECM") return 1;  // NSE Cash Market (E)
    if (segmentKey == "NSEFO") return 2;  // NSE F&O (F)
    if (segmentKey == "BSECM") return 11; // BSE Cash Market (E)
    if (segmentKey == "BSEFO") return 12; // BSE F&O (F)
    
    return -1;
}

bool RepositoryManager::saveProcessedCSVs(const QString& mastersPath) {
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
    
    if (anySaved) {
        qDebug() << "[RepositoryManager] Processed CSVs saved to:" << csvDir;
    }
    
    return anySaved;
}
