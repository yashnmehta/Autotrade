# Architecture Analysis & Recommendations

## Executive Summary

This document provides a comprehensive analysis of the current trading terminal architecture, focusing on the add-scrip-to-market-watch flow, identifies architectural issues, and proposes concrete improvements.

**Date**: 16 December 2025  
**Status**: Analysis Complete - Implementation Pending

---

## Current Architecture Assessment

### Component Overview

```
┌──────────────────────────────────────────────────────────────┐
│                        MainWindow                            │
│  Role: Orchestrator (❌ Too many responsibilities)           │
│  - Manages MDI area                                          │
│  - Connects signals between components                       │
│  - Maps InstrumentData ↔ ScripData                          │
│  - Handles XTS API callbacks                                 │
│  - Broadcasts price updates to all windows                   │
└──────────────────────────────────────────────────────────────┘
         │               │                    │
         ↓               ↓                    ↓
┌──────────────┐  ┌──────────────┐   ┌─────────────────────┐
│   ScripBar   │  │MarketWatch   │   │XTSMarketDataClient  │
│ (Search UI)  │  │  Windows     │   │  (API + WebSocket)  │
└──────────────┘  └──────────────┘   └─────────────────────┘
```

### Critical Issues Identified

#### 1. **Violation of Single Responsibility Principle**

**Problem**: MainWindow does too much
- UI orchestration (MDI management)
- Business logic (data transformation)
- API coordination (calling XTS methods)
- Event routing (broadcasting ticks)

**Impact**: 
- Hard to test individual components
- Changes ripple through MainWindow
- Can't reuse logic in different contexts

---

#### 2. **No Service Layer**

**Problem**: Business logic scattered across UI components

Current flow:
```cpp
// MainWindow.cpp (UI class doing business logic)
void MainWindow::onAddToWatchRequested(const InstrumentData &inst) {
    // Data transformation (business logic)
    ScripData data = mapToScripData(inst);
    
    // API calls (business logic)
    m_xtsClient->subscribe(ids, segment, callback);
    
    // UI updates (mixed with above)
    marketWatch->addScripFromContract(data);
}
```

**Should be**:
```cpp
// MainWindow.cpp (pure UI orchestration)
void MainWindow::onAddToWatchRequested(const InstrumentData &inst) {
    MarketWatchWindow *mw = getActiveMarketWatch();
    m_marketDataService->addInstrumentToWatch(inst, mw);
}

// MarketDataService.cpp (business logic)
void MarketDataService::addInstrumentToWatch(const InstrumentData &inst, 
                                            MarketWatchWindow *window) {
    // All complexity hidden here
    ScripData data = transform(inst);
    validateAndAdd(data, window);
    subscribeToUpdates(inst);
    handleCachedPrice(inst);
}
```

---

#### 3. **Missing Price Cache** (PARTIALLY SOLVED ✅)

**Problem**: Already-subscribed tokens don't get initial price

**Scenario**:
```
Window 1: Add NIFTY → Subscribe API returns listQuotes → LTP: 25960 ✅
Window 2: Add NIFTY → Subscribe API returns HTTP 400 e-session-0002 → LTP: 0.00 ❌

// User sees 0.00 until next WebSocket tick arrives
```

**Root Cause**: XTS API behavior (NOW UNDERSTOOD)
- First subscription: Returns HTTP 200 with initial snapshot in `listQuotes`
- Re-subscription: Returns HTTP 400 with error code `e-session-0002` ("Instrument Already Subscribed !")

**Current Solution** (Implemented Dec 16, 2025):
```cpp
// Detect "Already Subscribed" error and fallback to getQuote
if (httpStatus == 400 && obj["code"].toString() == "e-session-0002") {
    qDebug() << "⚠️ Instrument already subscribed - fetching snapshot via getQuote";
    
    // Fetch current price via getQuote (now working with publishFormat)
    getQuote(instrumentID, exchangeSegment, [this](bool ok, const QJsonObject& quote, const QString& msg) {
        if (ok) {
            processTickData(quote);  // Update UI with current price
        }
    });
}
```

