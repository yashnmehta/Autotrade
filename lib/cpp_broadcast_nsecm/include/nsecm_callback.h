#ifndef NSECM_MARKET_DATA_CALLBACK_H
#define NSECM_MARKET_DATA_CALLBACK_H

#include <cstdint>
#include <functional>
#include <vector>
#include <string>

namespace nsecm {

// ============================================================================
// PARSED DATA STRUCTURES FOR CALLBACKS
// ============================================================================

// Touchline data (from 7200, 7208)
struct TouchlineData {
    uint32_t token;
    double ltp;                 // Last Traded Price
    double open;
    double high;
    double low;
    double close;
    uint64_t volume;            // 64-bit for CM
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
    uint64_t quantity;          // 64-bit for CM
    double price;
    uint16_t orders;
};

// Market depth data (from 7200, 7208)
struct MarketDepthData {
    uint32_t token;
    DepthLevel bids[5];         // Fixed-size array (Zero-Copy)
    DepthLevel asks[5];         // Fixed-size array (Zero-Copy)
    uint64_t totalBuyQty;       // 64-bit for CM
    uint64_t totalSellQty;      // 64-bit for CM
    
    // Latency tracking
    uint64_t refNo = 0;
    int64_t timestampRecv = 0;
    int64_t timestampParsed = 0;
};

// Ticker data (from 18703)
struct TickerData {
    uint32_t token;
    double fillPrice;
    uint64_t fillVolume;        // 64-bit for CM
    uint64_t openInterest;      // FO only
    double marketIndexValue;    // For CM 18703
    uint16_t marketType;
    
    // Latency tracking
    uint64_t refNo = 0;
    int64_t timestampRecv = 0;
    int64_t timestampParsed = 0;
};

// Market watch level (Normal, Stop Loss, Auction)
struct MarketLevel {
    uint64_t buyVolume;
    double buyPrice;
    uint64_t sellVolume;
    double sellPrice;
    double ltp;
    uint32_t lastTradeTime;
};

// Market watch data (from 7201)
struct MarketWatchData {
    uint32_t token;
    uint64_t openInterest;      // FO only
    MarketLevel levels[3];      // Fixed-size array (3 levels)
};

// ============================================================================
// CALLBACK FUNCTION TYPES
// ============================================================================

// Callback for touchline updates (7200, 7208)
using TouchlineCallback = std::function<void(const TouchlineData&)>;

// Callback for market depth updates (7200, 7208)
using MarketDepthCallback = std::function<void(const MarketDepthData&)>;

// Callback for ticker updates (7202, 17202, 18703)
using TickerCallback = std::function<void(const TickerData&)>;

// Callback for market watch updates (7201, 17201)
using MarketWatchCallback = std::function<void(const MarketWatchData&)>;

// Admin message (text)
struct AdminMessage {
    uint32_t token;
    uint32_t timestamp;
    std::string message;
    std::string actionCode;
};

using AdminCallback = std::function<void(const AdminMessage&)>;

// Index item (from 7207)
struct IndexData {
    char name[21];
    double value;
    double high;
    double low;
    double open;
    double close;
    double percentChange;
    double yearlyHigh;
    double yearlyLow;
    uint32_t upMoves;
    uint32_t downMoves;
    double marketCap;
    char netChangeIndicator;
};

// Multiple Indices Update (from 7207, 7203)
struct IndicesUpdate {
    IndexData indices[28];      // Max records is 28
    uint16_t numRecords;
};

using IndexCallback = std::function<void(const IndicesUpdate&)>;

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

    void registerAdminCallback(AdminCallback callback) {
        adminCallback = callback;
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

    void dispatchIndices(const IndicesUpdate& data) {
        if (indexCallback) {
            indexCallback(data);
        }
    }

    void dispatchAdmin(const AdminMessage& data) {
        if (adminCallback) {
            adminCallback(data);
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
    AdminCallback adminCallback;
};

} // namespace nsecm

#endif // NSECM_MARKET_DATA_CALLBACK_H
