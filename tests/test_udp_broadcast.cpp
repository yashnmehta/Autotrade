#include <iostream>
#include <thread>
#include <chrono>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <QJsonObject>
#include <QJsonDocument>
#include "api/UDPBroadcastProvider.h"
#include "api/NSEProtocol.h"

// Helper to send UDP packet
void send_packet(int port, int32_t token, double price) {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) return;

    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    // Construct Trade Message
    Trade_Message trMsg;
    memset(&trMsg, 0, sizeof(trMsg));
    
    trMsg.stream_header.message_length = sizeof(Trade_Message);
    trMsg.stream_header.stream_id = 1;
    trMsg.stream_header.sequence_number = 100;
    
    trMsg.message_type = 'T';
    trMsg.timestamp = 1600000000;
    trMsg.token = token;
    trMsg.trade_price = (int32_t)(price * 100);
    trMsg.trade_quantity = 50;
    
    sendto(sockfd, &trMsg, sizeof(trMsg), 0, (const struct sockaddr *) &servaddr, sizeof(servaddr));
    close(sockfd);
    std::cout << "Sent packet to port " << port << " for token " << token << std::endl;
}

int main() {
    UDPBroadcastProvider provider;
    
    // Configuration
    QJsonObject config;
    QJsonObject udp;
    QJsonObject exchanges;
    QJsonObject nse;
    
    QJsonObject nseFo;
    nseFo["enabled"] = true;
    nseFo["multicastGroup"] = "233.1.2.5"; 
    nseFo["port"] = 34330;
    nseFo["protocol"] = "binary";
    
    QJsonObject nseCm;
    nseCm["enabled"] = true;
    nseCm["multicastGroup"] = "233.1.2.5"; 
    nseCm["port"] = 8270;
    nseCm["protocol"] = "binary";
    
    exchanges["NSEFO"] = nseFo;
    exchanges["NSECM"] = nseCm;
    udp["exchanges"] = exchanges;
    config["udp"] = udp;
    
    std::atomic<int> received_count{0};
    
    provider.registerCallback([&](const Quote& quote) {
        std::cout << "Received tick! Token: " << quote.token << " LTP: " << quote.ltp << " (Source Segment: " << quote.exchangeSegment << ")" << std::endl;
        received_count++;
    });
    
    provider.connect(config, [](bool success) {
        std::cout << "Connect result: " << success << std::endl;
    });
    
    // Subscribe to tokens (one for FO, one for CM)
    // Assuming token 1 is CM, token 2 is FO (tokens are usually unique across segments but let's assume mapping logic)
    // Actually provider just filters by token. Segment info is derived from socket.
    // CM socket -> Segment 1. FO socket -> Segment 2.
    
    provider.subscribe({1001}, 1, nullptr); // CM
    provider.subscribe({2001}, 2, nullptr); // FO
    
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    // Send to CM port (8270)
    std::cout << "Sending to CM port 8270..." << std::endl;
    send_packet(8270, 1001, 1500.0);
    
    // Send to FO port (34330)
    std::cout << "Sending to FO port 34330..." << std::endl;
    send_packet(34330, 2001, 25000.0);
    
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    if (received_count >= 2) {
        std::cout << "SUCCESS: Received " << received_count << " packets." << std::endl;
        return 0;
    } else {
        std::cout << "FAILURE: Received " << received_count << " packets (Expected >= 2)." << std::endl;
        return 1;
    }
}
