#ifndef NSECM_PRICE_STORE_H
#define NSECM_PRICE_STORE_H

#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <cstdint>
#include <QDebug>
#include "nsecm_callback.h"

namespace nsecm {

/**
 * @brief Distributed price store for NSE CM (hash map)
 * 
 * NSE CM uses HASH MAP (unordered_map):
 * - Token range: Large, sparse (1-99999)
 * - Hash map for memory efficiency
 * - Still very fast (~30ns per operation)
 */
class PriceStore {
public:
    PriceStore() = default;  // Hash maps initialize empty automatically
    
    inline const TouchlineData* updateTouchline(const TouchlineData& data) {
        touchlines[data.token] = data;
        return &touchlines[data.token];
    }
    
    inline const MarketDepthData* updateDepth(const MarketDepthData& data) {
        depths[data.token] = data;
        return &depths[data.token];
    }
    
    inline const TickerData* updateTicker(const TickerData& data) {
        tickers[data.token] = data;
        return &tickers[data.token];
    }
    
    inline const TouchlineData* getTouchline(uint32_t token) const {
        auto it = touchlines.find(token);
        return (it != touchlines.end()) ? &it->second : nullptr;
    }
    
    inline const MarketDepthData* getDepth(uint32_t token) const {
        auto it = depths.find(token);
        return (it != depths.end()) ? &it->second : nullptr;
    }
    
    inline const TickerData* getTicker(uint32_t token) const {
        auto it = tickers.find(token);
        return (it != tickers.end()) ? &it->second : nullptr;
    }
    
    /**
     * @brief Initialize valid tokens from contract master
     * @param tokens Vector of valid NSE CM tokens
     * 
     * Pre-populate valid token set. Called once after loading contract master.
     */
    void initializeFromMaster(const std::vector<uint32_t>& tokens) {
        validTokens.clear();
        validTokens.reserve(tokens.size());
        for (uint32_t token : tokens) {
            validTokens.insert(token);
        }
        qDebug() << "[NSE CM Store] Initialized" << validTokens.size() << "valid tokens";
    }
    
    /**
     * @brief Initialize single token with contract master data
     */
    void initializeToken(uint32_t token, const char* symbol, const char* displayName,
                        const char* series, int32_t lotSize, double tickSize,
                        double priceBandHigh, double priceBandLow) {
        // Pre-create entry with static fields
        TouchlineData& tl = touchlines[token];
        tl.token = token;
        strncpy(tl.symbol, symbol, 31);
        strncpy(tl.displayName, displayName, 63);
        strncpy(tl.series, series, 15);
        tl.lotSize = lotSize;
        tl.tickSize = tickSize;
        tl.priceBandHigh = priceBandHigh;
        tl.priceBandLow = priceBandLow;
    }
    
    bool isValidToken(uint32_t token) const {
        return validTokens.find(token) != validTokens.end();
    }
    
    size_t touchlineCount() const { return touchlines.size(); }
    size_t depthCount() const { return depths.size(); }
    size_t tickerCount() const { return tickers.size(); }
    size_t getValidTokenCount() const { return validTokens.size(); }
    
    void clear() {
        touchlines.clear();
        depths.clear();
        tickers.clear();
        validTokens.clear();
    }

private:
    std::unordered_map<uint32_t, TouchlineData> touchlines;
    std::unordered_map<uint32_t, MarketDepthData> depths;
    std::unordered_map<uint32_t, TickerData> tickers;
    std::unordered_set<uint32_t> validTokens;  // Valid tokens from master
};

/**
 * @brief Index data store for NSE CM (hash map)
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
extern PriceStore g_nseCmPriceStore;      // Hash map for instruments
extern IndexStore g_nseCmIndexStore;      // Hash map for indices

} // namespace nsecm

#endif // NSECM_PRICE_STORE_H