**Impact**: Problem SOLVED for getQuote fallback, but still need global cache for optimal performance

**Solution**: Global price cache
```cpp
class PriceCache {
    QMap<int, XTS::Tick> m_lastKnownPrices;
    QMap<int, QDateTime> m_timestamps;
    
    void update(int token, const XTS::Tick &tick);
    std::optional<XTS::Tick> get(int token) const;
    bool isStale(int token, int maxAgeSeconds = 60) const;
};

// Usage in subscribe callback
if (listQuotes.isEmpty()) {
    // Use cached price instead of waiting
    auto cached = PriceCache::instance()->get(token);
    if (cached.has_value()) {
        updatePrice(token, cached->lastTradedPrice, ...);
    }
}
```

---

#### 4. **No State Management**

**Problem**: Can't track what's happening (loading, error, success)

Current: Binary state (added or not added)
```cpp
bool added = marketWatch->addScripFromContract(data);
// User doesn't know: Is it loading? Did it fail? Is it waiting for data?
```

**Needed**: Explicit states
```cpp
enum class SubscriptionState {
    NotSubscribed,
    Subscribing,      // API call in progress → Show "Loading..."
    WaitingForData,   // Subscribed but no price yet → Show "Waiting..."
    Active,           // Receiving updates → Show actual price
    Error,            // Failed → Show error icon
    Stale            // No updates for X seconds → Show warning
};
```

**UI Benefits**:
- Loading indicators
- Error messages
- User feedback
- Timeout detection

---

#### 5. **No Error Recovery**

**Problem**: API failures are silent or terminal

Current:
```cpp
m_xtsClient->subscribe(ids, segment, [](bool ok, const QString &msg) {
    if (!ok) {
        qWarning() << "Subscribe failed:" << msg;  // ❌ Just logs, no retry
    }
});
```

**Needed**: Retry with exponential backoff
```cpp
class RetryHelper {
    void retryWithBackoff(std::function<void()> operation,
                         int maxRetries = 3,
                         std::vector<int> delays = {1000, 2000, 4000}) {
        // Retry logic with increasing delays
    }
};

// Usage
m_retryHelper->retryWithBackoff([this, token]() {
    m_xtsClient->subscribe({token}, segment, callback);
}, 3);  // Try 3 times before giving up
```

---

#### 6. **Tight Coupling to XTS**

**Problem**: XTS-specific logic spread throughout codebase

Examples:
- Socket.IO packet parsing in XTSMarketDataClient
- Touchline nested structure parsing
- Message code 1502 hardcoded
- Exchange segment mapping

**Impact**: Can't easily switch to different broker API

**Solution**: Provider abstraction
```cpp
// Generic interface
class IMarketDataProvider {
public:
    virtual void subscribe(const QVector<int64_t> &tokens, 
                          std::function<void(bool)> callback) = 0;
    virtual void getQuote(int64_t token,
                         std::function<void(const Quote&)> callback) = 0;
    virtual void connect(const ConnectionParams &params) = 0;
};

// XTS implementation
class XTSProvider : public IMarketDataProvider {
    // All XTS-specific details hidden here
};

// Easy to add new providers
class ZerodhaProvider : public IMarketDataProvider { };
class AliceBlueProvider : public IMarketDataProvider { };
```

---

#### 7. **No Comprehensive Testing**

**Problem**: Manual testing only, no automated tests

**Missing**:
- Unit tests for data transformations
- Integration tests for API flows
- Mock providers for testing without live API
- Test coverage tracking

