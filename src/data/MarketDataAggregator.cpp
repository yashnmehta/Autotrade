#include "data/MarketDataAggregator.h"
#include "nsefo_price_store.h"
#include "nsecm_price_store.h"
#include <QMetaObject>
#include <QDebug>
#include <iostream>

// Singleton instance access
MarketDataAggregator& MarketDataAggregator::instance() {
    static MarketDataAggregator instance;
    return instance;
}

MarketDataAggregator::MarketDataAggregator() {
    qDebug() << "[MarketDataAggregator] Initializing...";
    
    // Register FO Callbacks
    auto& foRegistry = nsefo::MarketDataCallbackRegistry::instance();
    foRegistry.registerTouchlineCallback(onTouchlineCallbackFO);
    foRegistry.registerMarketDepthCallback(onDepthCallbackFO);
    foRegistry.registerTickerCallback(onTickerCallbackFO);
    
    // Register CM Callbacks
    auto& cmRegistry = nsecm::MarketDataCallbackRegistry::instance();
    cmRegistry.registerTouchlineCallback(onTouchlineCallbackCM);
    cmRegistry.registerMarketDepthCallback(onDepthCallbackCM);
    cmRegistry.registerTickerCallback(onTickerCallbackCM);
    
    qDebug() << "[MarketDataAggregator] Callbacks registered";
}

MarketDataAggregator::~MarketDataAggregator() {
    stop();
}

void MarketDataAggregator::start() {
    if (m_running.exchange(true)) {
        qDebug() << "[MarketDataAggregator] Already running";
        return;
    }
    qDebug() << "[MarketDataAggregator] Started (Data-only mode, actual receivers managed by MainWindow)";
}

void MarketDataAggregator::stop() {
    if (!m_running.exchange(false)) {
        return;
    }
    qDebug() << "[MarketDataAggregator] Stopped";
}

// =============================================================================
// STATIC CALLBACK FUNCTIONS (FO)
// =============================================================================

// =============================================================================
// STATIC CALLBACK FUNCTIONS (FO) - UPDATED FOR UNIFIED STORE
// =============================================================================

void MarketDataAggregator::onTouchlineCallbackFO(int32_t token) {
    const auto* u = nsefo::g_nseFoPriceStore.getUnifiedState(token);
    if (!u) return;
    
    // Map UnifiedState to Legacy TouchlineData
    nsefo::TouchlineData data;
    data.token = u->token;
    data.ltp = u->ltp;
    data.open = u->open;
    data.high = u->high;
    data.low = u->low;
    data.close = u->close;
    data.volume = u->volume;
    data.lastTradeQty = u->lastTradeQty;
    data.lastTradeTime = u->lastTradeTime;
    data.avgPrice = u->avgPrice;
    data.netChangeIndicator = u->netChangeIndicator;
    data.netChange = u->netChange;
    data.tradingStatus = u->tradingStatus;
    data.bookType = u->bookType;
    data.timestampRecv = u->lastPacketTimestamp;
    
    // Static fields (already in UnifiedState, but Aggregator copies them)
    std::strncpy(data.symbol, u->symbol, sizeof(data.symbol));
    std::strncpy(data.displayName, u->displayName, sizeof(data.displayName));
    
    instance().updateTouchlineCacheFO(data);
}

void MarketDataAggregator::onDepthCallbackFO(int32_t token) {
    const auto* u = nsefo::g_nseFoPriceStore.getUnifiedState(token);
    if (!u) return;

    nsefo::MarketDepthData data;
    data.token = u->token;
    data.totalBuyQty = u->totalBuyQty;
    data.totalSellQty = u->totalSellQty;
    data.timestampRecv = u->lastPacketTimestamp;
    
    std::memcpy(data.bids, u->bids, sizeof(data.bids));
    std::memcpy(data.asks, u->asks, sizeof(data.asks));

    instance().updateDepthCacheFO(data);
}

void MarketDataAggregator::onTickerCallbackFO(int32_t token) {
    const auto* u = nsefo::g_nseFoPriceStore.getUnifiedState(token);
    if (!u) return;

    nsefo::TickerData data;
    data.token = u->token;
    data.openInterest = u->openInterest;
    data.timestampRecv = u->lastPacketTimestamp;
    // Note: UnifiedState doesn't store fillPrice/fillVolume from last ticker packet specifically unless fused.
    // TickerData usually carries "This trade" info. UnifiedState carries "Last Trade" info (ltp, lastTradeQty).
    // So we can map LTP -> fillPrice?
    data.fillPrice = u->ltp;
    data.fillVolume = u->lastTradeQty; 
    
    instance().updateTickerCacheFO(data);
}

// =============================================================================
// STATIC CALLBACK FUNCTIONS (CM)
// =============================================================================

void MarketDataAggregator::onTouchlineCallbackCM(int32_t token) {
    const auto* u = nsecm::g_nseCmPriceStore.getUnifiedState(token);
    if (!u) return;

    nsecm::TouchlineData data;
    data.token = u->token;
    data.ltp = u->ltp;
    data.open = u->open;
    data.high = u->high;
    data.low = u->low;
    data.close = u->close;
    data.volume = u->volume;
    data.lastTradeQty = u->lastTradeQty;
    data.lastTradeTime = u->lastTradeTime;
    data.avgPrice = u->avgPrice;
    data.netChangeIndicator = u->netChangeIndicator;
    data.netChange = u->netChange;
    data.tradingStatus = u->tradingStatus;
    data.bookType = u->bookType;
    data.timestampRecv = u->lastPacketTimestamp;
    
    // Copy static fields if available in UnifiedState (assuming they are populated)
    // CM UnifiedState has symbol/series
    // TouchlineData (Legacy) doesn't strictly have them, but let's check
    // nsecm_callback.h doesn't show symbol/res in TouchlineData (Step 1335)
    // So we just update fields present in TouchlineData
    
    instance().updateTouchlineCacheCM(data);
}

