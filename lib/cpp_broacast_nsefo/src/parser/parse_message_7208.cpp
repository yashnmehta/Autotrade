#include "nse_parsers.h"
#include "protocol.h"
#include "market_data_callback.h"
#include <iostream>

void parse_message_7208(const MS_BCAST_ONLY_MBP* msg) {
    // Convert NoOfRecords from Big Endian
    uint16_t numRecords = be16toh_func(msg->noOfRecords);
    
    for (int i = 0; i < numRecords && i < 2; i++) {
        const auto& data = msg->data[i];
        
        // Convert all multi-byte fields from Big Endian to host byte order
        int32_t token = be32toh_func(data.token);
        
        if (token > 0) {
            // Parse touchline data
            TouchlineData touchline;
            touchline.token = token;
            touchline.ltp = be32toh_func(data.lastTradedPrice) / 100.0;
            touchline.open = be32toh_func(data.openPrice) / 100.0;
            touchline.high = be32toh_func(data.highPrice) / 100.0;
            touchline.low = be32toh_func(data.lowPrice) / 100.0;
            touchline.close = be32toh_func(data.closingPrice) / 100.0;
            touchline.volume = be32toh_func(data.volumeTradedToday);
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
            depth.totalBuyQty = data.totalBuyQuantity;
            depth.totalSellQty = data.totalSellQuantity;
            
            // Parse bid/ask levels (5 levels each from recordBuffer)
            for (int j = 0; j < 5; j++) {
                DepthLevel bid;
                bid.quantity = be32toh_func(data.recordBuffer[j].quantity);
                bid.price = be32toh_func(data.recordBuffer[j].price) / 100.0;
                bid.orders = be16toh_func(data.recordBuffer[j].numberOfOrders);
                depth.bids.push_back(bid);
            }
            
            for (int j = 5; j < 10; j++) {
                DepthLevel ask;
                ask.quantity = be32toh_func(data.recordBuffer[j].quantity);
                ask.price = be32toh_func(data.recordBuffer[j].price) / 100.0;
                ask.orders = be16toh_func(data.recordBuffer[j].numberOfOrders);
                depth.asks.push_back(ask);
            }
            
            // Dispatch market depth callback
            MarketDataCallbackRegistry::instance().dispatchMarketDepth(depth);
        }
        
        /*
        std::cout << "    [" << i << "] Token: " << token
                  << " | Last Price: " << lastPrice
                  << " | Volume: " << volume
                  << " | Open: " << openPrice
                  << " | High: " << highPrice
                  << " | Low: " << lowPrice
                  << " | Close: " << closePrice << std::endl;
        
        // Show top 3 bid/ask levels
        std::cout << "      Top Bids: ";
        for (int j = 0; j < 3 && j < 10; j++) {
            int32_t price = be32toh_func(data.recordBuffer[j].price);
            int32_t quantity = be32toh_func(data.recordBuffer[j].quantity);
            if (price > 0) {
                std::cout << price << "(" << quantity << ") ";
            }
        }
        std::cout << std::endl;
        */
    }
}

// Redirect the original function to the new one
void parse_bcast_only_mbp(const MS_BCAST_ONLY_MBP* msg) {
    parse_message_7208(msg);
}
