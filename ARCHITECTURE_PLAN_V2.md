# Trading Terminal - Comprehensive Architecture Plan v2.0

**Document Purpose:** Clean-slate architectural design for multi-threaded, high-performance trading terminal with focus on thread safety, data flow, and communication patterns.

---

## 1. EXECUTIVE SUMMARY

### 1.1 Core Principles
1. **Separation of Concerns**: Clear boundaries between data acquisition, processing, storage, and UI
2. **Thread Safety First**: Design threading model upfront, not retrofit
3. **Performance Critical**: Sub-millisecond latency for market data updates
4. **Scalability**: Support 10K+ simultaneous instrument subscriptions
5. **Reliability**: Graceful degradation, automatic reconnection, error isolation

### 1.2 Technology Stack
- **UI Framework**: Qt 5.x (Signal/Slot for thread-safe communication)
- **Network I/O**: Native C++ sockets (UDP), Boost.Asio (WebSocket)
- **Threading**: std::thread for I/O workers, Qt event loop for UI
- **Concurrency**: std::shared_mutex (readers-writer locks), std::atomic
- **Data Structures**: Lock-free queues, memory-mapped caches

---

## 2. THREADING MODEL

### 2.1 Thread Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                         MAIN/UI THREAD                           │
│  - Qt Event Loop                                                 │
│  - All UI components (QWidget, QTableView, etc.)               │
│  - Receives signals from worker threads                         │
│  - Never blocks on I/O or computation                          │
└─────────────────────────────────────────────────────────────────┘
                              ▲
                              │ Qt Signals (queued)
                              │
┌─────────────────────────────────────────────────────────────────┐
│                    FEED ORCHESTRATOR (Main Thread)               │
│  - FeedHandler singleton                                        │
│  - Routes data from I/O threads to subscribers                 │
│  - Manages subscription registry                               │
└─────────────────────────────────────────────────────────────────┘
                              ▲
                              │ Lock-free publish
                              │
┌──────────────────────┬──────────────────────┬──────────────────┐
│   NSE FO UDP Thread  │   NSE CM UDP Thread  │  BSE UDP Threads │
│  - Socket recv()     │  - Socket recv()     │  - Socket recv() │
│  - LZO decompress    │  - LZO decompress    │  - Parsing       │
│  - Parse packets     │  - Parse packets     │  - Publishing    │
│  - Publish to cache  │  - Publish to cache  │                  │
└──────────────────────┴──────────────────────┴──────────────────┘

┌─────────────────────────────────────────────────────────────────┐
│              XTS WebSocket Thread (Boost.Asio)                   │
│  - io_context.run()                                             │
│  - WebSocket read/write                                         │
│  - Heartbeat management                                         │
│  - JSON parsing                                                 │
│  - Publish ticks via FeedHandler                               │
└─────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────┐
│              XTS HTTP Thread (Async REST calls)                  │
│  - Login, subscription API calls                                │
│  - Master file downloads                                        │
│  - Order placement/modification                                 │
│  - Position/Order book queries                                  │
└─────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────┐
│              MASTER LOADER Thread (QThread)                      │
│  - CSV parsing (NSE/BSE master files)                          │
│  - Signals progress to UI                                       │
│  - Terminates after load complete                              │
└─────────────────────────────────────────────────────────────────┘
```

### 2.2 Thread Ownership & Lifecycle

| Thread | Created By | Lifetime | Termination |
|--------|-----------|----------|-------------|
| Main/UI | Qt Application | App lifetime | App exit |
| NSE FO UDP | UdpBroadcastService | start() → stop() | stop() sets atomic flag |
| NSE CM UDP | UdpBroadcastService | start() → stop() | stop() sets atomic flag |
| BSE UDP | UdpBroadcastService | start() → stop() | stop() sets atomic flag |
| XTS WebSocket | XTSMarketDataClient | connectWebSocket() | disconnect() |
| XTS HTTP | Ad-hoc (per API call) | Call duration | Callback completion |
| Master Loader | LoginFlowService | Post-login | CSV parse complete |

### 2.3 Thread Safety Rules

**RULE 1: UI Thread Exclusivity**
- Only UI thread modifies Qt widgets
- Worker threads use `QMetaObject::invokeMethod(Qt::QueuedConnection)` or signals

**RULE 2: Data Structure Ownership**
- PriceCache: Multiple reader threads (lock-free), single writer thread (UDP)
- FeedHandler: Multiple publishers (UDP/XTS threads), multiple subscribers (UI thread)
- SubscriptionManager: Protected by mutex, short critical sections

**RULE 3: No Blocking in I/O Threads**
- UDP threads: recv() with timeout, non-blocking parse
- WebSocket thread: Asio async operations only

**RULE 4: Lock Hierarchy (Deadlock Prevention)**
```
Level 1: SubscriptionManager::m_mutex
Level 2: PriceCache::m_segmentLocks[i]
Level 3: FeedHandler::m_subscribersMutex