**Needed**:
```cpp
// tests/test_market_data_service.cpp
TEST(MarketDataService, AddInstrument_FirstTime_ReturnsSnapshot) {
    MockXTSProvider mockProvider;
    mockProvider.setSubscribeResponse({{"listQuotes": ["..."]}});
    
    MarketDataService service(&mockProvider);
    service.addInstrument(testInstrument, window);
    
    EXPECT_TRUE(window->hasScrip(testToken));
    EXPECT_GT(window->getPrice(testToken), 0.0);
}

TEST(MarketDataService, AddInstrument_AlreadySubscribed_UsesCachedPrice) {
    // First add
    service.addInstrument(testInstrument, window1);
    double price1 = window1->getPrice(testToken);
    
    // Second add (already subscribed)
    service.addInstrument(testInstrument, window2);
    double price2 = window2->getPrice(testToken);
    
    EXPECT_EQ(price1, price2);  // Should use cached price
}
```

---

## Proposed Architecture

### New Component Structure

```
┌──────────────────────────────────────────────────────────────┐
│                        MainWindow                            │
│  Role: Pure UI orchestration                                 │
│  - Manages MDI windows                                       │
│  - Routes UI events to services                              │
│  - NO business logic                                         │
└──────────────────────────────────────────────────────────────┘
                                │
                                ↓
┌──────────────────────────────────────────────────────────────┐
│                   MarketDataService                          │
│  Role: Business logic & coordination                         │
│  - Add/remove instruments                                    │
│  - Subscribe/unsubscribe                                     │
│  - Handle price caching                                      │
│  - Manage subscription state                                 │
│  - Retry logic                                               │
└──────────────────────────────────────────────────────────────┘
         │                    │                    │
         ↓                    ↓                    ↓
┌──────────────┐    ┌─────────────────┐    ┌───────────────┐
│  PriceCache  │    │IMarketDataProvider│   │ StateManager  │
│              │    │   (interface)    │    │               │
└──────────────┘    └─────────────────┘    └───────────────┘
                            │
                            ↓
                    ┌───────────────┐
                    │  XTSProvider  │
                    │  (concrete)   │
                    └───────────────┘
```

### Layer Responsibilities

#### 1. **Presentation Layer** (UI Components)
- ScripBar: Search UI
- MarketWatchWindow: Table display
- MainWindow: Window management
- **NO business logic**

#### 2. **Service Layer** (NEW)
- MarketDataService: Core business logic
- PriceCache: Price storage & retrieval
- StateManager: Subscription state tracking
- RetryHelper: Error recovery

#### 3. **Provider Layer** (Abstracted)
- IMarketDataProvider: Generic interface
- XTSProvider: XTS-specific implementation
- MockProvider: For testing

#### 4. **Data Layer**
- Models: MarketWatchModel, etc.
- TokenAddressBook: Token → Row mapping
- RepositoryManager: Contract master data

---

## Implementation Plan

### Phase 1: Critical Fixes (Week 1)

#### 1.1 Implement Price Cache
```cpp
// src/services/PriceCache.h
class PriceCache : public QObject {
    Q_OBJECT
public:
    static PriceCache* instance();
    
    void updatePrice(int token, const XTS::Tick &tick);
    std::optional<XTS::Tick> getPrice(int token) const;
    bool hasPrice(int token) const;
    void clearStale(int maxAgeSeconds = 300);
    
signals:
    void priceUpdated(int token, const XTS::Tick &tick);
    
private:
    struct CachedPrice {
        XTS::Tick tick;
        QDateTime timestamp;
    };
    QMap<int, CachedPrice> m_cache;
    mutable QReadWriteLock m_lock;  // Thread safety
};
```

**Integration**:
```cpp
// In XTSMarketDataClient::subscribe callback
for (quote : listQuotes) {
    XTS::Tick tick = parseQuote(quote);
    PriceCache::instance()->updatePrice(tick.exchangeInstrumentID, tick);
    processTickData(tick);
}

// In XTSMarketDataClient::processTickData
void XTSMarketDataClient::processTickData(const QJsonObject &json) {
    XTS::Tick tick = parseTick(json);
    PriceCache::instance()->updatePrice(tick.exchangeInstrumentID, tick);
    emit tickReceived(tick);
}

// In MainWindow::onAddToWatchRequested
if (added) {
    // Try to use cached price first
    auto cached = PriceCache::instance()->getPrice(instrument.exchangeInstrumentID);
    if (cached.has_value()) {
        double change = cached->lastTradedPrice - cached->close;
        double changePercent = (cached->close != 0) ? (change / cached->close) * 100.0 : 0.0;
        marketWatch->updatePrice(instrument.exchangeInstrumentID, 
                                cached->lastTradedPrice, change, changePercent);
    }
    
    // Then subscribe (will update again if price changed)
    m_xtsClient->subscribe({instrument.exchangeInstrumentID}, instrument.exchangeSegment, callback);
}
```

