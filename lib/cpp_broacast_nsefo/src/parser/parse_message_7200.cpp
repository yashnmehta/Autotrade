#include "nse_parsers.h"
#include "protocol.h"
#include "nsefo_callback.h"
#include <iostream>
#include <chrono>

namespace nsefo {

void parse_message_7200(const MS_BCAST_MBO_MBP* msg) {
    uint32_t token = be32toh_func(msg->data.token);
    
    // Capture timestamps for latency tracking
    static uint64_t refNoCounter = 0;
    uint64_t refNo = ++refNoCounter;
    auto now = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    
    // Parse touchline data
    TouchlineData touchline;
    touchline.token = token;
    touchline.refNo = refNo;
    touchline.timestampRecv = now;
    touchline.timestampParsed = now;
    touchline.ltp = be32toh_func(msg->data.lastTradedPrice) / 100.0;
    touchline.open = be32toh_func(msg->openPrice) / 100.0;
    touchline.high = be32toh_func(msg->highPrice) / 100.0;
    touchline.low = be32toh_func(msg->lowPrice) / 100.0;
    touchline.close = be32toh_func(msg->closingPrice) / 100.0;
    touchline.volume = be32toh_func(msg->data.volumeTradedToday);
    touchline.lastTradeQty = be32toh_func(msg->data.lastTradeQuantity);
    touchline.lastTradeTime = be32toh_func(msg->data.lastTradeTime);
    touchline.avgPrice = be32toh_func(msg->data.averageTradePrice) / 100.0;
    touchline.netChangeIndicator = msg->data.netChangeIndicator;
    touchline.netChange = be32toh_func(msg->data.netPriceChangeFromClosingPrice) / 100.0;
    touchline.tradingStatus = be16toh_func(msg->data.tradingStatus);
    touchline.bookType = be16toh_func(msg->data.bookType);
    
    // Dispatch touchline callback
    MarketDataCallbackRegistry::instance().dispatchTouchline(touchline);
    
    // Parse market depth data
    MarketDepthData depth;
    depth.token = token;
    depth.refNo = refNo;
    depth.timestampRecv = now;
    depth.timestampParsed = now;
    depth.totalBuyQty = msg->totalBuyQuantity;
    depth.totalSellQty = msg->totalSellQuantity;
    
    // Parse bid/ask levels (5 levels from recordBuffer)
    // Parse bid/ask levels (5 levels from recordBuffer)
    for (int i = 0; i < 5; i++) {
        depth.bids[i].quantity = be32toh_func(msg->recordBuffer[i].qty);
        depth.bids[i].price = be32toh_func(msg->recordBuffer[i].price) / 100.0;
        depth.bids[i].orders = be16toh_func(msg->recordBuffer[i].noOfOrders);
    }
    
    for (int i = 5; i < 10; i++) {
        int idx = i - 5;
        depth.asks[idx].quantity = be32toh_func(msg->recordBuffer[i].qty);
        depth.asks[idx].price = be32toh_func(msg->recordBuffer[i].price) / 100.0;
        depth.asks[idx].orders = be16toh_func(msg->recordBuffer[i].noOfOrders);
    }
    
    // Dispatch market depth callback
    MarketDataCallbackRegistry::instance().dispatchMarketDepth(depth);
}

void parse_bcast_mbo_mbp(const MS_BCAST_MBO_MBP* msg) {
    parse_message_7200(msg);
}

} // namespace nsefo