Always acquire locks in ascending order!
```

---

## 3. DATA FLOW ARCHITECTURE

### 3.1 Market Data Pipeline

```
┌─────────────┐
│   Exchange  │ (NSE/BSE multicast UDP)
└──────┬──────┘
       │ Raw UDP packets (compressed)
       ▼
┌─────────────────────────────────────────┐
│   UDP Receiver Thread (per segment)     │
│  1. socket.recv() → buffer              │
│  2. LZO decompress → uncompressed       │
│  3. Parse protocol → struct             │
└──────┬──────────────────────────────────┘
       │ UDP::MarketTick (stack struct)
       ▼
┌─────────────────────────────────────────┐
│   PriceCache (Zero-Copy Update)         │
│  - Direct memcpy to segment array       │
│  - O(1) token lookup                    │
│  - No malloc/free                       │
└──────┬──────────────────────────────────┘
       │ Broadcast notification
       ▼
┌─────────────────────────────────────────┐
│   FeedHandler (Pub/Sub)                 │
│  - Hash lookup: token → subscribers     │
│  - For each subscriber:                 │
│    - Read from PriceCache (const&)      │
│    - Emit Qt signal → UI thread         │
└──────┬──────────────────────────────────┘
       │ Qt Signal (queued connection)
       ▼
┌─────────────────────────────────────────┐
│   UI Components (MarketWatch, etc.)     │
│  - Slot executes on main thread         │
│  - QTableView::update()                 │
└─────────────────────────────────────────┘
```

### 3.2 XTS WebSocket Data Pipeline

```
┌─────────────┐
│  XTS Server │ (Symphony Fintech WebSocket)
└──────┬──────┘
       │ JSON over WebSocket
       ▼
┌─────────────────────────────────────────┐
│   NativeWebSocketClient (Asio Thread)   │
│  - on_message() callback                │
│  - JSON parse → XTS::Tick               │
└──────┬──────────────────────────────────┘
       │ XTS::Tick
       ▼
┌─────────────────────────────────────────┐
│   PriceCache::update(XTS::Tick)         │
│  - Convert XTS::Tick → ConsolidatedData │
│  - Write to cache                       │
└──────┬──────────────────────────────────┘
       │
       ▼
┌─────────────────────────────────────────┐
│   FeedHandler (Same as UDP path)        │
└─────────────────────────────────────────┘
```

### 3.3 Order Flow Pipeline

```
┌─────────────┐
│ User Action │ (Click "Buy" button)
└──────┬──────┘
       │ UI Thread
       ▼
┌─────────────────────────────────────────┐
│   BuyWindow::onSubmit()                 │
│  - Validate inputs                      │
│  - Prepare order struct                 │
└──────┬──────────────────────────────────┘
       │ XTS::OrderRequest
       ▼
┌─────────────────────────────────────────┐
│   XTSInteractiveClient::placeOrder()    │
│  - Spawn std::thread                    │
│  - HTTP POST to XTS                     │
│  - Emit orderPlaced signal              │
└──────┬──────────────────────────────────┘
       │ Qt Signal (queued from HTTP thread)
       ▼
┌─────────────────────────────────────────┐
│   Slot in UI Thread                     │
│  - Process order response               │
└──────┬──────────────────────────────────┘
       │ Direct call (same thread)
       ▼
┌─────────────────────────────────────────┐
│   OrderBookWindow::onOrderUpdate()      │
│  - Update QTableView                    │
│  - Show notification                    │
└─────────────────────────────────────────┘
```

---

## 4. COMPONENT DESIGN

### 4.1 FeedHandler (Centralized Pub/Sub)

**Purpose**: Single source of truth for market data routing

**Interface**:
```cpp
class FeedHandler : public QObject {
    Q_OBJECT
public:
    static FeedHandler& instance();
    
    // Subscribe to market data updates
    void subscribe(int exchangeSegment, int token, 
                   QObject* receiver, const char* slot);
    
    void unsubscribe(int exchangeSegment, int token, QObject* receiver);
    
    // Publish from I/O threads (thread-safe)
    void publish(const UDP::MarketTick& tick);
    void publish(const XTS::Tick& tick);
    
signals:
    // Never emitted directly - use per-token publishers
    
private:
    struct Subscription {
        QObject* receiver;
        std::unique_ptr<TokenPublisher> publisher;
    };
    
