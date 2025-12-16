# Architecture Analysis & Improvement Suggestions

## Document Info

**Project**: Trading Terminal C++ (XTS Integration)  
**Date**: December 16, 2025  
**Status**: Active Development - XTS MarketData Integration Phase  
**Related Docs**: ADD_SCRIP_FLOW.md, ARCHITECTURE.md

---

## Executive Summary

The current architecture follows a **layered MVC pattern** with Qt widgets, separating UI (views), business logic (controllers), and data (models). While functional, the XTS API integration has revealed coupling issues, missing abstractions, and opportunities for improvement.

### Key Strengths ✅
- Clear separation between UI and data models
- Efficient O(1) token lookups via TokenAddressBook
- Reusable signal/slot connections
- Good use of Qt frameworks

### Key Weaknesses ⚠️
- **Tight coupling** between MainWindow and API clients
- **No service layer** - business logic scattered across UI controllers
- **Limited error handling** - API failures not gracefully handled (partially improved with e-session-0002 handling)
- **Missing abstractions** - direct dependency on XTS implementation details
- **No state management** - subscription state implicit, not explicit

### Recent Improvements (Dec 16, 2025) ✅
- **getQuote API fixed** - Added required `publishFormat: "JSON"` parameter
- **Multiple instrument support** - getQuote now handles multiple instruments in one call
- **Error handling enhanced** - Detects "Already Subscribed" (e-session-0002) and falls back to getQuote
- **API behavior documented** - Complete analysis of subscribe/getQuote/WebSocket responses

---

## Current Architecture (As-Is)

```
┌─────────────────────────────────────────────────────────────────┐
│                          UI Layer                               │
│  ┌──────────┐  ┌────────────┐  ┌─────────────┐  ┌───────────┐ │
│  │ScripBar  │  │MainWindow  │  │MarketWatch  │  │BuySell    │ │
│  │(Search)  │  │(Orchestr.) │  │  Window     │  │  Window   │ │
│  └────┬─────┘  └─────┬──────┘  └──────┬──────┘  └─────┬─────┘ │
└───────┼──────────────┼─────────────────┼───────────────┼───────┘
        │              │                 │               │
        │ ┌────────────┼─────────────────┼───────────────┼───────┐
        │ │            │    Mixed Layer  │               │       │
        │ │            │ (Controller +   │               │       │
        │ │            │  Business Logic)│               │       │
        │ │  ┌─────────▼──────┐  ┌───────▼──────┐       │       │
        │ │  │RepositoryMgr   │  │TokenSubscr.  │       │       │
        │ │  │(ContractData)  │  │  Manager     │       │       │
        │ │  └────────────────┘  └──────────────┘       │       │
        │ └────────────────────────────────────────────────────┐ │
        │                                                        │ │
        │ ┌──────────────────────────────────────────────────┐ │ │
        │ │                 Model Layer                      │ │ │
        │ │  ┌──────────────┐  ┌──────────────┐  ┌─────────┐│ │ │
        │ │  │MarketWatch   │  │TokenAddress  │  │Position │││ │ │
        │ │  │   Model      │  │    Book      │  │  Model  │││ │ │
        │ │  └──────────────┘  └──────────────┘  └─────────┘│ │ │
        │ └──────────────────────────────────────────────────┘ │ │
        │                                                        │ │
        └────────────────────────────────────────────────────────┘ │
                                                                   │
        ┌──────────────────────────────────────────────────────────┘
        │                 Data Layer (API Clients)
        │  ┌────────────────────┐  ┌──────────────────────┐
        │  │XTSMarketDataClient │  │XTSInteractiveClient  │
        │  │ (REST + WebSocket) │  │  (Orders/Trades)     │
        │  └──────────┬─────────┘  └──────────┬───────────┘
        └─────────────┼────────────────────────┼─────────────
                      │                        │
                      ▼                        ▼
                 ┌─────────────────────────────────┐
                 │     XTS API Server              │
                 │  (Market Data + Interactive)    │
                 └─────────────────────────────────┘
```

