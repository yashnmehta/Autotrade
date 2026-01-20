#ifndef NSECM_PRICE_STORE_H
#define NSECM_PRICE_STORE_H

#include <vector>
#include <shared_mutex>
#include <atomic>
#include <cstring>
#include "nsecm_callback.h"

#include "data/UnifiedPriceState.h"

namespace nsecm {

using UnifiedTokenState = MarketData::UnifiedState;


/**
 * @brief Thread-Safe Distributed Price Store for NSE CM
 * Uses a pre-allocated vector for O(1) access by token ID.
 * Protected by shared_mutex for concurrent reads and exclusive writes.
 */
class PriceStore {
public:
    static const size_t MAX_TOKENS = 26000; // Covers NSE CM token range
    
    PriceStore();
    
    // Initialization
    void initializeFromMaster(const std::vector<uint32_t>& validTokens);
    void initializeToken(uint32_t token, const char* symbol, const char* series, 
                        const char* displayName, int32_t lotSize, double tickSize,
                        double priceBandHigh, double priceBandLow);
    
    // Updates
    void updateTouchline(int32_t token, double ltp, double open, double high, double low, double close,
                        uint64_t volume, uint32_t lastTradeQty, uint32_t lastTradeTime,
                        double avgPrice, double netChange, char netChangeInd, uint16_t status, uint16_t bookType) {
        if (token < 0 || token >= (int32_t)tokenStates.size()) return;
        std::unique_lock lock(mutex);
        if (!tokenStates[token]) return;
        auto& row = *tokenStates[token];
        row.ltp = ltp;
        row.open = open;
        row.high = high;
        row.low = low;
        row.close = close;
        row.volume = volume;
        row.lastTradeQty = lastTradeQty;
        row.lastTradeTime = lastTradeTime;
        row.avgPrice = avgPrice;
        row.netChange = netChange;
        row.netChangeIndicator = netChangeInd;
        row.tradingStatus = status;
        row.bookType = bookType;
        row.isUpdated = true;
    }
                        
    void updateMarketDepth(int32_t token, const DepthLevel* bids, const DepthLevel* asks,
                          uint64_t totalBuy, uint64_t totalSell) {
        if (token < 0 || token >= (int32_t)tokenStates.size()) return;
        std::unique_lock lock(mutex);
        if (!tokenStates[token]) return;
        auto& row = *tokenStates[token];
        if (bids) std::memcpy(row.bids, bids, sizeof(row.bids));
        if (asks) std::memcpy(row.asks, asks, sizeof(row.asks));
        row.totalBuyQty = totalBuy;
        row.totalSellQty = totalSell;
        row.isUpdated = true;
    }
                          
    void updateTicker(int32_t token, double fillPrice, uint64_t fillVol) {
        if (token < 0 || token >= (int32_t)tokenStates.size()) return;
        std::unique_lock lock(mutex);
        if (!tokenStates[token]) return;
        // Ticker update normally updates LTP and last trade info
        auto& row = *tokenStates[token];
        row.ltp = fillPrice;
        row.lastTradeQty = (uint32_t)fillVol;
        row.isUpdated = true;
    }
    
    // Read Access
    const UnifiedTokenState* getUnifiedState(int32_t token) const;
    
    ~PriceStore();
    
    // Index Store (Keep simple generic access or separate class?)
    // For now, IndexStore is separate in nsecm_price_store.cpp/h, 
    // but the previous header had it inline. I will declare the class here to keep it compiling.
    
    size_t getTokenCount() const { return MAX_TOKENS; }

private:
    std::vector<UnifiedTokenState*> tokenStates;
    mutable std::shared_mutex mutex;
};

//min token for indiaces 26000

/**
 * @brief Index data store for NSE CM (Thread-Safe Wrapper)
 */
class IndexStore {
public:
    IndexStore() = default;
    
    const IndexData* updateIndex(const IndexData& data);
    const IndexData* getIndex(const std::string& name) const;
    size_t indexCount() const;
    void clear();

private:
    std::unordered_map<std::string, IndexData> indices;
    mutable std::shared_mutex mutex;
};

// Global instances
extern PriceStore g_nseCmPriceStore;
extern IndexStore g_nseCmIndexStore;

} // namespace nsecm

#endif // NSECM_PRICE_STORE_H