    std::unordered_map<int64_t, std::vector<Subscription>> m_subscriptions;
    std::shared_mutex m_mutex;  // Readers-writer lock
};
```

**Threading**:
- `subscribe/unsubscribe`: Called from UI thread
- `publish`: Called from UDP/XTS threads
- Uses `std::shared_mutex` for concurrent reads, exclusive writes

**Performance**:
- Subscribe: O(1) hash insert
- Publish: O(subscribers) but typically 1-5
- No heap allocation in hot path

### 4.2 PriceCache (Zero-Copy Storage)

**Purpose**: Centralized market data storage with minimal latency

**Architecture**:
```cpp
class PriceCacheZeroCopy : public QObject {
    Q_OBJECT
public:
    static PriceCacheZeroCopy& instance();
    
    // Write interface (from I/O threads)
    void update(const UDP::MarketTick& tick);
    void update(const XTS::Tick& tick);
    
    // Read interface (from any thread)
    const ConsolidatedMarketData* get(int exchangeSegment, int token) const;
    
    // Bulk reads
    std::vector<const ConsolidatedMarketData*> 
        getSubscribed(int exchangeSegment) const;
    
signals:
    void priceUpdated(int exchangeSegment, int token);
    
private:
    // Memory layout: Direct arrays per segment
    std::unique_ptr<ConsolidatedMarketData[]> m_nsefoData;  // 50K elements
    std::unique_ptr<ConsolidatedMarketData[]> m_nsecmData;  // 50K elements
    std::unique_ptr<ConsolidatedMarketData[]> m_bsefoData;  // 20K elements
    std::unique_ptr<ConsolidatedMarketData[]> m_bsecmData;  // 20K elements
    
    // Subscription tracking (what to publish)
    std::unordered_set<int64_t> m_subscribedTokens;
    mutable std::shared_mutex m_subscriptionMutex;
};
```

**Memory Management**:
- Pre-allocated arrays (no dynamic allocation)
- Cache-aligned structures (512 bytes per token)
- Memory-mapped for potential IPC

**Thread Safety**:
- Writes: Only from UDP threads (one writer per segment)
- Reads: Any thread, lock-free (atomic flags for dirty checking)

### 4.3 UdpBroadcastService (Multi-Segment Manager)

**Purpose**: Orchestrates multiple UDP receivers

**Responsibilities**:
1. Lifecycle management (start/stop per segment)
2. Configuration loading
3. Statistics aggregation
4. Error handling & reconnection

**Interface**:
```cpp
class UdpBroadcastService : public QObject {
    Q_OBJECT
public:
    static UdpBroadcastService& instance();
    
    // Global control
    void start(const Config& config);
    void stop();
    
    // Per-segment control
    bool startReceiver(ExchangeReceiver receiver, 
                      const std::string& ip, int port);
    void stopReceiver(ExchangeReceiver receiver);
    bool isReceiverActive(ExchangeReceiver receiver) const;
    
    // Statistics
    Stats getStats() const;
    
signals:
    void receiverStatusChanged(ExchangeReceiver receiver, bool active);
    void statsUpdated(const Stats& stats);
    
private:
    std::thread m_nseFoThread;
    std::thread m_nseCmThread;
    std::thread m_bseFoThread;
    std::thread m_bseCmThread;
    
    std::atomic<bool> m_nseFoRunning{false};
    std::atomic<bool> m_nseCmRunning{false};
    std::atomic<bool> m_bseFoRunning{false};
    std::atomic<bool> m_bseCmRunning{false};
};
```

**Thread Model**:
- Each receiver runs on dedicated thread
- No shared state between receivers
- Atomic flags for status checking

### 4.4 XTSMarketDataClient (WebSocket Manager)

**Purpose**: XTS API interactions (login, subscription, WebSocket)

**Threading**:
- WebSocket I/O: Dedicated thread (Boost.Asio io_context)
- HTTP calls: Short-lived threads (std::thread per call)
- Callbacks: Executed on calling thread

**Interface**:
```cpp
class XTSMarketDataClient : public QObject {
    Q_OBJECT
public:
    // Async operations (no callbacks - use signals)
    void login();  // Emits loginCompleted
    void connectWebSocket();  // Emits wsConnectionStatusChanged
    void disconnectWebSocket();
    void subscribe(const QVector<int64_t>& tokens, int exchangeSegment);  // Emits subscriptionCompleted
    void unsubscribe(const QVector<int64_t>& tokens, int exchangeSegment);
    
