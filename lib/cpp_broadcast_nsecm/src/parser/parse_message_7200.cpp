#include "../../include/nse_parsers.h"
#include "../../include/protocol.h"
#include "../../include/market_data_callback.h"
#include <iostream>
#include <chrono>

void parse_message_7200(const MS_BCAST_MBO_MBP* msg) {
    uint32_t token = be32toh_func(msg->mboData.token);
    
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
    touchline.ltp = be32toh_func(msg->mboData.lastTradedPrice) / 100.0;
    
    touchline.open = be32toh_func(msg->openPrice) / 100.0;
    touchline.high = be32toh_func(msg->highPrice) / 100.0;
    touchline.low = be32toh_func(msg->lowPrice) / 100.0;
    touchline.close = be32toh_func(msg->closingPrice) / 100.0;
    touchline.volume = be64toh_func(msg->mboData.volumeTradedToday); // CM is LONG LONG
    touchline.lastTradeQty = be32toh_func(msg->mboData.lastTradeQuantity);
    touchline.lastTradeTime = be32toh_func(msg->mboData.lastTradeTime);
    touchline.avgPrice = be32toh_func(msg->mboData.averageTradePrice) / 100.0;
    touchline.netChangeIndicator = msg->mboData.netChangeIndicator;
    touchline.netChange = be32toh_func(msg->mboData.netPriceChangeFromClosingPrice) / 100.0;
    touchline.tradingStatus = be16toh_func(msg->mboData.tradingStatus);
    touchline.bookType = be16toh_func(msg->mboData.bookType);
    
    // Dispatch touchline callback
    MarketDataCallbackRegistry::instance().dispatchTouchline(touchline);
    
    // Parse market depth data (from mbpBuffer)
    MarketDepthData depth;
    depth.token = token;
    depth.refNo = refNo;
    depth.timestampRecv = now;
    depth.timestampParsed = now;
    depth.totalBuyQty = be64toh_func(msg->totalBuyQuantity); // CM is LONG LONG
    depth.totalSellQty = be64toh_func(msg->totalSellQuantity); // CM is LONG LONG
    
    // Parse bid/ask levels from MBP Information (next 5 Buy, next 5 Sell)
    // Parse bid/ask levels from MBP Information (next 5 Buy, next 5 Sell)
    for (int i = 0; i < 5; i++) {
        depth.bids[i].quantity = be64toh_func(msg->mbpBuffer[i].quantity); // CM is LONG LONG
        depth.bids[i].price = be32toh_func(msg->mbpBuffer[i].price) / 100.0;
        depth.bids[i].orders = be16toh_func(msg->mbpBuffer[i].numberOfOrders);
    }
    
    for (int i = 5; i < 10; i++) {
        int idx = i - 5;
        depth.asks[idx].quantity = be64toh_func(msg->mbpBuffer[i].quantity); // CM is LONG LONG
        depth.asks[idx].price = be32toh_func(msg->mbpBuffer[i].price) / 100.0;
        depth.asks[idx].orders = be16toh_func(msg->mbpBuffer[i].numberOfOrders);
    }
    
    // Dispatch market depth callback
    MarketDataCallbackRegistry::instance().dispatchMarketDepth(depth);
    
    // Note: MBO Data (Individual Orders) is in msg->mboData.mboBuffer
    // We currently don't have a callback for individual orders, but the data is parsed.
}

void parse_bcast_mbo_mbp(const MS_BCAST_MBO_MBP* msg) {
    parse_message_7200(msg);
}
