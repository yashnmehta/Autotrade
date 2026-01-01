#ifndef NSECM_MULTICAST_RECEIVER_H
#define NSECM_MULTICAST_RECEIVER_H

#include <string>
#include "socket_platform.h"
#include <atomic>
#include "nsecm_udp_receiver.h"

namespace nsecm {

constexpr size_t kBufferSize = 65535;

class MulticastReceiver {
public:
    MulticastReceiver(const std::string& ip, int port);
    ~MulticastReceiver();

    void start();
    void stop();
    
    // Check if receiver is properly initialized
    bool isValid() const { return sockfd != socket_invalid; }
    
    // Get statistics
    const UDPStats& getStats() const { return stats; }

private:
    socket_t sockfd;
    struct sockaddr_in addr;
    std::atomic<bool> running;
    
    alignas(8) char buffer[kBufferSize];
    
    // Statistics tracking
    UDPStats stats;
    uint32_t lastSeqNo;
};

} // namespace nsecm

#endif // NSECM_MULTICAST_RECEIVER_H