#ifndef MULTICAST_RECEIVER_H
#define MULTICAST_RECEIVER_H

#include <string>
#include <netinet/in.h>
#include <atomic>
#include "udp_receiver.h"

#define BUFFER_SIZE 65535

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
    
    char buffer[BUFFER_SIZE];
    
    // Statistics tracking
    UDPStats stats;
    uint32_t last_seq_no;
};

#endif // MULTICAST_RECEIVER_H