### Problem Areas

#### 1. MainWindow God Object
```cpp
class MainWindow {
    // Manages windows
    void createMarketWatchWindow();
    void createBuySellWindow();
    
    // Handles API calls
    void onAddToWatchRequested(const InstrumentData& instrument);
    void onTickReceived(const XTS::Tick& tick);
    
    // Manages client connections
    void setXTSClients(...);
    
    // Routes data to windows
    void updateAllMarketWatchWindows(...);
    
    // Plus 50+ other responsibilities...
};
```
**Issue**: Too many responsibilities - violates Single Responsibility Principle

#### 2. Direct API Coupling
```cpp
// MainWindow directly calls XTS API
m_xtsMarketDataClient->subscribe(ids, segment, callback);
m_xtsMarketDataClient->getQuote(id, segment, callback);
```
**Issue**: Changing API provider requires changes throughout UI code

#### 3. Scattered Business Logic
```cpp
// MainWindow.cpp
double change = tick.lastTradedPrice - tick.close;
double changePercent = (tick.close != 0) ? (change / tick.close) * 100.0 : 0.0;

// Also duplicated in other places...
```
**Issue**: Business logic in UI controller, not reusable

#### 4. No Error Recovery
```cpp
if (!ok) {
    qWarning() << "Subscribe failed:" << msg;
    // No retry, no fallback, no user notification
}
```
**Issue**: Silent failures, no resilience

---

## Proposed Architecture (To-Be)

```
┌───────────────────────────────────────────────────────────────────┐
│                         UI Layer (Views)                          │
│  ┌──────────┐  ┌────────────┐  ┌─────────────┐  ┌────────────┐  │
│  │ScripBar  │  │MainWindow  │  │MarketWatch  │  │BuySellWin  │  │
│  │ Widget   │  │(Container) │  │   Window    │  │   dow      │  │
│  └────┬─────┘  └─────┬──────┘  └──────┬──────┘  └─────┬──────┘  │
└───────┼──────────────┼─────────────────┼───────────────┼─────────┘
        │              │                 │               │
        │ Signal/Slot  │                 │               │
        ↓              ↓                 ↓               ↓
┌───────────────────────────────────────────────────────────────────┐
│                    Service Layer (NEW)                            │
│  ┌──────────────────┐  ┌─────────────────┐  ┌─────────────────┐ │
│  │ MarketDataSvc    │  │  TradingService │  │  OrderService   │ │
│  │ - subscribe()    │  │  - placeOrder() │  │  - modify()     │ │
│  │ - unsubscribe()  │  │  - getPnL()     │  │  - cancel()     │ │
│  │ - getQuote()     │  │  - getPosition()│  │  - getOrderBook │ │
│  └────────┬─────────┘  └────────┬────────┘  └────────┬────────┘ │
└───────────┼─────────────────────┼─────────────────────┼──────────┘
            │                     │                     │
            │ Uses                │                     │
            ↓                     ↓                     ↓
┌───────────────────────────────────────────────────────────────────┐
│                Repository Layer (Data Access)                     │
│  ┌───────────────────┐  ┌──────────────────┐  ┌────────────────┐│
│  │ MarketDataRepo    │  │  PositionRepo    │  │  OrderRepo     ││
│  │ - fetchQuote()    │  │  - getAll()      │  │  - getAll()    ││
│  │ - subscribe()     │  │  - getBySymbol() │  │  - getByID()   ││
│  └─────────┬─────────┘  └──────────────────┘  └────────────────┘│
└────────────┼──────────────────────────────────────────────────────┘
             │ Adapts to
             ↓
┌───────────────────────────────────────────────────────────────────┐
│              API Client Layer (3rd Party Integration)             │
│  ┌──────────────────┐         ┌──────────────────────┐           │
│  │  XTSAPIClient    │         │  FutureAPIClient     │           │
│  │  (Current)       │         │  (Pluggable)         │           │
│  └──────────────────┘         └──────────────────────┘           │
└───────────────────────────────────────────────────────────────────┘
            │                              │
            ↓                              ↓
       ┌─────────┐                    ┌─────────┐
       │XTS API  │                    │Other API│
       └─────────┘                    └─────────┘
```

