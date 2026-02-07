#include "nsecm_price_store.h"
#include <QDebug>
#include <cstring>
#include <iostream>
#include <unordered_map>


namespace nsecm {

// Global instances
PriceStore g_nseCmPriceStore;
std::unordered_map<std::string, uint32_t> g_indexNameToToken;

void initializeIndexMapping(
    const std::unordered_map<std::string, uint32_t> &mapping) {
  g_indexNameToToken = mapping;
  std::cout << "[NSE CM Store] Initialized index name mapping with "
            << mapping.size() << " entries" << std::endl;
}

// ============================================================================
// PriceStore Implementation
// ============================================================================

PriceStore::PriceStore() { tokenStates.assign(MAX_TOKENS + 1, nullptr); }

PriceStore::~PriceStore() {
  for (auto *ptr : tokenStates) {
    delete ptr;
  }
}

void PriceStore::initializeFromMaster(
    const std::vector<uint32_t> &validTokens) {
  std::unique_lock<std::shared_mutex> lock(mutex);
  if (tokenStates.size() <= MAX_TOKENS) {
    tokenStates.assign(MAX_TOKENS + 1, nullptr);
  }
  std::cout << "[NSE CM Store] Sparse Store Initialized" << std::endl;
}

void PriceStore::initializeToken(uint32_t token, const char *symbol,
                                 const char *series, const char *displayName,
                                 int32_t lotSize, double tickSize,
                                 double priceBandHigh, double priceBandLow) {
  if (token > MAX_TOKENS)
    return;

  std::unique_lock<std::shared_mutex> lock(mutex);
  if (!tokenStates[token]) {
    tokenStates[token] = new UnifiedTokenState();
  }

  UnifiedTokenState &state = *tokenStates[token];
  state.token = token;

  if (symbol)
    strncpy(state.symbol, symbol, 31);
  if (series)
    strncpy(state.series, series, 15);
  if (displayName)
    strncpy(state.displayName, displayName, 63);

  state.lotSize = lotSize;
  state.tickSize = tickSize;
  state.upperCircuit = priceBandHigh;
  state.lowerCircuit = priceBandLow;
  state.isUpdated = true;
}

void PriceStore::clear() {
  std::unique_lock<std::shared_mutex> lock(mutex);
  for (auto *ptr : tokenStates) {
    delete ptr;
  }
  tokenStates.assign(MAX_TOKENS + 1, nullptr);
}

const UnifiedTokenState *PriceStore::getUnifiedState(int32_t token) const {
  if (token < 0 || token > (int32_t)MAX_TOKENS)
    return nullptr;

  std::shared_lock<std::shared_mutex> lock(mutex);
  return tokenStates[token];
}

static std::string trimRight(const std::string &s) {
  size_t end = s.find_last_not_of(" \t\n\r");
  return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

double getGenericLtp(uint32_t token) {
  auto state = g_nseCmPriceStore.getUnifiedSnapshot(token);
  return (state.token != 0) ? state.ltp : 0.0;
}

} // namespace nsecm
