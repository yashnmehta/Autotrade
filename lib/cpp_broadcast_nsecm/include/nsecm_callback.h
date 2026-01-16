#ifndef NSECM_MARKET_DATA_CALLBACK_H
#define NSECM_MARKET_DATA_CALLBACK_H

#include <cstdint>
#include <functional>
#include <vector>
#include <string>
#include <mutex>
#include <atomic>

namespace nsecm {

// ============================================================================
// PARSED DATA STRUCTURES FOR CALLBACKS
// ============================================================================

// Touchline data (from 7200, 7208)
struct TouchlineData {
    uint32_t token;
    
    // === CONTRACT MASTER DATA (Static - initialized once) ===
    char symbol[32] = {0};          // Symbol name (e.g., RELIANCE, TCS)
    char displayName[64] = {0};     // Full display name
    char series[16] = {0};          // EQUITY, BE, BZ, etc.
    int32_t lotSize = 0;
    double tickSize = 0.0;
    double priceBandHigh = 0.0;     // Upper circuit
    double priceBandLow = 0.0;      // Lower circuit
    
    // === DYNAMIC MARKET DATA (Updated by UDP) ===
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

// System information (from 7206)
struct SystemInformationData {
    // Market status fields
    int16_t normalMarketStatus;
    int16_t oddlotMarketStatus;
    int16_t spotMarketStatus;
    int16_t auctionMarketStatus;
    int16_t callAuction1Status;
    int16_t callAuction2Status;
    
    // Market parameters
    int32_t marketIndex;
    int16_t defaultSettlementPeriodNormal;
    int16_t defaultSettlementPeriodSpot;
    int16_t defaultSettlementPeriodAuction;
    int16_t competitorPeriod;
    int16_t solicitorPeriod;
    
    // Risk parameters
    int16_t warningPercent;
    int16_t volumeFreezePercent;
    int16_t terminalIdleTime;
    
    // Trading parameters
    int32_t boardLotQuantity;
    int32_t tickSize;
    int16_t maximumGtcDays;
    int16_t disclosedQuantityPercentAllowed;
    
    // Bit flags
    bool booksMerged;
    bool minimumFillAllowed;
    bool aonAllowed;
    
    // Timestamp
    uint64_t timestampRecv = 0;
};

using SystemInformationCallback = std::function<void(const SystemInformationData&)>;

// Call Auction Order Cancellation details (from 7210)
struct OrderCancellationDetail {
    uint32_t token;
    int64_t buyOrdCxlCount;
    int64_t buyOrdCxlVol;
    int64_t sellOrdCxlCount;
    int64_t sellOrdCxlVol;
};

// Call Auction Order Cancellation update (from 7210)
struct CallAuctionOrderCxlData {
    int16_t noOfRecords;
    OrderCancellationDetail records[8];  // Max 8 securities
    uint64_t timestampRecv = 0;
};

using CallAuctionOrderCxlCallback = std::function<void(const CallAuctionOrderCxlData&)>;

// Market Open/Close/Pre-Open Messages (from 6511, 6521, 6531, 6571, 6583, 6584)
struct MarketOpenMessage {
    uint16_t txCode;            // Transaction code (6511, 6521, etc.)
    uint32_t timestamp;         // Log time
    std::string symbol;         // Symbol
    std::string series;         // Series
    int16_t marketType;         // 1=Normal, 2=Odd Lot, 3=Spot, 4=Auction, 5=Call auction1, 6=Call auction2
    std::string message;        // Broadcast message content
    uint64_t timestampRecv = 0; // Reception timestamp
};

using MarketOpenCallback = std::function<void(const MarketOpenMessage&)>;

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
    
    // Register callbacks (thread-safe)
    void registerTouchlineCallback(TouchlineCallback callback) {
        std::lock_guard<std::mutex> lock(callbackMutex);
        touchlineCallback = callback;
    }
    
    void registerMarketDepthCallback(MarketDepthCallback callback) {
        std::lock_guard<std::mutex> lock(callbackMutex);
        marketDepthCallback = callback;
    }
    
    void registerTickerCallback(TickerCallback callback) {
        std::lock_guard<std::mutex> lock(callbackMutex);
        tickerCallback = callback;
    }
    
    void registerMarketWatchCallback(MarketWatchCallback callback) {
        std::lock_guard<std::mutex> lock(callbackMutex);
        marketWatchCallback = callback;
    }