---

## Improvement Proposals

### 1. Introduce Service Layer

**Problem**: Business logic scattered across UI controllers

**Solution**: Create dedicated service classes

```cpp
// include/services/MarketDataService.h
class MarketDataService : public QObject {
    Q_OBJECT
public:
    // High-level API for UI
    void subscribeInstrument(const InstrumentData& instrument);
    void unsubscribeInstrument(int64_t token);
    void refreshQuote(int64_t token);
    
    // Returns cached data immediately if available
    ScripData getScripData(int64_t token) const;
    
signals:
    void priceUpdated(int64_t token, const PriceUpdate& update);
    void quoteReceived(int64_t token, const Quote& quote);
    void subscriptionFailed(int64_t token, const QString& error);
    void connectionStatusChanged(bool connected);
    
private:
    // Encapsulates API client
    IMarketDataAPI* m_apiClient;  // Interface, not XTS directly
    
    // Cache for quick access
    QMap<int64_t, ScripData> m_scripCache;
    QSet<int64_t> m_subscribedTokens;
    
    // Business logic
    PriceUpdate calculatePriceChange(const Tick& current, const Tick& previous);
    void handleTickData(const Tick& tick);
    void handleSubscriptionResponse(const QJsonObject& response);
};
```

**Benefits**:
- ✅ UI code simplified - just call service methods
- ✅ Business logic centralized and reusable
- ✅ Easier to test (mock service layer)
- ✅ API changes isolated to service implementation

**Usage in UI**:
```cpp
// MainWindow - simplified
void MainWindow::onAddToWatchRequested(const InstrumentData& instrument) {
    // Service handles everything
    m_marketDataService->subscribeInstrument(instrument);
}

// MarketWatch - receives updates via signal
connect(m_marketDataService, &MarketDataService::priceUpdated,
        this, [this](int64_t token, const PriceUpdate& update) {
    updatePrice(token, update.ltp, update.change, update.changePercent);
});
```

### 2. API Abstraction Layer

**Problem**: Direct coupling to XTS API implementation

**Solution**: Define interfaces for swappable API providers

```cpp
// include/api/IMarketDataAPI.h
class IMarketDataAPI {
public:
    virtual ~IMarketDataAPI() = default;
    
    virtual void login(const QString& userId, const QString& password,
                      Callback<LoginResponse> callback) = 0;
    
    virtual void subscribe(const QVector<int64_t>& tokens, int segment,
                          Callback<SubscriptionResponse> callback) = 0;
    
    virtual void getQuote(int64_t token, int segment,
                         Callback<QuoteResponse> callback) = 0;
    
    virtual void connectWebSocket(Callback<bool> callback) = 0;
    
    virtual void setTickHandler(TickHandler handler) = 0;
};

// src/api/XTSMarketDataAPI.cpp
class XTSMarketDataAPI : public IMarketDataAPI {
    // XTS-specific implementation
    void subscribe(...) override {
        // XTS REST API call
    }
};

// Future: Add other providers
class ZerodhaMarketDataAPI : public IMarketDataAPI { ... };
class AngelBrokingMarketDataAPI : public IMarketDataAPI { ... };
```

**Benefits**:
- ✅ Easy to switch API providers
- ✅ Can run multiple providers simultaneously
- ✅ Simplified testing with mock implementations
- ✅ Adapter pattern for legacy/new APIs

### 3. Event Bus / Mediator Pattern

**Problem**: Direct connections between many components

**Solution**: Central event bus for loose coupling

