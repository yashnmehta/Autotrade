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
    tokenStates.assign(MAX_TOKENS + 1, nullptr);
}

PriceStore::~PriceStore() {
    for (auto* ptr : tokenStates) {
        delete ptr;
    }
}

void PriceStore::initializeFromMaster(const std::vector<uint32_t>& validTokens) {
    std::unique_lock<std::shared_mutex> lock(mutex);
    if (tokenStates.size() <= MAX_TOKENS) {
        tokenStates.assign(MAX_TOKENS + 1, nullptr);
    }
    std::cout << "[NSE CM Store] Sparse Store Initialized" << std::endl;
}

void PriceStore::initializeToken(uint32_t token, const char* symbol, const char* series,
                                const char* displayName, int32_t lotSize, double tickSize,
                                double priceBandHigh, double priceBandLow) {
    if (token > MAX_TOKENS) return;
    
    std::unique_lock<std::shared_mutex> lock(mutex);
    if (!tokenStates[token]) {
        tokenStates[token] = new UnifiedTokenState();
    }
    
    UnifiedTokenState& state = *tokenStates[token];
    state.token = token;
    
    if (symbol) strncpy(state.symbol, symbol, 31);
    if (series) strncpy(state.series, series, 15);
    if (displayName) strncpy(state.displayName, displayName, 63);
    
    state.lotSize = lotSize;
    state.tickSize = tickSize;
    state.upperCircuit = priceBandHigh;
    state.lowerCircuit = priceBandLow;
    state.isUpdated = true;
}

const UnifiedTokenState* PriceStore::getUnifiedState(int32_t token) const {
    if (token < 0 || token > (int32_t)MAX_TOKENS) return nullptr;
    
    std::shared_lock<std::shared_mutex> lock(mutex);
    return tokenStates[token];
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
