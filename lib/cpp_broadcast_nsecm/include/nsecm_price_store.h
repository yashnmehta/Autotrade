#ifndef NSECM_PRICE_STORE_H
#define NSECM_PRICE_STORE_H

#include <vector>
#include <shared_mutex>
#include <atomic>
#include <cstring>
#include <QDebug>
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
    static const size_t MAX_TOKENS = 100000; // Increased to cover all NSE tokens and indices
    
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
        if (!tokenStates[token]) {
            // qDebug() << "[NSECM Store] updateTouchline FAILED: Token" << token << "not initialized!";
            return;
        }
        auto& row = *tokenStates[token];
        
        // // Debug log for index tokens (26000-26999)
        // if (token >= 26000 && token <= 26999) {
        //     qDebug() << "[NSECM Store] updateTouchline: Token=" << token 
        //              << "LTP=" << ltp << "Volume=" << volume;
        // }
        
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
    /**
     * @deprecated Use getUnifiedSnapshot() for thread-safe access
     */
    const UnifiedTokenState* getUnifiedState(int32_t token) const;
    
    /**
     * @brief Get thread-safe snapshot copy of token state
     * @return Copy of token state, guaranteed consistent under lock
     * @note Returns empty state (token=0) if not found
     */
    [[nodiscard]] UnifiedTokenState getUnifiedSnapshot(int32_t token) const {
        if (token < 0 || token >= (int32_t)tokenStates.size()) return UnifiedTokenState{};
        std::shared_lock lock(mutex);
        if (!tokenStates[token]) return UnifiedTokenState{};
        return *tokenStates[token]; // Copy under lock - thread safe
    }
    
    ~PriceStore();
    
    // Index Store (Keep simple generic access or separate class?)
    // For now, IndexStore is separate in nsecm_price_store.cpp/h, 
    // but the previous header had it inline. I will declare the class here to keep it compiling.
    
    size_t getTokenCount() const { return MAX_TOKENS; }
    
    void clear();

private:
    std::vector<UnifiedTokenState*> tokenStates;
    mutable std::shared_mutex mutex;
};

//min token for indiaces 26000

// Global instances
extern PriceStore g_nseCmPriceStore;

/**
 * @brief Map broadcast index names (e.g. "Nifty 50") to tokens (e.g. 26000)
 * This is populated during repository load from nse_cm_index_master.csv
 */
extern std::unordered_map<std::string, uint32_t> g_indexNameToToken;

/**
 * @brief Initialize index name mapping from RepositoryManager
 */
void initializeIndexMapping(const std::unordered_map<std::string, uint32_t>& mapping);

/**
 * @brief Get LTP for any NSE token (Stock or Index)
 * @param token NSE Token (0-25999 for stocks, 26000+ for indices)
 * @return LTP or 0.0 if not found
 */
double getGenericLtp(uint32_t token);

} // namespace nsecm

#endif // NSECM_PRICE_STORE_H
