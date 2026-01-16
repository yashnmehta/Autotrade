# Signal/Slot Refactoring - Implementation Status

**Date**: 2026-01-16  
**Status**: Phase 1 In Progress

---

## ✅ Completed

### 1. Architecture Documentation
- ✅ Created [ARCHITECTURE_PLAN_V2.md](ARCHITECTURE_PLAN_V2.md) - Comprehensive threading and signal/slot architecture
- ✅ Created [SIGNAL_SLOT_MIGRATION_GUIDE.md](SIGNAL_SLOT_MIGRATION_GUIDE.md) - Practical migration examples

### 2. XTSMarketDataClient - Core Methods ✅
**File**: [include/api/XTSMarketDataClient.h](include/api/XTSMarketDataClient.h)

#### Header Changes:
- ✅ Removed all `std::function<>` callback parameters
- ✅ Added new signals:
  - `loginCompleted(bool success, const QString& error)`
  - `wsConnectionStatusChanged(bool connected, const QString& error)`
  - `subscriptionCompleted(bool success, const QString& error)`
  - `unsubscriptionCompleted(bool success, const QString& error)`
  - `instrumentsReceived(bool success, const QVector<XTS::Instrument>& instruments, const QString& error)`
  - `masterContractsDownloaded(bool success, const QString& filePath, const QString& error)`
  - `quoteReceived(bool success, const QJsonObject& quote, const QString& error)`
  - `quoteBatchReceived(bool success, const QVector<QJsonObject>& quotes, const QString& error)`

**File**: [src/api/XTSMarketDataClient.cpp](src/api/XTSMarketDataClient.cpp)

#### Implementation Changes:
- ✅ **login()** - Now emits `loginCompleted` signal, runs in thread
- ✅ **connectWebSocket()** - Emits `wsConnectionStatusChanged` 
- ✅ **subscribe()** - Emits `subscriptionCompleted`, runs in thread
- ✅ **unsubscribe()** - Emits `unsubscriptionCompleted`
- ✅ **downloadMasterContracts()** - Emits `masterContractsDownloaded`, runs in thread
- ✅ **onWSConnected()** - Updated to emit signal only (removed callback)
- ✅ **onWSDisconnected()** - Updated to emit signal only (removed callback)
- ✅ **onWSError()** - Updated to emit signal only (removed callback)
- ✅ **processTickData()** - Removed tick handler callback, uses signal only
- ✅ Removed `setTickHandler()` method
- ✅ Removed `m_tickHandler` and `m_wsConnectCallback` member variables

---

## 🚧 In Progress

### XTSMarketDataClient - Remaining Methods
**Need to Update**:
1. ❌ `getInstruments(int exchangeSegment)` - Still has callback parameter
2. ❌ `searchInstruments(const QString& searchString, int exchangeSegment)` - Still has callback
3. ❌ `getQuote(int64_t exchangeInstrumentID, int exchangeSegment)` - Still has callback
4. ❌ `getQuoteBatch(const QVector<int64_t>& instrumentIDs, int exchangeSegment)` - Still has callback

**Implementation needed**: Update these methods to:
- Remove callback parameter
- Run in std::thread
- Emit appropriate signal (`instrumentsReceived`, `quoteReceived`, `quoteBatchReceived`)

---

## 📋 TODO

### 3. XTSInteractiveClient Migration
**File**: `include/api/XTSInteractiveClient.h`

**Changes Needed**:
- [ ] Remove all callback parameters from:
  - `login()`
  - `placeOrder(const XTS::OrderRequest& request)`
  - `modifyOrder(const QString& appOrderID, const XTS::ModifyRequest& request)`
  - `cancelOrder(const QString& appOrderID)`
  - `getOrderbook()`
  - `getPositions()`
  - `getTradebook()`

- [ ] Add signals:
  ```cpp
  signals:
      void loginCompleted(bool success, const QString& error);
      void orderPlaced(bool success, const XTS::OrderResponse& order, const QString& error);
      void orderModified(bool success, const QString& appOrderID, const QString& error);
      void orderCancelled(bool success, const QString& appOrderID, const QString& error);
      void orderbookReceived(bool success, const QVector<XTS::Order>& orders, const QString& error);
      void positionsReceived(bool success, const QVector<XTS::Position>& positions, const QString& error);
      void tradebookReceived(bool success, const QVector<XTS::Trade>& trades, const QString& error);
      void errorOccurred(const QString& error);
  ```

### 4. LoginFlowService Update
**File**: `src/services/LoginFlowService.cpp`

**Changes Needed**:
```cpp
// Setup connections (in constructor or init method)
void LoginFlowService::setupConnections() {
    // Chain: login -> connectWS -> loadMaster
    connect(m_marketDataClient, &XTSMarketDataClient::loginCompleted,
            this, &LoginFlowService::onMarketDataLoginCompleted);
    
    connect(m_marketDataClient, &XTSMarketDataClient::wsConnectionStatusChanged,
            this, &LoginFlowService::onWSConnectionChanged);
    
    connect(m_masterLoader, &MasterLoaderWorker::loadingComplete,
            this, &LoginFlowService::onMasterLoadComplete);
    
    connect(m_interactiveClient, &XTSInteractiveClient::loginCompleted,
            this, &LoginFlowService::onInteractiveLoginCompleted);
}

// Replace callback chains with sequential signal handlers
void LoginFlowService::startLogin() {
    m_marketDataClient->login();  // No callback!
}

void LoginFlowService::onMarketDataLoginCompleted(bool success, const QString& error) {
    if (success) {
        m_marketDataClient->connectWebSocket();  // No callback!
    } else {
        emit loginFailed(error);
    }
}

void LoginFlowService::onWSConnectionChanged(bool connected, const QString& error) {
    if (connected) {
        m_masterLoader->start();  // No callback!
    } else {
        emit loginFailed("WebSocket connection failed: " + error);
    }
}

void LoginFlowService::onMasterLoadComplete() {
    m_interactiveClient->login();  // No callback!
}

void LoginFlowService::onInteractiveLoginCompleted(bool success, const QString& error) {
    if (success) {
        emit loginComplete();
    } else {
        emit loginFailed("Interactive login failed: " + error);
    }
}
```