**Files to create**:
- `include/services/PriceCache.h`
- `src/services/PriceCache.cpp`

**Testing**:
```cpp
// Manual test: Add NIFTY to Window 1, then add NIFTY to Window 2
// Expected: Window 2 shows price immediately (from cache)
```

---

#### 1.2 Add State Management
```cpp
// src/services/SubscriptionStateManager.h
enum class SubscriptionState {
    NotSubscribed,
    Subscribing,
    WaitingForData,
    Active,
    Error,
    Stale
};

class SubscriptionStateManager : public QObject {
    Q_OBJECT
public:
    static SubscriptionStateManager* instance();
    
    void setState(int token, SubscriptionState state, const QString &message = "");
    SubscriptionState getState(int token) const;
    QString getMessage(int token) const;
    
    void markStale(int token);
    void clearStale();
    
signals:
    void stateChanged(int token, SubscriptionState state, const QString &message);
    
private:
    struct State {
        SubscriptionState state;
        QString message;
        QDateTime lastUpdate;
    };
    QMap<int, State> m_states;
};
```

**UI Integration**:
```cpp
// In MarketWatchModel::data()
if (role == Qt::DisplayRole && column == COL_LTP) {
    SubscriptionState state = SubscriptionStateManager::instance()->getState(scrip.token);
    
    switch (state) {
        case SubscriptionState::Subscribing:
            return "Loading...";
        case SubscriptionState::WaitingForData:
            return "Waiting...";
        case SubscriptionState::Error:
            return "Error";
        case SubscriptionState::Active:
            return QString::number(scrip.ltp, 'f', 2);
        case SubscriptionState::Stale:
            return QString::number(scrip.ltp, 'f', 2) + " ⚠";
        default:
            return "-";
    }
}
```

**Files to create**:
- `include/services/SubscriptionStateManager.h`
- `src/services/SubscriptionStateManager.cpp`

---

#### 1.3 Add Retry Logic
```cpp
// src/utils/RetryHelper.h
class RetryHelper : public QObject {
    Q_OBJECT
public:
    using Operation = std::function<void(std::function<void(bool)>)>;
    
    void retryWithBackoff(Operation operation,
                         std::function<void(bool)> finalCallback,
                         int maxRetries = 3,
                         QVector<int> delaysMs = {1000, 2000, 4000});
    
private:
    struct RetryContext {
        Operation operation;
        std::function<void(bool)> finalCallback;
        int retriesLeft;
        QVector<int> delaysMs;
    };
    
    void executeWithRetry(RetryContext context);
};
```

**Usage**:
```cpp
// In MainWindow::onAddToWatchRequested
m_retryHelper->retryWithBackoff(
    [this, ids, segment](auto callback) {
        m_xtsClient->subscribe(ids, segment, callback);
    },
    [token](bool success) {
        if (success) {
            qDebug() << "Subscribed successfully";
        } else {
            qWarning() << "Failed after all retries";
            SubscriptionStateManager::instance()->setState(token, 
                SubscriptionState::Error, "Failed to subscribe");
        }
    },
    3  // Max retries
);
```

**Files to create**:
- `include/utils/RetryHelper.h`
- `src/utils/RetryHelper.cpp`

---

### Phase 2: Service Layer (Week 2)

