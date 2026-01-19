#ifndef NSECM_PRICE_STORE_H
#define NSECM_PRICE_STORE_H

#include <vector>
#include <shared_mutex>
#include <atomic>
#include <cstring>
#include "nsecm_callback.h"

namespace nsecm {

/**
 * @brief Unified Token State for NSE CM
 * Combines Touchline, Market Depth, and other related data into a single structure.
 * Optimized for cache locality and thread-safe access.
 */
struct UnifiedTokenState {
    int32_t token = 0;
    
    // Contract Master Info (Static)
    char symbol[32] = {0};
    char series[16] = {0};
    char displayName[64] = {0};
    int32_t lotSize = 0;
    double tickSize = 0.0;
    double priceBandHigh = 0.0;
    double priceBandLow = 0.0;
    
    // Market Data (Dynamic)
    double ltp = 0.0;
    double open = 0.0;
    double high = 0.0;
    double low = 0.0;
    double close = 0.0;
    double avgPrice = 0.0;
    
    uint64_t volume = 0;       // Volume Traded Today (CM uses 64-bit)
    uint32_t lastTradeQty = 0;
    uint32_t lastTradeTime = 0;
    
    double netChange = 0.0;
    char netChangeIndicator = ' ';
    
    uint16_t tradingStatus = 0;
    uint16_t bookType = 0;
    uint64_t totalBuyQty = 0;
    uint64_t totalSellQty = 0;
    
    // Market Depth (5 levels)
    DepthLevel bids[5];
    DepthLevel asks[5];
    
    // Latency / Updates
    int64_t lastPacketTimestamp = 0;
    bool isUpdated = false;
    
    UnifiedTokenState() {
        // Explicit zero initialization for arrays
        std::memset(bids, 0, sizeof(bids));
        std::memset(asks, 0, sizeof(asks));
    }
};

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
                        double avgPrice, double netChange, char netChangeInd, uint16_t status, uint16_t bookType);
                        
    void updateMarketDepth(int32_t token, const DepthLevel* bids, const DepthLevel* asks,
                          uint64_t totalBuy, uint64_t totalSell);
                          
    void updateTicker(int32_t token, double fillPrice, uint64_t fillVol);
    
    // Read Access
    const UnifiedTokenState* getUnifiedState(int32_t token) const;
    
    // Index Store (Keep simple generic access or separate class?)
    // For now, IndexStore is separate in nsecm_price_store.cpp/h, 
    // but the previous header had it inline. I will declare the class here to keep it compiling.
    
    size_t getTokenCount() const { return MAX_TOKENS; }

private:
    std::vector<UnifiedTokenState> tokenStates;
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