    // State queries
    bool isLoggedIn() const;
    bool isConnected() const;
    QString token() const;
    
signals:
    // Async operation results
    void loginCompleted(bool success, const QString& error);
    void wsConnectionStatusChanged(bool connected, const QString& error);
    void subscriptionCompleted(bool success, const QString& error);
    
    // Real-time data
    void tickReceived(const XTS::Tick& tick);
    void errorOccurred(const QString& error);
    
private:
    std::unique_ptr<NativeWebSocketClient> m_wsClient;
    std::thread m_wsThread;
    std::atomic<bool> m_wsConnected{false};
    std::atomic<bool> m_loggedIn{false};
};
```

**Critical Path Optimization**:
```cpp
// Optimized: Minimal work on WebSocket thread
void XTSMarketDataClient::onWSMessage(const std::string& json) {
    auto tick = parseJson(json);  // 50-100μs
    
    // Update cache (fast)
    PriceCache::instance().update(tick);  // 200ns
    
    // Publish via FeedHandler (emits signals)
    FeedHandler::instance().publish(tick);  // 500ns
    
    // Total: ~100μs on WebSocket thread (acceptable)
    // UI updates happen asynchronously via queued signals
}
```

### 4.5 SubscriptionManager (Hybrid Data Source Coordinator)

**Purpose**: Coordinate UDP and XTS subscriptions

**Design**:
```cpp
class SubscriptionManager : public QObject {
    Q_OBJECT
public:
    enum class DataSource {
        UDP_ONLY,       // Prefer UDP, no XTS fallback
        XTS_ONLY,       // XTS WebSocket only
        UDP_WITH_XTS    // UDP primary, XTS fallback if UDP unavailable
    };
    
    // High-level subscription API
    void subscribe(int exchangeSegment, int token, DataSource source);
    void unsubscribe(int exchangeSegment, int token);
    
    // Automatic fallback logic
    void onUdpConnectionLost(int exchangeSegment);
    void onUdpConnectionRestored(int exchangeSegment);
    
private:
    struct SubscriptionInfo {
        DataSource source;
        bool udpActive;
        bool xtsActive;
    };
    
    std::unordered_map<int64_t, SubscriptionInfo> m_subscriptions;
    std::mutex m_mutex;
    
    UdpBroadcastService& m_udpService;
    XTSMarketDataClient& m_xtsClient;
};
```

**Decision Logic**:
```
subscribe(token, UDP_WITH_XTS):
  ├─ UDP active?
  │  ├─ YES → Subscribe UDP only
  │  └─ NO  → Subscribe XTS as fallback
  └─ Store preference for future recovery

onUdpConnectionLost():
  ├─ Find all UDP_WITH_XTS subscriptions
  ├─ For each token:
  │  └─ Subscribe via XTS
  └─ Set fallback flag

onUdpConnectionRestored():
  ├─ Find all fallback subscriptions
  ├─ For each token:
  │  ├─ Subscribe via UDP
  │  └─ Unsubscribe from XTS
  └─ Clear fallback flag
```

---

## 5. COMMUNICATION PATTERNS

### 5.1 Callbacks vs Signals/Slots

| Use Case | Pattern | Rationale |
|----------|---------|-----------|
| **HTTP API Responses** | Callbacks (std::function) | - Short-lived threads<br>- No UI interaction<br>- Flexibility |
| **WebSocket Ticks** | Hybrid (callback → signal) | - Callback for parsing (WS thread)<br>- Signal to notify UI |
| **UDP Ticks** | Direct publish | - Lock-free publish to FeedHandler<br>- FeedHandler emits signals |
| **UI Updates** | Signals/Slots | - Thread-safe by design<br>- Qt event loop handles queuing |

### 5.2 Pattern Examples

**Pattern 1: HTTP Request (Signal/Slot)**
```cpp
// In UI thread (setup)
connect(xtsClient, &XTSMarketDataClient::loginCompleted,
        this, [this](bool success, const QString& error) {
    // This executes on UI thread (automatically queued)
    if (success) {
        statusLabel->setText("Logged in");
    } else {
        showError(error);
    }
}, Qt::QueuedConnection);

// Trigger login
xtsClient->login();
```

**Pattern 2: Real-Time Market Data (Signal)**
```cpp
// In UI thread (setup)
connect(FeedHandler::instance().getPublisher(exchangeSegment, token),
        &TokenPublisher::udpTickUpdated,
        this,
        &MarketWatchWindow::onTickUpdate,
        Qt::QueuedConnection);  // CRITICAL: Queued for thread safety

