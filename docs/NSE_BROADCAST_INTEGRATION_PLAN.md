# NSE UDP Broadcast Integration Plan

**Date**: December 22, 2025  
**Status**: Planning Phase  
**Goal**: Integrate NSE UDP broadcast (message 7200) with Market Watch using centralized feed management

---

## 🎯 Objectives

1. ✅ Receive NSE UDP broadcast data (message 7200 - Market By Price)
2. ✅ Parse and normalize to common format
3. ✅ Route to subscribed Market Watch windows via FeedHandler
4. ✅ Support dual feeds: XTS WebSocket + UDP Broadcast
5. ✅ Centralized subscription management
6. ✅ Zero-copy data flow where possible

---

## 📐 Architecture Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                      DATA SOURCES                                │
├──────────────────────────────┬──────────────────────────────────┤
│   XTS WebSocket API          │   NSE UDP Broadcast (7200)       │
│   • Interactive quotes        │   • Real-time market depth       │
│   • Subscription based        │   • Continuous multicast         │
│   • REST + WebSocket          │   • No subscription needed       │
└───────────┬──────────────────┴──────────────┬───────────────────┘
            │                                 │
            ↓                                 ↓
   ┌────────────────────┐          ┌─────────────────────────┐
   │ XTSMarketData      │          │ MulticastReceiver       │
   │ Client             │          │ (cpp_broadcast_nsefo)   │
   └────────┬───────────┘          └──────────┬──────────────┘
            │                                 │
            │ XTS::Tick                       │ MS_BCAST_MBO_MBP
            │                                 │
            ↓                                 ↓
   ┌────────────────────┐          ┌─────────────────────────┐
   │ (no adapter)       │          │ BroadcastFeedAdapter    │
   │ (already XTS fmt)  │          │ • Parse 7200 message    │
   └────────┬───────────┘          │ • Convert to XTS::Tick  │
            │                      │ • Extract token/LTP/etc │
            │                      └──────────┬──────────────┘
            │                                 │
            └──────────────┬──────────────────┘
                           ↓
                ┌──────────────────────┐
                │   FeedHandler        │ ← CENTRAL HUB
                │   (Singleton)        │
                ├──────────────────────┤
                │ Features:            │
                │ • Token-based subs   │
                │ • Source priority    │
                │ • Callback dispatch  │
                │ • Thread-safe        │
                └──────────┬───────────┘
                           │
           ┌───────────────┼────────────────┬──────────────┐
           ↓               ↓                ↓              ↓
      ┌────────┐      ┌────────┐      ┌────────┐    ┌────────┐
      │ MW #1  │      │ MW #2  │      │ MW #3  │    │ Charts │
      │ Tokens:│      │ Tokens:│      │ Tokens:│    │        │
      │ 26000  │      │ 26009  │      │ 2885   │    │        │
      │ 26009  │      │ 2885   │      │ 26000  │    │        │
      └────────┘      └────────┘      └────────┘    └────────┘
