#include "data/MarketDataAggregator.h"
#include "multicast_receiver.h"
#include "market_data_callback.h"
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
    
    // Register static callbacks with the global callback registry
    auto& registry = MarketDataCallbackRegistry::instance();
    registry.registerTouchlineCallback(onTouchlineCallback);
    registry.registerMarketDepthCallback(onDepthCallback);
    registry.registerTickerCallback(onTickerCallback);
    registry.registerMarketWatchCallback(onMarketWatchCallback);
    
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
    
    qDebug() << "[MarketDataAggregator] Starting broadcast receivers...";
    
    try {
        // TODO: Load from config file - for now use hardcoded test values
        // NSE FO (Futures & Options)
        std::string nseFOIp = "239.255.1.1";
        int nseFOPort = 54321;
        std::string nseFOInterface = "";  // Empty = auto-select
        
        // NSE CM (Cash Market)
        std::string nseCMIp = "239.255.1.2";
        int nseCMPort = 54322;
        std::string nseCMInterface = "";
        
        // BSE FO (Futures & Options)
        std::string bseFOIp = "239.255.2.1";
        int bseFOPort = 54323;
        std::string bseFOInterface = "";
        
        // BSE CM (Cash Market)
        std::string bseCMIp = "239.255.2.2";
        int bseCMPort = 54324;
        std::string bseCMInterface = "";
        
        // Create 4 native C++ receivers
        m_nseFOReceiver = std::make_unique<MulticastReceiver>(nseFOIp, nseFOPort, nseFOInterface);
        m_nseCMReceiver = std::make_unique<MulticastReceiver>(nseCMIp, nseCMPort, nseCMInterface);
        m_bseFOReceiver = std::make_unique<MulticastReceiver>(bseFOIp, bseFOPort, bseFOInterface);
        m_bseCMReceiver = std::make_unique<MulticastReceiver>(bseCMIp, bseCMPort, bseCMInterface);
        
        // Start 4 threads with native C++ receivers
        m_nseFOThread = std::thread([this]() {
            std::cout << "[NSEFO Thread] Started, waiting for packets..." << std::endl;
            if (m_nseFOReceiver && m_nseFOReceiver->isValid()) {
                m_nseFOReceiver->start();
            } else {
                std::cerr << "[NSEFO Thread] ERROR: Receiver is invalid" << std::endl;
            }
            std::cout << "[NSEFO Thread] Stopped" << std::endl;
        });
        
        m_nseCMThread = std::thread([this]() {
            std::cout << "[NSECM Thread] Started, waiting for packets..." << std::endl;
            if (m_nseCMReceiver && m_nseCMReceiver->isValid()) {
                m_nseCMReceiver->start();
            } else {
                std::cerr << "[NSECM Thread] ERROR: Receiver is invalid" << std::endl;
            }
            std::cout << "[NSECM Thread] Stopped" << std::endl;
        });
        
        m_bseFOThread = std::thread([this]() {
            std::cout << "[BSEFO Thread] Started, waiting for packets..." << std::endl;
            if (m_bseFOReceiver && m_bseFOReceiver->isValid()) {
                m_bseFOReceiver->start();
            } else {
                std::cerr << "[BSEFO Thread] ERROR: Receiver is invalid" << std::endl;
            }
            std::cout << "[BSEFO Thread] Stopped" << std::endl;
        });
        
        m_bseCMThread = std::thread([this]() {
            std::cout << "[BSECM Thread] Started, waiting for packets..." << std::endl;
            if (m_bseCMReceiver && m_bseCMReceiver->isValid()) {
                m_bseCMReceiver->start();
            } else {
                std::cerr << "[BSECM Thread] ERROR: Receiver is invalid" << std::endl;
            }
            std::cout << "[BSECM Thread] Stopped" << std::endl;
        });
        
        qDebug() << "[MarketDataAggregator] All 4 receivers started successfully";
        
    } catch (const std::exception& e) {
        qCritical() << "[MarketDataAggregator] Exception during start:" << e.what();
        m_running = false;
        throw;
    }
}

