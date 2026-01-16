#include "bse_price_store.h"

namespace bse {

// Global instances for BSE reader threads
PriceStore g_bseFoPriceStore;     // Hash map for BSE FO instruments
PriceStore g_bseCmPriceStore;     // Hash map for BSE CM instruments
IndexStore g_bseFoIndexStore;     // Hash map for BSE FO indices
IndexStore g_bseCmIndexStore;     // Hash map for BSE CM indices

} // namespace bse
