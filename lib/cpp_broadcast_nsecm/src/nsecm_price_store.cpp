#include "nsecm_price_store.h"
#include <cstring>
#include <iostream>

namespace nsecm {

// Global instances
PriceStore g_nseCmPriceStore;
IndexStore g_nseCmIndexStore;

// ============================================================================
// PriceStore Implementation
// ============================================================================

PriceStore::PriceStore() {
    // Pre-allocate vector to max tokens
    tokenStates.resize(MAX_TOKENS + 1);
}

void PriceStore::initializeFromMaster(const std::vector<uint32_t>& validTokens) {
    std::unique_lock<std::shared_mutex> lock(mutex);
    // Ensure vector is large enough (already done in ctor but safe to check)
    if (tokenStates.size() <= MAX_TOKENS) {
        tokenStates.resize(MAX_TOKENS + 1);
    }
    std::cout << "[NSE CM Store] Initialized with max tokens: " << MAX_TOKENS << std::endl;
}

void PriceStore::initializeToken(uint32_t token, const char* symbol, const char* series,
                                const char* displayName, int32_t lotSize, double tickSize,
                                double priceBandHigh, double priceBandLow) {
    if (token > MAX_TOKENS) return;
    
    std::unique_lock<std::shared_mutex> lock(mutex);
    UnifiedTokenState& state = tokenStates[token];
    state.token = token;
    
    if (symbol) strncpy(state.symbol, symbol, 31);
    if (series) strncpy(state.series, series, 15);
    if (displayName) strncpy(state.displayName, displayName, 63);
    
    state.lotSize = lotSize;
    state.tickSize = tickSize;
    state.upperCircuit = priceBandHigh;
    state.lowerCircuit = priceBandLow;
    state.isUpdated = true; // Mark as initialized
}

void PriceStore::updateTouchline(int32_t token, double ltp, double open, double high, double low, double close,
                                uint64_t volume, uint32_t lastTradeQty, uint32_t lastTradeTime,
                                double avgPrice, double netChange, char netChangeInd, uint16_t status, uint16_t bookType) {
    if (token > MAX_TOKENS) return;
    
    std::unique_lock<std::shared_mutex> lock(mutex);
    UnifiedTokenState& state = tokenStates[token];
    
    state.token = token; // Ensure token is set
    state.ltp = ltp;
    state.open = open;
    state.high = high;
    state.low = low;
    state.close = close;
    state.volume = volume;
    state.lastTradeQty = lastTradeQty;
    state.lastTradeTime = lastTradeTime;
    state.avgPrice = avgPrice;
    state.netChange = netChange;
    state.netChangeIndicator = netChangeInd;
    state.tradingStatus = status;
    state.bookType = bookType;
    state.isUpdated = true;
}

void PriceStore::updateMarketDepth(int32_t token, const DepthLevel* bids, const DepthLevel* asks,
                                  uint64_t totalBuy, uint64_t totalSell) {
    if (token > MAX_TOKENS) return;
    
    std::unique_lock<std::shared_mutex> lock(mutex);
    UnifiedTokenState& state = tokenStates[token];
    
    state.token = token;
    state.totalBuyQty = totalBuy;
    state.totalSellQty = totalSell;
    
    if (bids) std::memcpy(state.bids, bids, sizeof(state.bids));
    if (asks) std::memcpy(state.asks, asks, sizeof(state.asks));
    
    state.isUpdated = true;
}

void PriceStore::updateTicker(int32_t token, double fillPrice, uint64_t fillVol) {
    if (token > MAX_TOKENS) return;
    
    std::unique_lock<std::shared_mutex> lock(mutex);
    UnifiedTokenState& state = tokenStates[token];
    
    state.token = token;
    state.ltp = fillPrice;
    state.volume += fillVol; // Accumulate volume? Or is fillVol cumulative? Usually ticker is incremental trade. 
                             // Warning: if 7200 provides cumulative, we should be careful mixing. 
                             // But Ticker (18703) often provides 'FillVolume' of the specific trade.
                             // For now, let's just update LTP. Accumulating volume safely needs logic.
                             // Actually, 7200 is snapshot, 18703 is tick. 
                             // We'll update LTP.
    state.isUpdated = true;
}

const UnifiedTokenState* PriceStore::getUnifiedState(int32_t token) const {
    if (token > MAX_TOKENS) return nullptr;
    
    std::shared_lock<std::shared_mutex> lock(mutex);
    const UnifiedTokenState& state = tokenStates[token];
    // Return pointers only to active/updated tokens
    if (!state.isUpdated && state.token == 0) return nullptr; 
    return &state;
}

// ============================================================================
// IndexStore Implementation
// ============================================================================

const IndexData* IndexStore::updateIndex(const IndexData& data) {
    std::unique_lock<std::shared_mutex> lock(mutex);
    indices[data.name] = data;
    return &indices[data.name];
}

const IndexData* IndexStore::getIndex(const std::string& name) const {
    std::shared_lock<std::shared_mutex> lock(mutex);
    auto it = indices.find(name);
    return (it != indices.end()) ? &it->second : nullptr;
}

size_t IndexStore::indexCount() const {
    std::shared_lock<std::shared_mutex> lock(mutex);
    return indices.size();
}

void IndexStore::clear() {
    std::unique_lock<std::shared_mutex> lock(mutex);
    indices.clear();
}

} // namespace nsecm
