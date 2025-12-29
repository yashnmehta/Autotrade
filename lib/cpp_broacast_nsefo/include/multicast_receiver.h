#ifndef MULTICAST_RECEIVER_H
#define MULTICAST_RECEIVER_H

#include <string>
#include "socket_platform.h"
#include <atomic>
#include "udp_receiver.h"

#define BUFFER_SIZE 2048    

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
    
    char buffer[BUFFER_SIZE];
    
    // Statistics tracking
    UDPStats stats;
    uint32_t last_seq_no;
};

#endif // MULTICAST_RECEIVER_H