#include "../../include/nse_parsers.h"
#include "../../include/protocol.h"
#include "../../include/nsecm_callback.h"
#include "../../include/nsecm_price_store.h"
#include <iostream>
#include <chrono>

namespace nsecm {

void parse_message_7200(const MS_BCAST_MBO_MBP* msg) {
    if (!msg) return;
    
    // Parse Token
    uint32_t token = be32toh_func(msg->mboData.token);
    if (token == 0) return;
    
    // Parse Touchline Fields
    double ltp = be32toh_func(msg->mboData.lastTradedPrice) / 100.0;
    double open = be32toh_func(msg->openPrice) / 100.0;
    double high = be32toh_func(msg->highPrice) / 100.0;
    double low = be32toh_func(msg->lowPrice) / 100.0;
    double close = be32toh_func(msg->closingPrice) / 100.0;
    
    uint64_t volume = be64toh_func(msg->mboData.volumeTradedToday); // CM uses 64-bit
    uint32_t lastTradeQty = be32toh_func(msg->mboData.lastTradeQuantity);
    uint32_t lastTradeTime = be32toh_func(msg->mboData.lastTradeTime);
    double avgPrice = be32toh_func(msg->mboData.averageTradePrice) / 100.0;
    
    double netChange = be32toh_func(msg->mboData.netPriceChangeFromClosingPrice) / 100.0;
    char netChangeInd = msg->mboData.netChangeIndicator;
    uint16_t tradingStatus = be16toh_func(msg->mboData.tradingStatus);
    uint16_t bookType = be16toh_func(msg->mboData.bookType);
    
    // Update Global Price Store (Thread-Safe)
    g_nseCmPriceStore.updateTouchline(token, ltp, open, high, low, close, volume,
                                     lastTradeQty, lastTradeTime, avgPrice,
                                     netChange, netChangeInd, tradingStatus, bookType);
    
    // Parse Market Depth
    DepthLevel bids[5];
    DepthLevel asks[5];
    
    uint64_t totalBuyQty = be64toh_func(msg->totalBuyQuantity);
    uint64_t totalSellQty = be64toh_func(msg->totalSellQuantity);
    
    // 0-4 Bids, 5-9 Asks
    for (int i = 0; i < 5; i++) {
        bids[i].quantity = be64toh_func(msg->mbpBuffer[i].quantity);
        bids[i].price = be32toh_func(msg->mbpBuffer[i].price) / 100.0;
        bids[i].orders = be16toh_func(msg->mbpBuffer[i].numberOfOrders);
        
        asks[i].quantity = be64toh_func(msg->mbpBuffer[i + 5].quantity);
        asks[i].price = be32toh_func(msg->mbpBuffer[i + 5].price) / 100.0;
        asks[i].orders = be16toh_func(msg->mbpBuffer[i + 5].numberOfOrders);
    }
    
    g_nseCmPriceStore.updateMarketDepth(token, bids, asks, totalBuyQty, totalSellQty);
    
    // Dispatch Callbacks (Signal only)
    MarketDataCallbackRegistry::instance().dispatchTouchline(token);
    MarketDataCallbackRegistry::instance().dispatchMarketDepth(token);
}

void parse_bcast_mbo_mbp(const MS_BCAST_MBO_MBP* msg) {
    parse_message_7200(msg);
}

} // namespace nsecm