```cpp
// include/core/EventBus.h
class EventBus : public QObject {
    Q_OBJECT
public:
    static EventBus* instance();
    
signals:
    // Market data events
    void tickReceived(const TickData& tick);
    void quoteReceived(const QuoteData& quote);
    void subscriptionStatusChanged(int64_t token, bool subscribed);
    
    // UI events
    void instrumentAddRequested(const InstrumentData& instrument);
    void instrumentRemoveRequested(int64_t token);
    
    // Trading events
    void orderPlaced(const Order& order);
    void orderModified(const Order& order);
    void positionUpdated(const Position& position);
    
    // System events
    void connectionStatusChanged(const QString& service, bool connected);
    void errorOccurred(const QString& service, const QString& error);
};

// Usage - any component can emit/listen
EventBus::instance()->emit tickReceived(tick);

connect(EventBus::instance(), &EventBus::tickReceived,
        this, &MarketWatchWindow::onTickReceived);
```

**Benefits**:
- ✅ Decoupled components
- ✅ Easy to add new listeners
- ✅ Centralized event logging/debugging
- ✅ Simpler testing (inject mock event bus)

### 4. State Management

**Problem**: Subscription state implicit and scattered

**Solution**: Explicit state tracking

```cpp
// include/models/SubscriptionState.h
enum class SubscriptionStatus {
    NotSubscribed,
    Subscribing,
    Subscribed,
    Unsubscribing,
    Failed
};

class SubscriptionState {
public:
    int64_t token;
    int exchangeSegment;
    SubscriptionStatus status;
    QDateTime lastUpdateTime;
    QString errorMessage;
    int retryCount;
    bool hasInitialSnapshot;
};

class SubscriptionManager {
public:
    void subscribe(int64_t token, int segment);
    void unsubscribe(int64_t token);
    
    SubscriptionStatus getStatus(int64_t token) const;
    QList<int64_t> getSubscribedTokens() const;
    
    // Check if token needs re-subscription
    bool needsRefresh(int64_t token) const;
    
signals:
    void stateChanged(int64_t token, SubscriptionStatus oldState, 
                     SubscriptionStatus newState);
    
private:
    QMap<int64_t, SubscriptionState> m_states;
};
```

**Benefits**:
- ✅ Clear visibility into subscription status
- ✅ Can show status in UI (subscribing, failed, etc.)
- ✅ Automatic retry logic based on state
- ✅ Persistence (save/restore subscriptions)

### 5. Error Handling Framework

**Problem**: Inconsistent error handling, silent failures

**Solution**: Structured error handling with recovery

```cpp
// include/core/ErrorHandler.h
enum class ErrorSeverity {
    Info,        // Informational
    Warning,     // Recoverable
    Error,       // Needs attention
    Critical     // System failure
};

class TradingError {
public:
    QString code;           // "SUBSCRIBE_FAILED"
    QString message;        // "Failed to subscribe token 49543"
    ErrorSeverity severity;
    QString component;      // "MarketDataService"
    QDateTime timestamp;
    QVariant context;       // Additional data
};

class ErrorHandler : public QObject {
    Q_OBJECT
public:
    static void handle(const TradingError& error);
    
    // Retry logic
    static void retryOperation(std::function<void()> operation,
                              int maxRetries = 3,
                              int delayMs = 1000);
    
signals:
    void errorOccurred(const TradingError& error);
    void errorResolved(const QString& errorCode);
};

// Usage
void MarketDataService::subscribeInstrument(...) {
    m_apiClient->subscribe(tokens, segment, [this](bool ok, QString msg) {
        if (!ok) {
            TradingError error;
            error.code = "SUBSCRIBE_FAILED";
            error.message = msg;
            error.severity = ErrorSeverity::Warning;
            error.component = "MarketDataService";
            error.context = QVariant::fromValue(tokens);
            
            ErrorHandler::handle(error);
            
            // Auto-retry
            ErrorHandler::retryOperation([this, tokens, segment]() {
                m_apiClient->subscribe(tokens, segment, ...);
            });
        }
    });
}
```

