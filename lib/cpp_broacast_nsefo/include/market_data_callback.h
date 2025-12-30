#ifndef MARKET_DATA_CALLBACK_H
#define MARKET_DATA_CALLBACK_H

#include <cstdint>
#include <functional>
#include <vector>

// ============================================================================
// PARSED DATA STRUCTURES FOR CALLBACKS
// ============================================================================

// Touchline data (from 7200, 7208)
struct TouchlineData {
    int32_t token;
    double ltp;              // Last Traded Price
    double open;
    double high;
    double low;
    double close;
    uint32_t volume;
    uint32_t lastTradeQty;
    uint32_t lastTradeTime;
    double avgPrice;
    char netChangeIndicator; // '+' or '-'
    double netChange;
    uint16_t tradingStatus;
    uint16_t bookType;
    
    // Latency tracking fields
    uint64_t refNo = 0;           // Reference number from packet
    int64_t timestampRecv = 0;    // When UDP packet received (µs)
    int64_t timestampParsed = 0;  // When packet parsed (µs)
};

// Market depth info (bid/ask levels)
struct DepthLevel {
    uint32_t quantity;
    double price;
    uint16_t orders;
};

// Market depth data (from 7200, 7208)
struct MarketDepthData {
    int32_t token;
    std::vector<DepthLevel> bids;  // 5 levels
    std::vector<DepthLevel> asks;  // 5 levels
    double totalBuyQty;
    double totalSellQty;
    
    // Latency tracking fields
    uint64_t refNo = 0;           // Reference number from packet
    int64_t timestampRecv = 0;    // When UDP packet received (µs)
    int64_t timestampParsed = 0;  // When packet parsed (µs)
};

// Ticker data (from 7202)
struct TickerData {
    int32_t token;
    double fillPrice;
    uint32_t fillVolume;
    int64_t openInterest;
    int64_t dayHiOI;
    int64_t dayLoOI;
    uint16_t marketType;
    
    // Latency tracking fields
    uint64_t refNo = 0;           // Reference number from packet
    int64_t timestampRecv = 0;    // When UDP packet received (µs)
    int64_t timestampParsed = 0;  // When packet parsed (µs)
};

// Market watch data (from 7201)
struct MarketWatchData {
    int32_t token;
    int64_t openInterest;
    
    // Market wise info (3 levels - Normal, Stop Loss, Auction)
    struct MarketLevel {
        uint32_t buyVolume;
        double buyPrice;
        uint32_t sellVolume;
        double sellPrice;
    };
    
    std::vector<MarketLevel> levels;  // 3 levels
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
    
private:
    MarketDataCallbackRegistry() = default;
    ~MarketDataCallbackRegistry() = default;
    MarketDataCallbackRegistry(const MarketDataCallbackRegistry&) = delete;
    MarketDataCallbackRegistry& operator=(const MarketDataCallbackRegistry&) = delete;
    
    TouchlineCallback touchlineCallback;
    MarketDepthCallback marketDepthCallback;
    TickerCallback tickerCallback;
    MarketWatchCallback marketWatchCallback;
};

#endif // MARKET_DATA_CALLBACK_H