```

---

## 🗂️ Key Components

### 1. **MulticastReceiver** (Already exists in cpp_broadcast_nsefo)
- **Location**: `lib/cpp_broacast_nsefo/`
- **Purpose**: Receive raw UDP packets from NSE multicast groups
- **Status**: ✅ Already implemented
- **Config**: Reads from `config.ini` (IP, port, interface)

### 2. **BroadcastFeedAdapter** (NEW - To be created)
- **Location**: `include/services/BroadcastFeedAdapter.h/cpp`
- **Purpose**: Parse NSE messages and convert to XTS::Tick format
- **Responsibilities**:
  - Parse message 7200 (MS_BCAST_MBO_MBP)
  - Extract: token, LTP, volume, bid/ask, OHLC
  - Normalize to XTS::Tick structure
  - Filter by subscribed tokens (optional optimization)

### 3. **FeedHandler** (Already exists)
- **Location**: `include/services/FeedHandler.h/cpp`
- **Status**: ✅ Already implemented (Phase 1 complete)
- **Enhancements Needed**:
  - Add source priority (UDP > XTS for faster updates)
  - Add feed statistics (UDP vs XTS message count)
  - Optional: Add timestamp comparison for data freshness

### 4. **MarketWatch Integration** (Update existing)
- **Location**: `src/views/MarketWatchWindow.cpp`
- **Status**: ⏳ Needs FeedHandler subscription (Phase 2 pending)
- **Changes**: Already planned in MARKETWATCH_CALLBACK_MIGRATION_ROADMAP.md

---

## 📋 Implementation Phases

### **Phase 1: Test UDP Reception (30 minutes)**

**Goal**: Verify UDP broadcast is being received and parsed

**Steps**:

1. **Add UDP receiver initialization in MainWindow**
   ```cpp
   // In MainWindow.h
   #include "multicast_receiver.h"
   
   class MainWindow : public CustomMainWindow {
       // ...
   private:
       std::unique_ptr<MulticastReceiver> m_udpReceiver;
       void startBroadcastReceiver();
       void stopBroadcastReceiver();
   };
   ```

2. **Start receiver in MainWindow constructor**
   ```cpp
   // In MainWindow.cpp
   void MainWindow::startBroadcastReceiver() {
       qDebug() << "[UDP] Starting NSE broadcast receiver...";
       
       // Config from file
       std::string multicastIP = "233.146.10.50";  // Example NSE F&O IP
       int port = 64550;                           // Example port
       
       m_udpReceiver = std::make_unique<MulticastReceiver>(multicastIP, port);
       
       if (!m_udpReceiver->isValid()) {
           qWarning() << "[UDP] Failed to initialize receiver!";
           return;
       }
       
       // Register callback for 7200 messages
       MarketDataCallbackRegistry::instance().registerTouchlineCallback(
           [this](const TouchlineData& data) {
               qDebug() << "[UDP 7200] Token:" << data.token
                       << "LTP:" << data.ltp
                       << "Volume:" << data.volume
                       << "Bid:" << data.bids[0].price  // Best bid
                       << "Ask:" << data.asks[0].price; // Best ask
           }
       );
       
       m_udpReceiver->start();
       qDebug() << "[UDP] Receiver started successfully";
   }
   ```

3. **Call in setupContent()**
   ```cpp
   void MainWindow::setupContent() {
       // ... existing code ...
       
       // Start UDP broadcast receiver
       startBroadcastReceiver();
   }
   ```

4. **Build and test**
   ```bash
   cd build
   cmake --build . --target TradingTerminal -j4
   ./TradingTerminal.app/Contents/MacOS/TradingTerminal
   ```

**Expected Output**:
```
[UDP] Starting NSE broadcast receiver...
[UDP] Receiver started successfully
[UDP 7200] Token: 26000 LTP: 21450.50 Volume: 1250000 Bid: 21450.00 Ask: 21451.00
[UDP 7200] Token: 26009 LTP: 45200.25 Volume: 890000 Bid: 45200.00 Ask: 45200.50
[UDP 7200] Token: 2885 LTP: 2850.75 Volume: 2340000 Bid: 2850.50 Ask: 2851.00
```

---

### **Phase 2: Create BroadcastFeedAdapter (1 hour)**

**Goal**: Convert NSE format to XTS::Tick for FeedHandler compatibility

**File**: `include/services/BroadcastFeedAdapter.h`

```cpp
#ifndef BROADCASTFEEDADAPTER_H
#define BROADCASTFEEDADAPTER_H

#include "api/XTSMarketDataClient.h"
#include "market_data_callback.h"
#include <QObject>
#include <memory>

/**
 * @brief Adapter to convert NSE UDP broadcast data to XTS::Tick format
 * 
 * This adapter listens to NSE broadcast callbacks and converts the data
 * into XTS::Tick format so it can be seamlessly integrated with FeedHandler.
 */
class BroadcastFeedAdapter : public QObject {
    Q_OBJECT
    
public:
    explicit BroadcastFeedAdapter(QObject *parent = nullptr);
    
    /**
     * @brief Start listening to UDP broadcast callbacks
     */
    void start();
    
    /**
     * @brief Stop listening to callbacks
     */
    void stop();
    
    /**
     * @brief Get statistics (messages received/converted)
     */
    struct Stats {
        uint64_t messagesReceived = 0;
        uint64_t messagesConverted = 0;
        uint64_t conversionErrors = 0;
    };
    const Stats& getStats() const { return m_stats; }
    
signals:
    /**
     * @brief Emitted when a tick is converted and ready for distribution
     */
    void tickReady(const XTS::Tick &tick);
    
private:
    // Callback handlers for different message types
    void onTouchlineData(const TouchlineData& data);
    void onMarketDepthData(const MarketDepthData& data);
    void onTickerData(const TickerData& data);
    
