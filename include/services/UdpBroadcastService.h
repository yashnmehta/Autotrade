#ifndef UDPBROADCASTSERVICE_H
#define UDPBROADCASTSERVICE_H

#include <QObject>
#include <memory>
#include <atomic>
#include <string>
#include "multicast_receiver.h"
#include "api/XTSTypes.h"
#include "utils/LockFreeQueue.h"

class UdpBroadcastService : public QObject {
    Q_OBJECT
public:
    static UdpBroadcastService& instance();

    void start(const std::string& ip = "233.1.2.5", int port = 34330);
    void stop();
    bool isActive() const;

    struct Stats {
        uint64_t msg7200Count;
        uint64_t msg7201Count;
        uint64_t msg7202Count;
        uint64_t depthCount;
        nsefo::UDPStats udpStats;
    };
    Stats getStats() const;

signals:
    void tickReceived(const XTS::Tick& tick);
    void statusChanged(bool active);

private:
    UdpBroadcastService(QObject* parent = nullptr);
    ~UdpBroadcastService();

    std::unique_ptr<nsefo::MulticastReceiver> m_receiver;
    std::atomic<bool> m_active{false};
    
    std::atomic<uint64_t> m_msg7200Count{0};
    std::atomic<uint64_t> m_msg7201Count{0};
    std::atomic<uint64_t> m_msg7202Count{0};
    std::atomic<uint64_t> m_depthCount{0};
};

#endif // UDPBROADCASTSERVICE_H
