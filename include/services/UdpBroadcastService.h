#ifndef UDPBROADCASTSERVICE_H
#define UDPBROADCASTSERVICE_H

#include <QObject>
#include <memory>
#include <atomic>
#include <string>
#include <vector>
#include <thread>

// Exchange-specific headers
#include "multicast_receiver.h"        // NSE FO
#include "nsecm_multicast_receiver.h"  // NSE CM
#include "bse_receiver.h"             // BSE FO/CM

#include "api/XTSTypes.h"

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
    void tickReceived(const XTS::Tick& tick);
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
};

#endif // UDPBROADCASTSERVICE_H
