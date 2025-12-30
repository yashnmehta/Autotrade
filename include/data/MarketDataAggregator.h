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
#include "nsefo_callback.h"
#include "nsecm_callback.h"

// Forward declarations (actual classes from broadcast libraries)
class MulticastReceiver;

/**
 * @brief Singleton class that aggregates market data from UDP broadcast sources
 * 
 * Architecture:
 * - Runs native C++ MulticastReceiver instances in std::threads
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
     * @brief Start all broadcast receivers in separate threads
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
    
    // =========================================================================
    // FO DATA ACCESS
    // =========================================================================
    
    nsefo::TouchlineData getTouchlineFO(int32_t token);
    nsefo::MarketDepthData getDepthFO(int32_t token);
    nsefo::TickerData getTickerFO(int32_t token);
    
    // =========================================================================
    // CM DATA ACCESS
    // =========================================================================
    
    nsecm::TouchlineData getTouchlineCM(int32_t token);
    nsecm::MarketDepthData getDepthCM(int32_t token);
    nsecm::TickerData getTickerCM(int32_t token);
    
signals:
    // FO SIGNALS
    void touchlineUpdatedFO(int32_t token, const nsefo::TouchlineData& data);
    void depthUpdatedFO(int32_t token, const nsefo::MarketDepthData& data);
    void tickerUpdatedFO(int32_t token, const nsefo::TickerData& data);
    
    // CM SIGNALS
    void touchlineUpdatedCM(int32_t token, const nsecm::TouchlineData& data);
    void depthUpdatedCM(int32_t token, const nsecm::MarketDepthData& data);
    void tickerUpdatedCM(int32_t token, const nsecm::TickerData& data);
    
private:
    MarketDataAggregator();
    ~MarketDataAggregator();
    
    // Prevent copying
    MarketDataAggregator(const MarketDataAggregator&) = delete;
    MarketDataAggregator& operator=(const MarketDataAggregator&) = delete;
    
    // STATIC CALLBACKS (FO)
    static void onTouchlineCallbackFO(const nsefo::TouchlineData& data);
    static void onDepthCallbackFO(const nsefo::MarketDepthData& data);
    static void onTickerCallbackFO(const nsefo::TickerData& data);
    
    // STATIC CALLBACKS (CM)
    static void onTouchlineCallbackCM(const nsecm::TouchlineData& data);
    static void onDepthCallbackCM(const nsecm::MarketDepthData& data);
    static void onTickerCallbackCM(const nsecm::TickerData& data);
    
    // CACHE UPDATE METHODS
    void updateTouchlineCacheFO(const nsefo::TouchlineData& data);
    void updateDepthCacheFO(const nsefo::MarketDepthData& data);
    void updateTickerCacheFO(const nsefo::TickerData& data);
    
    void updateTouchlineCacheCM(const nsecm::TouchlineData& data);
    void updateDepthCacheCM(const nsecm::MarketDepthData& data);
    void updateTickerCacheCM(const nsecm::TickerData& data);
    
    // Running state flag
    std::atomic<bool> m_running{false};
    
    // Thread-safe cache with single mutex
    mutable std::mutex m_cacheMutex;
    
    // FO CACHES
    std::unordered_map<int32_t, nsefo::TouchlineData> m_touchlineCacheFO;
    std::unordered_map<int32_t, nsefo::MarketDepthData> m_depthCacheFO;
    std::unordered_map<int32_t, nsefo::TickerData> m_tickerCacheFO;
    
    // CM CACHES
    std::unordered_map<int32_t, nsecm::TouchlineData> m_touchlineCacheCM;
    std::unordered_map<int32_t, nsecm::MarketDepthData> m_depthCacheCM;
    std::unordered_map<int32_t, nsecm::TickerData> m_tickerCacheCM;
};

#endif // MARKETDATAAGGREGATOR_H