**Benefits**:
- ✅ Consistent error reporting
- ✅ Automatic retry logic
- ✅ User-friendly error messages
- ✅ Centralized error logging

### 6. Repository Pattern for Data Access

**Problem**: Direct API calls mixed with business logic

**Solution**: Repository layer abstracts data access

```cpp
// include/repository/MarketDataRepository.h
class MarketDataRepository {
public:
    // Abstract away whether data comes from API, cache, or DB
    Future<Quote> getQuote(int64_t token);
    Future<SubscriptionResult> subscribe(int64_t token);
    
    // Cache management
    void setCacheExpiry(int seconds);
    void invalidateCache(int64_t token);
    
signals:
    void dataUpdated(int64_t token, const Quote& quote);
    
private:
    IMarketDataAPI* m_api;
    QMap<int64_t, Quote> m_cache;
    QMap<int64_t, QDateTime> m_cacheTimestamps;
    
    bool isCacheValid(int64_t token) const;
};
```

**Benefits**:
- ✅ Transparent caching
- ✅ Can switch between API/DB/file
- ✅ Easier testing (mock repository)
- ✅ Optimized data access patterns

### 7. Command Pattern for User Actions

**Problem**: Undo/redo, action history not supported

**Solution**: Encapsulate actions as commands

```cpp
// include/commands/ICommand.h
class ICommand {
public:
    virtual ~ICommand() = default;
    virtual void execute() = 0;
    virtual void undo() = 0;
    virtual QString description() const = 0;
};

class AddInstrumentCommand : public ICommand {
public:
    AddInstrumentCommand(MarketWatchWindow* window, 
                        const InstrumentData& instrument)
        : m_window(window), m_instrument(instrument) {}
    
    void execute() override {
        m_window->addInstrument(m_instrument);
    }
    
    void undo() override {
        m_window->removeInstrument(m_instrument.exchangeInstrumentID);
    }
    
    QString description() const override {
        return QString("Add %1 to Market Watch").arg(m_instrument.symbol);
    }
    
private:
    MarketWatchWindow* m_window;
    InstrumentData m_instrument;
};

class CommandManager {
public:
    void executeCommand(ICommand* cmd);
    void undo();
    void redo();
    QStringList getHistory() const;
    
private:
    QStack<ICommand*> m_undoStack;
    QStack<ICommand*> m_redoStack;
};
```

**Benefits**:
- ✅ Undo/redo functionality
- ✅ Action history for debugging
- ✅ Macro recording (batch commands)
- ✅ Transactional operations

### 8. Dependency Injection

**Problem**: Hard-coded dependencies, difficult testing

**Solution**: Inject dependencies via constructor/setters

```cpp
// OLD (tight coupling)
class MarketWatchWindow {
public:
    MarketWatchWindow() {
        m_model = new MarketWatchModel();  // Hard-coded
        m_apiClient = new XTSMarketDataClient();  // Hard-coded
    }
};

// NEW (dependency injection)
class MarketWatchWindow {
public:
    MarketWatchWindow(MarketWatchModel* model,
                     IMarketDataService* service,
                     QWidget* parent = nullptr)
        : m_model(model), m_service(service), QWidget(parent) {
        // Dependencies injected, can use mocks for testing
    }
    
private:
    MarketWatchModel* m_model;
    IMarketDataService* m_service;
};

// Application setup (dependency injection container)
class ServiceContainer {
public:
    static void initialize() {
        // Register services
        register<IMarketDataAPI>(new XTSMarketDataAPI());
        register<MarketDataService>(new MarketDataService(
            resolve<IMarketDataAPI>()
        ));
    }
    
    template<typename T>
    static T* resolve();
    
    template<typename T>
    static void register(T* instance);
};
```