#### 2.1 Create MarketDataService
```cpp
// include/services/MarketDataService.h
class MarketDataService : public QObject {
    Q_OBJECT
public:
    explicit MarketDataService(IMarketDataProvider *provider, QObject *parent = nullptr);
    
    // High-level operations
    void addInstrumentToWatch(const InstrumentData &instrument, MarketWatchWindow *window);
    void removeInstrumentFromWatch(int token, MarketWatchWindow *window);
    void refreshPrice(int token);
    
    // Connection management
    void connectProvider();
    void disconnectProvider();
    bool isConnected() const;
    
signals:
    void instrumentAdded(int token, const ScripData &scripData);
    void priceUpdated(int token, const XTS::Tick &tick);
    void error(int token, const QString &message);
    void connectionStateChanged(bool connected);
    
private:
    void handleSubscribeSuccess(const InstrumentData &instrument, 
                               MarketWatchWindow *window,
                               const QJsonArray &listQuotes);
    void handleSubscribeFailure(int token, const QString &error);
    void applyInitialPrice(int token, MarketWatchWindow *window);
    ScripData transformToScripData(const InstrumentData &instrument) const;
    
    IMarketDataProvider *m_provider;
    PriceCache *m_priceCache;
    SubscriptionStateManager *m_stateManager;
    RetryHelper *m_retryHelper;
    QMap<int, int> m_subscriptionRefCount;  // Track usage across windows
};
```

**Implementation**:
```cpp
// src/services/MarketDataService.cpp
void MarketDataService::addInstrumentToWatch(const InstrumentData &instrument,
                                            MarketWatchWindow *window) {
    int token = instrument.exchangeInstrumentID;
    
    // 1. Set state to subscribing
    m_stateManager->setState(token, SubscriptionState::Subscribing, "Subscribing...");
    
    // 2. Transform data
    ScripData scripData = transformToScripData(instrument);
    
    // 3. Add to window
    if (!window->addScripFromContract(scripData)) {
        m_stateManager->setState(token, SubscriptionState::Error, "Failed to add");
        emit error(token, "Duplicate or invalid token");
        return;
    }
    
    // 4. Check if we have cached price
    if (m_priceCache->hasPrice(token)) {
        applyInitialPrice(token, window);
        m_stateManager->setState(token, SubscriptionState::Active, "Using cached price");
    } else {
        m_stateManager->setState(token, SubscriptionState::WaitingForData, "Waiting for price...");
    }
    
    // 5. Subscribe (or increment ref count if already subscribed)
    if (m_subscriptionRefCount.value(token, 0) == 0) {
        // First subscription for this token
        m_retryHelper->retryWithBackoff(
            [this, instrument](auto callback) {
                m_provider->subscribe({instrument.exchangeInstrumentID}, 
                                    instrument.exchangeSegment, 
                                    callback);
            },
            [this, token, instrument, window](bool success) {
                if (success) {
                    handleSubscribeSuccess(instrument, window, ...);
                } else {
                    handleSubscribeFailure(token, "Subscribe failed after retries");
                }
            }
        );
    }
    
    // 6. Increment ref count
    m_subscriptionRefCount[token]++;
    
    emit instrumentAdded(token, scripData);
}

void MarketDataService::applyInitialPrice(int token, MarketWatchWindow *window) {
    auto cached = m_priceCache->getPrice(token);
    if (cached.has_value()) {
        double change = cached->lastTradedPrice - cached->close;
        double changePercent = (cached->close != 0) ? (change / cached->close) * 100.0 : 0.0;
        window->updatePrice(token, cached->lastTradedPrice, change, changePercent);
    }
}
```

**Files to create**:
- `include/services/MarketDataService.h`
- `src/services/MarketDataService.cpp`

**MainWindow refactoring**:
```cpp
// Before (in MainWindow)
void MainWindow::onAddToWatchRequested(const InstrumentData &instrument) {
    // 50+ lines of business logic
}

// After (in MainWindow)
void MainWindow::onAddToWatchRequested(const InstrumentData &instrument) {
    MarketWatchWindow *mw = getActiveMarketWatch();
    m_marketDataService->addInstrumentToWatch(instrument, mw);
}
```

