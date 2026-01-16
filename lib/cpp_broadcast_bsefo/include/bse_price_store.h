#ifndef BSE_PRICE_STORE_H
#define BSE_PRICE_STORE_H

#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <cstdint>
#include <QDebug>
#include "bse_protocol.h"

namespace bse {

/**
 * @brief Distributed price store for BSE (hash map)
 * 
 * BSE uses HASH MAP for both FO and CM:
 * - Sparse token distribution
 * - Hash map for flexibility
 */
class PriceStore {
public:
    PriceStore() = default;  // Hash maps initialize empty automatically
    
    inline const DecodedRecord* updateRecord(const DecodedRecord& data) {
        records[data.token] = data;
        return &records[data.token];
    }
    
    inline const DecodedOpenInterest* updateOI(const DecodedOpenInterest& data) {
        openInterests[data.token] = data;
        return &openInterests[data.token];
    }
    
    inline const DecodedRecord* getRecord(uint32_t token) const {
        auto it = records.find(token);
        return (it != records.end()) ? &it->second : nullptr;
    }
    
    inline const DecodedOpenInterest* getOI(uint32_t token) const {
        auto it = openInterests.find(token);
        return (it != openInterests.end()) ? &it->second : nullptr;
    }
    
    /**
     * @brief Initialize valid tokens from contract master
     * @param tokens Vector of valid tokens
     * 
     * Pre-populate valid token set. Called once after loading contract master.
     */
    void initializeFromMaster(const std::vector<uint32_t>& tokens) {
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
        return validTokens.find(token) != validTokens.end();
    }
    
    size_t recordCount() const { return records.size(); }
    size_t oiCount() const { return openInterests.size(); }
    size_t getValidTokenCount() const { return validTokens.size(); }
    
    void clear() {
        records.clear();
        openInterests.clear();
        validTokens.clear();
    }

private:
    std::unordered_map<uint32_t, DecodedRecord> records;
    std::unordered_map<uint32_t, DecodedOpenInterest> openInterests;
    std::unordered_set<uint32_t> validTokens;  // Valid tokens from master
};

/**
 * @brief Index data store for BSE (hash map)
 */
class IndexStore {
public:
    IndexStore() = default;  // Hash map initializes empty automatically
    
    inline const DecodedRecord* updateIndex(const DecodedRecord& data) {
        indices[data.token] = data;
        return &indices[data.token];
    }
    
    inline const DecodedRecord* getIndex(uint32_t token) const {
        auto it = indices.find(token);
        return (it != indices.end()) ? &it->second : nullptr;
    }
    
    size_t indexCount() const { return indices.size(); }
    void clear() { indices.clear(); }

private:
    std::unordered_map<uint32_t, DecodedRecord> indices;
};

// Global instances
extern PriceStore g_bseFoPriceStore;      // Hash map for BSE FO instruments
extern PriceStore g_bseCmPriceStore;      // Hash map for BSE CM instruments
extern IndexStore g_bseFoIndexStore;      // Hash map for BSE FO indices
extern IndexStore g_bseCmIndexStore;      // Hash map for BSE CM indices

} // namespace bse

#endif // BSE_PRICE_STORE_H