// In UDP thread (publish)
void UdpBroadcastService::onPacket(const UDP::MarketTick& tick) {
    PriceCache::instance().update(tick);
    FeedHandler::instance().publish(tick);  // Emits signal
}

// In UI thread (receive)
void MarketWatchWindow::onTickUpdate(const UDP::MarketTick& tick) {
    // Safely update QTableView
    updateRow(tick.token, tick.ltp);
}
```

**Pattern 3: Inter-Service Communication (Direct Call)**
```cpp
// Services in same thread can call directly
void LoginFlowService::afterSuccessfulLogin() {
    auto& cache = PriceCache::instance();
    cache.clear();  // Safe: Both on main thread
    
    // Spawn worker for heavy operation
    m_masterLoader->start();
}
```

### 5.3 Thread-Safe Communication Checklist

**Before calling a method, ask:**
1. ✅ **Same thread?** → Direct call (fastest)
2. ✅ **Different thread, non-UI?** → Emit signal (Qt handles queuing)
3. ✅ **Different thread, UI target?** → Emit signal with Qt::QueuedConnection
4. ❌ **Never:** Direct UI widget manipulation from worker thread

**Signal/Slot Connection Types:**
- **Qt::QueuedConnection**: Cross-thread, always thread-safe (use for worker→UI)
- **Qt::DirectConnection**: Same-thread, no queuing (use within UI thread)
- **Qt::AutoConnection**: Qt decides based on thread affinity (generally safe but be explicit)
- **Qt::BlockingQueuedConnection**: Caller waits for slot completion (avoid in most cases)

---

## 6. CRITICAL SYNCHRONIZATION POINTS

### 6.1 PriceCache Updates

**Scenario**: UDP thread writes, UI thread reads

**Solution**:
```cpp
// Writer (UDP thread)
void PriceCache::update(const UDP::MarketTick& tick) {
    auto* data = getSegmentData(tick.exchangeSegment, tick.token);
    
    // No lock needed: UDP threads have exclusive segment ownership
    data->lastTradedPrice = tick.ltp;
    data->bidPrice[0] = tick.bidPrice;
    data->askPrice[0] = tick.askPrice;
    // ... copy other fields
    
    data->lastUpdateSeq.fetch_add(1, std::memory_order_release);  // Atomic flag
}

// Reader (UI thread)
const ConsolidatedMarketData* PriceCache::get(int segment, int token) const {
    auto* data = getSegmentData(segment, token);
    
    uint64_t seq1 = data->lastUpdateSeq.load(std::memory_order_acquire);
    // Read data fields...
    uint64_t seq2 = data->lastUpdateSeq.load(std::memory_order_acquire);
    
    if (seq1 != seq2) {
        // Concurrent update occurred, retry
        return get(segment, token);  // Rare, acceptable recursion
    }
    
    return data;
}
```

### 6.2 FeedHandler Subscriptions

**Scenario**: UI subscribes while UDP publishes

**Solution**:
```cpp
class FeedHandler {
    mutable std::shared_mutex m_mutex;
    
    void subscribe(int segment, int token, QObject* receiver, const char* slot) {
        std::unique_lock lock(m_mutex);  // Exclusive write
        // Modify subscription map...
    }
    
    void publish(const UDP::MarketTick& tick) {
        std::shared_lock lock(m_mutex);  // Shared read
        int64_t key = makeKey(tick.exchangeSegment, tick.token);
        auto it = m_subscriptions.find(key);
        if (it != m_subscriptions.end()) {
            for (auto& sub : it->second) {
                sub.publisher->publish(tick);  // Emit signal
            }
        }
    }
};
```

**Deadlock Prevention**:
- Never acquire PriceCache lock while holding FeedHandler lock
- Publish order: Update PriceCache → Publish to FeedHandler
- Read order: Check FeedHandler → Read PriceCache

### 6.3 WebSocket Reconnection

**Scenario**: UI triggers reconnect while callbacks are executing

**Solution**:
```cpp
class XTSMarketDataClient {
    std::atomic<bool> m_shouldStop{false};
    std::mutex m_wsMutex;
    
    void disconnectWebSocket() {
        m_shouldStop.store(true);  // Signal thread to stop
        
        {
            std::lock_guard lock(m_wsMutex);
            if (m_wsClient) {
                m_wsClient->close();  // Graceful shutdown
            }
        }
        
        if (m_wsThread.joinable()) {
            m_wsThread.join();  // Wait for thread termination
        }
    }
    