void MarketDataAggregator::onDepthCallbackCM(int32_t token) {
    const auto* u = nsecm::g_nseCmPriceStore.getUnifiedState(token);
    if (!u) return;

    nsecm::MarketDepthData data;
    data.token = u->token;
    data.totalBuyQty = u->totalBuyQty;
    data.totalSellQty = u->totalSellQty;
    data.timestampRecv = u->lastPacketTimestamp;
    
    std::memcpy(data.bids, u->bids, sizeof(data.bids));
    std::memcpy(data.asks, u->asks, sizeof(data.asks));

    instance().updateDepthCacheCM(data);
}

void MarketDataAggregator::onTickerCallbackCM(int32_t token) {
    const auto* u = nsecm::g_nseCmPriceStore.getUnifiedState(token);
    if (!u) return;

    nsecm::TickerData data;
    data.token = u->token;
    data.fillPrice = u->ltp; // Fast Ticker mapping
    data.fillVolume = u->lastTradeQty;
    // TickerData has marketType, marketIndexValue which might not be in UnifiedState dynamic part always
    // But for 18703, they are. UnifiedState UpdateTicker didn't store marketType.
    // That's a minor loss of data for the Legacy View but OK for Distributed Cache focus.
    
    instance().updateTickerCacheCM(data);
}

// =============================================================================
// THREAD-SAFE CACHE UPDATE METHODS (FO)
// =============================================================================

void MarketDataAggregator::updateTouchlineCacheFO(const nsefo::TouchlineData& data) {
    {
        std::lock_guard<std::mutex> lock(m_cacheMutex);
        m_touchlineCacheFO[data.token] = data;
    }
    QMetaObject::invokeMethod(this, [this, data]() {
        emit touchlineUpdatedFO(data.token, data);
    }, Qt::QueuedConnection);
}

void MarketDataAggregator::updateDepthCacheFO(const nsefo::MarketDepthData& data) {
    {
        std::lock_guard<std::mutex> lock(m_cacheMutex);
        m_depthCacheFO[data.token] = data;
    }
    QMetaObject::invokeMethod(this, [this, data]() {
        emit depthUpdatedFO(data.token, data);
    }, Qt::QueuedConnection);
}

void MarketDataAggregator::updateTickerCacheFO(const nsefo::TickerData& data) {
    {
        std::lock_guard<std::mutex> lock(m_cacheMutex);
        m_tickerCacheFO[data.token] = data;
    }
    QMetaObject::invokeMethod(this, [this, data]() {
        emit tickerUpdatedFO(data.token, data);
    }, Qt::QueuedConnection);
}

// =============================================================================
// THREAD-SAFE CACHE UPDATE METHODS (CM)
// =============================================================================

void MarketDataAggregator::updateTouchlineCacheCM(const nsecm::TouchlineData& data) {
    {
        std::lock_guard<std::mutex> lock(m_cacheMutex);
        m_touchlineCacheCM[data.token] = data;
    }
    QMetaObject::invokeMethod(this, [this, data]() {
        emit touchlineUpdatedCM(data.token, data);
    }, Qt::QueuedConnection);
}

void MarketDataAggregator::updateDepthCacheCM(const nsecm::MarketDepthData& data) {
    {
        std::lock_guard<std::mutex> lock(m_cacheMutex);
        m_depthCacheCM[data.token] = data;
    }
    QMetaObject::invokeMethod(this, [this, data]() {
        emit depthUpdatedCM(data.token, data);
    }, Qt::QueuedConnection);
}

void MarketDataAggregator::updateTickerCacheCM(const nsecm::TickerData& data) {
    {
        std::lock_guard<std::mutex> lock(m_cacheMutex);
        m_tickerCacheCM[data.token] = data;
    }
    QMetaObject::invokeMethod(this, [this, data]() {
        emit tickerUpdatedCM(data.token, data);
    }, Qt::QueuedConnection);
}

// =============================================================================
// DATA ACCESS METHODS
// =============================================================================

nsefo::TouchlineData MarketDataAggregator::getTouchlineFO(int32_t token) {
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    auto it = m_touchlineCacheFO.find(token);
    if (it != m_touchlineCacheFO.end()) return it->second;
    return nsefo::TouchlineData{};
}

nsefo::MarketDepthData MarketDataAggregator::getDepthFO(int32_t token) {
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    auto it = m_depthCacheFO.find(token);
    if (it != m_depthCacheFO.end()) return it->second;
    return nsefo::MarketDepthData{};
}

nsefo::TickerData MarketDataAggregator::getTickerFO(int32_t token) {
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    auto it = m_tickerCacheFO.find(token);
    if (it != m_tickerCacheFO.end()) return it->second;
    return nsefo::TickerData{};
}

nsecm::TouchlineData MarketDataAggregator::getTouchlineCM(int32_t token) {
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    auto it = m_touchlineCacheCM.find(token);
    if (it != m_touchlineCacheCM.end()) return it->second;
    return nsecm::TouchlineData{};
}

nsecm::MarketDepthData MarketDataAggregator::getDepthCM(int32_t token) {
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    auto it = m_depthCacheCM.find(token);
    if (it != m_depthCacheCM.end()) return it->second;
    return nsecm::MarketDepthData{};
}

nsecm::TickerData MarketDataAggregator::getTickerCM(int32_t token) {
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    auto it = m_tickerCacheCM.find(token);
    if (it != m_tickerCacheCM.end()) return it->second;
    return nsecm::TickerData{};
}
