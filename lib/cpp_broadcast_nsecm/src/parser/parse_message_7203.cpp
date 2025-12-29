#include "../../include/nse_parsers.h"
#include "../../include/protocol.h"
#include "../../include/market_data_callback.h"
#include <vector>
#include <cstring>

namespace nsecm {

void parse_message_7203(const MS_BCAST_INDUSTRY_INDICES* msg) {
    uint16_t numRecords = be16toh_func(msg->noOfRecs);
    IndicesUpdate update;
    update.numRecords = std::min(numRecords, (uint16_t)17);
    
    for (int i = 0; i < update.numRecords; i++) {
        const auto& rec = msg->industryIndices[i];
        IndexData& data = update.indices[i];
        std::memset(&data, 0, sizeof(data));
        std::memcpy(data.name, rec.industryName, 21);
        data.value = (int32_t)be32toh_func(rec.indexValue) / 100.0;
    }
    
    MarketDataCallbackRegistry::instance().dispatchIndices(update);
}

void parse_bcast_industry_indices(const MS_BCAST_INDUSTRY_INDICES* msg) {
    parse_message_7203(msg);
}

} // namespace nsecm
