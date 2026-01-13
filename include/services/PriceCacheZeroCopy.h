#ifndef PRICECACHE_ZEROCOPY_H
#define PRICECACHE_ZEROCOPY_H

#include <cstdint>
#include <cstring>  // For std::memset
#include <atomic>
#include <memory>
#include <unordered_map>
#include <shared_mutex>
#include <QString>
#include <QObject>
#include <QMetaType>

/**
 * @brief Zero-Copy Price Cache - High-Performance Market Data Storage
 * 
 * Architecture:
 * - Direct memory arrays for each segment (NSE CM, NSE FO, BSE CM, BSE FO)
 * - Zero-copy updates from UDP receivers
 * - Pointer-based subscriptions (REST-like)
 * - Lock-free reads, minimal write locks
 * - Cache-aligned structures for CPU efficiency
 * 
 * Memory Layout:
 * - Each token gets 512 bytes (cache-aligned)
 * - ~24 MB per 50K tokens per segment
 * - Total: ~96 MB for all segments
 * 
 * @see docs/PriceCache_Update/README.md for architecture details
 */

namespace PriceCacheTypes {

// ============================================================================
// Market Segments
// ============================================================================

enum class MarketSegment : uint16_t {
    UNKNOWN = 0,
    NSE_CM  = 1,    // NSE Cash Market (2-level indexed)
    NSE_FO  = 2,    // NSE Futures & Options (indexed array)
    BSE_CM  = 11,   // BSE Cash Market (2-level indexed)
    BSE_FO  = 12    // BSE Futures & Options (2-level indexed)
};

// ============================================================================
// Consolidated Market Data Structure (512 bytes, cache-aligned)
// ============================================================================

/**
 * @brief Cache-aligned market data structure (512 bytes)
 * 
 * Layout optimized for CPU cache lines (64 bytes each):
 * - Cache Line 0-1 (0-127): Ultra-hot fields (LTP, Best Bid/Ask, OHLC)
 * - Cache Line 2-3 (128-255): Hot fields (Market depth 5 levels)
 * - Cache Line 4-7 (256-511): Warm/Cold fields (Metadata, indicators)
 * 
 * Note: Using #pragma pack(1) to ensure exact 512-byte size
 */
#pragma pack(push, 1)
struct ConsolidatedMarketData {
    // ========================================================================
    // CACHE LINE 0: Core Identification + Critical Prices (0-63 bytes)
    // ========================================================================
    
    // A1. Core Identification (12 bytes) - Offset 0-11
    uint32_t token;                 // Token identifier (PRIMARY KEY)
    uint16_t exchangeSegment;       // 1=NSECM, 2=NSEFO, 11=BSECM, 12=BSEFO
    uint16_t bookType;              // RL/ST/SL/OL/SP/AU
    uint16_t tradingStatus;         // 1=Preopen, 2=Open, 3=Suspended
    uint16_t marketType;            // Market type code
    
    // A2. Critical Price Fields (24 bytes) - Offset 12-35
    int32_t lastTradedPrice;        // LTP (paise) - Most accessed field
    int32_t bidPrice[5];            // Best 5 bid prices (paise) - Level 1 at [0]
    int32_t askPrice[5];            // Best 5 ask prices (paise) - Level 1 at [0]
    
    // Padding to complete cache line (remaining bytes filled by next fields)
    
    // ========================================================================
    // CACHE LINE 1: Quantities + Volume (64-127 bytes)
    // ========================================================================
    
    // A3. Market Depth Quantities (80 bytes)
    int64_t bidQuantity[5];         // Best 5 bid quantities
    int64_t askQuantity[5];         // Best 5 ask quantities
    
    // A4. Essential Trade Data (16 bytes)
    int64_t volumeTradedToday;      // Total traded volume
    int32_t lastTradeTime;          // Last trade time (HHMM format)
    int32_t lastTradeQuantity;      // Last trade quantity
    
    // ========================================================================
    // CACHE LINE 2: OHLC + Trade Info (128-191 bytes)
    // ========================================================================
    
    // B1. OHLC Data (16 bytes) - Offset 128-143
    int32_t openPrice;              // Opening price (paise)
    int32_t highPrice;              // Day high (paise)
    int32_t lowPrice;               // Day low (paise)
    int32_t closePrice;             // Previous close (paise)
    
