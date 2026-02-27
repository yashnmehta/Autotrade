#include "nsefo_price_store.h"

namespace nsefo {

// Global instances
PriceStore g_nseFoPriceStore;

void PriceStore::initializeToken(uint32_t token, const char* symbol, const char* displayName,
                    int32_t lotSize, double strikePrice, const char* optionType,
                    const char* expiryDate, int64_t assetToken, 
                    int32_t instrumentType, double tickSize) {
    uint32_t index = token - MIN_TOKEN;
    if (index >= ARRAY_SIZE) return;
    
    std::unique_lock lock(mutex_);
    
    if (!store_[index]) {
        store_[index] = new UnifiedTokenState();
    }
    
    auto& row = *store_[index];
    row.token = token;
    strncpy(row.symbol, symbol, 31);
    strncpy(row.displayName, displayName, 63);
    row.lotSize = lotSize;
    row.strikePrice = strikePrice;
    strncpy(row.optionType, optionType, 2);
    strncpy(row.expiryDate, expiryDate, 15);
    row.assetToken = assetToken;
    row.instrumentType = instrumentType;
    row.tickSize = tickSize;
}

void PriceStore::initializeFromMaster(const std::vector<uint32_t>& tokens) {
    std::unique_lock lock(mutex_);
    validTokenCount = 0;
    for (uint32_t token : tokens) {
        if (token >= MIN_TOKEN && token <= MAX_TOKEN) {
            uint32_t index = token - MIN_TOKEN;
            if (!store_[index]) {
                store_[index] = new UnifiedTokenState();
            }
            store_[index]->token = token;
            validTokenCount++;
        }
    }
    qDebug() << "[NSE FO Store] Initialized" << validTokenCount << "valid tokens in Unified Store";
}

} // namespace nsefo
