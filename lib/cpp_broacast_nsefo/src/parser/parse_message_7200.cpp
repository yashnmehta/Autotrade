#include "nse_parsers.h"
#include "protocol.h"
#include "market_data_callback.h"
#include <iostream>
#include <chrono>

void parse_message_7200(const MS_BCAST_MBO_MBP* msg) {
    int32_t token = be32toh_func(msg->data.token);
    
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
    
    // Debug logging for token 49543
    if (token == 49543) {
        std::cout << "[7200-TOUCHLINE] Token: 49543 | RefNo: " << refNo 
                  << " | LTP: " << touchline.ltp
                  << " | timestampParsed: " << touchline.timestampParsed << " µs" << std::endl;
    }
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
    for (int i = 0; i < 5; i++) {
        DepthLevel bid;
        bid.quantity = be32toh_func(msg->recordBuffer[i].qty);
        bid.price = be32toh_func(msg->recordBuffer[i].price) / 100.0;
        bid.orders = be16toh_func(msg->recordBuffer[i].noOfOrders);
        depth.bids.push_back(bid);
    }
    
    for (int i = 5; i < 10; i++) {
        DepthLevel ask;
        ask.quantity = be32toh_func(msg->recordBuffer[i].qty);
        ask.price = be32toh_func(msg->recordBuffer[i].price) / 100.0;
        ask.orders = be16toh_func(msg->recordBuffer[i].noOfOrders);
        depth.asks.push_back(ask);
    }
    
    // Dispatch market depth callback
    MarketDataCallbackRegistry::instance().dispatchMarketDepth(depth);
}

void parse_bcast_mbo_mbp(const MS_BCAST_MBO_MBP* msg) {
    parse_message_7200(msg);
}
