#ifndef XTSFEEDBRIDGE_H
#define XTSFEEDBRIDGE_H

#include <QObject>
#include <QTimer>
#include <QElapsedTimer>
#include <QSet>
#include <QMap>
#include <QVector>
#include <QMutex>
#include <queue>
#include <atomic>

class XTSMarketDataClient;

/**
 * @brief Feed mode for the application
 * 
 * HYBRID   — UDP multicast + XTS WebSocket (office/co-located environment)
 *            UDP provides ultra-low-latency ticks for ALL subscribed tokens;
 *            XTS WebSocket supplements with 1-min OHLC candles (1505 events).
 *            XTS REST subscribe() is only called for chart windows (candles).
 * 
 * XTS_ONLY — No UDP available (internet/home user)
 *            ALL price data comes through XTS WebSocket.
 *            Every token that FeedHandler receives a subscribe() for must
 *            be subscribed on the XTS REST API, subject to rate limits
 *            and per-segment token caps.
 */
enum class FeedMode {
    HYBRID,     // UDP + XTS (default for office)
    XTS_ONLY    // XTS WebSocket only (internet user)
};

/**
 * @brief Bridges FeedHandler subscriptions to XTS REST subscribe API
 * 
 * In XTS_ONLY mode, this service:
 * 1. Intercepts every FeedHandler::subscribe() call
 * 2. Queues the token for XTS REST subscription
 * 3. Batches tokens by exchange segment
 * 4. Sends batched REST subscribe() calls respecting rate limits
 * 5. Manages a sliding window of active tokens (LRU eviction when at cap)
 * 6. Handles unsubscribe to free capacity for new tokens
 * 
 * XTS API Limits (from official docs):
 * ┌──────────────────────────────┬──────────────────────────────────────────┐
 * │ Limit                        │ Value                                    │
 * ├──────────────────────────────┼──────────────────────────────────────────┤
 * │ Total subscription limit     │ 1000 (GLOBAL across all segments)        │
 * │ Batch validation             │ ALL-OR-NOTHING (if batch > remaining     │
 * │                              │ capacity, entire request is rejected)    │
 * │ xtsMessageCode 1512 (LTP)   │ Lightest event: LTP+LTQ+LastUpdateTime   │
 * │ xtsMessageCode 1501 (Touch) │ LTP+OHLC+BBO+Volume+OI (recommended)     │
 * │ xtsMessageCode 1502 (Depth) │ Full 5-level depth (heaviest)             │
 * │ Unsubscribe method           │ PUT /instruments/subscription             │
 * │ Rate limit (quotes)          │ 1 req/sec                                │
 * │ Rate limit (subscription)    │ ~10 req/sec (empirical, conservative)    │
 * └──────────────────────────────┴──────────────────────────────────────────┘
 * 
 * Strategy: Subscribe with 1512 (LTP) by default for minimal bandwidth.
 *           Use 1501 (Touchline) for ATMWatch/OptionChain that need OHLC+OI.
 *           Use 1502 (Depth) only for SnapQuote windows requesting full depth.
 * 
 * Thread Safety: All public methods are thread-safe (QMutex protected).
 */
class XTSFeedBridge : public QObject
{
    Q_OBJECT

public:
    static XTSFeedBridge& instance();

    /**
     * @brief Set the current feed mode
     * In HYBRID mode, this bridge is dormant (no XTS subscribe calls).
     * In XTS_ONLY mode, every FeedHandler subscription triggers XTS subscribe.
     */
    void setFeedMode(FeedMode mode);
    FeedMode feedMode() const { return m_feedMode.load(); }

    /**
     * @brief Inject the XTSMarketDataClient to use for subscribe/unsubscribe
     * Must be called once after login, before any subscriptions are made.
     */
    void setMarketDataClient(XTSMarketDataClient* client);

    // ═══════════════════════════════════════════════════════════════════
    // Subscription API (called from FeedHandler::registerTokenWithUdpService)
    // ═══════════════════════════════════════════════════════════════════

    /**
     * @brief Request subscription for a token
     * If mode is XTS_ONLY, queues the token for batched XTS REST subscribe.
     * If mode is HYBRID, this is a no-op (UDP handles it).
     * 
     * @param token Exchange instrument ID
     * @param exchangeSegment 1=NSECM, 2=NSEFO, 11=BSECM, 12=BSEFO
     * @param xtsMessageCode  1512=LTP (default, lightest), 1501=Touchline, 1502=Depth
     */
    void requestSubscribe(uint32_t token, int exchangeSegment,
                          int xtsMessageCode = 1501);

    /**
     * @brief Request unsubscription for a token
     * Frees a slot in the global subscription cap.
     */
    void requestUnsubscribe(uint32_t token, int exchangeSegment,
                            int xtsMessageCode = 1501);

    /**
     * @brief Unsubscribe all tokens except 1505 (candle) subscriptions
     * 
     * Used during XTS→UDP migration to free up the XTS subscription cap
     * while keeping candle data flowing for chart windows.
     * 1505 subscriptions are ALWAYS kept alive regardless of primary source.
     */
    void unsubscribeAllExceptCandles();

    // ═══════════════════════════════════════════════════════════════════
    // Configuration
    // ═══════════════════════════════════════════════════════════════════