**Benefits**:
- ✅ Testable code (inject mocks)
- ✅ Flexible configuration
- ✅ Clear dependencies
- ✅ Easier to swap implementations

---

## Migration Strategy

### Phase 1: Foundation (Week 1-2)

**Goals**: Establish core abstractions without breaking existing code

**Tasks**:
1. ✅ Create `IMarketDataAPI` interface
2. ✅ Wrap existing `XTSMarketDataClient` in adapter implementing `IMarketDataAPI`
3. ✅ Create `EventBus` singleton
4. ✅ Define `TradingError` and `ErrorHandler` classes
5. ✅ Create `SubscriptionState` and `SubscriptionManager`

**Result**: New abstractions available but old code still works

### Phase 2: Service Layer (Week 3-4)

**Goals**: Move business logic from MainWindow to services

**Tasks**:
1. ✅ Implement `MarketDataService`
2. ✅ Implement `TradingService`
3. ✅ Implement `OrderService`
4. ✅ Update MainWindow to use services instead of direct API calls
5. ✅ Connect services to EventBus

**Result**: Cleaner MainWindow, centralized business logic

### Phase 3: Repository Layer (Week 5-6)

**Goals**: Abstract data access with caching

**Tasks**:
1. ✅ Implement `MarketDataRepository`
2. ✅ Implement `PositionRepository`
3. ✅ Implement `OrderRepository`
4. ✅ Add cache invalidation logic
5. ✅ Update services to use repositories

**Result**: Optimized data access, transparent caching

### Phase 4: Error Handling (Week 7-8)

**Goals**: Robust error handling with recovery

**Tasks**:
1. ✅ Implement retry logic in `ErrorHandler`
2. ✅ Add error UI components (toast notifications)
3. ✅ Update all API calls to use `ErrorHandler`
4. ✅ Add error logging and reporting

**Result**: Resilient system with user-friendly errors

### Phase 5: Advanced Features (Week 9+)

**Goals**: Command pattern, DI, advanced state management

**Tasks**:
1. ✅ Implement `CommandManager` for undo/redo
2. ✅ Set up dependency injection container
3. ✅ Add persistence layer for state
4. ✅ Implement workspace save/restore

**Result**: Professional-grade architecture

---

## Code Examples

### Before (Current)

```cpp
// MainWindow.cpp - God object doing everything
void MainWindow::onAddToWatchRequested(const InstrumentData& instrument) {
    MarketWatchWindow *marketWatch = getActiveMarketWatch();
    if (!marketWatch) return;
    
    // Map data
    ScripData scripData;
    scripData.symbol = instrument.symbol;
    scripData.token = instrument.exchangeInstrumentID;
    // ... 40+ lines of mapping ...
    
    // Add to window
    bool added = marketWatch->addScripFromContract(scripData);
    
    if (added && m_xtsMarketDataClient) {
        // Direct API call
        QVector<int64_t> ids;
        ids.append(instrument.exchangeInstrumentID);
        
        m_xtsMarketDataClient->subscribe(ids, instrument.exchangeSegment, 
            [](bool ok, const QString& msg) {
            if (!ok) {
                qWarning() << "Subscribe failed:" << msg;
                // No retry, no user notification
            }
        });
    }
}

void MainWindow::onTickReceived(const XTS::Tick &tick) {
    // Loop through all windows
    QList<CustomMDISubWindow*> windows = m_mdiArea->windowList();
    for (CustomMDISubWindow *win : windows) {
        if (win->windowType() == "MarketWatch") {
            MarketWatchWindow *mw = qobject_cast<MarketWatchWindow*>(win->contentWidget());
            if (mw) {
                // Duplicate business logic
                double change = tick.lastTradedPrice - tick.close;
                double changePercent = (tick.close != 0) ? (change / tick.close) * 100.0 : 0.0;
                mw->updatePrice(tick.exchangeInstrumentID, tick.lastTradedPrice, change, changePercent);
            }
        }
    }
}
```