    void onWSMessage(const std::string& msg) {
        if (m_shouldStop.load()) {
            return;  // Early exit if shutting down
        }
        
        // Process message...
    }
};
```

---

## 7. ERROR HANDLING & RESILIENCE

### 7.1 UDP Receiver Failures

**Failure Modes**:
1. Socket bind failure (port in use)
2. Multicast join failure (network config)
3. Packet corruption (LZO decompress failure)
4. Data staleness (no packets for >5s)

**Recovery Strategy**:
```cpp
void UdpBroadcastService::udpThreadFunc(ExchangeReceiver receiver) {
    while (m_running[receiver].load()) {
        try {
            int result = socket.recv(buffer, sizeof(buffer), timeout=1000ms);
            
            if (result == 0) {
                // Timeout - check staleness
                if (now() - lastPacketTime > 5s) {
                    emit dataStale(receiver);
                }
                continue;
            }
            
            // Process packet...
            
        } catch (const std::exception& e) {
            LOG_ERROR << "UDP error: " << e.what();
            
            // Attempt recovery
            if (++errorCount > 10) {
                emit receiverFailed(receiver);
                break;  // Exit thread
            }
            
            std::this_thread::sleep_for(1s);  // Backoff
        }
    }
}
```

**UI Response**:
```cpp
void MainWindow::onReceiverFailed(ExchangeReceiver receiver) {
    // Show notification
    showWarning(QString("NSE FO UDP feed lost. Switching to XTS fallback."));
    
    // Trigger fallback
    SubscriptionManager::instance().activateXtsFallback(receiver);
}
```

### 7.2 XTS WebSocket Disconnection

**Auto-Reconnection Logic**:
```cpp
void XTSMarketDataClient::onWSDisconnected(const std::string& reason) {
    emit wsConnectionStatusChanged(false, QString::fromStdString(reason));
    
    if (!m_shouldStop.load()) {
        // Exponential backoff
        int delay = std::min(m_reconnectDelay, 30000);  // Max 30s
        m_reconnectDelay *= 2;
        
        QTimer::singleShot(delay, this, [this]() {
            connectWebSocket();  // Will emit wsConnectionStatusChanged when done
        });
    }
}

// In initialization, connect to handle reconnection success
void XTSMarketDataClient::setupConnections() {
    connect(this, &XTSMarketDataClient::wsConnectionStatusChanged,
            this, [this](bool connected, const QString& error) {
        if (connected) {
            m_reconnectDelay = 1000;  // Reset backoff
            resubscribeAllInstruments();
        }
    });
    }
}

// In initialization, connect to handle reconnection success
void XTSMarketDataClient::setupConnections() {
    connect(this, &XTSMarketDataClient::wsConnectionStatusChanged,
            this, [this](bool connected, const QString& error) {
        if (connected) {
            m_reconnectDelay = 1000;  // Reset backoff
            resubscribeAllInstruments();
        }
    });
}
```

### 7.3 Master File Loading Failures

**Graceful Degradation**:
```cpp
void MasterLoaderWorker::run() {
    try {
        loadNseCmMaster();  // 2-3 seconds
        emit progressUpdate(25, "NSE CM loaded");
    } catch (const FileNotFoundException&) {
        LOG_ERROR << "NSE CM master not found";
        emit segmentFailed(NSECM);
        // Continue with other segments
    }
    
    try {
        loadNseFoMaster();
        emit progressUpdate(50, "NSE FO loaded");
    } catch (...) {
        emit segmentFailed(NSEFO);
    }
    
    // Best-effort: Load what we can
}
```

---

## 8. PERFORMANCE OPTIMIZATION STRATEGIES

### 8.1 Hot Path Optimization

**Market Data Update (Target: <1μs per tick)**
```
UDP recv() → Parse → Cache update → FeedHandler publish
  [OS]      [50ns]   [200ns]       [500ns]
          
Total: ~1μs (excluding network latency)
```

**Optimization Techniques**:
1. **Zero-copy**: Direct memcpy to cache (no intermediate allocations)
2. **Lock-free**: Atomic sequence numbers for read validation
3. **Inline functions**: Force inline for parse helpers
4. **Cache alignment**: 512-byte cache lines prevent false sharing

### 8.2 Memory Layout

**Cache-Friendly Structure**:
```cpp
// BAD: 
struct BadTick {
    int32_t ltp;
    std::string symbol;  // Heap allocation!
    int32_t volume;
    std::vector<int32_t> bidPrices;  // Another allocation!
};

