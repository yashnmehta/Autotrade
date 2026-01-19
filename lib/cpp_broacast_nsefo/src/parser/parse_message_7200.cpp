#include "nse_parsers.h"
#include "protocol.h"
#include "nsefo_callback.h"
#include "nsefo_price_store.h"
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
    
    // Unified Token State (Combines Price + Depth)
    UnifiedTokenState state;
    std::memset(&state, 0, sizeof(state)); // Zero-init

    state.token = token;
    state.lastPacketTimestamp = now; 
    
    // 1. Parse Price Fields
    state.ltp = be32toh_func(msg->data.lastTradedPrice) / 100.0;
    state.open = be32toh_func(msg->openPrice) / 100.0;
    state.high = be32toh_func(msg->highPrice) / 100.0;
    state.low = be32toh_func(msg->lowPrice) / 100.0;
    state.close = be32toh_func(msg->closingPrice) / 100.0;
    state.volume = be32toh_func(msg->data.volumeTradedToday);
    state.lastTradeQty = be32toh_func(msg->data.lastTradeQuantity);
    state.lastTradeTime = be32toh_func(msg->data.lastTradeTime);
    state.avgPrice = be32toh_func(msg->data.averageTradePrice) / 100.0;
    state.netChangeIndicator = msg->data.netChangeIndicator;
    state.netChange = be32toh_func(msg->data.netPriceChangeFromClosingPrice) / 100.0;
    state.tradingStatus = be16toh_func(msg->data.tradingStatus);
    state.bookType = be16toh_func(msg->data.bookType);

    // 2. Parse Depth Fields
    state.totalBuyQty = msg->totalBuyQuantity;
    state.totalSellQty = msg->totalSellQuantity;

    for (int i = 0; i < 5; i++) {
        state.bids[i].quantity = be32toh_func(msg->recordBuffer[i].qty);
        state.bids[i].price = be32toh_func(msg->recordBuffer[i].price) / 100.0;
        state.bids[i].orders = be16toh_func(msg->recordBuffer[i].noOfOrders);
    }
    
    for (int i = 5; i < 10; i++) {
        int idx = i - 5;
        state.asks[idx].quantity = be32toh_func(msg->recordBuffer[i].qty);
        state.asks[idx].price = be32toh_func(msg->recordBuffer[i].price) / 100.0;
        state.asks[idx].orders = be16toh_func(msg->recordBuffer[i].noOfOrders);
    }
    
    // 3. Update Global Store (Partial Updates)
    // We update both Price and Depth logic since 7200 contains both
    g_nseFoPriceStore.updateTouchline(state);
    g_nseFoPriceStore.updateDepth(state);
    
    // 4. Dispatch ID (Selective Notification handled by Receiver)
    // Only dispatching Touchline is enough as Unified Callback handles everything
    MarketDataCallbackRegistry::instance().dispatchTouchline(token);
}

void parse_bcast_mbo_mbp(const MS_BCAST_MBO_MBP* msg) {
    parse_message_7200(msg);
}

} // namespace nsefo