---

#### 2.2 Provider Abstraction
```cpp
// include/api/IMarketDataProvider.h
struct Quote {
    int64_t token;
    double ltp;
    double open, high, low, close;
    int64_t volume;
    double bidPrice, askPrice;
    // Normalized structure
};

class IMarketDataProvider {
public:
    virtual ~IMarketDataProvider() = default;
    
    virtual void connect(const QJsonObject &config,
                        std::function<void(bool)> callback) = 0;
    virtual void disconnect() = 0;
    virtual bool isConnected() const = 0;
    
    virtual void subscribe(const QVector<int64_t> &tokens,
                          int exchangeSegment,
                          std::function<void(bool, const QJsonArray&)> callback) = 0;
    virtual void unsubscribe(const QVector<int64_t> &tokens,
                            std::function<void(bool)> callback) = 0;
    
    virtual void getQuote(int64_t token,
                         int exchangeSegment,
                         std::function<void(bool, const Quote&)> callback) = 0;
    
signals:
    virtual void tickReceived(const Quote &quote) = 0;
    virtual void connectionStateChanged(bool connected) = 0;
    virtual void errorOccurred(const QString &error) = 0;
};
```

**XTS Implementation**:
```cpp
// src/api/XTSMarketDataProvider.cpp
class XTSMarketDataProvider : public IMarketDataProvider {
public:
    void subscribe(const QVector<int64_t> &tokens, int exchangeSegment,
                  std::function<void(bool, const QJsonArray&)> callback) override {
        // All XTS-specific logic here
        // - Build XTS request format
        // - Parse XTS response (Touchline)
        // - Handle Socket.IO protocol
        // - Convert to normalized Quote structure
    }
    
private:
    Quote parseXTSTouchline(const QJsonObject &touchline);
    // All XTS-specific helper methods
};

// src/api/UDPBroadcastProvider.cpp
class UDPBroadcastProvider : public IMarketDataProvider {
public:
    UDPBroadcastProvider(const QString &multicastGroup, quint16 port);
    
    void subscribe(const QVector<int64_t> &tokens, int exchangeSegment,
                  std::function<void(bool, const QJsonArray&)> callback) override {
        // UDP broadcast subscription
        // - Listen on configured multicast group
        // - Parse binary/FIX protocol packets
        // - Filter by subscribed tokens
        // - Convert to normalized Quote structure
    }
    
private:
    QUdpSocket *m_udpSocket;
    QSet<int64_t> m_subscribedTokens;
    
    void initUDPSocket();
    void onPacketReceived();
    Quote parseNSEBroadcast(const QByteArray &packet);
    Quote parseBSEBroadcast(const QByteArray &packet);
};

// src/api/HybridMarketDataProvider.cpp
class HybridMarketDataProvider : public IMarketDataProvider {
public:
    // Uses UDP for NSE/BSE, XTS for MCX (no UDP available) and fallback
    void subscribe(const QVector<int64_t> &tokens, int exchangeSegment,
                  std::function<void(bool, const QJsonArray&)> callback) override {
        if (shouldUseUDP(exchangeSegment) && m_config.value("useUDP").toBool()) {
            m_udpProvider->subscribe(tokens, exchangeSegment, callback);
        } else {
            m_xtsProvider->subscribe(tokens, exchangeSegment, callback);
        }
    }
    
private:
    UDPBroadcastProvider *m_udpProvider;
    XTSMarketDataProvider *m_xtsProvider;
    QJsonObject m_config;  // From config.json
    
    bool shouldUseUDP(int exchangeSegment) const {
        // NSE and BSE have UDP, MCX doesn't (not yet developed)
        return (exchangeSegment == 1 || exchangeSegment == 2 ||   // NSECM, NSEFO
                exchangeSegment == 11 || exchangeSegment == 12 ||  // BSECM, BSEFO
                exchangeSegment == 13 || exchangeSegment == 61);   // NSECD, BSECD
    }
};
```