// GOOD:
struct GoodTick {
    int32_t ltp;
    int32_t volume;
    int32_t bidPrices[5];  // Fixed-size array
    char symbol[32];       // Stack storage
};
```

### 8.3 Subscription Scaling

**Problem**: 10K subscriptions = 10K hash lookups per UDP packet

**Solution**: Bloom filter pre-check
```cpp
void FeedHandler::publish(const UDP::MarketTick& tick) {
    // Fast negative check (false positive rate <1%)
    if (!m_bloomFilter.contains(tick.token)) {
        return;  // 99% of tokens have no subscribers
    }
    
    // Slow hash lookup only for subscribed tokens
    std::shared_lock lock(m_mutex);
    auto it = m_subscriptions.find(makeKey(tick.exchangeSegment, tick.token));
    // ...
}
```

---

## 9. IMPLEMENTATION ROADMAP

### Phase 1: Core Infrastructure (Week 1-2)
- [ ] PriceCache redesign with zero-copy architecture
- [ ] FeedHandler pub/sub implementation
- [ ] UdpBroadcastService refactoring (per-segment threads)
- [ ] Thread safety audit & mutex placement

### Phase 2: Data Source Integration (Week 3-4)
- [ ] UDP receiver integration with FeedHandler
- [ ] XTS WebSocket integration with FeedHandler
- [ ] SubscriptionManager implementation
- [ ] Fallback logic testing

### Phase 3: UI Integration (Week 5-6)
- [ ] MarketWatch signal/slot migration
- [ ] OrderBook/TradeBook thread-safe updates
- [ ] IndicesView real-time updates
- [ ] Performance profiling & optimization

### Phase 4: Production Hardening (Week 7-8)
- [ ] Error handling & recovery
- [ ] Logging & diagnostics
- [ ] Load testing (10K+ subscriptions)
- [ ] Memory leak detection

---

## 10. TESTING STRATEGY

### 10.1 Unit Tests
- PriceCache: Concurrent read/write stress test
- FeedHandler: Subscription lifecycle
- UdpBroadcastService: Multi-segment coordination

### 10.2 Integration Tests
- UDP → Cache → FeedHandler → UI (end-to-end)
- XTS fallback scenarios
- Master file loading with UI progress

### 10.3 Load Tests
- 10K instrument subscriptions
- 1000 ticks/second throughput
- WebSocket reconnection under load

### 10.4 Thread Safety Tests
- ThreadSanitizer (TSan) runs
- Helgrind (Valgrind) analysis
- Race condition detection

---

## 11. KEY DECISIONS & RATIONALE

| Decision | Rationale |
|----------|-----------|
| **std::thread for UDP** | Full control, no Qt overhead |
| **Qt signals for UI** | Thread-safe by design, idiomatic |
| **Callbacks for HTTP** | Flexibility, short-lived threads |
| **std::shared_mutex** | Readers-writer pattern (many readers, few writers) |
| **Zero-copy cache** | Minimize latency (target: <1μs) |
| **Per-segment threads** | Isolation, no shared state |
| **Boost.Asio for WebSocket** | Industry-standard, high-performance |
| **FeedHandler singleton** | Single point of routing, centralized control |

---

## 12. ANTI-PATTERNS TO AVOID

### ❌ DON'T:
1. **Block UI thread**: Never do I/O or heavy computation on main thread
2. **Recursive locks**: Use std::lock_guard, not std::recursive_mutex
3. **Shared pointers everywhere**: Use references for known lifetimes
4. **Over-engineering**: Start simple, optimize hot paths only
5. **Direct widget access from threads**: Always use signals or invokeMethod
6. **Ignoring Qt connection type**: Default is AutoConnection (can be unsafe)
7. **Mutex in hot path**: Use atomics or lock-free structures

### ✅ DO:
1. **Design threading upfront**: Don't retrofit thread safety
2. **Profile before optimizing**: Measure, don't guess
3. **Use RAII**: std::lock_guard, std::unique_lock for automatic unlocking
4. **Explicit connection types**: Specify Qt::QueuedConnection for cross-thread
5. **Const-correctness**: Helps compiler optimize and catch threading bugs
6. **Atomic flags for status**: std::atomic<bool> for running/stopped state
7. **Log with context**: Include thread ID in log messages

---

## 13. CRITICAL CODE PATTERNS

### Pattern 1: Safe Cross-Thread Signal Emission
```cpp
// In worker thread
void UdpReceiverThread::onPacket(const UDP::MarketTick& tick) {
    // Process on worker thread
    PriceCache::instance().update(tick);
    
    // Emit signal (automatically queued to UI thread)
    emit tickReceived(tick);  // Safe if receiver is on UI thread
}

