#ifndef UDP_TYPES_H
#define UDP_TYPES_H

#include <cstdint>
#include <array>
#include "udp/UDPEnums.h"

namespace UDP {

/**
 * @brief Type of market data update
 * Indicates which fields were updated in this tick
 */
enum class UpdateType : uint8_t {
    TRADE_TICK = 0,      // LTP, volume, OI changed (7202, 17202)
    DEPTH_UPDATE = 1,    // Order book depth changed (7208)
    TOUCHLINE = 2,       // BBO + basic stats (7200)
    MARKET_WATCH = 3,    // Enhanced market watch (7201, 17201)
    OI_CHANGE = 4,       // OI-only update
    FULL_SNAPSHOT = 5,   // Complete state refresh
    CIRCUIT_LIMIT = 6,   // Circuit limit update (7220)
    UNKNOWN = 255
};

/**
 * @brief Bitmask flags for field validity
 * Indicates which fields contain valid/updated data
 */
enum ValidFlags : uint32_t {
    VALID_LTP         = 1 << 0,   // LTP field valid
    VALID_VOLUME      = 1 << 1,   // Volume field valid
    VALID_OI          = 1 << 2,   // Open Interest valid
    VALID_DEPTH       = 1 << 3,   // Depth arrays valid
    VALID_BID_TOP     = 1 << 4,   // Best bid valid
    VALID_ASK_TOP     = 1 << 5,   // Best ask valid
    VALID_OHLC        = 1 << 6,   // OHLC fields valid
    VALID_PREV_CLOSE  = 1 << 7,   // Previous close valid
    VALID_ATP         = 1 << 8,   // Average traded price valid
    VALID_TIMESTAMP   = 1 << 9,   // Timestamp valid
    VALID_ALL         = 0xFFFFFFFF // All fields valid
};

/**
 * @brief Single level of market depth
 * 
 * Represents one price level in the order book.
 * All exchanges provide 5 levels of depth (both bid and ask).
 */
struct DepthLevel {
    double price = 0.0;          // Price in rupees (already converted from paise for BSE)
    uint64_t quantity = 0;       // Total quantity at this price level
    uint32_t orders = 0;         // Number of orders (0 if not available)
    
    DepthLevel() = default;
    DepthLevel(double p, uint64_t q, uint32_t o = 0) 
        : price(p), quantity(q), orders(o) {}
};

/**
 * @brief UDP Broadcast Market Tick
 * 
 * This structure contains ONLY data received from UDP broadcast feeds.
 * It is separate from XTS::Tick which is used for WebSocket/REST API data.
 * 
 * Key Semantic Differences from XTS::Tick:
 * - `prevClose`: Previous day's closing price (not "close")
 * - `atp`: Average Traded Price (not generic "averagePrice")
 * - 5-level depth always present (not optional)
 * - Latency tracking fields for performance measurement
 * 
 * Supported Exchanges:
 * - NSE FO (segment=2): Messages 7200, 7201, 7202, 7208, etc.
 * - NSE CM (segment=1): Messages 7200, 7201, 7207, 18703, etc.
 * - BSE FO (segment=12): Messages 2020, 2021, 2015, 2012
 * - BSE CM (segment=11): Messages 2020, 2021, 2012
 */
struct MarketTick {
    // ========== IDENTIFICATION ==========
    
    ExchangeSegment exchangeSegment;  // 1=NSECM, 2=NSEFO, 11=BSECM, 12=BSEFO
    uint32_t token;                   // Exchange instrument token
    
    // ========== PRICE DATA ==========
    
    double ltp = 0.0;                 // Last Traded Price
    double open = 0.0;                // Day open price
    double high = 0.0;                // Day high price
    double low = 0.0;                 // Day low price
    double prevClose = 0.0;           // Previous day's closing price (NOT today's close)
    double atp = 0.0;                 // Average Traded Price (VWAP-like)
    
    // ========== VOLUME & TRADES ==========
    
    uint64_t volume = 0;              // Total traded volume (contracts/shares)
    uint64_t ltq = 0;                 // Last Trade Quantity
    uint64_t totalTrades = 0;         // Number of trades (if available)
    
    // ========== OPEN INTEREST (Derivatives Only) ==========
    
    int64_t openInterest = 0;         // Current open interest
    int64_t oiChange = 0;             // OI change from previous value
    int64_t oiDayHigh = 0;            // Day high OI
    int64_t oiDayLow = 0;             // Day low OI
    
    // ========== 5-LEVEL MARKET DEPTH ==========
    
    std::array<DepthLevel, 5> bids;   // 5 levels of bid depth (buy orders)
    std::array<DepthLevel, 5> asks;   // 5 levels of ask depth (sell orders)
    
    // Aggregated depth (sum of all 5 levels)
    uint64_t totalBidQty = 0;         // Total bid quantity across all levels
    uint64_t totalAskQty = 0;         // Total ask quantity across all levels
    
    // ========== LATENCY TRACKING ==========
    // All timestamps in microseconds since epoch
    
    uint64_t refNo = 0;               // Packet sequence number / reference
    uint64_t timestampUdpRecv = 0;    // When UDP packet received (baseline: 0µs)
    uint64_t timestampParsed = 0;     // When packet parsing completed
    uint64_t timestampEmitted = 0;    // When signal emitted from receiver
    uint64_t timestampFeedHandler = 0;// When FeedHandler processed
    uint64_t timestampModelUpdate = 0;// When data model updated
    uint64_t timestampViewUpdate = 0; // When UI view rendered
    
    // ========== METADATA ==========
    