    void registerIndexCallback(IndexCallback callback) {
        std::lock_guard<std::mutex> lock(callbackMutex);
        indexCallback = callback;
    }

    void registerAdminCallback(AdminCallback callback) {
        std::lock_guard<std::mutex> lock(callbackMutex);
        adminCallback = callback;
    }

    void registerSystemInformationCallback(SystemInformationCallback callback) {
        std::lock_guard<std::mutex> lock(callbackMutex);
        systemInformationCallback = callback;
    }

    void registerCallAuctionOrderCxlCallback(CallAuctionOrderCxlCallback callback) {
        std::lock_guard<std::mutex> lock(callbackMutex);
        callAuctionOrderCxlCallback = callback;
    }

    void registerMarketOpenCallback(MarketOpenCallback callback) {
        std::lock_guard<std::mutex> lock(callbackMutex);
        marketOpenCallback = callback;
    }
    
    // Dispatch callbacks (called by parsers) - thread-safe
    void dispatchTouchline(const TouchlineData& data) {
        TouchlineCallback callback;
        {
            std::lock_guard<std::mutex> lock(callbackMutex);
            callback = touchlineCallback;
        }
        if (callback) {
            callback(data);
        }
    }
    
    void dispatchMarketDepth(const MarketDepthData& data) {
        MarketDepthCallback callback;
        {
            std::lock_guard<std::mutex> lock(callbackMutex);
            callback = marketDepthCallback;
        }
        if (callback) {
            callback(data);
        }
    }
    
    void dispatchTicker(const TickerData& data) {
        TickerCallback callback;
        {
            std::lock_guard<std::mutex> lock(callbackMutex);
            callback = tickerCallback;
        }
        if (callback) {
            callback(data);
        }
    }
    
    void dispatchMarketWatch(const MarketWatchData& data) {
        MarketWatchCallback callback;
        {
            std::lock_guard<std::mutex> lock(callbackMutex);
            callback = marketWatchCallback;
        }
        if (callback) {
            callback(data);
        }
    }

    void dispatchIndices(const IndicesUpdate& data) {
        IndexCallback callback;
        {
            std::lock_guard<std::mutex> lock(callbackMutex);
            callback = indexCallback;
        }
        if (callback) {
            callback(data);
        }
    }

    void dispatchAdmin(const AdminMessage& data) {
        AdminCallback callback;
        {
            std::lock_guard<std::mutex> lock(callbackMutex);
            callback = adminCallback;
        }
        if (callback) {
            callback(data);
        }
    }

    void dispatchSystemInformation(const SystemInformationData& data) {
        SystemInformationCallback callback;
        {
            std::lock_guard<std::mutex> lock(callbackMutex);
            callback = systemInformationCallback;
        }
        if (callback) {
            callback(data);
        }
    }

    void dispatchCallAuctionOrderCxl(const CallAuctionOrderCxlData& data) {
        CallAuctionOrderCxlCallback callback;
        {
            std::lock_guard<std::mutex> lock(callbackMutex);
            callback = callAuctionOrderCxlCallback;
        }
        if (callback) {
            callback(data);
        }
    }

    void dispatchMarketOpen(const MarketOpenMessage& data) {
        MarketOpenCallback callback;
        {
            std::lock_guard<std::mutex> lock(callbackMutex);
            callback = marketOpenCallback;
        }
        if (callback) {
            callback(data);
        }
    }
    
private:
    MarketDataCallbackRegistry() = default;
    ~MarketDataCallbackRegistry() = default;
    MarketDataCallbackRegistry(const MarketDataCallbackRegistry&) = delete;
    MarketDataCallbackRegistry& operator=(const MarketDataCallbackRegistry&) = delete;
    
    // Thread-safe callback storage
    mutable std::mutex callbackMutex;
    TouchlineCallback touchlineCallback;
    MarketDepthCallback marketDepthCallback;
    TickerCallback tickerCallback;
    MarketWatchCallback marketWatchCallback;
    IndexCallback indexCallback;
    AdminCallback adminCallback;
    SystemInformationCallback systemInformationCallback;
    CallAuctionOrderCxlCallback callAuctionOrderCxlCallback;
    MarketOpenCallback marketOpenCallback;
};

} // namespace nsecm

#endif // NSECM_MARKET_DATA_CALLBACK_H
