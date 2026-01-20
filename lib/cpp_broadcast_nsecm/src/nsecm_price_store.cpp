#include "nsecm_price_store.h"
#include <cstring>
#include <iostream>
#include <unordered_map>

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

static std::string trimRight(const std::string& s) {
    size_t end = s.find_last_not_of(" \t\n\r");
    return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

const IndexData* IndexStore::updateIndex(const IndexData& data) {
    std::unique_lock<std::shared_mutex> lock(mutex);
    std::string key = trimRight(std::string(data.name));
    indices[key] = data;
    return &indices[key];
}

const IndexData* IndexStore::getIndex(const std::string& name) const {
    std::shared_lock<std::shared_mutex> lock(mutex);
    std::string key = trimRight(name);
    auto it = indices.find(key);
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

double getGenericLtp(uint32_t token) {
    if (token < 26000) {
        auto* state = g_nseCmPriceStore.getUnifiedState(token);
        return state ? state->ltp : 0.0;
    } else {
        // Map common tokens to their broadcast index names
        // These can be verified from MS_INDICES broadcast
        static const std::unordered_map<uint32_t, std::string> tokenToName = {
            {26000, "Nifty 50"},
            {26009, "BANKNIFTY"},
            {26037, "NIFTY FIN SERVICE"},
            {26074, "NIFTY MID SELECT"},
            {26013, "Nifty Next 50"}
        };
        
        auto it = tokenToName.find(token);
        if (it != tokenToName.end()) {
            // Try common name variants
            const std::string& primaryName = it->second;
            auto* index = g_nseCmIndexStore.getIndex(primaryName);
            
            // Fallback variants if primary name not found
            if (!index) {
                if (token == 26000) {
                     index = g_nseCmIndexStore.getIndex("NIFTY 50");
                     if (!index) index = g_nseCmIndexStore.getIndex("NIFTY50");
                } else if (token == 26009) {
                     index = g_nseCmIndexStore.getIndex("Nifty Bank");
                     if (!index) index = g_nseCmIndexStore.getIndex("NIFTY BANK");
                }
            }

            if (index) {
                return index->value;
            } else {
                // Log failed lookup (throttled)
                static int failCount = 0;
                if (++failCount % 100 == 1) {
                    std::cout << "[nsecm] Index not found in store: " << it->second << " (Token: " << token << ")" << std::endl;
                }
            }
        }
        return 0.0;
    }
}

} // namespace nsecm