    // Convert NSE format to XTS format
    XTS::Tick convertToXTSTick(const TouchlineData& data);
    
    Stats m_stats;
    bool m_running = false;
};

#endif // BROADCASTFEEDADAPTER_H
```

**File**: `src/services/BroadcastFeedAdapter.cpp`

```cpp
#include "services/BroadcastFeedAdapter.h"
#include "market_data_callback.h"
#include <QDebug>

BroadcastFeedAdapter::BroadcastFeedAdapter(QObject *parent)
    : QObject(parent)
{
}

void BroadcastFeedAdapter::start() {
    if (m_running) return;
    
    qDebug() << "[BroadcastAdapter] Starting...";
    
    // Register with NSE broadcast callback registry
    MarketDataCallbackRegistry::instance().registerTouchlineCallback(
        [this](const TouchlineData& data) {
            onTouchlineData(data);
        }
    );
    
    MarketDataCallbackRegistry::instance().registerMarketDepthCallback(
        [this](const MarketDepthData& data) {
            onMarketDepthData(data);
        }
    );
    
    MarketDataCallbackRegistry::instance().registerTickerCallback(
        [this](const TickerData& data) {
            onTickerData(data);
        }
    );
    
    m_running = true;
    qDebug() << "[BroadcastAdapter] Started successfully";
}

void BroadcastFeedAdapter::stop() {
    if (!m_running) return;
    
    qDebug() << "[BroadcastAdapter] Stopping...";
    // TODO: Unregister callbacks if API supports it
    m_running = false;
}

void BroadcastFeedAdapter::onTouchlineData(const TouchlineData& data) {
    m_stats.messagesReceived++;
    
    try {
        XTS::Tick tick = convertToXTSTick(data);
        m_stats.messagesConverted++;
        
        // Emit for FeedHandler to distribute
        emit tickReady(tick);
        
    } catch (const std::exception& e) {
        m_stats.conversionErrors++;
        qWarning() << "[BroadcastAdapter] Conversion error:" << e.what();
    }
}

void BroadcastFeedAdapter::onMarketDepthData(const MarketDepthData& data) {
    // TODO: Phase 3 - Enhance XTS::Tick to include full depth
    // For now, we get this from TouchlineData which includes best bid/ask
}

void BroadcastFeedAdapter::onTickerData(const TickerData& data) {
    // TODO: Phase 4 - Use for OI updates
}

