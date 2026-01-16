#include "nsefo_price_store.h"

namespace nsefo {

// Global instances for NSE FO reader thread
PriceStore g_nseFoPriceStore;     // Indexed array for instruments
IndexStore g_nseFoIndexStore;     // Hash map for indices

} // namespace nsefo