### 5. UI Component Updates

#### MarketWatchWindow
```cpp
// OLD:
xtsClient->subscribe(instruments, segment, [this](bool success, const QString& error) {
    if (success) {
        // Update UI
    }
});

// NEW:
// Setup once in constructor
connect(xtsClient, &XTSMarketDataClient::subscriptionCompleted,
        this, &MarketWatchWindow::onSubscriptionCompleted);

// Trigger subscription
xtsClient->subscribe(instruments, segment);  // No callback!

// Handler
void MarketWatchWindow::onSubscriptionCompleted(bool success, const QString& error) {
    if (success) {
        // Update UI (already on UI thread)
    }
}
```

#### BuyWindow
```cpp
// OLD:
xtsInteractive->placeOrder(order, [this](bool success, auto order, auto error) {
    QMetaObject::invokeMethod(...);  // Manual marshalling!
});

// NEW:
// Setup once
connect(xtsInteractive, &XTSInteractiveClient::orderPlaced,
        this, &BuyWindow::onOrderPlaced);

// Trigger order
xtsInteractive->placeOrder(order);  // No callback!

// Handler (automatic UI thread)
void BuyWindow::onOrderPlaced(bool success, const XTS::OrderResponse& order, const QString& error) {
    if (success) {
        showNotification("Order placed: " + order.appOrderID);
        close();
    }
}
```

#### OrderBookWindow  
```cpp
// Setup connections for all order events
connect(xtsInteractive, &XTSInteractiveClient::orderPlaced,
        this, &OrderBookWindow::onOrderUpdate);
connect(xtsInteractive, &XTSInteractiveClient::orderModified,
        this, &OrderBookWindow::onOrderUpdate);
connect(xtsInteractive, &XTSInteractiveClient::orderCancelled,
        this, &OrderBookWindow::onOrderUpdate);
```

### 6. Search All Callback Usage

**Files to Check**:
```bash
# Find all callback usage
grep -r "std::function" include/
grep -r "std::function" src/

# Find all callback invocations
grep -r "callback(" src/

# Find QMetaObject::invokeMethod (should be minimal after migration)
grep -r "QMetaObject::invokeMethod" src/
```

---

## 🎯 Benefits After Complete Migration

### Before (Callback Hell):
```cpp
xtsClient->login([this](bool success, auto error) {
    if (success) {
        xtsClient->connectWebSocket([this](bool success, auto error) {
            if (success) {
                masterLoader->load([this](bool success) {
                    if (success) {
                        // Finally ready!
                    }
                });
            }
        });
    }
});
```

### After (Clean Signal Chain):
```cpp
// Setup once
connect(xtsClient, &XTSMarketDataClient::loginCompleted, 
        this, &LoginFlow::onLoginComplete);
connect(xtsClient, &XTSMarketDataClient::wsConnectionStatusChanged,
        this, &LoginFlow::onWSConnected);
connect(masterLoader, &MasterLoader::loadComplete,
        this, &LoginFlow::onMasterLoaded);

// Trigger flow
xtsClient->login();  // That's it!

// Linear handlers
void LoginFlow::onLoginComplete(bool success, const QString& error) {
    if (success) xtsClient->connectWebSocket();
}

void LoginFlow::onWSConnected(bool connected, const QString& error) {
    if (connected) masterLoader->start();
}

void LoginFlow::onMasterLoaded() {
    emit ready();
}
```

---

## 📊 Progress Summary

| Component | Status | Completion |
|-----------|--------|------------|
| Architecture Docs | ✅ Complete | 100% |
| XTSMarketDataClient Core | ✅ Complete | 85% |
| XTSMarketDataClient Remaining | 🚧 In Progress | 15% |
| XTSInteractiveClient | ❌ Not Started | 0% |
| LoginFlowService | ❌ Not Started | 0% |
| UI Components | ❌ Not Started | 0% |
| Testing | ❌ Not Started | 0% |

**Overall Progress**: ~35%

---

## 🔧 Next Steps

1. **Complete XTSMarketDataClient** (15 min)
   - Update getInstruments, searchInstruments, getQuote methods
   
2. **Refactor XTSInteractiveClient** (30 min)
   - Update header with new signals
   - Remove callbacks from all methods
   - Update implementation to emit signals

3. **Update LoginFlowService** (20 min)
   - Add signal connections
   - Replace callback chain with sequential handlers

4. **Update UI Components** (1 hour)
   - MarketWatchWindow
   - BuyWindow/SellWindow
   - OrderBookWindow
   - TradeBookWindow
   - PositionWindow

5. **Test & Validate** (30 min)
   - Compile and fix any errors
   - Test login flow
   - Test order placement
   - Test market data subscriptions

**Estimated Total Time**: ~2.5 hours

---

## 🐛 Known Issues to Fix

1. Some methods still have callback parameters in implementation
2. Need to search for all `QMetaObject::invokeMethod` usage and validate necessity
3. UI components may have direct callback usage that needs migration
4. Error handling patterns should be consistent across all signal emissions

---

**Last Updated**: 2026-01-16  
**Next Review**: After XTSMarketDataClient completion