**Benefits**:
- Can swap to different broker easily
- XTS details isolated
- **Exchange UDP broadcast for ultra-low latency (<1ms)**
- **Hybrid approach: UDP for NSE/BSE, XTS for MCX**
- **Configurable per exchange segment**
- Easy to mock for testing

**Files to create**:
- `include/api/IMarketDataProvider.h`
- `include/api/XTSMarketDataProvider.h`
- `src/api/XTSMarketDataProvider.cpp`
- **`include/api/UDPBroadcastProvider.h` (NEW - Direct exchange feed)**
- **`src/api/UDPBroadcastProvider.cpp` (NEW)**
- **`include/api/HybridMarketDataProvider.h` (NEW - Smart routing)**
- **`src/api/HybridMarketDataProvider.cpp` (NEW)**

---

### Phase 3: Testing & Polish (Week 3)

#### 3.1 Unit Tests
```cpp
// tests/test_price_cache.cpp
TEST(PriceCache, UpdateAndGet) {
    PriceCache cache;
    XTS::Tick tick;
    tick.exchangeInstrumentID = 12345;
    tick.lastTradedPrice = 100.50;
    
    cache.updatePrice(12345, tick);
    
    auto retrieved = cache.getPrice(12345);
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_DOUBLE_EQ(retrieved->lastTradedPrice, 100.50);
}

TEST(PriceCache, ClearStale) {
    PriceCache cache;
    // Add price
    cache.updatePrice(12345, tick);
    
    // Wait 2 seconds
    QTest::qWait(2000);
    
    // Clear stale (max age 1 second)
    cache.clearStale(1);
    
    // Should be gone
    EXPECT_FALSE(cache.hasPrice(12345));
}
```

```cpp
// tests/test_market_data_service.cpp
class MockProvider : public IMarketDataProvider {
public:
    MOCK_METHOD(void, subscribe, (const QVector<int64_t>&, int, 
                                  std::function<void(bool, const QJsonArray&)>), (override));
    // ... other mocks
};

TEST(MarketDataService, AddInstrument_FirstTime_SubscribesAndUpdates) {
    MockProvider mockProvider;
    MarketDataService service(&mockProvider);
    
    // Setup expectation
    EXPECT_CALL(mockProvider, subscribe(_, _, _))
        .WillOnce([](auto tokens, auto segment, auto callback) {
            QJsonArray quotes;  // Empty (no initial snapshot)
            callback(true, quotes);
        });
    
    // Execute
    InstrumentData inst;
    inst.exchangeInstrumentID = 12345;
    service.addInstrumentToWatch(inst, window);
    
    // Verify
    EXPECT_TRUE(window->hasScrip(12345));
}
```

**Files to create**:
- `tests/test_price_cache.cpp`
- `tests/test_market_data_service.cpp`
- `tests/test_subscription_state_manager.cpp`
- `tests/mocks/MockMarketDataProvider.h`

---

#### 3.2 Integration Tests
```cpp
// tests/integration/test_add_scrip_flow.cpp
TEST_F(AddScripFlowTest, CompleteFlow_WithRealMockProvider) {
    // Setup
    TestXTSProvider provider;  // Test implementation that simulates XTS
    MarketDataService service(&provider);
    MainWindow mainWindow;
    mainWindow.setMarketDataService(&service);
    
    // Create market watch
    MarketWatchWindow *mw = mainWindow.createMarketWatch();
    
    // Simulate user action
    InstrumentData nifty = createNiftyFuture();
    mainWindow.onAddToWatchRequested(nifty);
    
    // Wait for async operations
    QTest::qWait(500);
    
    // Verify
    EXPECT_TRUE(mw->hasScrip(nifty.exchangeInstrumentID));
    EXPECT_GT(mw->getPrice(nifty.exchangeInstrumentID), 0.0);
    EXPECT_EQ(provider.getSubscriptionCount(), 1);
}
```

---

