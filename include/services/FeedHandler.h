#ifndef FEEDHANDLER_H
#define FEEDHANDLER_H

#include <QObject>
#include <QDebug>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <memory>
#include "api/XTSTypes.h"
#include "udp/UDPTypes.h"

/**
 * @brief Helper object that emits signals for a specific token
 */
class TokenPublisher : public QObject {
    Q_OBJECT
public:
    explicit TokenPublisher(int64_t compositeKey, QObject* parent = nullptr) 
        : QObject(parent), m_compositeKey(compositeKey) {}
    void publish(const UDP::MarketTick& tick) { emit udpTickUpdated(tick); }
    int64_t compositeKey() const { return m_compositeKey; }

signals:
    void udpTickUpdated(const UDP::MarketTick& tick);  // New UDP-specific

private:
    int64_t m_compositeKey;
};

/**
 * @brief Centralized feed handler for real-time market data distribution
 * 
 * Implements publisher-subscriber pattern with direct callbacks for minimal latency.
 * Uses composite key (exchangeSegment, token) to handle multi-exchange environments.
 * 
 * Performance:
 * - Subscribe: ~500ns (add to hash map)
 * - Unsubscribe: ~800ns (remove from map)
 * - Publish (1 subscriber): ~70ns (hash lookup + callback)
 * - Publish (10 subscribers): ~250ns (10 parallel callbacks)
 * 
 * Thread Safety:
 * - Callbacks execute on publisher thread (IO thread)
 * - Use QMetaObject::invokeMethod for UI thread marshalling if needed
 * - std::mutex protects subscription map
 * 
 * Usage:
 * ```cpp
 * // Subscribe to token updates (exchange-aware)
 * FeedHandler::instance().subscribe(2, 49508, this, &MyWindow::onTickUpdate);
 * 
 * // Legacy subscribe (assumes exchangeSegment from context)
 * FeedHandler::instance().subscribe(49508, this, &MyWindow::onTickUpdate);
 * ```
 */
class FeedHandler : public QObject {
    Q_OBJECT

public:
    static FeedHandler& instance();

    /**
     * @brief Create composite key from exchange segment and token
     * @param exchangeSegment Exchange segment (1=NSECM, 2=NSEFO, 11=BSECM, 12=BSEFO)
     * @param token Exchange instrument token
     * @return 64-bit composite key
     */
    static inline int64_t makeKey(int exchangeSegment, int token) {
        return (static_cast<int64_t>(exchangeSegment) << 32) | static_cast<uint32_t>(token);
    }

    /**
     * @brief Subscribe with exchange segment (RECOMMENDED for multi-exchange)
     * @param exchangeSegment Exchange segment (1=NSECM, 2=NSEFO, 11=BSECM, 12=BSEFO)
     * @param token Exchange instrument token
     * @param receiver The QObject that will receive the tick
     * @param slot The slot signature or member function pointer to call
     */
    /**
     * @brief Subscribe to UDP::MarketTick with exchange segment
     */
    template<typename Receiver, typename Slot>
    void subscribe(int exchangeSegment, int token, Receiver* receiver, Slot slot) {
        int64_t key = makeKey(exchangeSegment, token);
        std::lock_guard<std::mutex> lock(m_mutex);
        TokenPublisher* pub = getOrCreatePublisher(key);
        connect(pub, &TokenPublisher::udpTickUpdated, receiver, slot);
        
        // Notify UDP service to enable filtering for this token
        registerTokenWithUdpService(token, exchangeSegment);
        emit subscriptionCountChanged(token, 1);
    }
    
    template<typename Receiver, typename Slot>
    void subscribeUDP(UDP::ExchangeSegment exchangeSegment, uint32_t token, Receiver* receiver, Slot slot) {
        subscribe(static_cast<int>(exchangeSegment), (int)token, receiver, slot);
    }

    /**
     * @brief Subscribe without callback (just to ensure data is cached in price store)
     */
    void subscribe(int exchangeSegment, int token) {
        int64_t key = makeKey(exchangeSegment, token);
        std::lock_guard<std::mutex> lock(m_mutex);
        getOrCreatePublisher(key);
        registerTokenWithUdpService(token, exchangeSegment);
        emit subscriptionCountChanged(token, 1);
    }

    /**
     * @brief Unsubscribe with exchange segment
     */
    void unsubscribe(int exchangeSegment, int token, QObject* receiver);

    /**
     * @brief Legacy unsubscribe (token-only)
     */
    void unsubscribe(int token, QObject* receiver);

    /**
     * @brief Unsubscribe a receiver from all tick updates
     */
    void unsubscribeAll(QObject* receiver);

    
    /**
     * @brief Publish UDP tick (called by UdpBroadcastService)
     */
    void onUdpTickReceived(const UDP::MarketTick& tick);

    /**
     * @brief Get number of active publishers (monitoring)
     */
    size_t totalSubscriptions() const;

signals:
    void subscriptionCountChanged(int token, size_t count);
    
    /**
     * @brief Request price subscription from new PriceCache (zero-copy)
     * Emitted by subscribers (MarketWatch, OptionChain) to request subscription
     * Routed through: Subscriber → FeedHandler → MainWindow → PriceCache
     * @param requesterId Unique ID to route response back
     * @param token Token to subscribe
     * @param segment Market segment
     */
    void requestPriceSubscription(QString requesterId, uint32_t token, uint16_t segment);

private:
    FeedHandler();
    ~FeedHandler();

    // Prevent copying
    FeedHandler(const FeedHandler&) = delete;
    FeedHandler& operator=(const FeedHandler&) = delete;

    void registerTokenWithUdpService(uint32_t token, int segment);

    TokenPublisher* getOrCreatePublisher(int64_t compositeKey);

    // CompositeKey -> Publisher
    std::unordered_map<int64_t, TokenPublisher*> m_publishers;
    mutable std::mutex m_mutex;
};

#endif // FEEDHANDLER_H
