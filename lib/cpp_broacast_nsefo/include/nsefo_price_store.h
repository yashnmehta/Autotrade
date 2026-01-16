#ifndef NSEFO_PRICE_STORE_H
#define NSEFO_PRICE_STORE_H

#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <cstdint>
#include <cstring>
#include <QDebug>
#include "nsefo_callback.h"

namespace nsefo {

/**
 * @brief Distributed price store for NSE FO (indexed array)
 * 
 * NSE FO uses INDEXED ARRAY for ultra-fast direct access:
 * - Token range: 10,000 - 99,999 (total ~90K tokens)
 * - Array[token] = direct O(1) access
 * - No hash lookup overhead (fastest possible)
 * - Perfect cache locality
 * 
 * Performance:
 * - Update: ~10ns (direct array write)
 * - Lookup: ~5ns (direct array read)
 * - No mutex (thread-local)
 */
class PriceStore {
public:
    static constexpr uint32_t MIN_TOKEN = 10000;
    static constexpr uint32_t MAX_TOKEN = 99999;
    static constexpr uint32_t ARRAY_SIZE = MAX_TOKEN - MIN_TOKEN + 1;  // 90,000 slots
    
    PriceStore() = default;  // Arrays zero-initialized at program start
    
    /**
     * @brief Update touchline (direct array write, ~10ns)
     */
    inline const TouchlineData* updateTouchline(const TouchlineData& data) {
        if (data.token < MIN_TOKEN || data.token > MAX_TOKEN) return nullptr;
        uint32_t index = data.token - MIN_TOKEN;
        touchlines[index] = data;
        touchlines[index].token = data.token;  // Ensure token is set
        return &touchlines[index];
    }
    
    /**
     * @brief Update market depth (direct array write)
     */
    inline const MarketDepthData* updateDepth(const MarketDepthData& data) {
        if (data.token < MIN_TOKEN || data.token > MAX_TOKEN) return nullptr;
        uint32_t index = data.token - MIN_TOKEN;
        depths[index] = data;
        depths[index].token = data.token;
        return &depths[index];
    }
    
    /**
     * @brief Update ticker (direct array write)
     */
    inline const TickerData* updateTicker(const TickerData& data) {
        if (data.token < MIN_TOKEN || data.token > MAX_TOKEN) return nullptr;
        uint32_t index = data.token - MIN_TOKEN;
        tickers[index] = data;
        tickers[index].token = data.token;
        return &tickers[index];
    }
    
    /**
     * @brief Get touchline (direct array read, ~5ns)
     */
    inline const TouchlineData* getTouchline(uint32_t token) const {
        if (token < MIN_TOKEN || token > MAX_TOKEN) return nullptr;
        uint32_t index = token - MIN_TOKEN;
        return (touchlines[index].token == token) ? &touchlines[index] : nullptr;
    }
    
    /**
     * @brief Get depth (direct array read)
     */
    inline const MarketDepthData* getDepth(uint32_t token) const {
        if (token < MIN_TOKEN || token > MAX_TOKEN) return nullptr;
        uint32_t index = token - MIN_TOKEN;
        return (depths[index].token == token) ? &depths[index] : nullptr;
    }
    
    /**
     * @brief Get ticker (direct array read)
     */
    inline const TickerData* getTicker(uint32_t token) const {
        if (token < MIN_TOKEN || token > MAX_TOKEN) return nullptr;
        uint32_t index = token - MIN_TOKEN;
        return (tickers[index].token == token) ? &tickers[index] : nullptr;
    }
    
    /**
     * @brief Get active count (tokens with data)
     */
    size_t touchlineCount() const {
        size_t count = 0;
        for (uint32_t i = 0; i < ARRAY_SIZE; i++) {
            if (touchlines[i].token >= MIN_TOKEN) count++;
        }
        return count;
    }
    
