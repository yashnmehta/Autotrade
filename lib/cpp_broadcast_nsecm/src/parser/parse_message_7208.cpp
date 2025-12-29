#include "nse_parsers.h"
#include "protocol.h"
#include "market_data_callback.h"
#include <iostream>
#include <chrono>

void parse_message_7208(const MS_BCAST_ONLY_MBP* msg) {
    // Convert NoOfRecords from Big Endian
    uint16_t numRecords = be16toh_func(msg->noOfRecords);
    
    // Capture timestamps for latency tracking
    static uint64_t refNoCounter = 0;
    auto now = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    
    for (int i = 0; i < numRecords && i < 2; i++) {
        const auto& data = msg->data[i];
        
        // Convert all multi-byte fields from Big Endian to host byte order
        uint32_t token = be32toh_func(data.token);
        
        if (token > 0) {
            uint64_t refNo = ++refNoCounter;
            
            // Parse touchline data
            TouchlineData touchline;
            touchline.token = token;
            touchline.refNo = refNo;
            touchline.timestampRecv = now;
            touchline.timestampParsed = now;
            touchline.ltp = be32toh_func(data.lastTradedPrice) / 100.0;
            
            touchline.open = be32toh_func(data.openPrice) / 100.0;
            touchline.high = be32toh_func(data.highPrice) / 100.0;
            touchline.low = be32toh_func(data.lowPrice) / 100.0;
            touchline.close = be32toh_func(data.closingPrice) / 100.0;
            touchline.volume = be64toh_func(data.volumeTradedToday); // CM is LONG LONG
            touchline.lastTradeQty = be32toh_func(data.lastTradeQuantity);
            touchline.lastTradeTime = be32toh_func(data.lastTradeTime);
            touchline.avgPrice = be32toh_func(data.averageTradePrice) / 100.0;
            touchline.netChangeIndicator = data.netChangeIndicator;
            touchline.netChange = be32toh_func(data.netPriceChangeFromClosingPrice) / 100.0;
            touchline.tradingStatus = be16toh_func(data.tradingStatus);
            touchline.bookType = be16toh_func(data.bookType);
            
            // Dispatch touchline callback
            MarketDataCallbackRegistry::instance().dispatchTouchline(touchline);
            
            // Parse market depth data
            MarketDepthData depth;
            depth.token = token;
            depth.refNo = refNo;
            depth.timestampRecv = now;
            depth.timestampParsed = now;
            depth.totalBuyQty = be64toh_func(data.totalBuyQuantity); // CM is LONG LONG
            depth.totalSellQty = be64toh_func(data.totalSellQuantity); // CM is LONG LONG
            
            // Parse bid/ask levels (5 levels each from recordBuffer)
            for (int j = 0; j < 5; j++) {
                depth.bids[j].quantity = be64toh_func(data.recordBuffer[j].quantity); // CM is LONG LONG
                depth.bids[j].price = be32toh_func(data.recordBuffer[j].price) / 100.0;
                depth.bids[j].orders = be16toh_func(data.recordBuffer[j].numberOfOrders);
            }
            
            for (int j = 5; j < 10; j++) {
                int idx = j - 5;
                depth.asks[idx].quantity = be64toh_func(data.recordBuffer[j].quantity); // CM is LONG LONG
                depth.asks[idx].price = be32toh_func(data.recordBuffer[j].price) / 100.0;
                depth.asks[idx].orders = be16toh_func(data.recordBuffer[j].numberOfOrders);
            }
            
            // Dispatch market depth callback
            MarketDataCallbackRegistry::instance().dispatchMarketDepth(depth);
        }
    }
}

// Redirect the original function to the new one
void parse_bcast_only_mbp(const MS_BCAST_ONLY_MBP* msg) {
    parse_message_7208(msg);
}