### After (Proposed)

```cpp
// MainWindow.cpp - Thin controller
void MainWindow::onAddToWatchRequested(const InstrumentData& instrument) {
    // Service handles everything
    m_marketDataService->subscribeInstrument(instrument);
}

// MarketDataService.cpp - Business logic encapsulated
void MarketDataService::subscribeInstrument(const InstrumentData& instrument) {
    // Check cache first
    if (m_subscribedTokens.contains(instrument.exchangeInstrumentID)) {
        emit subscriptionStatusChanged(instrument.exchangeInstrumentID, true);
        return;
    }
    
    // Update state
    SubscriptionState state;
    state.token = instrument.exchangeInstrumentID;
    state.status = SubscriptionStatus::Subscribing;
    m_subscriptionManager->setState(instrument.exchangeInstrumentID, state);
    
    // Call API via repository
    m_repository->subscribe(instrument.exchangeInstrumentID, instrument.exchangeSegment)
        .then([this, token = instrument.exchangeInstrumentID](SubscriptionResult result) {
            if (result.success) {
                m_subscribedTokens.insert(token);
                
                // Process initial snapshot if available
                if (!result.initialQuote.isEmpty()) {
                    processQuote(result.initialQuote);
                }
                
                // Update state
                SubscriptionState state;
                state.status = SubscriptionStatus::Subscribed;
                state.hasInitialSnapshot = !result.initialQuote.isEmpty();
                m_subscriptionManager->setState(token, state);
                
                emit subscriptionStatusChanged(token, true);
            } else {
                // Error handling with retry
                TradingError error;
                error.code = "SUBSCRIBE_FAILED";
                error.message = result.errorMessage;
                error.severity = ErrorSeverity::Warning;
                ErrorHandler::handle(error);
                
                // Automatic retry
                ErrorHandler::retryOperation([this, instrument]() {
                    subscribeInstrument(instrument);
                }, 3, 2000);
            }
        });
}

void MarketDataService::handleTickData(const Tick& tick) {
    // Calculate price change (centralized business logic)
    PriceUpdate update = calculatePriceChange(tick);
    
    // Emit via event bus (decoupled)
    EventBus::instance()->emit priceUpdated(tick.exchangeInstrumentID, update);
}

// MarketWatchWindow.cpp - Just listens to events
MarketWatchWindow::MarketWatchWindow(MarketDataService* service, ...) {
    // Listen to event bus
    connect(EventBus::instance(), &EventBus::priceUpdated,
            this, &MarketWatchWindow::onPriceUpdated);
}

void MarketWatchWindow::onPriceUpdated(int64_t token, const PriceUpdate& update) {
    // Simple update, no business logic
    updatePrice(token, update.ltp, update.change, update.changePercent);
}
```

---

## Testing Strategy

### Unit Testing

**With New Architecture**:
```cpp
// Test service with mock API
TEST_F(MarketDataServiceTest, SubscribeInstrument_Success) {
    // Arrange
    MockMarketDataAPI mockAPI;
    EXPECT_CALL(mockAPI, subscribe(_, _, _))
        .WillOnce(InvokeCallback(true, "Success"));
    
    MarketDataService service(&mockAPI);
    
    InstrumentData instrument;
    instrument.exchangeInstrumentID = 49543;
    
    // Act
    service.subscribeInstrument(instrument);
    
    // Assert
    EXPECT_TRUE(service.isSubscribed(49543));
}
```

### Integration Testing

```cpp
// Test full flow with real components
TEST_F(IntegrationTest, AddInstrumentToMarketWatch) {
    // Setup real services (but with mock API)
    MockXTSAPI mockAPI;
    MarketDataRepository repo(&mockAPI);
    MarketDataService service(&repo);
    MarketWatchWindow window(&service);
    
    // Act
    InstrumentData nifty = createNiftyInstrument();
    service.subscribeInstrument(nifty);
    
    // Simulate tick arrival
    Tick tick = createTick(nifty.exchangeInstrumentID, 25960.0);
    service.handleTickData(tick);
    
    // Assert
    EXPECT_EQ(window.getRowCount(), 1);
    EXPECT_EQ(window.getPrice(nifty.exchangeInstrumentID), 25960.0);
}
```