#### 3.3 Logging Infrastructure
```cpp
// include/utils/Logger.h
enum class LogLevel {
    DEBUG,
    INFO,
    WARNING,
    ERROR
};

enum class LogCategory {
    MKTDATA,    // Market data operations
    API,        // API calls
    UI,         // UI events
    WEBSOCKET   // WebSocket messages
};

class Logger {
public:
    static void setLevel(LogLevel level);
    static void enableCategory(LogCategory category);
    static void disableCategory(LogCategory category);
    
    static void debug(LogCategory category, const QString &message);
    static void info(LogCategory category, const QString &message);
    static void warning(LogCategory category, const QString &message);
    static void error(LogCategory category, const QString &message);
    
private:
    static LogLevel s_level;
    static QSet<LogCategory> s_enabledCategories;
};

// Usage
Logger::debug(LogCategory::MKTDATA, 
             QString("Subscribe: token=%1 segment=%2").arg(token).arg(segment));
Logger::info(LogCategory::API, 
            QString("API Response: %1 bytes").arg(response.size()));
```

---

## Migration Strategy

### Step 1: Add New Components (Non-Breaking)
1. Create PriceCache (Week 1 Day 1-2)
2. Create SubscriptionStateManager (Week 1 Day 3-4)
3. Create RetryHelper (Week 1 Day 5)
4. **Test alongside existing code** (Week 1 Day 5-7)

### Step 2: Integrate Without Breaking (Week 2)
1. Integrate PriceCache into XTSMarketDataClient
2. Add state management to MarketWatchWindow
3. Use retry logic for subscribe calls
4. **Run parallel: old flow + new helpers**

### Step 3: Create Service Layer (Week 2-3)
1. Create IMarketDataProvider interface
2. Refactor XTSMarketDataClient → XTSMarketDataProvider
3. Create MarketDataService
4. **Keep both paths working**

### Step 4: Migrate Gradually (Week 3-4)
1. Route new instrument additions through service
2. Keep old path for existing code
3. Add feature flag: `USE_SERVICE_LAYER`
4. Test extensively

### Step 5: Remove Old Code (Week 4)
1. Remove old path from MainWindow
2. Clean up redundant code
3. Update documentation

---

## Success Metrics

### Technical Metrics
- ✅ Test coverage > 80%
- ✅ All unit tests pass
- ✅ No regression in existing features
- ✅ Reduced coupling (measurable via static analysis)

### User Experience Metrics
- ✅ Already-subscribed tokens show price immediately (< 100ms)
- ✅ Loading indicators visible during subscription
- ✅ Errors displayed with retry option
- ✅ No "0.00" flash on second window

### Code Quality Metrics
- ✅ MainWindow < 500 lines (currently ~1000+)
- ✅ Clear separation of concerns
- ✅ Easy to add new broker support
- ✅ Comprehensive logging

---

## Risk Mitigation

### Risk 1: Breaking Existing Functionality
**Mitigation**: 
- Gradual migration with feature flags
- Parallel paths during transition
- Extensive integration testing

### Risk 2: Performance Regression
**Mitigation**:
- Profile before/after
- Use efficient data structures (hash maps)
- Benchmark critical paths

### Risk 3: XTS API Changes
**Mitigation**:
- Provider abstraction isolates changes
- Standalone test scripts to verify API
- Version-specific adapters if needed

---

## Conclusion

**Current State**: Functional but architectural debt accumulating

**Recommended Actions** (Priority Order):
1. **Week 1**: Implement PriceCache (fixes immediate UX issue)
2. **Week 2**: Add state management + retry logic (improves reliability)
3. **Week 3**: Create service layer (reduces coupling)
4. **Week 4**: Add tests + migrate gradually

**Expected Benefits**:
- ✅ Better user experience (no 0.00 flash)
- ✅ More reliable (retry logic)
- ✅ Easier to maintain (clear separation)
- ✅ Easier to test (mocked providers)
- ✅ Future-proof (provider abstraction)

**Effort Estimate**: 3-4 weeks with 1 developer

---

**Document Version**: 1.0  
**Author**: System Architect  
**Review Date**: 16 December 2025
