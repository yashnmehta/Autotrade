#ifndef NSEFO_MARKET_DATA_CALLBACK_H
#define NSEFO_MARKET_DATA_CALLBACK_H

#include <cstdint>
#include <functional>
#include <vector>

namespace nsefo {

// ============================================================================
// PARSED DATA STRUCTURES FOR CALLBACKS
// ============================================================================

// Touchline data (from 7200, 7208)
struct TouchlineData {
    uint32_t token;
    
    // === CONTRACT MASTER DATA (Static - initialized once) ===
    char symbol[32] = {0};          // Symbol name (e.g., NIFTY, BANKNIFTY)
    char displayName[64] = {0};     // Full name (e.g., NIFTY 30JAN2026 CE 24000)
    int32_t lotSize = 0;
    double strikePrice = 0.0;
    char optionType[3] = {0};       // CE/PE/XX
    char expiryDate[16] = {0};      // DDMMMYYYY
    int64_t assetToken = 0;
    int32_t instrumentType = 0;     // 1=Future, 2=Option
    double tickSize = 0.0;
    
    // === DYNAMIC MARKET DATA (Updated by UDP) ===
    double ltp;                 // Last Traded Price
    double open;
    double high;
    double low;
    double close;
    uint32_t volume;            // 32-bit for FO (usually) but can be upgraded if needed
    uint32_t lastTradeQty;
    uint32_t lastTradeTime;
    double avgPrice;
    char netChangeIndicator;    // '+' or '-'
    double netChange;
    uint16_t tradingStatus;
    uint16_t bookType;
    
    // Latency tracking
    uint64_t refNo = 0;
    int64_t timestampRecv = 0;
    int64_t timestampParsed = 0;
};

// Market depth level info
struct DepthLevel {
    uint32_t quantity;
    double price;
    uint16_t orders;
};

// Market depth data (from 7200, 7208)
struct MarketDepthData {
    uint32_t token;
    DepthLevel bids[5];         // Fixed-size array (Zero-Copy)
    DepthLevel asks[5];         // Fixed-size array (Zero-Copy)
    double totalBuyQty;
    double totalSellQty;
    
    // Latency tracking
    uint64_t refNo = 0;
    int64_t timestampRecv = 0;
    int64_t timestampParsed = 0;
};

// Ticker data (from 7202)
struct TickerData {
    uint32_t token;
    double fillPrice;
    uint32_t fillVolume;
    int64_t openInterest;
    int64_t dayHiOI;
    int64_t dayLoOI;
    uint16_t marketType;
    
    // Latency tracking
    uint64_t refNo = 0;
    int64_t timestampRecv = 0;
    int64_t timestampParsed = 0;
};

// Market watch level (Normal, Stop Loss, Auction)
struct MarketLevel {
    uint32_t buyVolume;
    double buyPrice;
    uint32_t sellVolume;
    double sellPrice;
};

// Market watch data (from 7201)
struct MarketWatchData {
    uint32_t token;
    uint32_t openInterest;
    MarketLevel levels[3];      // Fixed-size array (3 levels)
};

// Index data (from 7207)
struct IndexData {
    char name[21];              // Index name (null-terminated)
    double value;               // Current index value
    double high;                // Day high
    double low;                 // Day low
    double open;                // Opening value
    double close;               // Previous close
    double percentChange;       // % change from previous close
    double yearlyHigh;          // 52-week high
    double yearlyLow;           // 52-week low
    uint32_t noOfUpmoves;       // Number of advancing stocks
    uint32_t noOfDownmoves;     // Number of declining stocks
    double marketCap;           // Total market capitalization
    char netChangeIndicator;    // '+' or '-'
    
    // Latency tracking
    int64_t timestampRecv = 0;
    int64_t timestampParsed = 0;
};

// Industry index data (from 7203)
struct IndustryIndexData {
    char name[16];              // Industry name (null-terminated)
    double value;               // Current index value
    
    // Latency tracking
    int64_t timestampRecv = 0;
    int64_t timestampParsed = 0;
};

// Circuit limit data (from 7220)
struct CircuitLimitData {
    uint32_t token;
    double upperLimit;          // Upper execution band (price protection)
    double lowerLimit;          // Lower execution band (price protection)
    
