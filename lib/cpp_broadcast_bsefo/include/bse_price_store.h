#ifndef BSE_PRICE_STORE_H
#define BSE_PRICE_STORE_H


#include <vector>
#include <unordered_map>
#include <array>
#include <cstdint>
#include <shared_mutex>
#include <cstring>
#include <atomic>
#include <mutex>
#include "bse_protocol.h"

namespace bse {

// Maximum tokens for BSE (Scrip codes go up to ~6 digits, e.g., 500325, 9xxxxx for new)
// 1 Million covers standard equity. FO tokens might be different.
// Safe upper bound 1,200,000 for vector.
constexpr size_t MAX_BSE_TOKENS = 1200000; // verify this figure from actual data (master data export)

/**
 * @brief Consolidated state for a single BSE token
 * Aligned for cache line performance.
 */
struct UnifiedTokenState {
    uint32_t token = 0;

    // === STATIC MASTER DATA ===
    char symbol[32] = {0};
    char displayName[64] = {0};
    char scripCode[16] = {0};
    char series[16] = {0};
    char optionType[3] = {0};
    char expiryDate[16] = {0};
    int32_t lotSize = 0;
    double strikePrice = 0.0;
    double tickSize = 0.0;
    int32_t instrumentType = 0;
    int32_t assetToken = 0;
    double lowerCircuit = 0.0;
    double upperCircuit = 0.0;

    // === DYNAMIC MARKET DATA ===
    double ltp = 0.0;
    double open = 0.0;
    double high = 0.0;
    double low = 0.0;
    double close = 0.0; // Previous Close
    uint64_t volume = 0;
    uint64_t turnover = 0; // Traded Value
    uint64_t ltq = 0; // Last Traded Qty
    double weightedAvgPrice = 0.0;
    
    uint64_t totalBuyQty = 0;
    uint64_t totalSellQty = 0;

    // Depth (Zero-Copy Array)
    struct DepthLevel {
        double price = 0.0;
        uint64_t quantity = 0;
        uint32_t orders = 0;
    };
    DepthLevel bids[5];
    DepthLevel asks[5];

    // Derivatives Specific
    int64_t openInterest = 0;
    int32_t openInterestChange = 0;
    int64_t impliedVolatility = 0;

    // Latency Tracking
    uint64_t lastPacketTimestamp = 0;
    bool isUpdated = false;
};

/**
 * @brief High-Performance Distributed Price Store for BSE
 * Uses pre-allocated vector for O(1) lock-free(ish) access.
 * Shared Mutex protects concurrent writes/reads.
 */
class PriceStore {
public:
    PriceStore();
    ~PriceStore() = default;

    // Delete copy/move
    PriceStore(const PriceStore&) = delete;
    PriceStore& operator=(const PriceStore&) = delete;

    /**
     * @brief Get unified state for reading (Thread-Safe Shared Lock)
     */
    const UnifiedTokenState* getUnifiedState(uint32_t token) const;

    /**
     * @brief Update Market Picture (Msg 2020/2021)
     */
    void updateMarketPicture(uint32_t token, double ltp, double open, double high, double low, double close,
                             uint64_t volume, uint64_t turnover, uint64_t ltq, double atp,
                             uint64_t totalBuy, uint64_t totalSell, double lowerCir, double upperCir,
                             const bse::DecodedDepthLevel* bids, const bse::DecodedDepthLevel* asks,
                             uint64_t timestamp);

    /**
     * @brief Update Open Interest (Msg 2015)
     */
    void updateOpenInterest(uint32_t token, int64_t oi, int32_t oiChange, uint64_t timestamp);

    /**
     * @brief Update Close Price (Msg 2014)
     */
    void updateClosePrice(uint32_t token, double closePrice, uint64_t timestamp);

    /**
     * @brief Update Implied Volatility (Msg 2028)
     */
    void updateImpliedVolatility(uint32_t token, int64_t iv, uint64_t timestamp);

    /**
     * @brief Initialize Token from Master (Thread-Safe)
     */
    void initializeToken(uint32_t token, const char* symbol, const char* name, const char* scripCode,
                        const char* series, int32_t lot, double strike, const char* optType, const char* expiry,
                        int32_t assetToken, int32_t instType, double tick);

    /**
     * @brief Clear all data
     */
    void clear();

    /**
     * @brief Initialize from master (List of tokens)
     */
    void initializeFromMaster(const std::vector<uint32_t>& tokens);

    /**
     * @brief Check if token is valid
     */
    bool isValidToken(uint32_t token) const;

private:
    std::vector<UnifiedTokenState> tokenStates; // Fixed size vector
    mutable std::shared_mutex mutex;            // Shared mutex for thread safety
};

/**
 * @brief Index Store (Kept separate for Indices)
 */
class IndexStore {
public:
    IndexStore() = default;

    void updateIndex(uint32_t token, const char* name, double value, double open, double high, double low, double close, double changePerc, uint64_t timestamp);
    const DecodedRecord* getIndex(uint32_t token) const; // Keep DecodedRecord for now or simplify?
                                                         // Let's rely on DecodedRecord for indices as defined in bse_protocol.h
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