// In main thread (setup)
void MarketWatchWindow::subscribeToToken(int token) {
    auto* publisher = FeedHandler::instance().getPublisher(segment, token);
    
    connect(publisher, &TokenPublisher::udpTickUpdated,
            this, &MarketWatchWindow::onTick,
            Qt::QueuedConnection);  // EXPLICIT queuing
}
```

### Pattern 2: Thread-Safe Initialization
```cpp
class ServiceManager {
    static ServiceManager& instance() {
        static ServiceManager inst;  // C++11 thread-safe static
        return inst;
    }
    
private:
    ServiceManager() {
        // Initialize services in dependency order
        PriceCache::instance();        // First
        FeedHandler::instance();       // Second (depends on cache)
        UdpBroadcastService::instance();  // Third (publishes to handler)
    }
};
```

### Pattern 3: Graceful Thread Shutdown
```cpp
class UdpBroadcastService {
    void stop() {
        // 1. Set atomic flag
        m_nseFoRunning.store(false);
        
        // 2. Wake up blocking recv() (if needed)
        // socket.close(); // Or send shutdown signal
        
        // 3. Wait for thread termination
        if (m_nseFoThread.joinable()) {
            m_nseFoThread.join();
        }
        
        // 4. Clean up resources
        m_nseFoSocket.reset();
    }
};
```

---

## 14. DEBUGGING & DIAGNOSTICS

### 14.1 Thread-Aware Logging
```cpp
#define LOG_DEBUG qDebug() << "[" << QThread::currentThread() << "]"
#define LOG_MARKET LOG_DEBUG << "[MARKET]"
#define LOG_UDP LOG_DEBUG << "[UDP]"

// Usage:
LOG_UDP << "Received packet for token" << token;
// Output: [0x7f8a2c000b70] [UDP] Received packet for token 12345
```

### 14.2 Performance Metrics
```cpp
struct FeedHandlerMetrics {
    std::atomic<uint64_t> publishCalls{0};
    std::atomic<uint64_t> totalPublishTimeNs{0};
    
    void recordPublish(uint64_t durationNs) {
        publishCalls.fetch_add(1);
        totalPublishTimeNs.fetch_add(durationNs);
    }
    
    double avgLatencyUs() const {
        return (totalPublishTimeNs.load() / publishCalls.load()) / 1000.0;
    }
};
```

### 14.3 Deadlock Detection
```cpp
// Enable thread sanitizer during development
// CMakeLists.txt:
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=thread -g")

// Runtime check
#ifdef DEBUG
#define LOCK_GUARD(mutex) \
    std::lock_guard<std::mutex> lock(mutex); \
    LOG_DEBUG << "Acquired lock on" << #mutex;
#else
#define LOCK_GUARD(mutex) std::lock_guard<std::mutex> lock(mutex);
#endif
```

---

## 15. MIGRATION CHECKLIST

### From Current Architecture to v2.0

- [ ] **Step 1**: Implement new PriceCache (parallel to old)
- [ ] **Step 2**: Implement FeedHandler with TokenPublisher
- [ ] **Step 3**: Refactor UdpBroadcastService to use FeedHandler
- [ ] **Step 4**: Refactor XTSMarketDataClient to use FeedHandler
- [ ] **Step 5**: Migrate MarketWatch to new signal/slot pattern
- [ ] **Step 6**: Migrate other windows (OrderBook, TradeBook)
- [ ] **Step 7**: Remove old PriceCache and direct connections
- [ ] **Step 8**: Performance testing & validation
- [ ] **Step 9**: Production deployment

**Rollback Plan**: Keep old PriceCache until new system is validated

---

## 16. CONCLUSION

This architecture provides:
1. **Clear separation**: Data acquisition → Storage → Distribution → UI
2. **Thread safety**: Designed-in via Qt signals/slots throughout
3. **Performance**: Sub-microsecond data path with zero-copy cache
4. **Scalability**: Supports 10K+ subscriptions
5. **Reliability**: Automatic fallback and recovery
6. **Consistency**: Pure Qt signal/slot communication eliminates callback complexity

**Key Benefits of Signal/Slot Architecture:**
- Automatic thread-safe queuing (no manual QMetaObject::invokeMethod)
- Type-safe connections checked at compile time
- Natural Qt idioms throughout codebase
- Easier debugging (Qt Creator shows signal/slot connections)
- Consistent error handling patterns

**Next Steps**: Review this document with team, get consensus, start Phase 1 implementation.

---

**Document Version**: 2.0  
**Last Updated**: 2026-01-16  
**Status**: Architecture Proposal (Pending Implementation)
