#ifndef TOKENSUBSCRIPTIONMANAGER_H
#define TOKENSUBSCRIPTIONMANAGER_H

#include <QObject>
#include <QMap>
#include <QSet>
#include <QString>
#include <QList>

/**
 * @brief Singleton manager for tracking token subscriptions by exchange
 * 
 * Maintains exchange-wise lists of subscribed tokens for efficient API calls.
 * Used by Market Watch windows to request real-time price updates.
 * 
 * Thread-safe: Yes (Qt signals are thread-safe by default)
 * 
 * Usage:
 * @code
 * TokenSubscriptionManager *mgr = TokenSubscriptionManager::instance();
 * mgr->subscribe("NSE", 26000);  // Subscribe to NIFTY 50
 * 
 * // Later, when removing scrip
 * mgr->unsubscribe("NSE", 26000);
 * 
 * // Get all NSE subscriptions for API call
 * QSet<int> nseTokens = mgr->getSubscribedTokens("NSE");
 * @endcode
 */
class TokenSubscriptionManager : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Get the singleton instance
     * @return Pointer to the global TokenSubscriptionManager instance
     */
    static TokenSubscriptionManager* instance();
    
    /**
     * @brief Destroy the singleton instance (call on app exit)
     */
    static void destroy();
    
    // === Subscription Management ===
    
    /**
     * @brief Subscribe to a token for real-time updates
     * @param exchange Exchange name (e.g., "NSE", "BSE", "NFO")
     * @param token Unique token ID
     */
    void subscribe(const QString &exchange, int token);
    
    /**
     * @brief Unsubscribe from a token
     * @param exchange Exchange name
     * @param token Token ID to unsubscribe
     */
    void unsubscribe(const QString &exchange, int token);
    
    /**
     * @brief Unsubscribe all tokens for a specific exchange
     * @param exchange Exchange name
     */
    void unsubscribeAll(const QString &exchange);
    
    /**
     * @brief Clear all subscriptions for all exchanges
     */
    void clearAll();
    
    // === Batch Operations (More Efficient) ===
    
    /**
     * @brief Subscribe to multiple tokens at once
     * @param exchange Exchange name
     * @param tokens List of token IDs
     */
    void subscribeBatch(const QString &exchange, const QList<int> &tokens);
    
    /**
     * @brief Unsubscribe from multiple tokens at once
     * @param exchange Exchange name
     * @param tokens List of token IDs
     */
    void unsubscribeBatch(const QString &exchange, const QList<int> &tokens);
    
    // === Query Subscriptions ===
    
    /**
     * @brief Get all subscribed tokens for an exchange
     * @param exchange Exchange name
     * @return Set of subscribed token IDs
     */
    QSet<int> getSubscribedTokens(const QString &exchange) const;
    
    /**
     * @brief Get list of all exchanges with active subscriptions
     * @return List of exchange names
     */
    QList<QString> getSubscribedExchanges() const;
    
    /**
     * @brief Check if a token is currently subscribed
     * @param exchange Exchange name
     * @param token Token ID
     * @return true if subscribed
     */
    bool isSubscribed(const QString &exchange, int token) const;
    
    /**
     * @brief Get total number of subscriptions across all exchanges
     * @return Total subscription count
     */
    int totalSubscriptions() const;
    
    /**
     * @brief Get subscription count for a specific exchange
     * @param exchange Exchange name
     * @return Number of subscribed tokens
     */
    int getSubscriptionCount(const QString &exchange) const;
    
    // === Debug & Statistics ===
    
    /**
     * @brief Dump subscription state to qDebug
     */
    void dump() const;
    
    /**
     * @brief Get subscription statistics
     * @return Map of exchange -> subscription count
     */
    QMap<QString, int> getStatistics() const;

signals:
    /**
     * @brief Emitted when a token is subscribed
     * @param exchange Exchange name
     * @param token Token ID
     */
    void tokenSubscribed(const QString &exchange, int token);
    
    /**
     * @brief Emitted when a token is unsubscribed
     * @param exchange Exchange name
     * @param token Token ID
     */
    void tokenUnsubscribed(const QString &exchange, int token);
    
    /**
     * @brief Emitted when subscriptions change for an exchange
     * @param exchange Exchange name
     * @param count New subscription count
     */
    void exchangeSubscriptionsChanged(const QString &exchange, int count);
    
    /**
     * @brief Emitted when all subscriptions are cleared
     */
    void allSubscriptionsCleared();

private:
    explicit TokenSubscriptionManager(QObject *parent = nullptr);
    ~TokenSubscriptionManager() override = default;
    
    // Prevent copying
    TokenSubscriptionManager(const TokenSubscriptionManager&) = delete;
    TokenSubscriptionManager& operator=(const TokenSubscriptionManager&) = delete;
    
    static TokenSubscriptionManager *s_instance;
    
    // Exchange -> Set of subscribed token IDs
    QMap<QString, QSet<int>> m_subscriptions;
};

#endif // TOKENSUBSCRIPTIONMANAGER_H
