#ifndef UDPBROADCASTSERVICE_H
#define UDPBROADCASTSERVICE_H

#include <QObject>
#include <memory>
#include <atomic>
#include <string>
#include <vector>

// Exchange-specific headers
#include "multicast_receiver.h"        // NSE FO
#include "nsecm_multicast_receiver.h"  // NSE CM
#include "bse_receiver.h"             // BSE FO

#include "api/XTSTypes.h"

class UdpBroadcastService : public QObject {
    Q_OBJECT
public:
    static UdpBroadcastService& instance();

    // Start configuration for multiple segments
    struct Config {
        std::string nseFoIp;
        int nseFoPort;
        std::string nseCmIp;
        int nseCmPort;
        std::string bseFoIp;
        int bseFoPort;
        std::string bseCmIp;
        int bseCmPort;
        
        bool enableNSEFO = true;
        bool enableNSECM = true;
        bool enableBSEFO = true;
        bool enableBSECM = true;
    };

    void start(const Config& config);
    void stop();
    bool isActive() const;

    struct Stats {
        uint64_t nseFoPackets;
        uint64_t nseCmPackets;
        uint64_t bseFoPackets;
        uint64_t bseCmPackets;
        uint64_t totalTicks;
    };
    Stats getStats() const;

signals:
    void tickReceived(const XTS::Tick& tick);
    void statusChanged(bool active);

private:
    UdpBroadcastService(QObject* parent = nullptr);
    ~UdpBroadcastService();

    // Receivers
    std::unique_ptr<nsefo::MulticastReceiver> m_nseFoReceiver;
    std::unique_ptr<nsecm::MulticastReceiver> m_nseCmReceiver;
    std::unique_ptr<bse::BSEReceiver> m_bseFoReceiver;
    std::unique_ptr<bse::BSEReceiver> m_bseCmReceiver;

    std::atomic<bool> m_active{false};
    std::atomic<uint64_t> m_totalTicks{0};
};

#endif // UDPBROADCASTSERVICE_H
