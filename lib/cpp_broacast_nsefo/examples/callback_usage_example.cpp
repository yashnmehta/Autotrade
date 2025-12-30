// Example: How to use the NSE Broadcast Parser Callback System
// 
// This file demonstrates how to integrate the callback-based parsers
// into a Qt application for the trading terminal.

#include "nsefo_callback.h"
#include <iostream>

// ============================================================================
// EXAMPLE CALLBACK IMPLEMENTATIONS
// ============================================================================

// Callback for touchline updates (7200, 7208)
void onTouchlineUpdate(const TouchlineData& data) {
    std::cout << "[TOUCHLINE] Token: " << data.token
              << " | LTP: " << data.ltp
              << " | Volume: " << data.volume
              << " | Change: " << data.netChangeIndicator << data.netChange
              << std::endl;
    
    // In Qt application, you would emit a signal here:
    // emit marketDataAggregator->touchlineUpdated(data.token, data);
}

// Callback for market depth updates (7200, 7208)
void onMarketDepthUpdate(const MarketDepthData& data) {
    std::cout << "[DEPTH] Token: " << data.token
              << " | Best Bid: " << data.bids[0].price << " (" << data.bids[0].quantity << ")"
              << " | Best Ask: " << data.asks[0].price << " (" << data.asks[0].quantity << ")"
              << std::endl;
    
    // In Qt application:
    // emit marketDataAggregator->depthUpdated(data.token, data);
}

// Callback for ticker updates (7202, 17202)
void onTickerUpdate(const TickerData& data) {
    std::cout << "[TICKER] Token: " << data.token
              << " | Fill Price: " << data.fillPrice
              << " | Volume: " << data.fillVolume
              << " | OI: " << data.openInterest
              << std::endl;
    
    // In Qt application:
    // emit marketDataAggregator->tickerUpdated(data.token, data);
}

// Callback for market watch updates (7201, 17201)
void onMarketWatchUpdate(const MarketWatchData& data) {
    std::cout << "[MW] Token: " << data.token
              << " | OI: " << data.openInterest
              << " | Levels: " << data.levels.size()
              << std::endl;
    
    // In Qt application:
    // emit marketDataAggregator->marketWatchUpdated(data.token, data);
}

// ============================================================================
// SETUP FUNCTION - Call this during application initialization
// ============================================================================

void setupMarketDataCallbacks() {
    // Register all callbacks
    auto& registry = MarketDataCallbackRegistry::instance();
    
    registry.registerTouchlineCallback(onTouchlineUpdate);
    registry.registerMarketDepthCallback(onMarketDepthUpdate);
    registry.registerTickerCallback(onTickerUpdate);
    registry.registerMarketWatchCallback(onMarketWatchUpdate);
    
    std::cout << "Market data callbacks registered successfully" << std::endl;
}

// ============================================================================
// INTEGRATION WITH QT APPLICATION
// ============================================================================

/*

// In your MarketDataAggregator class (Qt-based):

class MarketDataAggregator : public QObject {
    Q_OBJECT
    
public:
    MarketDataAggregator(QObject* parent = nullptr) : QObject(parent) {
        setupCallbacks();
    }
    
signals:
    void touchlineUpdated(int32_t token, const TouchlineData& data);
    void depthUpdated(int32_t token, const MarketDepthData& data);
    void tickerUpdated(int32_t token, const TickerData& data);
    void marketWatchUpdated(int32_t token, const MarketWatchData& data);
    
private:
    void setupCallbacks() {
        auto& registry = MarketDataCallbackRegistry::instance();
        
        // Use lambda to capture 'this' and emit signals
        registry.registerTouchlineCallback([this](const TouchlineData& data) {
            emit touchlineUpdated(data.token, data);
        });
        
        registry.registerMarketDepthCallback([this](const MarketDepthData& data) {
            emit depthUpdated(data.token, data);
        });
        
        registry.registerTickerCallback([this](const TickerData& data) {
            emit tickerUpdated(data.token, data);
        });
        
        registry.registerMarketWatchCallback([this](const MarketWatchData& data) {
            emit marketWatchUpdated(data.token, data);
        });
    }
};

// In your MarketWatch widget:

class MarketWatch : public QWidget {
    Q_OBJECT
    
public:
    MarketWatch(MarketDataAggregator* aggregator, QWidget* parent = nullptr) 
        : QWidget(parent), m_aggregator(aggregator) {
        // Connect to aggregator signals
        connect(aggregator, &MarketDataAggregator::touchlineUpdated,
                this, &MarketWatch::onTouchlineUpdate);
        connect(aggregator, &MarketDataAggregator::depthUpdated,
                this, &MarketWatch::onDepthUpdate);
    }
    
private slots:
    void onTouchlineUpdate(int32_t token, const TouchlineData& data) {
        // Update UI for this token
        if (m_subscribedTokens.contains(token)) {
            updateRow(token, data);
        }
    }
    
    void onDepthUpdate(int32_t token, const MarketDepthData& data) {
        // Update depth display if this token is selected
        if (m_selectedToken == token) {
            updateDepthWidget(data);
        }
    }
    
private:
    MarketDataAggregator* m_aggregator;
    QSet<int32_t> m_subscribedTokens;
    int32_t m_selectedToken = -1;
};

*/

// ============================================================================
// THREADING NOTES
// ============================================================================

/*

The callback system is designed to be thread-safe when used with Qt:

1. UDP Receiver Thread (worker) calls parsers
2. Parsers dispatch callbacks (happens in worker thread)
3. Callbacks should emit Qt signals using Qt::QueuedConnection
4. Qt automatically marshals signals to main thread
5. UI updates happen in main thread

Example threading model:

[Worker Thread 1] -> UDP Receiver NSEFO Touch -> Parser 7208 -> Callback -> Emit Signal
                                                                                |
[Worker Thread 2] -> UDP Receiver NSEFO Depth -> Parser 7200 -> Callback -> Emit Signal
                                                                                |
[Worker Thread 3] -> UDP Receiver NSECM Touch -> Parser 7208 -> Callback -> Emit Signal
                                                                                |
[Worker Thread 4] -> UDP Receiver NSECM Depth -> Parser 7200 -> Callback -> Emit Signal
                                                                                |
                                                                                v
                                                        [Main Thread] <- Qt Signal Queue
                                                                                |
                                                                                v
                                                                        MarketDataAggregator
                                                                                |
                                                                                v
                                                                        UI Widgets (MarketWatch, etc.)

*/
