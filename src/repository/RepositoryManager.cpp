#include "repository/RepositoryManager.h"
#include <QFile>
#include <QDir>
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
    
    // Normalize segment names
    if (key == "NSECM" || key == "NSEE") return "NSECM";
    if (key == "NSEFO" || key == "NSEF" || key == "NSEO") return "NSEFO";
    if (key == "BSECM" || key == "BSEE") return "BSECM";
    if (key == "BSEFO" || key == "BSEF" || key == "BSEO") return "BSEFO";
    
    return key;
}

int RepositoryManager::getExchangeSegmentID(const QString& exchange, const QString& segment) {
    QString segmentKey = getSegmentKey(exchange, segment);
    
    // XTS API exchange segment IDs
    if (segmentKey == "NSECM") return 1;
    if (segmentKey == "NSEFO") return 2;
    if (segmentKey == "BSECM") return 11;
    if (segmentKey == "BSEFO") return 12;
    
    return -1;
}
