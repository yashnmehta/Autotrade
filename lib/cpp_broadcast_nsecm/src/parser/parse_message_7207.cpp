#include "nse_parsers.h"
#include "protocol.h"
#include "market_data_callback.h"
#include <vector>
#include <cstring>

void parse_message_7207(const MS_BCAST_INDICES* msg) {
    uint16_t numRecords = be16toh_func(msg->numberOfRecords);
    IndicesUpdate update;
    update.numRecords = std::min(numRecords, (uint16_t)6);
    
    for (int i = 0; i < update.numRecords; i++) {
        const auto& rec = msg->indices[i];
        IndexData& data = update.indices[i];
        std::memcpy(data.name, rec.indexName, 21);
        data.value = (int32_t)be32toh_func(rec.indexValue) / 100.0;
        data.high = (int32_t)be32toh_func(rec.highIndexValue) / 100.0;
        data.low = (int32_t)be32toh_func(rec.lowIndexValue) / 100.0;
        data.open = (int32_t)be32toh_func(rec.openingIndex) / 100.0;
        data.close = (int32_t)be32toh_func(rec.closingIndex) / 100.0;
        data.percentChange = (int32_t)be32toh_func(rec.percentChange) / 100.0;
        data.yearlyHigh = (int32_t)be32toh_func(rec.yearlyHigh) / 100.0;
        data.yearlyLow = (int32_t)be32toh_func(rec.yearlyLow) / 100.0;
        data.upMoves = be32toh_func(rec.noOfUpmoves);
        data.downMoves = be32toh_func(rec.noOfDownmoves);
        data.marketCap = rec.marketCapitalisation; 
        data.netChangeIndicator = rec.netChangeIndicator;
    }
    
    MarketDataCallbackRegistry::instance().dispatchIndices(update);
}

void parse_bcast_indices(const MS_BCAST_INDICES* msg) {
    parse_message_7207(msg);
}