    struct Config {
        int maxTotalSubscriptions = 1000;  // XTS GLOBAL limit (all segments combined)
        int maxRestCallsPerSec    = 10;    // REST rate limit (conservative)
        int batchSize             = 50;    // Tokens per REST subscribe call
        int batchIntervalMs       = 200;   // Min ms between REST calls
        int cooldownOnRateLimitMs = 5000;  // Back-off on HTTP 429
        int retryDelayMs          = 2000;  // Retry failed subscriptions
        int maxRetries            = 3;     // Max retries per batch
        int defaultMessageCode    = 1501;  // 1512=LTP(lightest), 1501=Touchline, 1502=Depth
        // NOTE: XTS enforces a single GLOBAL cap of 1000, not per-segment caps.
        // maxTotalSubscriptions is the only hard limit.
    };

    void setConfig(const Config& config);
    Config config() const;

    // ═══════════════════════════════════════════════════════════════════
    // Statistics & Monitoring
    // ═══════════════════════════════════════════════════════════════════

    struct Stats {
        int totalSubscribed = 0;          // GLOBAL count across all segments
        int totalCapacity   = 1000;       // XTS global limit (mirrors Config::maxTotalSubscriptions)
        int totalPending    = 0;          // Waiting in queue
        int totalEvicted    = 0;          // LRU evictions
        int restCallsMade   = 0;          // Total REST subscribe calls
        int restCallsFailed = 0;          // Failed REST calls
        int rateLimitHits   = 0;          // HTTP 429 responses
        QMap<int, int> perSegmentCount;   // segment -> active count
    };

    Stats getStats() const;
    void dumpStats() const;

signals:
    /**
     * @brief Emitted when feed mode changes
     */
    void feedModeChanged(FeedMode newMode);

    /**
     * @brief Emitted when subscription state changes (for status bar)
     */
    void subscriptionStatsChanged(int subscribed, int pending, int capacity);

    /**
     * @brief Emitted when a rate limit is hit (for user notification)
     */
    void rateLimitHit(int cooldownMs);

    /**
     * @brief Emitted when tokens are evicted due to cap
     */
    void tokensEvicted(int count, int exchangeSegment);

private:
    XTSFeedBridge();
    ~XTSFeedBridge();
    XTSFeedBridge(const XTSFeedBridge&) = delete;
    XTSFeedBridge& operator=(const XTSFeedBridge&) = delete;

    // ─── Internal Types ──────────────────────────────────────────────

    struct PendingSubscription {
        uint32_t token;
        int exchangeSegment;
        int xtsMessageCode = 1501;
        int retryCount = 0;
        qint64 queuedAtMs = 0;  // Timestamp when queued
    };

    /**
     * @brief Per-segment tracking of subscribed tokens with LRU ordering
     * 
     * m_activeTokens[segment] = ordered set of tokens (most-recently-used last)
     * When the cap is reached, the least-recently-used token is evicted.
     */
    struct SegmentState {
        QVector<uint32_t> lruOrder;           // Oldest → Newest (back = most recent)
        QSet<uint32_t>    activeSet;          // O(1) membership check
        QSet<uint32_t>    pendingSet;         // Queued but not yet REST-subscribed

        void touchToken(uint32_t token);      // Move to back of LRU
        uint32_t evictLRU();                  // Remove & return oldest token
        void addToken(uint32_t token);        // Add to active + LRU back
        void removeToken(uint32_t token);     // Remove from active + LRU
    };

    // ─── Queue Processing ────────────────────────────────────────────

    /**
     * @brief Timer-driven batch processor
     * Fires every batchIntervalMs, dequeues up to batchSize tokens,
     * groups them by segment, and fires REST subscribe calls.
     */
    void processPendingQueue();

    /**
     * @brief Send a batched XTS REST subscribe for a single segment
     */
    void sendBatchSubscribe(int exchangeSegment, const QVector<uint32_t>& tokens,
                            int xtsMessageCode);

    /**
     * @brief Send XTS REST unsubscribe for a single segment
     */
    void sendBatchUnsubscribe(int exchangeSegment, const QVector<uint32_t>& tokens,
                              int xtsMessageCode);

    /**
     * @brief Handle rate limit (HTTP 429) — pause queue processing
     */
    void enterCooldown(int cooldownMs);

    /**
     * @brief Evict LRU tokens (globally across all segments) to make room.
     * Picks the globally oldest tokens first (round-robin across segments).
     * @return Number of tokens evicted
     */
    int evictTokensIfNeeded(int needed);

    /**
     * @brief Get total active subscription count across all segments
     */
    int totalActiveCount() const;

    /**
     * @brief Emit updated stats signal
     */
    void emitStatsUpdate();

    // ─── State ───────────────────────────────────────────────────────

    std::atomic<FeedMode> m_feedMode{FeedMode::HYBRID};
    XTSMarketDataClient*  m_mdClient = nullptr;

    // Per-segment subscription state
    QMap<int, SegmentState> m_segments;  // segment ID -> state

    // Pending subscription queue (FIFO)
    std::queue<PendingSubscription> m_pendingQueue;

    // Rate limiting
    QElapsedTimer m_rateLimitTimer;
    int m_callsThisSecond = 0;
    qint64 m_lastCallTimestamp = 0;
    bool m_inCooldown = false;

    // Timers
    QTimer* m_batchTimer = nullptr;      // Processes pending queue
    QTimer* m_cooldownTimer = nullptr;   // Resumes after rate limit

    // Statistics
    mutable QMutex m_mutex;
    int m_totalEvicted = 0;
    int m_restCallsMade = 0;
    int m_restCallsFailed = 0;
    int m_rateLimitHits = 0;

    // Config
    Config m_config;
};

#endif // XTSFEEDBRIDGE_H