---

## Performance Considerations

### Current Bottlenecks

1. **updatePrice() loops through all windows** - O(n) per tick
2. **No throttling** - Can receive 100+ ticks/sec
3. **Synchronous UI updates** - Blocks on each dataChanged emit

### Optimizations

```cpp
// 1. Batch updates with QTimer
class MarketWatchModel {
private:
    QTimer* m_updateTimer;
    QSet<int> m_pendingUpdateRows;
    
    void scheduleUpdate(int row) {
        m_pendingUpdateRows.insert(row);
        if (!m_updateTimer->isActive()) {
            m_updateTimer->start(100);  // Batch every 100ms
        }
    }
    
    void flushUpdates() {
        if (m_pendingUpdateRows.isEmpty()) return;
        
        int minRow = *std::min_element(m_pendingUpdateRows.begin(), 
                                      m_pendingUpdateRows.end());
        int maxRow = *std::max_element(m_pendingUpdateRows.begin(),
                                      m_pendingUpdateRows.end());
        
        // Single dataChanged emit for range
        emit dataChanged(index(minRow, 0), index(maxRow, columnCount()-1));
        m_pendingUpdateRows.clear();
    }
};

// 2. Throttle ticks at source
class MarketDataService {
private:
    QMap<int64_t, QDateTime> m_lastTickTime;
    
    void handleTickData(const Tick& tick) {
        // Throttle to max 10 ticks/sec per instrument
        QDateTime now = QDateTime::currentDateTime();
        QDateTime lastTime = m_lastTickTime.value(tick.exchangeInstrumentID);
        
        if (lastTime.isValid() && lastTime.msecsTo(now) < 100) {
            return;  // Skip this tick
        }
        
        m_lastTickTime[tick.exchangeInstrumentID] = now;
        processTick(tick);
    }
};

// 3. Use EventBus for one-to-many efficiently
// Instead of looping through windows, windows register with EventBus
// EventBus uses Qt's signal/slot which is optimized for many listeners
```

---

## Summary of Recommendations

| Priority | Improvement | Effort | Impact | Timeline |
|----------|------------|--------|--------|----------|
| 🔥 High | Service Layer | Medium | High | Week 1-4 |
| 🔥 High | Error Handling | Low | High | Week 2-3 |
| 🔥 High | Event Bus | Low | Medium | Week 1-2 |
| ⚡ Medium | API Abstraction | Medium | High | Week 3-5 |
| ⚡ Medium | State Management | Medium | Medium | Week 4-6 |
| ⚡ Medium | Repository Pattern | High | Medium | Week 5-7 |
| 💡 Low | Command Pattern | High | Low | Week 9+ |
| 💡 Low | Dependency Injection | Medium | Low | Week 8+ |

### Quick Wins (Start Here)

1. **Extract MarketDataService** - Move subscribe logic from MainWindow
2. **Add ErrorHandler** - Consistent error handling with retry
3. **Create EventBus** - Decouple tick distribution
4. **Define IMarketDataAPI** - Prepare for future API flexibility

### Long-term Goals

1. **Full service layer** - All business logic in services
2. **Repository caching** - Optimize data access
3. **Undo/redo** - Command pattern for user actions
4. **Multi-API support** - Switch between XTS/Zerodha/etc.

---

**Next Steps**: 
1. Review and approve architecture direction
2. Create detailed implementation plan for Phase 1
3. Set up testing framework
4. Begin service layer extraction

---

**Document Status**: ✅ Complete - Ready for review  
**Next Review**: After Phase 1 completion
