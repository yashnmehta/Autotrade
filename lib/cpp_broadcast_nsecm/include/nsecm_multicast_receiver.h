#ifndef MULTICAST_RECEIVER_H
#define MULTICAST_RECEIVER_H

#include <string>
#include <netinet/in.h>
#include <atomic>
#include "nsecm_udp_receiver.h"

constexpr size_t kBufferSize = 65535;

namespace nsecm {

class MulticastReceiver {
public:
    MulticastReceiver(const std::string& ip, int port);
    ~MulticastReceiver();

    void start();
    void stop();
    
    // Check if receiver is properly initialized
    bool isValid() const { return sockfd >= 0; }
    
    // Get statistics
    const UDPStats& getStats() const { return stats; }

private:
    int sockfd;
    struct sockaddr_in addr;
    std::atomic<bool> running;
    
    alignas(8) char buffer[kBufferSize];
    
    // Statistics tracking
    UDPStats stats;
    uint32_t lastSeqNo;
};

} // namespace nsecm

#endif // MULTICAST_RECEIVER_H