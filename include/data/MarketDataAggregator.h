#ifndef MARKETDATAAGGREGATOR_H
#define MARKETDATAAGGREGATOR_H

#include <QObject>
#include <memory>
#include <thread>
#include <mutex>
#include <atomic>
#include <unordered_map>
#include <string>

// Include broadcast library headers
#include "multicast_receiver.h"
#include "market_data_callback.h"

// Forward declarations (actual classes from broadcast libraries)
class MulticastReceiver;

// Data structures are imported from market_data_callback.h:
// - TouchlineData
// - MarketDepthData (with vector<DepthLevel> bids/asks)
// - TickerData
// - MarketWatchData

/**
 * @brief Singleton class that aggregates market data from 4 UDP broadcast sources
 * 
 * Architecture:
 * - Runs 4 native C++ MulticastReceiver instances in std::threads
 * - Receives callbacks from parsers (executed in worker threads)
 * - Maintains thread-safe cache with single mutex
 * - Emits Qt signals to main thread for UI updates
 * 
 * Thread Safety:
 * - Cache writes: Protected by m_cacheMutex
 * - Qt signal emission: QMetaObject::invokeMethod with Qt::QueuedConnection
 * - No Qt wrappers around native C++ receivers
 */
class MarketDataAggregator : public QObject {
    Q_OBJECT
    
public:
    /**
     * @brief Get singleton instance
     */
    static MarketDataAggregator& instance();
    
    /**
     * @brief Start all 4 broadcast receivers in separate threads
     */
    void start();
    
    /**
     * @brief Stop all receivers and join threads
     */
    void stop();
    
    /**
     * @brief Check if aggregator is running
     */
    bool isRunning() const { return m_running.load(); }
    
    /**
     * @brief Thread-safe cache access for touchline data
     * @param token Instrument token
     * @return TouchlineData (empty struct if not found)
     */
    TouchlineData getTouchline(int32_t token);
    
    /**
     * @brief Thread-safe cache access for depth data
     * @param token Instrument token
     * @return MarketDepthData (empty struct if not found)
     */
    MarketDepthData getDepth(int32_t token);
    
    /**
     * @brief Thread-safe cache access for ticker data
     * @param token Instrument token
     * @return TickerData (empty struct if not found)
     */
    TickerData getTicker(int32_t token);
    
signals:
    /**
     * @brief Emitted when touchline data is updated
     * @note Connected with Qt::QueuedConnection - safe to update UI directly
     */
    void touchlineUpdated(int32_t token, const TouchlineData& data);
    
    /**
     * @brief Emitted when market depth data is updated
     */
    void depthUpdated(int32_t token, const MarketDepthData& data);
    
    /**
     * @brief Emitted when ticker data is updated
     */
    void tickerUpdated(int32_t token, const TickerData& data);
    
    /**
     * @brief Emitted when market watch data is updated
     */
    void marketWatchUpdated(int32_t token, const MarketWatchData& data);
    
private:
    MarketDataAggregator();
    ~MarketDataAggregator();
    
    // Prevent copying
    MarketDataAggregator(const MarketDataAggregator&) = delete;
    MarketDataAggregator& operator=(const MarketDataAggregator&) = delete;
    
    /**
     * @brief Static callback functions registered with broadcast parsers
     * @note These are called from worker threads - must be thread-safe
     */
    static void onTouchlineCallback(const TouchlineData& data);
    static void onDepthCallback(const MarketDepthData& data);
    static void onTickerCallback(const TickerData& data);
    static void onMarketWatchCallback(const MarketWatchData& data);
    
    /**
     * @brief Thread-safe cache update methods
     * @note These acquire mutex, update cache, then emit Qt signal
     */
    void updateTouchlineCache(const TouchlineData& data);
    void updateDepthCache(const MarketDepthData& data);
    void updateTickerCache(const TickerData& data);
    void updateMarketWatchCache(const MarketWatchData& data);
    
    // Native C++ receivers (no Qt wrappers) - TEMPORARILY DISABLED
    // std::unique_ptr<MulticastReceiver> m_nseFOReceiver;
    // std::unique_ptr<MulticastReceiver> m_nseCMReceiver;
    // std::unique_ptr<MulticastReceiver> m_bseFOReceiver;
    // std::unique_ptr<MulticastReceiver> m_bseCMReceiver;
    
    // Standard C++11 threads - TEMPORARILY DISABLED
    // std::thread m_nseFOThread;
    // std::thread m_nseCMThread;
    // std::thread m_bseFOThread;
    // std::thread m_bseCMThread;
    
    // Running state flag
    std::atomic<bool> m_running{false};
    
    // Thread-safe cache with single mutex
    mutable std::mutex m_cacheMutex;
    std::unordered_map<int32_t, TouchlineData> m_touchlineCache;
    std::unordered_map<int32_t, MarketDepthData> m_depthCache;
    std::unordered_map<int32_t, TickerData> m_tickerCache;
    std::unordered_map<int32_t, MarketWatchData> m_marketWatchCache;
};

#endif // MARKETDATAAGGREGATOR_H
