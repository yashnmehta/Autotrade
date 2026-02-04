#include "data/SymbolCacheManager.h"
#include "app/ScripBar.h"  // For InstrumentData struct
#include "repository/RepositoryManager.h"
#include <QDebug>
#include <QElapsedTimer>
#include <QMutexLocker>

// Singleton instance
SymbolCacheManager& SymbolCacheManager::instance()
{
    static SymbolCacheManager instance;
    return instance;
}

SymbolCacheManager::SymbolCacheManager()
    : QObject(nullptr)
    , m_cacheReady(false)
{
    qDebug() << "[SymbolCacheManager] Constructed (singleton)";
}

SymbolCacheManager::~SymbolCacheManager()
{
    clearCache();
}

void SymbolCacheManager::initialize()
{
    qDebug() << "[SymbolCacheManager] ========== INITIALIZATION START ==========";
    QElapsedTimer timer;
    timer.start();
    
    // Check if RepositoryManager is loaded
    RepositoryManager* repo = RepositoryManager::getInstance();
    if (!repo->isLoaded()) {
        qWarning() << "[SymbolCacheManager] Repository not loaded yet - cannot build cache";
        return;
    }
    
    QMutexLocker locker(&m_mutex);
    
    // Build caches for commonly used segments
    // Priority 1: NSE Cash Market (EQUITY) - 9000+ symbols, most frequently used
    qDebug() << "[SymbolCacheManager] Loading NSE_CM EQUITY symbols...";
    buildSymbolCache("NSE", "CM", "EQUITY");
    
    // Priority 2: NSE F&O (Futures & Options) - loaded on-demand in future if needed
    // buildSymbolCache("NSE", "FO", "FUTIDX");  // Index Futures
    // buildSymbolCache("NSE", "FO", "FUTSTK");  // Stock Futures
    // buildSymbolCache("NSE", "FO", "OPTIDX");  // Index Options
    // buildSymbolCache("NSE", "FO", "OPTSTK");  // Stock Options
    
    // Priority 3: BSE segments - loaded on-demand if needed
    // buildSymbolCache("BSE", "CM", "EQUITY");
    
    m_cacheReady = true;
    
    qint64 elapsed = timer.elapsed();
    qDebug() << "[SymbolCacheManager] ========== INITIALIZATION COMPLETE ==========";
    qDebug() << "[SymbolCacheManager] Total time:" << elapsed << "ms";
    qDebug() << "[SymbolCacheManager] Total cache entries:" << getTotalCacheSize();
    qDebug() << "[SymbolCacheManager] Cache keys loaded:" << m_loadedKeys.size();
    qDebug() << "[SymbolCacheManager] =========================================";
    
    emit cacheReady();
}

void SymbolCacheManager::buildSymbolCache(const QString& exchange, 
                                          const QString& segment, 
                                          const QString& series)
{
    QElapsedTimer timer;
    timer.start();
    
    QString key = getCacheKey(exchange, segment, series);
    
    qDebug() << "[SymbolCacheManager] Building cache for:" << key;
    
    // Get contracts from RepositoryManager
    RepositoryManager* repo = RepositoryManager::getInstance();
    QVector<ContractData> contracts = repo->getScrips(exchange, segment, series);
    
    if (contracts.isEmpty()) {
        qWarning() << "[SymbolCacheManager] No contracts found for" << key;
        return;
    }
    
    qDebug() << "[SymbolCacheManager] Found" << contracts.size() << "raw contracts for" << key;
    
    // Build InstrumentData cache (same logic as ScripBar::populateSymbols)
    QVector<InstrumentData> instrumentCache;
    instrumentCache.reserve(contracts.size());
    
    QSet<QString> uniqueSymbols;
    int exchangeSegmentId = RepositoryManager::getExchangeSegmentID(exchange, segment);
    
    for (const ContractData& contract : contracts) {
        // Filter out dummy/test symbols
        QString upperName = contract.name.toUpper();
        if (upperName.contains("DUMMY") || upperName.contains("TEST") || 
            upperName == "SCRIP" || upperName.startsWith("ZZZ")) {
            continue;
        }
        
        // Filter out spread contracts (instrumentType == 4)
        if (contract.instrumentType == 4) {
            continue;
        }
        
        if (!contract.name.isEmpty()) {
            uniqueSymbols.insert(contract.name);
        }
        
        // Build InstrumentData entry
        InstrumentData inst;
        inst.exchangeInstrumentID = contract.exchangeInstrumentID;
        inst.name = contract.name;
        inst.symbol = contract.name;
        inst.series = contract.series;
        inst.instrumentType = QString::number(contract.instrumentType);
        inst.expiryDate = contract.expiryDate;
        inst.strikePrice = contract.strikePrice;
        inst.optionType = contract.optionType;
        inst.exchangeSegment = exchangeSegmentId;
        inst.scripCode = contract.scripCode;
        instrumentCache.append(inst);
    }
    
    // Store in cache
    m_symbolCache[key] = instrumentCache;
    m_loadedKeys.insert(key);
    
    qint64 elapsed = timer.elapsed();
    qDebug() << "[SymbolCacheManager] Cache built for" << key 
             << "- Entries:" << instrumentCache.size() 
             << "- Unique symbols:" << uniqueSymbols.size()
             << "- Time:" << elapsed << "ms";
}

const QVector<InstrumentData>& SymbolCacheManager::getSymbols(const QString& exchange, 
                                                               const QString& segment, 
                                                               const QString& series)
{
    QMutexLocker locker(&m_mutex);
    
    QString key = getCacheKey(exchange, segment, series);
    
    // If cache exists, return it
    if (m_symbolCache.contains(key)) {
        qDebug() << "[SymbolCacheManager] Cache HIT for" << key 
                 << "- Returning" << m_symbolCache[key].size() << "entries";
        return m_symbolCache[key];
    }
    
    // Cache miss - try to build it on-demand
    qDebug() << "[SymbolCacheManager] Cache MISS for" << key << "- Building on-demand...";
    locker.unlock();  // Release lock during build
    buildSymbolCache(exchange, segment, series);
    locker.relock();
    
    // Return cache if now available
    if (m_symbolCache.contains(key)) {
        return m_symbolCache[key];
    }
    
    // Still not available - return empty
    qWarning() << "[SymbolCacheManager] Failed to build cache for" << key;
    return m_emptyCache;
}

bool SymbolCacheManager::isCacheReady() const
{
    QMutexLocker locker(&m_mutex);
    return m_cacheReady;
}

void SymbolCacheManager::clearCache()
{
    QMutexLocker locker(&m_mutex);
    qDebug() << "[SymbolCacheManager] Clearing cache...";
    m_symbolCache.clear();
    m_loadedKeys.clear();
    m_cacheReady = false;
}

QString SymbolCacheManager::getCacheKey(const QString& exchange, 
                                        const QString& segment, 
                                        const QString& series) const
{
    // Format: "NSE_CM_EQUITY"
    return QString("%1_%2_%3").arg(exchange).arg(segment).arg(series);
}

int SymbolCacheManager::getTotalCacheSize() const
{
    int total = 0;
    for (const auto& cache : m_symbolCache) {
        total += cache.size();
    }
    return total;
}
