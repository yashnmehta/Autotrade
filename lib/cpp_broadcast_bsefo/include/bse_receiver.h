#ifndef BSE_RECEIVER_H
#define BSE_RECEIVER_H

#include <string>
#include <atomic>
#include <vector>
#include <functional>
#include <thread>
#include "socket_platform.h"

#include "bse_protocol.h"
#include "bse_parser.h"

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
    
    // Callback setters - forward to parser
    void setRecordCallback(BSEParser::RecordCallback callback) { parser_.setRecordCallback(callback); }
    void setOpenInterestCallback(BSEParser::OpenInterestCallback callback) { parser_.setOpenInterestCallback(callback); }
    void setSessionStateCallback(BSEParser::SessionStateCallback callback) { parser_.setSessionStateCallback(callback); }
    void setClosePriceCallback(BSEParser::ClosePriceCallback callback) { parser_.setClosePriceCallback(callback); }

    void setIndexCallback(BSEParser::RecordCallback callback) { parser_.setIndexCallback(callback); }
    void setImpliedVolatilityCallback(BSEParser::ImpliedVolatilityCallback callback) { parser_.setImpliedVolatilityCallback(callback); }

    const ReceiverStats& getStats() const { return stats_; }
    const ParserStats& getParserStats() const { return parserStats_; }

private:
    void receiveLoop();
    bool validatePacket(const uint8_t* buffer, size_t length);
    
    std::string ip_;
    int port_;
    std::string segment_;
    
    socket_t sockfd_;
    struct sockaddr_in addr_;
    std::atomic<bool> running_;
    std::thread receiverThread_;
    
    alignas(8) uint8_t buffer_[2048]; // Receive buffer
    
    ReceiverStats stats_;
    ParserStats parserStats_;
    BSEParser parser_;
};

} // namespace bse

#endif // BSE_RECEIVER_H
