#include "data/MarketDataAggregator.h"
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

void MarketDataAggregator::onTouchlineCallbackFO(const nsefo::TouchlineData& data) {
    instance().updateTouchlineCacheFO(data);
}

void MarketDataAggregator::onDepthCallbackFO(const nsefo::MarketDepthData& data) {
    instance().updateDepthCacheFO(data);
}

void MarketDataAggregator::onTickerCallbackFO(const nsefo::TickerData& data) {
    instance().updateTickerCacheFO(data);
}

// =============================================================================
// STATIC CALLBACK FUNCTIONS (CM)
// =============================================================================

void MarketDataAggregator::onTouchlineCallbackCM(const nsecm::TouchlineData& data) {
    instance().updateTouchlineCacheCM(data);
}

void MarketDataAggregator::onDepthCallbackCM(const nsecm::MarketDepthData& data) {
    instance().updateDepthCacheCM(data);
}

void MarketDataAggregator::onTickerCallbackCM(const nsecm::TickerData& data) {
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
