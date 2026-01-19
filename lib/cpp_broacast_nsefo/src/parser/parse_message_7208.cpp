#include "nse_parsers.h"
#include "protocol.h"
#include "nsefo_callback.h"
#include "nsefo_price_store.h"
#include <iostream>
#include <chrono>

namespace nsefo {


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
            
            // Unified Token State
            UnifiedTokenState state;
            std::memset(&state, 0, sizeof(state));

            state.token = token;
            state.lastPacketTimestamp = now;

            // 1. Parse Price Fields
            state.ltp = be32toh_func(data.lastTradedPrice) / 100.0;
            state.open = be32toh_func(data.openPrice) / 100.0;
            state.high = be32toh_func(data.highPrice) / 100.0;
            state.low = be32toh_func(data.lowPrice) / 100.0;
            state.close = be32toh_func(data.closingPrice) / 100.0;
            state.volume = be32toh_func(data.volumeTradedToday);
            state.lastTradeQty = be32toh_func(data.lastTradeQuantity);
            state.lastTradeTime = be32toh_func(data.lastTradeTime);
            state.avgPrice = be32toh_func(data.averageTradePrice) / 100.0;
            state.netChangeIndicator = data.netChangeIndicator;
            state.netChange = be32toh_func(data.netPriceChangeFromClosingPrice) / 100.0;
            state.tradingStatus = be16toh_func(data.tradingStatus);
            state.bookType = be16toh_func(data.bookType);

            // 2. Parse Depth Fields
            state.totalBuyQty = data.totalBuyQuantity;
            state.totalSellQty = data.totalSellQuantity;

            for (int j = 0; j < 5; j++) {
                state.bids[j].quantity = be32toh_func(data.recordBuffer[j].quantity);
                state.bids[j].price = be32toh_func(data.recordBuffer[j].price) / 100.0;
                state.bids[j].orders = be16toh_func(data.recordBuffer[j].numberOfOrders);
            }

            for (int j = 5; j < 10; j++) {
                int idx = j - 5;
                state.asks[idx].quantity = be32toh_func(data.recordBuffer[j].quantity);
                state.asks[idx].price = be32toh_func(data.recordBuffer[j].price) / 100.0;
                state.asks[idx].orders = be16toh_func(data.recordBuffer[j].numberOfOrders);
            }

            // 3. Update Global Store
            g_nseFoPriceStore.updateTouchline(state);
            g_nseFoPriceStore.updateDepth(state);

            // 4. Dispatch ID
            MarketDataCallbackRegistry::instance().dispatchTouchline(token);
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

} // namespace nsefo