    // Latency tracking
    int64_t timestampRecv = 0;
    int64_t timestampParsed = 0;
};

// ============================================================================
// CALLBACK FUNCTION TYPES
// ============================================================================

// Callback for touchline updates (7200, 7208)
using TouchlineCallback = std::function<void(const TouchlineData&)>;

// Callback for market depth updates (7200, 7208)
using MarketDepthCallback = std::function<void(const MarketDepthData&)>;

// Callback for ticker updates (7202, 17202)
using TickerCallback = std::function<void(const TickerData&)>;

// Callback for market watch updates (7201, 17201)
using MarketWatchCallback = std::function<void(const MarketWatchData&)>;

// Callback for index updates (7207)
using IndexCallback = std::function<void(const IndexData&)>;

// Callback for industry index updates (7203)
using IndustryIndexCallback = std::function<void(const IndustryIndexData&)>;

// Callback for circuit limit updates (7220)
using CircuitLimitCallback = std::function<void(const CircuitLimitData&)>;

// ============================================================================
// CALLBACK REGISTRY
// ============================================================================

// Singleton class to register and dispatch callbacks
class MarketDataCallbackRegistry {
public:
    static MarketDataCallbackRegistry& instance() {
        static MarketDataCallbackRegistry instance;
        return instance;
    }
    
    // Register callbacks
    void registerTouchlineCallback(TouchlineCallback callback) {
        touchlineCallback = callback;
    }
    
    void registerMarketDepthCallback(MarketDepthCallback callback) {
        marketDepthCallback = callback;
    }
    
    void registerTickerCallback(TickerCallback callback) {
        tickerCallback = callback;
    }
    
    void registerMarketWatchCallback(MarketWatchCallback callback) {
        marketWatchCallback = callback;
    }
    
    void registerIndexCallback(IndexCallback callback) {
        indexCallback = callback;
    }
    
    void registerIndustryIndexCallback(IndustryIndexCallback callback) {
        industryIndexCallback = callback;
    }
    
    void registerCircuitLimitCallback(CircuitLimitCallback callback) {
        circuitLimitCallback = callback;
    }
    
    // Dispatch callbacks (called by parsers)
    void dispatchTouchline(const TouchlineData& data) {
        if (touchlineCallback) {
            touchlineCallback(data);
        }
    }
    
    void dispatchMarketDepth(const MarketDepthData& data) {
        if (marketDepthCallback) {
            marketDepthCallback(data);
        }
    }
    
    void dispatchTicker(const TickerData& data) {
        if (tickerCallback) {
            tickerCallback(data);
        }
    }
    
    void dispatchMarketWatch(const MarketWatchData& data) {
        if (marketWatchCallback) {
            marketWatchCallback(data);
        }
    }
    
    void dispatchIndex(const IndexData& data) {
        if (indexCallback) {
            indexCallback(data);
        }
    }
    
    void dispatchIndustryIndex(const IndustryIndexData& data) {
        if (industryIndexCallback) {
            industryIndexCallback(data);
        }
    }
    
    void dispatchCircuitLimit(const CircuitLimitData& data) {
        if (circuitLimitCallback) {
            circuitLimitCallback(data);
        }
    }
    
private:
    MarketDataCallbackRegistry() = default;
    ~MarketDataCallbackRegistry() = default;
    MarketDataCallbackRegistry(const MarketDataCallbackRegistry&) = delete;
    MarketDataCallbackRegistry& operator=(const MarketDataCallbackRegistry&) = delete;
    
    TouchlineCallback touchlineCallback;
    MarketDepthCallback marketDepthCallback;
    TickerCallback tickerCallback;
    MarketWatchCallback marketWatchCallback;
    IndexCallback indexCallback;
    IndustryIndexCallback industryIndexCallback;
    CircuitLimitCallback circuitLimitCallback;
};

} // namespace nsefo

#endif // NSEFO_MARKET_DATA_CALLBACK_H