    uint16_t messageType = 0;         // NSE: 7200, 7202, etc. | BSE: 2020, 2021, etc.
    uint32_t marketSeqNumber = 0;     // Market sequence number (BSE)
    
    UpdateType updateType = UpdateType::UNKNOWN;  // Type of update (trade, depth, etc.)
    uint32_t validFlags = 0;          // Bitmask of ValidFlags indicating which fields are valid
    
    // ========== CONSTRUCTORS ==========
    
    MarketTick() : exchangeSegment(ExchangeSegment::NSECM), token(0) {}
    
    MarketTick(ExchangeSegment seg, uint32_t tok) 
        : exchangeSegment(seg), token(tok) {}
    
    // ========== UTILITY METHODS ==========
    
    /**
     * @brief Check if this is a derivatives instrument (has valid OI)
     */
    bool isDerivative() const {
        return exchangeSegment == ExchangeSegment::NSEFO || 
               exchangeSegment == ExchangeSegment::BSEFO ||
               exchangeSegment == ExchangeSegment::NSECD ||
               exchangeSegment == ExchangeSegment::BSECD ||
               exchangeSegment == ExchangeSegment::MCXFO;
    }
    
    /**
     * @brief Get best bid price (Level 1)
     */
    double bestBid() const {
        return bids[0].price;
    }
    
    /**
     * @brief Get best ask price (Level 1)
     */
    double bestAsk() const {
        return asks[0].price;
    }
    
    /**
     * @brief Get bid-ask spread in rupees
     */
    double spread() const {
        return bestAsk() - bestBid();
    }
    
    /**
     * @brief Get bid-ask spread in basis points (1 bp = 0.01%)
     */
    double spreadBps() const {
        double mid = (bestBid() + bestAsk()) / 2.0;
        return mid > 0 ? (spread() / mid) * 10000.0 : 0.0;
    }
    
    /**
     * @brief Calculate total latency (UDP recv to view update) in microseconds
     */
    uint64_t totalLatency() const {
        if (timestampViewUpdate > 0 && timestampUdpRecv > 0) {
            return timestampViewUpdate - timestampUdpRecv;
        }
        return 0;
    }
    
    /**
     * @brief Calculate parse latency in microseconds
     */
    uint64_t parseLatency() const {
        if (timestampParsed > 0 && timestampUdpRecv > 0) {
            return timestampParsed - timestampUdpRecv;
        }
        return 0;
    }
};

/**
 * @brief Index data (NSE: 7207, 7203 | BSE: 2012)
 */
struct IndexTick {
    ExchangeSegment exchangeSegment;
    uint32_t token;                   // Index token
    char name[32];                    // Index name (e.g., "NIFTY 50", "SENSEX")
    
    double value = 0.0;               // Current index value
    double open = 0.0;
    double high = 0.0;
    double low = 0.0;
    double prevClose = 0.0;
    
    double change = 0.0;              // Absolute change
    double changePercent = 0.0;       // Percentage change
    
    uint64_t marketCap = 0;           // Market capitalization (if available)
    uint32_t numAdvances = 0;         // Number of advancing stocks
    uint32_t numDeclines = 0;         // Number of declining stocks
    uint32_t numUnchanged = 0;        // Number of unchanged stocks
    
    uint64_t timestampUdpRecv = 0;
    
    IndexTick() : exchangeSegment(ExchangeSegment::NSECM), token(0), name{0} {}
};

/**
 * @brief Market session state change (BSE: 2002)
 */
struct SessionStateTick {
    ExchangeSegment exchangeSegment;
    uint32_t sessionNumber = 0;
    uint16_t marketSegmentId = 0;
    SessionState state = SessionState::UNKNOWN;
    bool isStart = false;             // true=session start, false=session end
    
    uint64_t timestamp = 0;           // Exchange timestamp
    uint64_t timestampUdpRecv = 0;
    
    SessionStateTick() : exchangeSegment(ExchangeSegment::BSECM) {}
};

/**
 * @brief Circuit limit / price protection (NSE: 7220 | BSE: 2034)
 */
struct CircuitLimitTick {
    ExchangeSegment exchangeSegment;
    uint32_t token;
    
    double upperLimit = 0.0;          // Upper circuit limit price
    double lowerLimit = 0.0;          // Lower circuit limit price
    double upperExecutionBand = 0.0;  // Upper execution band (NSE)
    double lowerExecutionBand = 0.0;  // Lower execution band (NSE)
    
    bool isHalted = false;            // Trading halted due to circuit
    
    uint64_t timestampUdpRecv = 0;
    
    CircuitLimitTick() : exchangeSegment(ExchangeSegment::NSECM), token(0) {}
};

/**
 * @brief Implied Volatility tick (BSE: 2028)
 * Applicable for derivatives contracts only
 */
struct ImpliedVolatilityTick {
    ExchangeSegment exchangeSegment;
    uint32_t token;
    
    double impliedVolatility = 0.0;   // IV in percentage (e.g., 25.50 means 25.50%)
    
    uint64_t timestampUdpRecv = 0;
    uint64_t timestampEmitted = 0;
    
    ImpliedVolatilityTick() : exchangeSegment(ExchangeSegment::BSEFO), token(0) {}
    ImpliedVolatilityTick(ExchangeSegment seg, uint32_t tok) 
        : exchangeSegment(seg), token(tok) {}
};

} // namespace UDP

#include <QMetaType>
Q_DECLARE_METATYPE(UDP::MarketTick)
Q_DECLARE_METATYPE(UDP::IndexTick)
Q_DECLARE_METATYPE(UDP::CircuitLimitTick)

#endif // UDP_TYPES_H
