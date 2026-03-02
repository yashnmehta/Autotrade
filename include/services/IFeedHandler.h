#ifndef IFEEDHANDLER_H
#define IFEEDHANDLER_H

#include <QObject>
#include <cstdint>
#include <vector>
#include <utility>
#include "udp/UDPTypes.h"

/**
 * @brief Abstract interface for the feed handler subsystem
 *
 * Allows mocking the FeedHandler in unit tests without requiring
 * real network connections, UDP listeners, or XTS API clients.
 *
 * Production implementation: FeedHandler
 * Test implementation: MockFeedHandler (see tests/)
 *
 * Usage in testable code:
 * ```cpp
 * class MyService {
 * public:
 *     void setFeedHandler(IFeedHandler* feed) { m_feed = feed; }
 * private:
 *     IFeedHandler* m_feed = nullptr;
 * };
 * ```
 */
class IFeedHandler
{
public:
    virtual ~IFeedHandler() = default;

    /**
     * @brief Create composite key from exchange segment and token
     */
    static inline int64_t makeKey(int exchangeSegment, int token) {
        return (static_cast<int64_t>(exchangeSegment) << 32) | static_cast<uint32_t>(token);
    }

    /**
     * @brief Subscribe to token without callback (cache-only)
     */
    virtual void subscribe(int exchangeSegment, int token) = 0;

    /**
     * @brief Unsubscribe with exchange segment
     */
    virtual void unsubscribe(int exchangeSegment, int token, QObject* receiver) = 0;

    /**
     * @brief Unsubscribe (token-only, legacy)
     */
    virtual void unsubscribe(int token, QObject* receiver) = 0;

    /**
     * @brief Unsubscribe a receiver from all tick updates
     */
    virtual void unsubscribeAll(QObject* receiver) = 0;

    /**
     * @brief Publish a UDP tick (called by UdpBroadcastService)
     */
    virtual void onUdpTickReceived(const UDP::MarketTick& tick) = 0;

    /**
     * @brief Get number of active publishers
     */
    virtual size_t totalSubscriptions() const = 0;

    /**
     * @brief Re-register all active tokens with XTSFeedBridge
     */
    virtual void reRegisterAllTokens() = 0;

    /**
     * @brief Get all active (segment, token) pairs
     */
    virtual std::vector<std::pair<int, uint32_t>> getActiveTokens() const = 0;
};

#endif // IFEEDHANDLER_H
