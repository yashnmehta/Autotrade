#ifndef SYMBOLCACHEMANAGER_H
#define SYMBOLCACHEMANAGER_H

#include <QObject>
#include <QVector>
#include <QHash>
#include <QSet>
#include <QString>
#include <QMutex>

// Forward declaration - matches ScripBar's InstrumentData structure
struct InstrumentData;

/**
 * @brief SymbolCacheManager - Singleton class to cache 9000+ NSECM symbols once
 * 
 * Purpose: Eliminates redundant symbol loading across multiple ScripBar instances.
 * Instead of each ScripBar loading 9000 symbols independently (~800ms each),
 * this manager loads symbols once during startup and provides instant access.
 * 
 * Performance Impact:
 * - Before: 4 ScripBars × 800ms = 3200ms wasted CPU + 36,000 redundant entries
 * - After: 1 load × 800ms = 800ms + 9,000 entries shared across all ScripBars
 * - Savings: 75% CPU reduction, 75% memory reduction
 * 
 * Usage:
 * 1. Initialize during startup: SymbolCacheManager::instance().initialize()
 * 2. ScripBar requests symbols: getSymbols("NSE_CM", "EQUITY")
 * 3. Returns reference to pre-built cache (instant access)
 */
class SymbolCacheManager : public QObject
{
    Q_OBJECT

public:
    // Singleton access
    static SymbolCacheManager& instance();
    
    // Initialize and load symbol caches during startup
    void initialize();
    
    // Check if cache is ready for use
    bool isCacheReady() const;
    
    // Get cached symbols for a specific exchange segment and instrument type
    // Returns empty vector if cache not ready or key not found
    const QVector<InstrumentData>& getSymbols(const QString& exchange, 
                                               const QString& segment, 
                                               const QString& series);
    
    // Clear all caches (useful for refresh/reload scenarios)
    void clearCache();
    
    // Get total number of cached entries across all caches
    int getTotalCacheSize() const;

signals:
    void cacheReady();  // Emitted when cache loading completes

private:
    SymbolCacheManager();
    ~SymbolCacheManager();
    
    // Prevent copying
    SymbolCacheManager(const SymbolCacheManager&) = delete;
    SymbolCacheManager& operator=(const SymbolCacheManager&) = delete;
    
    // Build symbol cache for specific segment
    void buildSymbolCache(const QString& exchange, const QString& segment, const QString& series);
    
    // Generate cache key from exchange + segment + series
    QString getCacheKey(const QString& exchange, const QString& segment, const QString& series) const;
    
    // Cache storage: Key = "NSE_CM_EQUITY", Value = Vector of InstrumentData
    QHash<QString, QVector<InstrumentData>> m_symbolCache;
    
    // Track which caches have been loaded
    QSet<QString> m_loadedKeys;
    
    // Thread safety for cache access
    mutable QMutex m_mutex;
    
    // Cache ready flag
    bool m_cacheReady;
    
    // Empty vector for invalid requests
    QVector<InstrumentData> m_emptyCache;
};

#endif // SYMBOLCACHEMANAGER_H
