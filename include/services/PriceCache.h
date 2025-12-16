#ifndef PRICECACHE_H
#define PRICECACHE_H

#include <QObject>
#include <QMap>
#include <QDateTime>
#include <QReadWriteLock>
#include <optional>
#include "api/XTSTypes.h"

/**
 * @brief Singleton class to cache instrument prices globally
 * 
 * Purpose: Eliminate "0.00 flash" when adding already-subscribed instruments
 * to new market watch windows. Provides thread-safe access to cached tick data.
 */
class PriceCache : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Get singleton instance
     * @return Pointer to the global PriceCache instance
     */
    static PriceCache* instance();

    /**
     * @brief Update cached price for a token
     * @param token Exchange instrument ID
     * @param tick Latest tick data
     */
    void updatePrice(int token, const XTS::Tick &tick);

    /**
     * @brief Get cached price for a token
     * @param token Exchange instrument ID
     * @return Cached tick if available, std::nullopt otherwise
     */
    std::optional<XTS::Tick> getPrice(int token) const;

    /**
     * @brief Check if price exists in cache
     * @param token Exchange instrument ID
     * @return true if price is cached, false otherwise
     */
    bool hasPrice(int token) const;

    /**
     * @brief Clear stale prices from cache
     * @param maxAgeSeconds Maximum age in seconds (default: 300 = 5 minutes)
     */
    void clearStale(int maxAgeSeconds = 300);

    /**
     * @brief Get all cached tokens
     * @return List of all tokens currently in cache
     */
    QList<int> getAllTokens() const;

    /**
     * @brief Get cache size
     * @return Number of cached prices
     */
    int size() const;

    /**
     * @brief Clear all cached prices
     */
    void clear();

signals:
    /**
     * @brief Emitted when a price is updated in cache
     * @param token Exchange instrument ID
     * @param tick Updated tick data
     */
    void priceUpdated(int token, const XTS::Tick &tick);

private:
    explicit PriceCache(QObject *parent = nullptr);
    ~PriceCache() override = default;

    // Prevent copying
    PriceCache(const PriceCache&) = delete;
    PriceCache& operator=(const PriceCache&) = delete;

    struct CachedPrice {
        XTS::Tick tick;
        QDateTime timestamp;
    };

    QMap<int, CachedPrice> m_cache;
    mutable QReadWriteLock m_lock;  // Thread-safe access
    
    static PriceCache* s_instance;
};

#endif // PRICECACHE_H
