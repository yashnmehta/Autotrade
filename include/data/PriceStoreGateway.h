#ifndef PRICE_STORE_GATEWAY_H
#define PRICE_STORE_GATEWAY_H

#include <QObject>
#include <QString>
#include "data/UnifiedPriceState.h"

// Forward declarations
namespace nsefo { class PriceStore; }
namespace nsecm { class PriceStore; }
namespace bse { class PriceStore; }
#include <vector>

namespace MarketData {

/**
 * @brief Unified interface to access distributed price stores across segments.
 * 
 * This class acts as a router/gateway that allows the UI to fetch data for 
 * any instrument without knowing which exchange-specific store it belongs to.
 */
class PriceStoreGateway : public QObject {
    Q_OBJECT
public:
    static PriceStoreGateway& instance();

    /**
     * @brief Get pointer to the live state of a token in a specific segment.
     * 
     * @param segment Semantic segment (1=NSECM, 2=NSEFO, 11=BSECM, 12=BSEFO)
     * @param token Exchange instrument token
     * @return const UnifiedState* Pointer to live record, or nullptr if not found.
     *         REMARK: This pointer is managed by the respective PriceStore.
     */
    const UnifiedState* getUnifiedState(int segment, uint32_t token) const;

    /**
     * @brief Enable/Disable notifications for a token.
     * This affects whether the UDP parsers will emit Qt signals for this token.
     * Note: Data is ALWAYS updated in the background store regardless of this flag.
     */
    void setTokenEnabled(int segment, uint32_t token, bool enabled);

    /**
     * @brief Check if a token is enabled for notifications.
     */
    bool isTokenEnabled(int segment, uint32_t token) const;

    /**
     * @brief Initialize all background stores with master contract lists.
     */
    void initialize(
        const std::vector<uint32_t>& nseFoTokens,
        const std::vector<uint32_t>& nseCmTokens,
        const std::vector<uint32_t>& bseFoTokens,
        const std::vector<uint32_t>& bseCmTokens
    );

private:
    PriceStoreGateway();
    ~PriceStoreGateway() = default;
};

} // namespace MarketData

#endif // PRICE_STORE_GATEWAY_H
