#include "data/PriceStoreGateway.h"
#include "nsefo_price_store.h"
#include "nsecm_price_store.h"
#include "bse_price_store.h"
#include <QSet>
#include <mutex>

namespace MarketData {

// Internal notification filter state
static QSet<int64_t> g_enabledTokens;
static std::mutex g_filterMutex;

inline int64_t makeKey(int segment, uint32_t token) {
    return (static_cast<int64_t>(segment) << 32) | token;
}

PriceStoreGateway& PriceStoreGateway::instance() {
    static PriceStoreGateway instance;
    return instance;
}

PriceStoreGateway::PriceStoreGateway() {}

const UnifiedState* PriceStoreGateway::getUnifiedState(int segment, uint32_t token) const {
    switch (segment) {
        case 2:  return nsefo::g_nseFoPriceStore.getUnifiedState(token);
        case 1:  return nsecm::g_nseCmPriceStore.getUnifiedState(token);
        case 11: return bse::g_bseCmPriceStore.getUnifiedState(token);
        case 12: return bse::g_bseFoPriceStore.getUnifiedState(token);
        default: return nullptr;
    }
}

void PriceStoreGateway::setTokenEnabled(int segment, uint32_t token, bool enabled) {
    std::lock_guard<std::mutex> lock(g_filterMutex);
    int64_t key = makeKey(segment, token);
    if (enabled) {
        g_enabledTokens.insert(key);
    } else {
        g_enabledTokens.remove(key);
    }
}

bool PriceStoreGateway::isTokenEnabled(int segment, uint32_t token) const {
    std::lock_guard<std::mutex> lock(g_filterMutex);
    return g_enabledTokens.contains(makeKey(segment, token));
}

} // namespace MarketData
