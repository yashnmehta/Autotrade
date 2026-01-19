#include "bse_price_store.h"
#include <cstring>
#include <mutex>

namespace bse {

// ============================================================================
// PRICE STORE IMPLEMENTATION
// ============================================================================

PriceStore::PriceStore() {
    // Resize for O(1) access
    tokenStates.resize(MAX_TOKENS + 1); // +1 because tokens are 1-based usually
}

const UnifiedTokenState* PriceStore::getUnifiedState(uint32_t token) const {
    if (token >= tokenStates.size()) return nullptr;
    std::shared_lock<std::shared_mutex> lock(mutex);
    if (!tokenStates[token].isUpdated && tokenStates[token].token == 0) return nullptr;
    return &tokenStates[token];
}

void PriceStore::updateMarketPicture(uint32_t token, double ltp, double open, double high, double low, double close,
                         uint64_t volume, uint64_t turnover, uint64_t ltq, double atp,
                         uint64_t totalBuy, uint64_t totalSell, double lowerCir, double upperCir,
                         const bse::DecodedDepthLevel* bids, const bse::DecodedDepthLevel* asks,
                         uint64_t timestamp) {
    if (token >= tokenStates.size()) return;
    
    std::unique_lock<std::shared_mutex> lock(mutex);
    UnifiedTokenState& state = tokenStates[token];
    
    state.token = token;
    state.ltp = ltp;
    state.open = open;
    state.high = high;
    state.low = low;
    state.close = close; // Prev Close
    state.volume = volume;
    state.turnover = turnover;
    state.lastTradeQty = ltq;
    state.avgPrice = atp;
    state.totalBuyQty = totalBuy;
    state.totalSellQty = totalSell;
    state.lowerCircuit = lowerCir;
    state.upperCircuit = upperCir;
    
    // Depth (Fixed Copy)
    if (bids) {
        for (int i = 0; i < 5; i++) {
            state.bids[i].price = bids[i].price;
            state.bids[i].quantity = bids[i].quantity;
            state.bids[i].orders = bids[i].numOrders;
        }
    }
    
    if (asks) {
        for (int i = 0; i < 5; i++) {
            state.asks[i].price = asks[i].price;
            state.asks[i].quantity = asks[i].quantity;
            state.asks[i].orders = asks[i].numOrders;
        }
    }
    
    state.lastPacketTimestamp = timestamp;
    state.isUpdated = true;
}

void PriceStore::updateOpenInterest(uint32_t token, int64_t oi, int32_t oiChange, uint64_t timestamp) {
    if (token >= tokenStates.size()) return;
    
    std::unique_lock<std::shared_mutex> lock(mutex);
    UnifiedTokenState& state = tokenStates[token];
    
    // Ensure token is set if this is the first update
    if (state.token == 0) state.token = token;
    
    state.openInterest = oi;
    state.openInterestChange = oiChange;
    state.lastPacketTimestamp = timestamp;
    state.isUpdated = true;
}

void PriceStore::updateClosePrice(uint32_t token, double closePrice, uint64_t timestamp) {
    if (token >= tokenStates.size()) return;
    
    std::unique_lock<std::shared_mutex> lock(mutex);
    UnifiedTokenState& state = tokenStates[token];
    
    if (state.token == 0) state.token = token;
    
    // Message 2014 is "Close Price", effectively updating the closing price for the day
    // Wait, is it Prev Close or Today's Close?
    // Usually it's the final close price.
    // In bse_protocol.h: "MSG_TYPE_CLOSE_PRICE = 2014; // Close price broadcast"
    // We update 'close' or 'ltp'?
    // Usually LTP reflects the last trade. Close price is official close.
    // UnifiedTokenState has 'close' (Prev Close usually) and 'ltp'.
    // If market is closed, LTP should equal Close Price.
    // But 'close' field usually implies Previous Close until end of day rollover.
    // Let's assume we update LTP if market ended, or maybe we overwrite PrevClose?
    // BSE Parser logic earlier updated 'prevClose' in UdpBroadcastService (Step 1411 line 619).
    // "udpTick.prevClose = cp.closePrice / 100.0;"
    // So distinct message 2014 updates PrevClose? That sounds like next-day prep.
    // Let's update `close` field aka PrevClose.
    
    state.close = closePrice;
    state.lastPacketTimestamp = timestamp;
    state.isUpdated = true;
}

void PriceStore::updateImpliedVolatility(uint32_t token, int64_t iv, uint64_t timestamp) {
    if (token >= tokenStates.size()) return;
    
    std::unique_lock<std::shared_mutex> lock(mutex);
    UnifiedTokenState& state = tokenStates[token];
    
    // Ensure token is set
    if (state.token == 0) state.token = token;
    
    state.impliedVolatility = iv;
    state.lastPacketTimestamp = timestamp;
    state.isUpdated = true;
}

void PriceStore::initializeToken(uint32_t token, const char* symbol, const char* name, const char* scripCode,
                                const char* series, int32_t lot, double strike, const char* optType, const char* expiry,
                                int32_t assetToken, int32_t instType, double tick) {
    if (token >= tokenStates.size()) return;
    
    std::unique_lock<std::shared_mutex> lock(mutex);
    UnifiedTokenState& state = tokenStates[token];
    
    state.token = token;
    if (symbol) strncpy(state.symbol, symbol, sizeof(state.symbol) - 1);
    if (name) strncpy(state.displayName, name, sizeof(state.displayName) - 1);
    if (scripCode) strncpy(state.scripCode, scripCode, sizeof(state.scripCode) - 1);
    if (series) strncpy(state.series, series, sizeof(state.series) - 1);
    if (optType) strncpy(state.optionType, optType, sizeof(state.optionType) - 1);
    if (expiry) strncpy(state.expiryDate, expiry, sizeof(state.expiryDate) - 1);
    
    state.lotSize = lot;
    state.strikePrice = strike;
    state.assetToken = assetToken;
    state.instrumentType = instType;
    state.tickSize = tick;
}

void PriceStore::clear() {
    std::unique_lock<std::shared_mutex> lock(mutex);
    // Re-allocate to clear (easiest way to reset)
    tokenStates.clear();
    tokenStates.resize(MAX_TOKENS + 1);
}

bool PriceStore::isValidToken(uint32_t token) const {
    if (token >= tokenStates.size()) return false;
    std::shared_lock<std::shared_mutex> lock(mutex);
    return tokenStates[token].token != 0;
}

void PriceStore::initializeFromMaster(const std::vector<uint32_t>& tokens) {
    if (tokens.empty()) return;

    uint32_t maxToken = 0;
    for (uint32_t t : tokens) {
        if (t > maxToken) maxToken = t;
    }

    std::unique_lock<std::shared_mutex> lock(mutex);
    if (maxToken >= tokenStates.size()) {
        tokenStates.resize(maxToken + 10000); // Resize with buffer
    }
    
    // Mark these tokens as valid/known if not already?
    // initializeToken() likely already handled metadata. 
    // Just ensuring size is sufficient.
}

// ============================================================================
// INDEX STORE IMPLEMENTATION
// ============================================================================

void IndexStore::updateIndex(uint32_t token, const char* name, double value, double open, double high, double low, double close, double changePerc, uint64_t timestamp) {
    std::unique_lock<std::shared_mutex> lock(mutex);
    DecodedRecord& rec = indices[token];
    
    rec.token = token;
    rec.ltp = static_cast<int32_t>(value * 100); // DecodedRecord uses int32 paise
    rec.open = static_cast<int32_t>(open * 100);
    rec.high = static_cast<int32_t>(high * 100);
    rec.low = static_cast<int32_t>(low * 100);
    rec.close = static_cast<int32_t>(close * 100);
    rec.packetTimestamp = timestamp;
    
    // Store name if possible, DecodedRecord has symbol
    if (name) strncpy(rec.symbol, name, sizeof(rec.symbol) - 1);
}

const DecodedRecord* IndexStore::getIndex(uint32_t token) const {
    std::shared_lock<std::shared_mutex> lock(mutex);
    auto it = indices.find(token);
    if (it == indices.end()) return nullptr;
    return &it->second;
}

void IndexStore::clear() {
    std::unique_lock<std::shared_mutex> lock(mutex);
    indices.clear();
}

// Global Instances
PriceStore g_bseFoPriceStore;
PriceStore g_bseCmPriceStore;
IndexStore g_bseFoIndexStore;
IndexStore g_bseCmIndexStore;

} // namespace bse