    /**
     * @brief Initialize valid tokens from contract master
     * @param tokens Vector of valid NSE FO tokens
     * 
     * Mark valid token slots in all arrays. Called once after loading contract master.
     */
    void initializeFromMaster(const std::vector<uint32_t>& tokens) {
        validTokenCount = 0;
        for (uint32_t token : tokens) {
            if (token >= MIN_TOKEN && token <= MAX_TOKEN) {
                uint32_t index = token - MIN_TOKEN;
                // Mark as valid in all arrays
                touchlines[index].token = token;
                depths[index].token = token;
                tickers[index].token = token;
                validTokenCount++;
            }
        }
        qDebug() << "[NSE FO Store] Initialized" << validTokenCount << "valid tokens";
    }
    
    /**
     * @brief Initialize single token with contract master data
     * @param token Token ID
     * @param symbol Symbol name
     * @param displayName Full display name
     * @param lotSize Lot size
     * @param strikePrice Strike price (0 for futures)
     * @param optionType CE/PE/XX
     * @param expiryDate DDMMMYYYY
     * @param assetToken Underlying token
     * @param instrumentType 1=Future, 2=Option
     * @param tickSize Tick size
     */
    void initializeToken(uint32_t token, const char* symbol, const char* displayName,
                        int32_t lotSize, double strikePrice, const char* optionType,
                        const char* expiryDate, int64_t assetToken, 
                        int32_t instrumentType, double tickSize) {
        uint32_t index = token - MIN_TOKEN;
        if (index >= ARRAY_SIZE) return;
        
        // Initialize touchline static fields
        touchlines[index].token = token;
        strncpy(touchlines[index].symbol, symbol, 31);
        strncpy(touchlines[index].displayName, displayName, 63);
        touchlines[index].lotSize = lotSize;
        touchlines[index].strikePrice = strikePrice;
        strncpy(touchlines[index].optionType, optionType, 2);
        strncpy(touchlines[index].expiryDate, expiryDate, 15);
        touchlines[index].assetToken = assetToken;
        touchlines[index].instrumentType = instrumentType;
        touchlines[index].tickSize = tickSize;
    }
    
    size_t getValidTokenCount() const { return validTokenCount; }
    
    void clear() {
        std::memset(touchlines, 0, sizeof(touchlines));
        std::memset(depths, 0, sizeof(depths));
        std::memset(tickers, 0, sizeof(tickers));
        validTokenCount = 0;
    }

private:
    // Fixed-size arrays (indexed by token - MIN_TOKEN)
    TouchlineData touchlines[ARRAY_SIZE];      // ~45 MB (90K * 500 bytes)
    MarketDepthData depths[ARRAY_SIZE];        // ~36 MB
    TickerData tickers[ARRAY_SIZE];            // ~27 MB
    size_t validTokenCount = 0;                // Number of valid tokens from master
    // Total: ~108 MB for NSE FO store
};

/**
 * @brief Index data store (hash map for indices)
 */
class IndexStore {
public:
    IndexStore() = default;  // Hash maps initialize empty automatically
    
    inline const IndexData* updateIndex(const IndexData& data) {
        indices[data.token] = data;
        return &indices[data.token];
    }
    
    inline const IndustryIndexData* updateIndustryIndex(const IndustryIndexData& data) {
        industryIndices[data.token] = data;
        return &industryIndices[data.token];
    }
    
    inline const IndexData* getIndex(uint32_t token) const {
        auto it = indices.find(token);
        return (it != indices.end()) ? &it->second : nullptr;
    }
    
    size_t indexCount() const { return indices.size(); }
    void clear() { indices.clear(); industryIndices.clear(); }

private:
    std::unordered_map<uint32_t, IndexData> indices;
    std::unordered_map<uint32_t, IndustryIndexData> industryIndices;
};

// Global instances
extern PriceStore g_nseFoPriceStore;      // Indexed array for instruments
extern IndexStore g_nseFoIndexStore;      // Hash map for indices

} // namespace nsefo

#endif // NSEFO_PRICE_STORE_H
