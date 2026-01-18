#ifndef BSE_PRICE_STORE_H
#define BSE_PRICE_STORE_H

#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <cstdint>
#include <shared_mutex>
#include <QDebug>
#include "bse_protocol.h"

namespace bse {

/**
 * @brief Distributed price store for BSE (hash map with thread safety)
 * 
 * BSE uses HASH MAP for both FO and CM:
 * - Sparse token distribution
 * - Hash map for flexibility
 * - Thread-safe: UDP thread writes, Qt thread reads
 */
class PriceStore {
public:
    PriceStore() = default;
    
    inline const DecodedRecord* updateRecord(const DecodedRecord& data) {
        std::unique_lock lock(mutex_);
        records[data.token] = data;
        return &records[data.token];
    }
    
    inline const DecodedOpenInterest* updateOI(const DecodedOpenInterest& data) {
        std::unique_lock lock(mutex_);
        openInterests[data.token] = data;
        return &openInterests[data.token];
    }
    
    inline const DecodedRecord* getRecord(uint32_t token) const {
        std::shared_lock lock(mutex_);
        auto it = records.find(token);
        return (it != records.end()) ? &it->second : nullptr;
    }
    
    inline const DecodedOpenInterest* getOI(uint32_t token) const {
        std::shared_lock lock(mutex_);
        auto it = openInterests.find(token);
        return (it != openInterests.end()) ? &it->second : nullptr;
    }
    
    /**
     * @brief Initialize valid tokens from contract master
     */
    void initializeFromMaster(const std::vector<uint32_t>& tokens) {
        std::unique_lock lock(mutex_);
        validTokens.clear();
        validTokens.reserve(tokens.size());
        for (uint32_t token : tokens) {
            validTokens.insert(token);
        }
        qDebug() << "[BSE Store] Initialized" << validTokens.size() << "valid tokens";
    }
    
    /**
     * @brief Initialize single token with contract master data
     */
    void initializeToken(uint32_t token, const char* symbol, const char* displayName,
                        const char* scripCode, const char* series, int32_t lotSize,
                        double strikePrice, const char* optionType, const char* expiryDate,
                        int64_t assetToken, int32_t instrumentType, double tickSize) {
        std::unique_lock lock(mutex_);
        // Pre-create entry with static fields
        DecodedRecord& rec = records[token];
        rec.token = token;
        strncpy(rec.symbol, symbol, 31);
        strncpy(rec.displayName, displayName, 63);
        strncpy(rec.scripCode, scripCode, 15);
        strncpy(rec.series, series, 15);
        rec.lotSize = lotSize;
        rec.strikePrice = strikePrice;
        strncpy(rec.optionType, optionType, 2);
        strncpy(rec.expiryDate, expiryDate, 15);
        rec.assetToken = assetToken;
        rec.instrumentType = instrumentType;
        rec.tickSize = tickSize;
    }
    
    bool isValidToken(uint32_t token) const {
        std::shared_lock lock(mutex_);
        return validTokens.find(token) != validTokens.end();
    }
    
    size_t recordCount() const {
        std::shared_lock lock(mutex_);
        return records.size();
    }
    
    size_t oiCount() const {
        std::shared_lock lock(mutex_);
        return openInterests.size();
    }
    
    size_t getValidTokenCount() const {
        std::shared_lock lock(mutex_);
        return validTokens.size();
    }
    
    void clear() {
        std::unique_lock lock(mutex_);
        records.clear();
        openInterests.clear();
        validTokens.clear();
    }

private:
    std::unordered_map<uint32_t, DecodedRecord> records;
    std::unordered_map<uint32_t, DecodedOpenInterest> openInterests;
    std::unordered_set<uint32_t> validTokens;
    mutable std::shared_mutex mutex_;  // Thread-safe: UDP writes, Qt reads
};

/**
 * @brief Index data store for BSE (hash map with thread safety)
 */
class IndexStore {
public:
    IndexStore() = default;
    
    inline const DecodedRecord* updateIndex(const DecodedRecord& data) {
        std::unique_lock lock(mutex_);
        indices[data.token] = data;
        return &indices[data.token];
    }
    
    inline const DecodedRecord* getIndex(uint32_t token) const {
        std::shared_lock lock(mutex_);
        auto it = indices.find(token);
        return (it != indices.end()) ? &it->second : nullptr;
    }
    
    size_t indexCount() const {
        std::shared_lock lock(mutex_);
        return indices.size();
    }
    
    void clear() {
        std::unique_lock lock(mutex_);
        indices.clear();
    }

private:
    std::unordered_map<uint32_t, DecodedRecord> indices;
    mutable std::shared_mutex mutex_;  // Thread-safe: UDP writes, Qt reads
};

// Global instances
extern PriceStore g_bseFoPriceStore;      // Hash map for BSE FO instruments
extern PriceStore g_bseCmPriceStore;      // Hash map for BSE CM instruments
extern IndexStore g_bseFoIndexStore;      // Hash map for BSE FO indices
extern IndexStore g_bseCmIndexStore;      // Hash map for BSE CM indices

} // namespace bse

#endif // BSE_PRICE_STORE_H
