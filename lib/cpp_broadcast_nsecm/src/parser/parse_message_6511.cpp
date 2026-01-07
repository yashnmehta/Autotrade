#include "../../include/nse_parsers.h"
#include "../../include/protocol.h"
#include "../../include/nsecm_callback.h"
#include <iostream>
#include <algorithm>

namespace nsecm {

void parse_message_6511(const BCAST_VCT_MESSAGES* msg) {
    // Extract header information
    uint32_t logTime = be32toh_func(msg->header.logTime);
    uint16_t txCode = be16toh_func(msg->header.transactionCode);
    
    // Extract security information
    char symbolBuf[11] = {0};
    std::copy(msg->secInfo.symbol, msg->secInfo.symbol + 10, symbolBuf);
    std::string symbol(symbolBuf);
    symbol.erase(std::remove_if(symbol.begin(), symbol.end(), ::isspace), symbol.end());
    
    char seriesBuf[3] = {0};
    std::copy(msg->secInfo.series, msg->secInfo.series + 2, seriesBuf);
    std::string series(seriesBuf);
    series.erase(std::remove_if(series.begin(), series.end(), ::isspace), series.end());
    
    // Extract market type
    int16_t marketType = be16toh_func(msg->marketType);
    
    // Extract broadcast message
    int16_t msgLength = be16toh_func(msg->broadcastMessageLength);
    if (msgLength < 0 || msgLength > 240) {
        msgLength = 240; // Clamp to maximum
    }
    std::string broadcastMsg(msg->broadcastMessage, msgLength);
    
    // Remove trailing nulls and whitespace
    broadcastMsg.erase(std::find_if(broadcastMsg.rbegin(), broadcastMsg.rend(),
                                     [](unsigned char ch) { return ch != '\0' && !std::isspace(ch); }).base(),
                       broadcastMsg.end());
    
    // Create market open message data
    MarketOpenMessage openMsg;
    openMsg.txCode = txCode;
    openMsg.timestamp = logTime;
    openMsg.symbol = symbol;
    openMsg.series = series;
    openMsg.marketType = marketType;
    openMsg.message = broadcastMsg;
    
    // Dispatch callback
    MarketDataCallbackRegistry::instance().dispatchMarketOpen(openMsg);
    
    // Debug output (optional, can be removed in production)
    #ifdef DEBUG_PARSER_6511
    std::cout << "[6511] Market Open Message:" << std::endl;
    std::cout << "  Symbol: " << symbol << " | Series: " << series << std::endl;
    std::cout << "  Market Type: " << marketType << std::endl;
    std::cout << "  Message: " << broadcastMsg << std::endl;
    std::cout << "  Timestamp: " << logTime << std::endl;
    #endif
}

} // namespace nsecm
