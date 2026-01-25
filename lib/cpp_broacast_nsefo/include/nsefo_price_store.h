#ifndef NSEFO_PRICE_STORE_H
#define NSEFO_PRICE_STORE_H

#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <cstdint>
#include <cstring>
#include <shared_mutex>
#include <mutex>
#include <QDebug>
#include "nsefo_callback.h"

#include "data/UnifiedPriceState.h"

namespace nsefo {

using UnifiedTokenState = MarketData::UnifiedState;


/**
 * @brief Distributed price store for NSE FO (indexed array)
 * 
 * Architecture:
 * - Thread-Safe: Uses std::shared_mutex (Shared Read, Exclusive Write)
 * - Unified: Stores all fields in one struct per token
 * - Zero-Copy Read: Returns pointer to live record
 * - Direct Access: Array indexing O(1)
 */
class PriceStore {
public:
    static constexpr uint32_t MIN_TOKEN = 35000;
    static constexpr uint32_t MAX_TOKEN = 250000;
    static constexpr uint32_t ARRAY_SIZE = MAX_TOKEN - MIN_TOKEN + 1;  // 90,000 slots
    
    PriceStore() : store_(ARRAY_SIZE, nullptr) {}
    
    ~PriceStore() {
        for (auto* ptr : store_) {
            delete ptr;
        }
    }
    
    // =========================================================
    // PARTIAL UPDATES (Write Lock)
    // =========================================================

    /**
     * @brief Update price/volume fields from UnifiedTokenState (Msg 7200)
     */
    void updateTouchline(const UnifiedTokenState& data) {
        if (data.token < MIN_TOKEN || data.token > MAX_TOKEN) return;
        
        std::unique_lock lock(mutex_); // Exclusive Write
        auto* rowPtr = store_[data.token - MIN_TOKEN];
        if (!rowPtr) return;
        auto& row = *rowPtr;
        
        // Update only dynamic price fields
        row.token = data.token;
        row.ltp = data.ltp;
        row.open = data.open;
        row.high = data.high;
        row.low = data.low;
        row.close = data.close;
        row.volume = data.volume;
        row.lastTradeQty = data.lastTradeQty;
        row.lastTradeTime = data.lastTradeTime;
        row.avgPrice = data.avgPrice;
        row.netChangeIndicator = data.netChangeIndicator;
        row.netChange = data.netChange;
        row.tradingStatus = data.tradingStatus;
        row.bookType = data.bookType;
        row.lastPacketTimestamp = data.lastPacketTimestamp;
        
        // Note: We DO NOT update bids/asks here as Msg 7200 depth is often just top level
        // or handled by 7208. If 7200 contains depth, we should update it too.
        // Assuming 7200 *does* contain depth in current parser, we can update it optionally.
        // For now, adhering to strict "Partial Update" logic.
    }
    
    /**
     * @brief Update detailed market depth (Msg 7208)
     */
    void updateDepth(const UnifiedTokenState& data) {
        if (data.token < MIN_TOKEN || data.token > MAX_TOKEN) return;
        
        std::unique_lock lock(mutex_); // Exclusive Write
        auto* rowPtr = store_[data.token - MIN_TOKEN];
        if (!rowPtr) return;
        auto& row = *rowPtr;
        
        row.token = data.token;
        std::memcpy(row.bids, data.bids, sizeof(row.bids));
        std::memcpy(row.asks, data.asks, sizeof(row.asks));
        row.totalBuyQty = data.totalBuyQty;
        row.totalSellQty = data.totalSellQty;
        row.lastPacketTimestamp = data.lastPacketTimestamp;
    }
    
    /**
     * @brief Update OI and Ticker fields (Msg 7202)
     */
    void updateTicker(const UnifiedTokenState& data) {
        if (data.token < MIN_TOKEN || data.token > MAX_TOKEN) return;
        
        std::unique_lock lock(mutex_); // Exclusive Write
        auto* rowPtr = store_[data.token - MIN_TOKEN];
        if (!rowPtr) return;
        auto& row = *rowPtr;
        
        row.token = data.token;
        row.openInterest = data.openInterest;
        row.lastPacketTimestamp = data.lastPacketTimestamp;
    }

    /**
     * @brief Update LPP fields (Msg 7220)
     */
    void updateLPP(const UnifiedTokenState& data) {
        if (data.token < MIN_TOKEN || data.token > MAX_TOKEN) return;
        
        std::unique_lock lock(mutex_); // Exclusive Write
        auto* rowPtr = store_[data.token - MIN_TOKEN];
        if (!rowPtr) return;
        auto& row = *rowPtr;
        
        row.token = data.token;
        row.upperCircuit = data.upperCircuit;
        row.lowerCircuit = data.lowerCircuit;
        row.lastPacketTimestamp = data.lastPacketTimestamp;
    }

    // =========================================================
    // UNIFIED READ (Read Lock)
    // =========================================================

    /**
     * @brief Get the complete fused state of a token
     * @return const pointer to live record (valid until next write)
     */
    const UnifiedTokenState* getUnifiedState(uint32_t token) const {
        if (token < MIN_TOKEN || token > MAX_TOKEN) return nullptr;
        
        std::shared_lock lock(mutex_); // Shared Read
        const auto* rowPtr = store_[token - MIN_TOKEN];
        if (!rowPtr) return nullptr;
        return (rowPtr->token == token) ? rowPtr : nullptr;
    }
    
    // =========================================================
    // INITIALIZATION (One-time Startup)
    // =========================================================

    void initializeToken(uint32_t token, const char* symbol, const char* displayName,
                        int32_t lotSize, double strikePrice, const char* optionType,
                        const char* expiryDate, int64_t assetToken, 
                        int32_t instrumentType, double tickSize);
    
    void initializeFromMaster(const std::vector<uint32_t>& tokens);
    
    size_t getValidTokenCount() const { return validTokenCount; }
    
    void clear() {
        std::unique_lock lock(mutex_);
        for (auto* ptr : store_) {
            delete ptr;
        }
        store_.assign(ARRAY_SIZE, nullptr);
        validTokenCount = 0;
    }

private:
    std::vector<UnifiedTokenState*> store_; 
    mutable std::shared_mutex mutex_;
    size_t validTokenCount = 0;
};

extern PriceStore g_nseFoPriceStore;
// NOTE: IndexStore removed for NSE FO as per requirements

} // namespace nsefo

#endif // NSEFO_PRICE_STORE_H