XTS::Tick BroadcastFeedAdapter::convertToXTSTick(const TouchlineData& data) {
    XTS::Tick tick;
    
    // Identity
    tick.exchangeInstrumentID = data.token;
    tick.exchangeSegment = 2;  // Assuming NFO (F&O), TODO: detect from token range
    
    // Prices
    tick.lastTradedPrice = data.ltp;
    tick.open = data.open;
    tick.high = data.high;
    tick.low = data.low;
    tick.close = data.close;
    
    // Volume
    tick.lastTradedQuantity = data.lastTradeQty;
    tick.totalBuyQuantity = 0;   // TODO: Get from market depth
    tick.totalSellQuantity = 0;  // TODO: Get from market depth
    tick.averageTradedPrice = data.avgPrice;
    tick.totalTradedValue = data.ltp * data.volume;  // Approximation
    
    // Bid/Ask (will be filled by market depth callback)
    tick.bidPrice = 0;
    tick.bidQuantity = 0;
    tick.askPrice = 0;
    tick.askQuantity = 0;
    
    // Change
    tick.change = data.netChange;
    tick.changePercent = (data.close != 0) ? (data.netChange / data.close) * 100.0 : 0.0;
    
    // Timestamp
    tick.exchangeTimeStamp = data.lastTradeTime;  // NSE timestamp
    
    // Source identifier (for debugging/prioritization)
    tick.dataSource = "UDP";  // TODO: Add this field to XTS::Tick
    
    return tick;
}
```

---

### **Phase 3: Connect to FeedHandler (30 minutes)**

**Goal**: Route converted ticks to FeedHandler for distribution

**In MainWindow**:

```cpp
void MainWindow::setupContent() {
    // ... existing code ...
    
    // Create broadcast adapter
    m_broadcastAdapter = new BroadcastFeedAdapter(this);
    
    // Connect adapter to FeedHandler
    connect(m_broadcastAdapter, &BroadcastFeedAdapter::tickReady,
            this, [](const XTS::Tick& tick) {
        // Route to FeedHandler (central distribution hub)
        FeedHandler::instance().onTickReceived(tick);
        
        // Also update PriceCache for backward compatibility
        PriceCache::instance().updatePrice(tick.exchangeInstrumentID, tick);
    });
    
    // Start UDP receiver
    startBroadcastReceiver();
    
    // Start adapter
    m_broadcastAdapter->start();
}
```

---

### **Phase 4: Subscription Filtering (Optional Optimization) (1 hour)**

**Goal**: Only process UDP data for subscribed tokens (reduce CPU)

**Approach 1: Filter in Adapter**
```cpp
void BroadcastFeedAdapter::onTouchlineData(const TouchlineData& data) {
    // Check if anyone is subscribed to this token
    if (!FeedHandler::instance().hasSubscribers(data.token)) {
        m_stats.filteredOut++;
        return;  // Skip processing
    }
    
    // ... convert and emit ...
}
```

**Approach 2: Filter at UDP Level (more efficient)**
```cpp
// In cpp_broadcast_nsefo library - add token whitelist
class MulticastReceiver {
public:
    void setTokenFilter(const std::unordered_set<int>& tokens);
    // Only parse messages for these tokens
};
```

---

### **Phase 5: Dual Feed Strategy (Advanced) (2 hours)**

**Goal**: Use UDP for speed, fallback to XTS for reliability

**Strategy**:
1. UDP provides fast updates (multicast, no request needed)
2. XTS provides guaranteed delivery (request/response)
3. Use UDP as primary, XTS as fallback

**Implementation**:

```cpp
class FeedHandler {
private:
    struct TokenState {
        XTS::Tick lastTick;
        uint64_t lastUpdateTime;
        QString lastSource;  // "UDP" or "XTS"
        uint32_t udpCount = 0;
        uint32_t xtsCount = 0;
    };
    
    QMap<int, TokenState> m_tokenState;
    
public:
    void onTickReceived(const XTS::Tick& tick) {
        int token = tick.exchangeInstrumentID;
        uint64_t now = QDateTime::currentMSecsSinceEpoch();
        
        // Update statistics
        TokenState& state = m_tokenState[token];
        if (tick.dataSource == "UDP") {
            state.udpCount++;
        } else {
            state.xtsCount++;
        }
        
        // Check if this is a newer update
        if (tick.exchangeTimeStamp > state.lastTick.exchangeTimeStamp) {
            state.lastTick = tick;
            state.lastUpdateTime = now;
            state.lastSource = tick.dataSource;
            
            // Distribute to subscribers
            distributeToSubscribers(token, tick);
        } else {
            qDebug() << "[FeedHandler] Ignoring stale tick from" 
                     << tick.dataSource << "for token" << token;
        }
    }
    
    // Get feed statistics
    struct FeedStats {
        uint32_t udpMessages = 0;
        uint32_t xtsMessages = 0;
        double udpLatency = 0;     // Average latency
        double xtsLatency = 0;
        uint32_t duplicates = 0;
        uint32_t stale = 0;
    };
    
    FeedStats getStats() const;
};
```

---

## 🎯 Success Criteria

### **Phase 1 Complete When:**
- ✅ UDP receiver starts without errors
- ✅ Console prints show incoming 7200 messages
- ✅ Token, LTP, volume visible in logs
- ✅ Data updates continuously (every 1-2 seconds)

### **Phase 2 Complete When:**
- ✅ BroadcastAdapter successfully converts messages
- ✅ XTS::Tick format matches exactly
- ✅ No conversion errors in logs
- ✅ Statistics show messages received/converted

### **Phase 3 Complete When:**
- ✅ FeedHandler receives ticks from adapter
- ✅ Market Watch windows update from UDP data
- ✅ No duplicate subscriptions
- ✅ Zero latency compared to XTS

### **Phase 4 Complete When:**
- ✅ Only subscribed tokens are processed
- ✅ CPU usage reduced by 50-80%
- ✅ Memory usage stable
- ✅ Filter correctly updates when subscriptions change

### **Phase 5 Complete When:**
- ✅ UDP and XTS both feeding FeedHandler
- ✅ No duplicate updates to UI
- ✅ Statistics show which source was used
- ✅ Graceful fallback when UDP fails

---

## 📊 Performance Targets

| Metric | XTS WebSocket | UDP Broadcast | Improvement |
|--------|--------------|---------------|-------------|
| **Latency** | 50-200ms | 1-5ms | **10-200x faster** |
| **Bandwidth** | Varies | Continuous | Fixed multicast |
| **CPU Usage** | 2-5% | 0.5-1% | **50-80% less** |
| **Reliability** | 99.9% | 99.5% | XTS = backup |
| **Updates/sec** | ~100 | ~1000 | **10x more** |

---

## 🔧 Configuration

**File**: `configs/config.ini`

```ini
[UDP_Broadcast]
# NSE F&O Market Data
MulticastIP=233.146.10.50
Port=64550
Interface=en0