    // B2. Price Change (8 bytes) - Offset 144-151
    int32_t netPriceChange;         // Price change from close (paise)
    char netChangeIndicator;        // '+' or '-'
    char padding1[3];               // Alignment
    
    // B3. Depth Order Counts (20 bytes) - Offset 152-171
    int16_t bidOrders[5];           // Order count at each bid level
    int16_t askOrders[5];           // Order count at each ask level
    
    // B4. Depth Aggregates (20 bytes) - Offset 172-191
    int64_t totalBuyQuantity;       // Total buy side quantity
    int64_t totalSellQuantity;      // Total sell side quantity
    uint16_t bbTotalBuyFlag;        // Buyback in buy side
    uint16_t bbTotalSellFlag;       // Buyback in sell side
    
    // ========================================================================
    // CACHE LINE 3: Additional Trade & Market Data (192-255 bytes)
    // ========================================================================
    
    // C1. Trade Metrics (16 bytes) - Offset 192-207
    int32_t averageTradePrice;      // VWAP (paise)
    int32_t indicativeClosePrice;   // Indicative close price
    int32_t fillPrice;              // Ticker fill price
    int32_t fillVolume;             // Ticker fill volume
    
    // C2. Market Watch Data (32 bytes) - Offset 208-239
    int64_t bestBuyVolume;          // Best buy volume (7201)
    int32_t bestBuyPrice;           // Best buy price (7201)
    int64_t bestSellVolume;         // Best sell volume (7201)
    int32_t bestSellPrice;          // Best sell price (7201)
    int32_t mwLastTradePrice;       // Market watch LTP
    int32_t mwLastTradeTime;        // Market watch trade time
    uint16_t mwIndicator;           // Market watch indicator
    char padding2[6];               // Alignment
    
    // ========================================================================
    // CACHE LINE 4-7: Warm/Cold Fields (256-511 bytes)
    // ========================================================================
    
    // D. Auction Information (32 bytes) - Offset 256-287
    uint16_t auctionNumber;         // Auction number
    uint16_t auctionStatus;         // Auction status
    uint16_t initiatorType;         // Control/Trader
    char padding3[2];
    int32_t initiatorPrice;         // Auction initiator price
    int32_t initiatorQuantity;      // Auction initiator qty
    int32_t auctionPrice;           // Auction match price
    int32_t auctionQuantity;        // Auction match qty
    char padding_auction[8];        // Reserved
    
    // E. SPOS Statistics (40 bytes) - Offset 288-327
    int64_t buyOrdCxlCount;         // Buy orders cancelled count
    int64_t buyOrdCxlVol;           // Buy orders cancelled volume
    int64_t sellOrdCxlCount;        // Sell orders cancelled count
    int64_t sellOrdCxlVol;          // Sell orders cancelled volume
    int64_t sposLastUpdate;         // SPOS timestamp
    
    // F. Buyback Information (64 bytes) - Offset 328-391
    char symbol[10];                // Security symbol
    char series[2];                 // Series code
    double pdayCumVol;              // Previous day volume
    int32_t pdayHighPrice;          // Previous day high
    int32_t pdayLowPrice;           // Previous day low
    int32_t pdayWtAvg;              // Previous day VWAP
    double cdayCumVol;              // Current day volume
    int32_t cdayHighPrice;          // Current day high
    int32_t cdayLowPrice;           // Current day low
    int32_t cdayWtAvg;              // Current day VWAP
    int32_t buybackStartDate;       // Buyback start date
    int32_t buybackEndDate;         // Buyback end date
    bool isBuybackActive;           // Buyback flag
    char padding4[3];               // Alignment
    
    // G. Indicators & Flags (16 bytes) - Offset 392-407
    uint16_t stIndicator;           // Status indicator flags
    bool lastTradeLess;             // LTP < prev LTP
    bool lastTradeMore;             // LTP > prev LTP
    bool buyIndicator;              // Buy side active
    bool sellIndicator;             // Sell side active
    char reserved[10];              // Future use
    
