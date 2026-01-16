#include "nsecm_price_store.h"

namespace nsecm {

// Global instances for NSE CM reader thread
PriceStore g_nseCmPriceStore;     // Hash map for instruments
IndexStore g_nseCmIndexStore;     // Hash map for indices

} // namespace nsecm
