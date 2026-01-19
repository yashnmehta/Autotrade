#ifndef UDPBROADCASTSERVICE_H
#define UDPBROADCASTSERVICE_H

#include <QObject>
#include <memory>
#include <atomic>
#include <string>
#include <vector>
#include <thread>
#include <unordered_set>
#include <shared_mutex>

// Exchange-specific headers
#include "multicast_receiver.h"        // NSE FO
#include "nsecm_multicast_receiver.h"  // NSE CM
#include "bse_receiver.h"             // BSE FO/CM

#include "api/XTSTypes.h"
#include "udp/UDPTypes.h"

/**
 * @brief Exchange segment identifiers for individual receiver control
 */
enum class ExchangeReceiver {
    NSEFO = 0,
    NSECM = 1,
    BSEFO = 2,
    BSECM = 3
};

class UdpBroadcastService : public QObject {
    Q_OBJECT
public:
    static UdpBroadcastService& instance();

    // Configuration for multiple segments
    struct Config {
        std::string nseFoIp;
        int nseFoPort = 0;
        std::string nseCmIp;
        int nseCmPort = 0;
        std::string bseFoIp;
        int bseFoPort = 0;
        std::string bseCmIp;
        int bseCmPort = 0;
        
        bool enableNSEFO = true;
        bool enableNSECM = true;
        bool enableBSEFO = true;
        bool enableBSECM = true;
    };

    /**
     * @brief Start all enabled receivers based on config
     */
    void start(const Config& config);
    
    /**
     * @brief Stop all receivers
     */
    void stop();
    
    /**
     * @brief Check if any receiver is active
     */
    bool isActive() const;
    
    // ========== SUBSCRIPTION MANAGEMENT (Performance Optimization) ==========
    
    /**
     * @brief Subscribe to a specific token for signal emission
     * @param token The instrument token to subscribe to
     * @param exchangeSegment Exchange segment (NSEFO, NSECM, BSEFO, BSECM)
     * 
     * Only subscribed tokens will emit Qt signals. This reduces signal overhead
     * from 10,000+/sec (all ticks) to ~100/sec (subscribed only).
     * 
     * Performance: Unsubscribed ticks are filtered in < 50ns (hash lookup)
     */
    void subscribeToken(uint32_t token, int exchangeSegment = 0);
    
    /**
     * @brief Unsubscribe from a token
     */
    void unsubscribeToken(uint32_t token, int exchangeSegment = 0);
    
    /**
     * @brief Clear all subscriptions
     */
    void clearSubscriptions();
    
    /**
     * @brief Enable/disable subscription filtering
     * @param enabled If true, only subscribed tokens emit signals (recommended)
     *                If false, all ticks emit signals (legacy mode, high CPU)
     */
    void setSubscriptionFilterEnabled(bool enabled);

    // ========== INDIVIDUAL RECEIVER CONTROL ==========
    
    /**
     * @brief Start a specific receiver
     * @param receiver The receiver to start
     * @param ip Multicast IP address
     * @param port UDP port
     * @return true if started successfully
     */
    bool startReceiver(ExchangeReceiver receiver, const std::string& ip, int port);
    
    /**
     * @brief Stop a specific receiver
     * @param receiver The receiver to stop
     */
    void stopReceiver(ExchangeReceiver receiver);
    
    /**
     * @brief Check if a specific receiver is running
     * @param receiver The receiver to check
     * @return true if receiver is active
     */
    bool isReceiverActive(ExchangeReceiver receiver) const;
    
    /**
     * @brief Restart a specific receiver with new config
     */
    bool restartReceiver(ExchangeReceiver receiver, const std::string& ip, int port);

    // ========== STATISTICS ==========
    
    struct Stats {
        uint64_t nseFoPackets = 0;
        uint64_t nseCmPackets = 0;
        uint64_t bseFoPackets = 0;
        uint64_t bseCmPackets = 0;
        uint64_t totalTicks = 0;
        
        bool nseFoActive = false;
        bool nseCmActive = false;
        bool bseFoActive = false;
        bool bseCmActive = false;
    };
    Stats getStats() const;

signals:
    void udpTickReceived(const UDP::MarketTick& tick);  // New - UDP-specific
    void udpIndexReceived(const UDP::IndexTick& index);  // Index updates
    void udpSessionStateReceived(const UDP::SessionStateTick& state);  // Session state
    void udpCircuitLimitReceived(const UDP::CircuitLimitTick& limit);  // Circuit limits
    void udpImpliedVolatilityReceived(const UDP::ImpliedVolatilityTick& iv);  // Implied Volatility (BSE only)
    void statusChanged(bool active);
    void receiverStatusChanged(ExchangeReceiver receiver, bool active);

private:
    UdpBroadcastService(QObject* parent = nullptr);
    ~UdpBroadcastService();

    // Prevent copying
    UdpBroadcastService(const UdpBroadcastService&) = delete;
    UdpBroadcastService& operator=(const UdpBroadcastService&) = delete;

    // Setup callbacks for each receiver type
    void setupNseFoCallbacks();
    void setupNseCmCallbacks();
    void setupBseFoCallbacks();
    void setupBseCmCallbacks();

    // Receivers
    std::unique_ptr<nsefo::MulticastReceiver> m_nseFoReceiver;
    std::unique_ptr<nsecm::MulticastReceiver> m_nseCmReceiver;
    std::unique_ptr<bse::BSEReceiver> m_bseFoReceiver;
    std::unique_ptr<bse::BSEReceiver> m_bseCmReceiver;

    // Thread handles (for joinable threads instead of detached)
    std::thread m_nseFoThread;
    std::thread m_nseCmThread;
    std::thread m_bseFoThread;
    std::thread m_bseCmThread;

    // Status flags
    std::atomic<bool> m_active{false};
    std::atomic<bool> m_nseFoActive{false};
    std::atomic<bool> m_nseCmActive{false};
    std::atomic<bool> m_bseFoActive{false};
    std::atomic<bool> m_bseCmActive{false};
    
    std::atomic<uint64_t> m_totalTicks{0};
    
    // Store config for restart capability
    Config m_lastConfig;
    
    // Subscription filtering (performance optimization)
    std::unordered_set<uint32_t> m_subscribedTokens;
    mutable std::shared_mutex m_subscriptionMutex;
    std::atomic<bool> m_filteringEnabled{true};  // Enabled by default for performance
    
    // Fast lookup: should we emit signal for this token?
    inline bool shouldEmitSignal(uint32_t token) const {
        if (!m_filteringEnabled) return true;  // Legacy mode: emit all
        std::shared_lock lock(m_subscriptionMutex);
        return m_subscribedTokens.find(token) != m_subscribedTokens.end();
    }
};

#endif // UDPBROADCASTSERVICE_H
