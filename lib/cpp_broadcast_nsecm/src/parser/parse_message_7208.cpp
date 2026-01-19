#include "../../include/nse_parsers.h"
#include "../../include/protocol.h"
#include "../../include/nsecm_callback.h"
#include "../../include/nsecm_price_store.h"
#include <iostream>
#include <chrono>

namespace nsecm {

void parse_message_7208(const MS_BCAST_ONLY_MBP* msg) {
    if (!msg) return;
    
    // Convert NoOfRecords
    uint16_t numRecords = be16toh_func(msg->noOfRecords);
    
    for (int i = 0; i < numRecords && i < 2; i++) {
        const auto& data = msg->data[i];
        
        // Parse Token
        uint32_t token = be32toh_func(data.token);
        if (token == 0) continue;
        
        // Parse Touchline Fields
        double ltp = be32toh_func(data.lastTradedPrice) / 100.0;
        double open = be32toh_func(data.openPrice) / 100.0;
        double high = be32toh_func(data.highPrice) / 100.0;
        double low = be32toh_func(data.lowPrice) / 100.0;
        double close = be32toh_func(data.closingPrice) / 100.0;
        
        uint64_t volume = be64toh_func(data.volumeTradedToday); // CM uses 64-bit
        uint32_t lastTradeQty = be32toh_func(data.lastTradeQuantity);
        uint32_t lastTradeTime = be32toh_func(data.lastTradeTime);
        double avgPrice = be32toh_func(data.averageTradePrice) / 100.0;
        
        double netChange = be32toh_func(data.netPriceChangeFromClosingPrice) / 100.0;
        char netChangeInd = data.netChangeIndicator;
        uint16_t tradingStatus = be16toh_func(data.tradingStatus);
        uint16_t bookType = be16toh_func(data.bookType);
        
        // Update Global Price Store (Touchline)
        g_nseCmPriceStore.updateTouchline(token, ltp, open, high, low, close, volume,
                                         lastTradeQty, lastTradeTime, avgPrice,
                                         netChange, netChangeInd, tradingStatus, bookType);
        
        // Parse Market Depth
        DepthLevel bids[5];
        DepthLevel asks[5];
        
        uint64_t totalBuyQty = be64toh_func(data.totalBuyQuantity);
        uint64_t totalSellQty = be64toh_func(data.totalSellQuantity);
        
        for (int j = 0; j < 5; j++) {
            bids[j].quantity = be64toh_func(data.recordBuffer[j].quantity);
            bids[j].price = be32toh_func(data.recordBuffer[j].price) / 100.0;
            bids[j].orders = be16toh_func(data.recordBuffer[j].numberOfOrders);
        }
        
        for (int j = 5; j < 10; j++) {
            int idx = j - 5;
            asks[idx].quantity = be64toh_func(data.recordBuffer[j].quantity);
            asks[idx].price = be32toh_func(data.recordBuffer[j].price) / 100.0;
            asks[idx].orders = be16toh_func(data.recordBuffer[j].numberOfOrders);
        }
        
        // Update Global Price Store (Depth)
        g_nseCmPriceStore.updateMarketDepth(token, bids, asks, totalBuyQty, totalSellQty);
        
        // Dispatch Callbacks (Signal only)
        MarketDataCallbackRegistry::instance().dispatchTouchline(token);
        MarketDataCallbackRegistry::instance().dispatchMarketDepth(token);
    }
}

void parse_bcast_only_mbp(const MS_BCAST_ONLY_MBP* msg) {
    parse_message_7208(msg);
}

} // namespace nsecm
