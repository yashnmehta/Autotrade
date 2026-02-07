#ifndef BSE_PRICE_STORE_H
#define BSE_PRICE_STORE_H

#include "bse_protocol.h"
#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <vector>


#include "data/UnifiedPriceState.h"

namespace bse {

using UnifiedTokenState = MarketData::UnifiedState;
using DepthLevel = MarketData::DepthLevel;

/**
 * @brief High-Performance Distributed Price Store for BSE
 * Uses pre-allocated vector for O(1) lock-free(ish) access.
 * Shared Mutex protects concurrent writes/reads.
 */
class PriceStore {
public:
  static const size_t MAX_TOKENS = 60000; // Covers BSE token range
  PriceStore();
  ~PriceStore();

  // Delete copy/move
  PriceStore(const PriceStore &) = delete;
  PriceStore &operator=(const PriceStore &) = delete;

  /**
   * @brief Get unified state for reading (Thread-Safe Shared Lock)
   * @deprecated Use getUnifiedSnapshot() for thread-safe access
   */
  const UnifiedTokenState *getUnifiedState(uint32_t token) const;

  /**
   * @brief Get thread-safe snapshot copy of token state
   * @return Copy of token state, guaranteed consistent under lock
   * @note Returns empty state (token=0) if not found
   */
  [[nodiscard]] UnifiedTokenState getUnifiedSnapshot(uint32_t token) const {
    if (token >= MAX_TOKENS) return UnifiedTokenState{};
    std::shared_lock lock(mutex);
    if (!tokenStates[token]) return UnifiedTokenState{};
    return *tokenStates[token]; // Copy under lock - thread safe
  }

  /**
   * @brief Update Market Picture (Msg 2020/2021)
   */
  void updateMarketPicture(uint32_t token, double ltp, double open, double high,
                           double low, double close, uint64_t volume,
                           uint64_t turnover, uint64_t ltq, double atp,
                           uint64_t totalBuy, uint64_t totalSell,
                           double lowerCir, double upperCir,
                           const bse::DecodedDepthLevel *bids,
                           const bse::DecodedDepthLevel *asks,
                           uint64_t timestamp);

  /**
   * @brief Update Open Interest (Msg 2015)
   */
  void updateOpenInterest(uint32_t token, int64_t oi, int32_t oiChange,
                          uint64_t timestamp);

  /**
   * @brief Update Close Price (Msg 2014)
   */
  void updateClosePrice(uint32_t token, double closePrice, uint64_t timestamp);

  /**
   * @brief Update Implied Volatility (Msg 2028)
   */
  void updateImpliedVolatility(uint32_t token, int64_t iv, uint64_t timestamp);

  /**
   * @brief Update Greeks fields (from GreeksCalculationService)
   * @param token Token ID
   * @param iv Implied Volatility (IV)
   * @param bidIV Bid IV
   * @param askIV Ask IV
   * @param delta Delta
   * @param gamma Gamma
   * @param vega Vega (per 1% change)
   * @param theta Theta (daily decay)
   * @param theoreticalPrice Theoretical option price
   * @param timestamp Calculation timestamp
   */
  void updateGreeks(uint32_t token, double iv, double bidIV, double askIV,
                   double delta, double gamma, double vega, double theta,
                   double theoreticalPrice, int64_t timestamp);

  /**
   * @brief Initialize Token from Master (Thread-Safe)
   */
  void initializeToken(uint32_t token, const char *symbol, const char *name,
                       const char *scripCode, const char *series, int32_t lot,
                       double strike, const char *optType, const char *expiry,
                       int32_t assetToken, int32_t instType, double tick);

  /**
   * @brief Clear all data
   */
  void clear();

  /**
   * @brief Initialize from master (List of tokens)
   */
  void initializeFromMaster(const std::vector<uint32_t> &tokens);

  /**
   * @brief Check if token is valid
   */
  bool isValidToken(uint32_t token) const;

private:
  std::vector<UnifiedTokenState*> tokenStates; // Sparse vector of pointers
  mutable std::shared_mutex mutex;            // Shared mutex for thread safety
};

/**
 * @brief Index Store (Kept separate for Indices)
 */
class IndexStore {
public:
  IndexStore() = default;

  void updateIndex(uint32_t token, const char *name, double value, double open,
                   double high, double low, double close, double changePerc,
                   uint64_t timestamp);
  const DecodedRecord *
  getIndex(uint32_t token) const; // Keep DecodedRecord for now or simplify?
                                  // Let's rely on DecodedRecord for indices as
                                  // defined in bse_protocol.h
  void clear();

private:
  std::unordered_map<uint32_t, DecodedRecord> indices;
  mutable std::shared_mutex mutex;
};

// Global instances
extern PriceStore g_bseFoPriceStore;
extern PriceStore g_bseCmPriceStore;
extern IndexStore g_bseFoIndexStore;
extern IndexStore g_bseCmIndexStore;

} // namespace bse

#endif // BSE_PRICE_STORE_H