# Message types to subscribe
EnableTouchline=true      # 7200, 7208
EnableTicker=true         # 7202
EnableMarketWatch=true    # 7201
EnableDepth=true          # Full order book
EnableOI=true             # 7130

# Performance
BufferSize=65535
FilterBySubscription=true

[Feed_Strategy]
# Primary data source
PrimarySource=UDP          # UDP or XTS
FallbackSource=XTS         # XTS or NONE

# Timeouts
UDPTimeout=5000            # 5 seconds
XTSTimeout=10000           # 10 seconds

# Quality
PreferFreshData=true       # Use timestamp to pick newest
AllowDuplicates=false      # Skip if already processed
```

---

## 🐛 Debugging Guide

### **Issue: No UDP packets received**

**Check**:
1. Multicast IP and port correct?
   ```bash
   netstat -g | grep 233.146.10
   ```

2. Network interface supports multicast?
   ```bash
   ifconfig en0 | grep MULTICAST
   ```

3. Firewall blocking?
   ```bash
   sudo pfctl -s all | grep 64550
   ```

4. NSE broadcast active?
   - Market hours: 9:15 AM - 3:30 PM
   - Check exchange status

### **Issue: Messages parsed but not reaching Market Watch**

**Check**:
1. FeedHandler subscriptions
   ```cpp
   qDebug() << "Subscriptions:" << FeedHandler::instance().getSubscriptionCount();
   ```

2. Token matching
   ```cpp
   qDebug() << "Looking for token:" << token;
   qDebug() << "Subscribed tokens:" << FeedHandler::instance().getSubscribedTokens();
   ```

3. Market Watch callback registered
   ```cpp
   // In MarketWatch
   qDebug() << "Registered" << m_subscriptions.size() << "tokens";
   ```

### **Issue: Duplicate updates (both UDP and XTS)**

**Solution**: Enable timestamp comparison in FeedHandler
```cpp
if (tick.exchangeTimeStamp <= state.lastTick.exchangeTimeStamp) {
    return;  // Ignore older data
}
```

---

## 📅 Estimated Timeline

| Phase | Description | Time | Dependencies |
|-------|-------------|------|--------------|
| Phase 1 | Test UDP reception | 30 min | cpp_broadcast_nsefo |
| Phase 2 | Create adapter | 1 hour | Phase 1 |
| Phase 3 | Connect to FeedHandler | 30 min | Phase 2, FeedHandler |
| Phase 4 | Subscription filtering | 1 hour | Phase 3 |
| Phase 5 | Dual feed strategy | 2 hours | Phase 3 |
| **Total** | | **5 hours** | |

---

## 🚀 Next Steps

1. **Immediate (Phase 1)**: Add print statements to see UDP data
   - Start UDP receiver
   - Register simple callback
   - Verify data is being received

2. **Short-term (Phase 2-3)**: Create adapter and connect to FeedHandler
   - Parse 7200 messages
   - Convert to XTS::Tick
   - Route through FeedHandler

3. **Medium-term (Phase 4)**: Optimize with subscription filtering
   - Reduce CPU usage
   - Process only subscribed tokens

4. **Long-term (Phase 5)**: Advanced dual-feed strategy
   - UDP primary, XTS fallback
   - Timestamp-based deduplication
   - Feed statistics and monitoring

---

**Ready to start with Phase 1?** Let's add the print statements first! 🚀
