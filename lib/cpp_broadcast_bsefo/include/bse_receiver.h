#ifndef BSE_RECEIVER_H
#define BSE_RECEIVER_H

#include <string>
#include <atomic>
#include <vector>
#include <functional>
#include <thread>
#include "socket_platform.h"

#include "bse_protocol.h"

namespace bse {

struct ReceiverStats {
    uint64_t packetsReceived = 0;
    uint64_t packetsValid = 0;
    uint64_t packetsInvalid = 0;
    uint64_t packets2020 = 0; // MARKET_PICTURE
    uint64_t packets2021 = 0; // MARKET_PICTURE_COMPLEX
    uint64_t packets2015 = 0; // OPEN_INTEREST
    uint64_t packetsDecoded = 0;
    uint64_t bytesReceived = 0;
    uint64_t errors = 0;
};

class BSEReceiver {
public:
    BSEReceiver(const std::string& ip, int port, const std::string& segment);
    ~BSEReceiver();

    void start();
    void stop();
    
    // Callback for decoded records
    using RecordCallback = std::function<void(const DecodedRecord&)>;
    void setRecordCallback(RecordCallback callback) { recordCallback_ = callback; }
    
    // Callback for Open Interest updates
    using OpenInterestCallback = std::function<void(const DecodedOpenInterest&)>;
    void setOpenInterestCallback(OpenInterestCallback callback) { oiCallback_ = callback; }

    const ReceiverStats& getStats() const { return stats_; }

private:
    void receiveLoop();
    void processPacket(const uint8_t* buffer, size_t length);
    bool validatePacket(const uint8_t* buffer, size_t length);
    
    // Parsing helpers
    void decodeAndDispatch(const uint8_t* buffer, size_t length);
    void decodeOpenInterest(const uint8_t* buffer, size_t length);

    std::string ip_;
    int port_;
    std::string segment_;
    
    socket_t sockfd_;
    struct sockaddr_in addr_;
    std::atomic<bool> running_;
    std::thread receiverThread_;
    
    alignas(8) uint8_t buffer_[2048]; // Receive buffer
    
    ReceiverStats stats_;
    RecordCallback recordCallback_;
    OpenInterestCallback oiCallback_;
};

} // namespace bse

#endif // BSE_RECEIVER_H