    // H. Metadata & Timestamps (32 bytes) - Offset 408-439
    int64_t lastUpdateTime;         // Last update (nanoseconds)
    uint16_t lastUpdateSource;      // Last message code
    uint32_t updateCount;           // Number of updates
    uint8_t dataQuality;            // 0-100 quality score
    bool isValid;                   // Data validity
    bool isSubscribed;              // Subscription status
    char padding5[14];              // Alignment to 32 bytes
    
    // I. Final Padding - Complete 512-byte alignment
    char padding_final[57];         // Complete 512-byte alignment
    
    // Default constructor - initialize to zero
    ConsolidatedMarketData() {
        std::memset(this, 0, sizeof(ConsolidatedMarketData));
    }
};
#pragma pack(pop)

static_assert(sizeof(ConsolidatedMarketData) == 512, 
              "ConsolidatedMarketData must be exactly 512 bytes for cache alignment");

// ============================================================================
// Subscription Result
// ============================================================================

struct SubscriptionResult {
    ConsolidatedMarketData* dataPointer;  // Direct pointer to data
    uint32_t tokenIndex;                   // Index in array
    ConsolidatedMarketData snapshot;       // Initial data snapshot
    bool success;                          // Operation success
    QString errorMessage;                  // Error message if failed
    
    SubscriptionResult() 
        : dataPointer(nullptr)
        , tokenIndex(0)
        , success(false) {}
};

// ============================================================================
// Price Cache Zero Copy - Main Class (QObject for signals)
// ============================================================================

class PriceCacheZeroCopy : public QObject {
    Q_OBJECT
public:
    /**
     * @brief Get singleton instance
     */
    static PriceCacheZeroCopy& getInstance();
    
    /**
     * @brief Initialize cache with master data
     * 
     * Creates memory arrays for each segment based on token counts.
     * Must be called during splash screen after masters are loaded.
     * 
     * @param nseCmTokens Map of NSE CM tokens to indices
     * @param nseFoTokens Map of NSE FO tokens to indices
     * @param bseCmTokens Map of BSE CM tokens to indices
     * @param bseFoTokens Map of BSE FO tokens to indices
     * @return true if initialization successful
     */
    bool initialize(
        const std::unordered_map<uint32_t, uint32_t>& nseCmTokens,
        const std::unordered_map<uint32_t, uint32_t>& nseFoTokens,
        const std::unordered_map<uint32_t, uint32_t>& bseCmTokens,
        const std::unordered_map<uint32_t, uint32_t>& bseFoTokens
    );
    
    /**
     * @brief Subscribe to token data (REST-like, one-time request)
     * 
     * This is the ASYNC version that emits Qt signals.
     * Flow: Subscriber → FeedHandler → MainUI → PriceCache
     * 
     * Emits subscriptionReady signal with:
     * - Complete data snapshot
     * - Memory pointer for direct access
     * 
     * @param token Token identifier
     * @param segment Market segment
     * @param requesterId Unique ID to identify the requester (for signal routing)
     */
    void subscribeAsync(uint32_t token, MarketSegment segment, const QString& requesterId);
    
    /**
     * @brief Subscribe to token data (SYNC version for legacy code)
     * 
     * Returns immediately with pointer and snapshot.
     * Use this for non-UI code or when signals are not needed.
     * 
     * @param token Token identifier
     * @param segment Market segment
     * @return SubscriptionResult with pointer and snapshot
     */
    SubscriptionResult subscribe(uint32_t token, MarketSegment segment);
    
    /**
     * @brief Unsubscribe from token (optional, for cleanup)
     */
    void unsubscribe(uint32_t token, MarketSegment segment);
    
    /**
     * @brief Get latest data snapshot (on-demand query)
     * 
     * Returns copy of current data without subscription.
     * Less efficient than pointer-based access.
     */
    ConsolidatedMarketData getLatestData(uint32_t token, MarketSegment segment);
    
    /**
     * @brief Check if cache is initialized
     */
    bool isInitialized() const { return m_initialized.load(); }
    
    /**
     * @brief Get base address for segment (for UDP receivers)
     * 
     * UDP receivers use this to calculate write pointers.
     * DO NOT expose to subscribers - they should use subscribe()!
     */
    ConsolidatedMarketData* getSegmentBaseAddress(MarketSegment segment);
    
