#ifndef BSE_PARSER_H
#define BSE_PARSER_H

#include "bse_protocol.h"
#include <functional>
#include <vector>

namespace bse {

// Forward declaration of stats struct (assuming it might be moved or we redefine a subset)
struct ParserStats {
    uint64_t packets2020 = 0; // MARKET_PICTURE
    uint64_t packets2021 = 0; // MARKET_PICTURE_COMPLEX
    uint64_t packets2015 = 0; // OPEN_INTEREST
    uint64_t packets2002 = 0; // SESSION_STATE
    uint64_t packets2012 = 0; // INDEX
    uint64_t packets2014 = 0; // CLOSE_PRICE
    uint64_t packets2028 = 0; // IMPLIED_VOLATILITY
    uint64_t packetsDecoded = 0;
    uint64_t packetsInvalid = 0;
};

class BSEParser {
public:
    BSEParser();
    ~BSEParser() = default;

    // Main entry point for parsing a packet buffer
    void parsePacket(const uint8_t* buffer, size_t length, ParserStats& stats);

    // Callbacks
    using RecordCallback = std::function<void(const DecodedRecord&)>;
    using OpenInterestCallback = std::function<void(const DecodedOpenInterest&)>;
    using SessionStateCallback = std::function<void(const DecodedSessionState&)>;
    using ClosePriceCallback = std::function<void(const DecodedClosePrice&)>;
    // New callback for Implied Volatility
    using ImpliedVolatilityCallback = std::function<void(const DecodedImpliedVolatility&)>; // Need to define this struct

    void setRecordCallback(RecordCallback callback) { recordCallback_ = callback; }
    void setOpenInterestCallback(OpenInterestCallback callback) { oiCallback_ = callback; }
    void setSessionStateCallback(SessionStateCallback callback) { sessionStateCallback_ = callback; }
    void setClosePriceCallback(ClosePriceCallback callback) { closePriceCallback_ = callback; }
    void setIndexCallback(RecordCallback callback) { indexCallback_ = callback; }
    void setImpliedVolatilityCallback(ImpliedVolatilityCallback callback) { ivCallback_ = callback; }

private:
    // Internal decoding methods
    void decodeMarketPicture(const uint8_t* buffer, size_t length, uint16_t msgType, ParserStats& stats);
    void decodeOpenInterest(const uint8_t* buffer, size_t length, ParserStats& stats);
    void decodeSessionState(const uint8_t* buffer, size_t length, ParserStats& stats);
    void decodeClosePrice(const uint8_t* buffer, size_t length, ParserStats& stats);
    void decodeImpliedVolatility(const uint8_t* buffer, size_t length, ParserStats& stats);
    void decodeIndex(const uint8_t* buffer, size_t length, ParserStats& stats);

    RecordCallback recordCallback_;
    OpenInterestCallback oiCallback_;
    SessionStateCallback sessionStateCallback_;
    ClosePriceCallback closePriceCallback_;
    ImpliedVolatilityCallback ivCallback_;
    RecordCallback indexCallback_; // Reuse record callback for Index? Or new one? 
                                   // For now, let's assume Index might use DecodedRecord or similar. 
                                   // existing code reused DecodedRecord for Index.
};

} // namespace bse

#endif // BSE_PARSER_H