void MarketDataAggregator::stop() {
    if (!m_running.exchange(false)) {
        qDebug() << "[MarketDataAggregator] Already stopped";
        return;
    }
    
    qDebug() << "[MarketDataAggregator] Stopping receivers...";
    
    // Stop all receivers (will break their receive loops)
    if (m_nseFOReceiver) {
        m_nseFOReceiver->stop();
        qDebug() << "[MarketDataAggregator] NSEFO receiver stopped";
    }
    if (m_nseCMReceiver) {
        m_nseCMReceiver->stop();
        qDebug() << "[MarketDataAggregator] NSECM receiver stopped";
    }
    if (m_bseFOReceiver) {
        m_bseFOReceiver->stop();
        qDebug() << "[MarketDataAggregator] BSEFO receiver stopped";
    }
    if (m_bseCMReceiver) {
        m_bseCMReceiver->stop();
        qDebug() << "[MarketDataAggregator] BSECM receiver stopped";
    }
    
    // Join all threads
    if (m_nseFOThread.joinable()) {
        qDebug() << "[MarketDataAggregator] Joining NSEFO thread...";
        m_nseFOThread.join();
    }
    if (m_nseCMThread.joinable()) {
        qDebug() << "[MarketDataAggregator] Joining NSECM thread...";
        m_nseCMThread.join();
    }
    if (m_bseFOThread.joinable()) {
        qDebug() << "[MarketDataAggregator] Joining BSEFO thread...";
        m_bseFOThread.join();
    }
    if (m_bseCMThread.joinable()) {
        qDebug() << "[MarketDataAggregator] Joining BSECM thread...";
        m_bseCMThread.join();
    }
    
    qDebug() << "[MarketDataAggregator] All threads stopped and joined";
}

// =============================================================================
// STATIC CALLBACK FUNCTIONS (Called from worker threads via C++ parsers)
// =============================================================================

void MarketDataAggregator::onTouchlineCallback(const TouchlineData& data) {
    // Forward to instance method for cache update
    instance().updateTouchlineCache(data);
}

void MarketDataAggregator::onDepthCallback(const MarketDepthData& data) {
    instance().updateDepthCache(data);
}

void MarketDataAggregator::onTickerCallback(const TickerData& data) {
    instance().updateTickerCache(data);
}

void MarketDataAggregator::onMarketWatchCallback(const MarketWatchData& data) {
    instance().updateMarketWatchCache(data);
}

// =============================================================================
// THREAD-SAFE CACHE UPDATE METHODS
// =============================================================================

void MarketDataAggregator::updateTouchlineCache(const TouchlineData& data) {
    // Update cache with mutex protection
    {
        std::lock_guard<std::mutex> lock(m_cacheMutex);
        m_touchlineCache[data.token] = data;
    }
    
    // Emit Qt signal to main thread (thread-safe via QMetaObject::invokeMethod)
    // Lambda captures a copy of data
    QMetaObject::invokeMethod(this, [this, data]() {
        emit touchlineUpdated(data.token, data);
    }, Qt::QueuedConnection);
}

void MarketDataAggregator::updateDepthCache(const MarketDepthData& data) {
    {
        std::lock_guard<std::mutex> lock(m_cacheMutex);
        m_depthCache[data.token] = data;
    }
    
    QMetaObject::invokeMethod(this, [this, data]() {
        emit depthUpdated(data.token, data);
    }, Qt::QueuedConnection);
}

void MarketDataAggregator::updateTickerCache(const TickerData& data) {
    {
        std::lock_guard<std::mutex> lock(m_cacheMutex);
        m_tickerCache[data.token] = data;
    }
    
    QMetaObject::invokeMethod(this, [this, data]() {
        emit tickerUpdated(data.token, data);
    }, Qt::QueuedConnection);
}

void MarketDataAggregator::updateMarketWatchCache(const MarketWatchData& data) {
    {
        std::lock_guard<std::mutex> lock(m_cacheMutex);
        m_marketWatchCache[data.token] = data;
    }
    
    QMetaObject::invokeMethod(this, [this, data]() {
        emit marketWatchUpdated(data.token, data);
    }, Qt::QueuedConnection);
}

// =============================================================================
// THREAD-SAFE CACHE READ METHODS
// =============================================================================

TouchlineData MarketDataAggregator::getTouchline(int32_t token) {
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    auto it = m_touchlineCache.find(token);
    if (it != m_touchlineCache.end()) {
        return it->second;
    }
    // Return empty/default TouchlineData if not found
    return TouchlineData{};
}

MarketDepthData MarketDataAggregator::getDepth(int32_t token) {
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    auto it = m_depthCache.find(token);
    if (it != m_depthCache.end()) {
        return it->second;
    }
    return MarketDepthData{};
}

TickerData MarketDataAggregator::getTicker(int32_t token) {
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    auto it = m_tickerCache.find(token);
    if (it != m_tickerCache.end()) {
        return it->second;
    }
    return TickerData{};
}