    /**
     * @brief Get token index for direct array access (for UDP receivers)
     */
    bool getTokenIndex(uint32_t token, MarketSegment segment, uint32_t& outIndex);
    
    /**
     * @brief Get statistics
     */
    struct CacheStats {
        uint64_t nseCmTokenCount;
        uint64_t nseFoTokenCount;
        uint64_t bseCmTokenCount;
        uint64_t bseFoTokenCount;
        uint64_t totalMemoryBytes;
        uint64_t totalSubscriptions;
    };
    CacheStats getStats() const;

signals:
    /**
     * @brief Emitted when subscription is ready (ASYNC)
     * 
     * This signal is emitted in a thread-safe manner and will be
     * delivered to the main UI thread via Qt's event loop.
     * 
     * @param requesterId Unique ID of the requester (to route response)
     * @param token Token that was subscribed
     * @param segment Market segment
     * @param dataPointer Direct pointer for zero-copy reads (STORE THIS!)
     * @param snapshot Initial data snapshot
     * @param success Whether subscription succeeded
     * @param errorMessage Error message if failed
     */
    void subscriptionReady(
        QString requesterId,
        uint32_t token,
        PriceCacheTypes::MarketSegment segment,
        PriceCacheTypes::ConsolidatedMarketData* dataPointer,
        PriceCacheTypes::ConsolidatedMarketData snapshot,
        bool success,
        QString errorMessage
    );
    
    /**
     * @brief Emitted when data is updated (optional, for future use)
     * 
     * Can be used for push notifications instead of polling.
     * Currently not implemented - subscribers use timer or lock-free queue.
     */
    void dataUpdated(uint32_t token, PriceCacheTypes::MarketSegment segment);
    
private:
    // Private constructor (singleton)
    PriceCacheZeroCopy();
    ~PriceCacheZeroCopy();
    
    // Prevent copying
    PriceCacheZeroCopy(const PriceCacheZeroCopy&) = delete;
    PriceCacheZeroCopy& operator=(const PriceCacheZeroCopy&) = delete;
    
    // ========================================================================
    // Internal Helper Methods
    // ========================================================================
    
    /**
     * @brief Allocate memory array for segment
     */
    ConsolidatedMarketData* allocateSegmentArray(uint32_t tokenCount);
    
    /**
     * @brief Free segment array
     */
    void freeSegmentArray(ConsolidatedMarketData* array);
    
    /**
     * @brief Calculate pointer for token
     */
    ConsolidatedMarketData* calculatePointer(uint32_t tokenIndex, MarketSegment segment);
    
    // ========================================================================
    // Member Variables
    // ========================================================================
    
    // Initialization flag
    std::atomic<bool> m_initialized;
    
    // Memory arrays (one per segment)
    ConsolidatedMarketData* m_nseCmArray;
    ConsolidatedMarketData* m_nseFoArray;
    ConsolidatedMarketData* m_bseCmArray;
    ConsolidatedMarketData* m_bseFoArray;
    
    // Token to index mappings (read-only after initialization)
    std::unordered_map<uint32_t, uint32_t> m_nseCmTokenMap;
    std::unordered_map<uint32_t, uint32_t> m_nseFoTokenMap;
    std::unordered_map<uint32_t, uint32_t> m_bseCmTokenMap;
    std::unordered_map<uint32_t, uint32_t> m_bseFoTokenMap;
    
    // Token counts
    uint32_t m_nseCmCount;
    uint32_t m_nseFoCount;
    uint32_t m_bseCmCount;
    uint32_t m_bsFoCount;
    
    // Subscription tracking (for statistics)
    mutable std::shared_mutex m_subscriptionMutex;
    std::atomic<uint64_t> m_totalSubscriptions;
};

} // namespace PriceCacheTypes

// Register types for Qt meta-object system (MUST be outside namespace)
Q_DECLARE_METATYPE(PriceCacheTypes::ConsolidatedMarketData)
Q_DECLARE_METATYPE(PriceCacheTypes::SubscriptionResult)

#endif // PRICECACHE_ZEROCOPY_H

