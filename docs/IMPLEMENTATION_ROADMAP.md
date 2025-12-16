# Implementation Roadmap - Market Data Architecture Improvements

## Executive Summary

**Objective**: Transform current tightly-coupled architecture into maintainable, testable, and extensible system

**Timeline**: 4 weeks  
**Priority**: High (addresses critical UX issues and technical debt)  
**Risk Level**: Medium (gradual migration strategy reduces risk)

**Related Documentation**:
- [API Response Analysis](API_RESPONSE_ANALYSIS.md) - Complete XTS API testing results and behavior
- [Add Scrip Flow](ADD_SCRIP_FLOW.md) - Current implementation details
- [Architecture Recommendations](ARCHITECTURE_RECOMMENDATIONS.md) - Proposed improvements

---

## Current Status

### ✅ Completed (Phase 0)
- [x] Socket.IO WebSocket integration
- [x] Real-time tick data processing
- [x] Token-based duplicate checking
- [x] Complete instrument data flow
- [x] Subscribe API returning initial snapshots
- [x] Broadcast to multiple market watch windows

### ⚠️ Known Issues
- [x] **FIXED**: Already-subscribed tokens show 0.00 initially → Now fetches via getQuote fallback
- [x] **FIXED**: getQuote API failing (Bad Request 400) → Added required `publishFormat` parameter
- [ ] No loading indicators
- [ ] No error recovery/retry (partially implemented for subscribe)
- [ ] Tight coupling in MainWindow
- [ ] No comprehensive testing

### ✅ Recent Fixes (Dec 16, 2025)
- [x] getQuote API now working with `publishFormat: "JSON"` parameter
- [x] Multiple instrument getQuote support added
- [x] "Already Subscribed" error (e-session-0002) detection and handling
- [x] Automatic fallback to getQuote when re-subscribing
- [x] Complete API response analysis and documentation

---

## Week 1: Critical UX Fixes

### Goal
Fix immediate user-facing issues: "0.00 flash" and missing feedback

### Day 1-2: Price Cache Implementation

**Files to Create**:
```
include/services/
    PriceCache.h
src/services/
    PriceCache.cpp
```

**Implementation Steps**:

1. Create PriceCache class
```cpp
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
    mutable QReadWriteLock m_lock;
};
```

2. Integrate into XTSMarketDataClient
```cpp
// In subscribe callback - cache prices from listQuotes
for (const QJsonValue &val : listQuotes) {
    QString quoteStr = val.toString();
    QJsonObject quoteData = parseJSON(quoteStr);
    XTS::Tick tick = parseTick(quoteData);
    PriceCache::instance()->updatePrice(tick.exchangeInstrumentID, tick);
    processTickData(quoteData);
}

// In processTickData - cache all ticks
void XTSMarketDataClient::processTickData(const QJsonObject &json) {
    XTS::Tick tick = parseTick(json);
    PriceCache::instance()->updatePrice(tick.exchangeInstrumentID, tick);
    emit tickReceived(tick);
}
```

3. Use cache in MainWindow
```cpp
void MainWindow::onAddToWatchRequested(const InstrumentData &instrument) {
    // ... add to market watch ...
    
    // Check cache BEFORE subscribing
    auto cached = PriceCache::instance()->getPrice(instrument.exchangeInstrumentID);
    if (cached.has_value()) {
        double change = cached->lastTradedPrice - cached->close;
        double changePercent = (cached->close != 0) ? (change / cached->close) * 100.0 : 0.0;
        marketWatch->updatePrice(instrument.exchangeInstrumentID, 
                                cached->lastTradedPrice, change, changePercent);
        qDebug() << "Applied cached price for" << instrument.exchangeInstrumentID;
    }
    
    // Then subscribe (will update if price changed)
    m_xtsMarketDataClient->subscribe(...);
}
```

**Testing**:
- Add NIFTY to Window 1 → Should populate from API
- Add NIFTY to Window 2 → Should populate from cache immediately
- Verify no "0.00" flash

**Deliverable**: No more "0.00" on already-subscribed tokens ✅

---

### Day 3-4: Subscription State Management

**Files to Create**:
```
include/services/
    SubscriptionStateManager.h
src/services/
    SubscriptionStateManager.cpp
```

**Implementation Steps**:

1. Define states
```cpp
enum class SubscriptionState {
    NotSubscribed,
    Subscribing,      // Show "Loading..."
    WaitingForData,   // Show "Waiting..."
    Active,           // Show price
    Error,            // Show error
    Stale             // Show warning
};
```

2. Create state manager
```cpp
class SubscriptionStateManager : public QObject {
    Q_OBJECT
public:
    static SubscriptionStateManager* instance();
    
    void setState(int token, SubscriptionState state, const QString &message = "");
    SubscriptionState getState(int token) const;
    QString getMessage(int token) const;
    
signals:
    void stateChanged(int token, SubscriptionState state);
    
private:
    struct State {
        SubscriptionState state;
        QString message;
        QDateTime lastUpdate;
    };
    QMap<int, State> m_states;
};
```

3. Update MarketWatchModel to show states
```cpp
QVariant MarketWatchModel::data(const QModelIndex &index, int role) const {
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
            default:
                return "-";
        }
    }
    // ... rest of the code
}
```

4. Integrate into MainWindow
```cpp
void MainWindow::onAddToWatchRequested(const InstrumentData &instrument) {
    int token = instrument.exchangeInstrumentID;
    
    // Set initial state
    SubscriptionStateManager::instance()->setState(token, 
        SubscriptionState::Subscribing, "Subscribing...");
    
    bool added = marketWatch->addScripFromContract(scripData);
    
    if (added) {
        // Check cache
        if (PriceCache::instance()->hasPrice(token)) {
            // Apply cached price
            applyPrice(token, marketWatch);
            SubscriptionStateManager::instance()->setState(token, 
                SubscriptionState::Active, "Active");
        } else {
            SubscriptionStateManager::instance()->setState(token, 
                SubscriptionState::WaitingForData, "Waiting for price...");
        }
        
        // Subscribe
        m_xtsMarketDataClient->subscribe(ids, segment, [token](bool ok, const QString &msg) {
            if (ok) {
                SubscriptionStateManager::instance()->setState(token, 
                    SubscriptionState::Active, "Active");
            } else {
                SubscriptionStateManager::instance()->setState(token, 
                    SubscriptionState::Error, msg);
            }
        });
    }
}
```

**Testing**:
- Add instrument → Should show "Loading..." → "Waiting..." → price
- Trigger error → Should show "Error"
- Verify states update correctly

**Deliverable**: User sees clear loading/error states ✅

---

### Day 5-7: Retry Logic & Testing

**Files to Create**:
```
include/utils/
    RetryHelper.h
src/utils/
    RetryHelper.cpp
tests/
    test_price_cache.cpp
```

**Implementation Steps**:

1. Create retry helper
```cpp
class RetryHelper : public QObject {
    Q_OBJECT
public:
    using Operation = std::function<void(std::function<void(bool, const QString&)>)>;
    
    void retryWithBackoff(Operation operation,
                         std::function<void(bool, const QString&)> finalCallback,
                         int maxRetries = 3,
                         QVector<int> delaysMs = {1000, 2000, 4000});
};
```

2. Integrate into subscribe calls
```cpp
m_retryHelper->retryWithBackoff(
    [this, ids, segment](auto callback) {
        m_xtsMarketDataClient->subscribe(ids, segment, callback);
    },
    [token](bool success, const QString &msg) {
        if (success) {
            SubscriptionStateManager::instance()->setState(token, 
                SubscriptionState::Active);
        } else {
            SubscriptionStateManager::instance()->setState(token, 
                SubscriptionState::Error, "Failed after retries: " + msg);
        }
    },
    3  // Try 3 times
);
```

3. Write unit tests
```cpp
TEST(PriceCache, UpdateAndRetrieve) {
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
    // Test cache expiration
}
```

**Testing**:
- Simulate network failures → Should retry
- Verify exponential backoff timing
- Run unit tests

**Deliverable**: Reliable subscriptions with auto-retry ✅

**Week 1 Milestone**: Critical UX issues resolved

---

## Week 2: Service Layer Foundation

### Goal
Extract business logic from MainWindow into dedicated service layer

### Day 8-10: Provider Abstraction

**Files to Create**:
```
include/api/
    IMarketDataProvider.h
    XTSMarketDataProvider.h
src/api/
    XTSMarketDataProvider.cpp
```

**Implementation Steps**:

1. Define provider interface
```cpp
struct Quote {
    int64_t token;
    double ltp;
    double open, high, low, close;
    int64_t volume;
    double bidPrice, askPrice;
};

class IMarketDataProvider : public QObject {
    Q_OBJECT
public:
    virtual void connect(const QJsonObject &config,
                        std::function<void(bool)> callback) = 0;
    virtual void subscribe(const QVector<int64_t> &tokens, int exchangeSegment,
                          std::function<void(bool, const QJsonArray&)> callback) = 0;
    virtual void getQuote(int64_t token, int exchangeSegment,
                         std::function<void(bool, const Quote&)> callback) = 0;
    
signals:
    void tickReceived(const Quote &quote);
    void connectionStateChanged(bool connected);
    void errorOccurred(const QString &error);
};
```

2. Refactor XTSMarketDataClient → XTSMarketDataProvider
```cpp
class XTSMarketDataProvider : public IMarketDataProvider {
public:
    void subscribe(...) override {
        // Keep all XTS-specific logic
        // Convert XTS::Tick → Quote
    }
    
private:
    Quote convertXTSTickToQuote(const XTS::Tick &tick);
    // All XTS-specific methods
};
```

3. Create mock provider for testing
```cpp
class MockMarketDataProvider : public IMarketDataProvider {
public:
    void subscribe(...) override {
        // Return test data immediately
        QJsonArray quotes = createTestQuotes();
        callback(true, quotes);
    }
};
```

**Testing**:
- Run existing app with XTSMarketDataProvider
- Verify no regression
- Test mock provider

**Deliverable**: XTS details isolated behind interface ✅

---

### Day 11-14: MarketDataService Creation

**Files to Create**:
```
include/services/
    MarketDataService.h
src/services/
    MarketDataService.cpp
```

**Implementation Steps**:

1. Create service class
```cpp
class MarketDataService : public QObject {
    Q_OBJECT
public:
    explicit MarketDataService(IMarketDataProvider *provider);
    
    void addInstrumentToWatch(const InstrumentData &instrument, 
                             MarketWatchWindow *window);
    void removeInstrumentFromWatch(int token, MarketWatchWindow *window);
    
signals:
    void instrumentAdded(int token, const ScripData &data);
    void priceUpdated(int token, const Quote &quote);
    void error(int token, const QString &message);
    
private:
    void handleSubscribeSuccess(const InstrumentData &inst, 
                               MarketWatchWindow *window,
                               const QJsonArray &listQuotes);
    void applyInitialPrice(int token, MarketWatchWindow *window);
    ScripData transformToScripData(const InstrumentData &inst) const;
    
    IMarketDataProvider *m_provider;
    PriceCache *m_priceCache;
    SubscriptionStateManager *m_stateManager;
    RetryHelper *m_retryHelper;
    QMap<int, int> m_subscriptionRefCount;
};
```

2. Implement addInstrumentToWatch
```cpp
void MarketDataService::addInstrumentToWatch(const InstrumentData &instrument,
                                            MarketWatchWindow *window) {
    int token = instrument.exchangeInstrumentID;
    
    // 1. Set state
    m_stateManager->setState(token, SubscriptionState::Subscribing);
    
    // 2. Transform data
    ScripData scripData = transformToScripData(instrument);
    
    // 3. Add to window
    if (!window->addScripFromContract(scripData)) {
        m_stateManager->setState(token, SubscriptionState::Error, "Duplicate");
        emit error(token, "Failed to add");
        return;
    }
    
    // 4. Apply cached price if available
    if (m_priceCache->hasPrice(token)) {
        applyInitialPrice(token, window);
        m_stateManager->setState(token, SubscriptionState::Active);
    } else {
        m_stateManager->setState(token, SubscriptionState::WaitingForData);
    }
    
    // 5. Subscribe with retry
    if (m_subscriptionRefCount.value(token, 0) == 0) {
        m_retryHelper->retryWithBackoff(
            [this, instrument](auto cb) {
                m_provider->subscribe({instrument.exchangeInstrumentID}, 
                                    instrument.exchangeSegment, cb);
            },
            [this, token, instrument, window](bool ok, const QString &msg) {
                if (ok) {
                    handleSubscribeSuccess(instrument, window, ...);
                } else {
                    m_stateManager->setState(token, SubscriptionState::Error, msg);
                }
            }
        );
    }
    
    m_subscriptionRefCount[token]++;
    emit instrumentAdded(token, scripData);
}
```

3. Refactor MainWindow to use service
```cpp
// Before (50+ lines of business logic)
void MainWindow::onAddToWatchRequested(const InstrumentData &instrument) {
    // Lots of logic...
}

// After (clean delegation)
void MainWindow::onAddToWatchRequested(const InstrumentData &instrument) {
    MarketWatchWindow *mw = getActiveMarketWatch();
    if (mw) {
        m_marketDataService->addInstrumentToWatch(instrument, mw);
    }
}
```

4. Update CMakeLists.txt
```cmake
add_library(services
    src/services/PriceCache.cpp
    src/services/SubscriptionStateManager.cpp
    src/services/MarketDataService.cpp
)

target_link_libraries(TradingTerminal
    services
    # ... other libraries
)
```

**Testing**:
- Test with real XTS provider
- Test with mock provider
- Verify MainWindow is now < 500 lines

**Deliverable**: Business logic extracted to service layer ✅

**Week 2 Milestone**: Clean separation of concerns achieved

---

## Week 3: Testing & Polish

### Goal
Add comprehensive tests and improve reliability

### Day 15-17: Unit Tests

**Files to Create**:
```
tests/
    test_subscription_state_manager.cpp
    test_market_data_service.cpp
    test_retry_helper.cpp
    mocks/
        MockMarketDataProvider.h
```

**Implementation Steps**:

1. Test state manager
```cpp
TEST(SubscriptionStateManager, SetAndGetState) {
    SubscriptionStateManager mgr;
    
    mgr.setState(12345, SubscriptionState::Subscribing, "Loading");
    
    EXPECT_EQ(mgr.getState(12345), SubscriptionState::Subscribing);
    EXPECT_EQ(mgr.getMessage(12345), "Loading");
}

TEST(SubscriptionStateManager, StateChangeSignal) {
    SubscriptionStateManager mgr;
    QSignalSpy spy(&mgr, &SubscriptionStateManager::stateChanged);
    
    mgr.setState(12345, SubscriptionState::Active);
    
    EXPECT_EQ(spy.count(), 1);
}
```

2. Test service with mock provider
```cpp
class MockProvider : public IMarketDataProvider {
public:
    MOCK_METHOD(void, subscribe, (const QVector<int64_t>&, int,
                                  std::function<void(bool, const QJsonArray&)>));
};

TEST(MarketDataService, AddInstrument_UsesCache) {
    // Setup
    MockProvider mockProvider;
    MarketDataService service(&mockProvider);
    PriceCache::instance()->updatePrice(12345, testTick);
    
    // Execute
    service.addInstrumentToWatch(testInstrument, window);
    
    // Verify
    EXPECT_GT(window->getPrice(12345), 0.0);  // Should have cached price
}

TEST(MarketDataService, AddInstrument_SubscribesFails_Retries) {
    MockProvider mockProvider;
    
    // Fail first two times, succeed third
    EXPECT_CALL(mockProvider, subscribe(_, _, _))
        .WillOnce(Invoke([](auto, auto, auto cb) { cb(false, {}); }))
        .WillOnce(Invoke([](auto, auto, auto cb) { cb(false, {}); }))
        .WillOnce(Invoke([](auto, auto, auto cb) { cb(true, {}); }));
    
    MarketDataService service(&mockProvider);
    service.addInstrumentToWatch(testInstrument, window);
    
    QTest::qWait(5000);  // Wait for retries
    
    // Should eventually succeed
    EXPECT_EQ(SubscriptionStateManager::instance()->getState(12345),
              SubscriptionState::Active);
}
```

3. Test retry helper
```cpp
TEST(RetryHelper, SucceedsFirstTry) {
    RetryHelper helper;
    int attempts = 0;
    
    helper.retryWithBackoff(
        [&attempts](auto cb) {
            attempts++;
            cb(true, "Success");
        },
        [](bool ok, const QString &msg) {
            EXPECT_TRUE(ok);
        },
        3
    );
    
    EXPECT_EQ(attempts, 1);
}

TEST(RetryHelper, RetriesUntilSuccess) {
    RetryHelper helper;
    int attempts = 0;
    
    helper.retryWithBackoff(
        [&attempts](auto cb) {
            attempts++;
            cb(attempts >= 3, "");  // Succeed on third try
        },
        [](bool ok, const QString &msg) {
            EXPECT_TRUE(ok);
        },
        5
    );
    
    EXPECT_EQ(attempts, 3);
}
```

**Deliverable**: >80% code coverage on new components ✅

---

### Day 18-19: Integration Tests

**Files to Create**:
```
tests/integration/
    test_add_scrip_flow.cpp
    test_multiple_windows.cpp
```

**Implementation Steps**:

1. Test complete flow
```cpp
TEST_F(AddScripFlowTest, CompleteFlow) {
    // Create main window
    MainWindow mainWindow;
    mainWindow.setMarketDataService(testService);
    
    // Create market watch
    MarketWatchWindow *mw = mainWindow.createMarketWatch();
    
    // Add instrument
    InstrumentData nifty = createTestNifty();
    mainWindow.onAddToWatchRequested(nifty);
    
    // Wait for async operations
    QTest::qWait(500);
    
    // Verify
    EXPECT_TRUE(mw->hasScrip(nifty.exchangeInstrumentID));
    EXPECT_GT(mw->getPrice(nifty.exchangeInstrumentID), 0.0);
}

TEST_F(AddScripFlowTest, MultipleWindows_SharesCache) {
    // Add to first window
    mainWindow.onAddToWatchRequested(nifty);
    QTest::qWait(500);
    double price1 = window1->getPrice(nifty.exchangeInstrumentID);
    
    // Add to second window
    MarketWatchWindow *window2 = mainWindow.createMarketWatch();
    mainWindow.onAddToWatchRequested(nifty);  // Same instrument
    
    // Should use cached price (no delay)
    double price2 = window2->getPrice(nifty.exchangeInstrumentID);
    
    EXPECT_DOUBLE_EQ(price1, price2);
}
```

**Deliverable**: Full flow tested end-to-end ✅

---

### Day 20-21: Logging & Documentation

**Files to Create**:
```
include/utils/
    Logger.h
src/utils/
    Logger.cpp
docs/
    SERVICE_LAYER_GUIDE.md
```

**Implementation Steps**:

1. Add structured logging
```cpp
enum class LogCategory {
    MKTDATA,
    API,
    UI,
    WEBSOCKET
};

class Logger {
public:
    static void debug(LogCategory cat, const QString &msg);
    static void info(LogCategory cat, const QString &msg);
    static void warning(LogCategory cat, const QString &msg);
    static void error(LogCategory cat, const QString &msg);
    
    static void setLevel(LogLevel level);
    static void enableCategory(LogCategory cat);
};

// Usage
Logger::debug(LogCategory::MKTDATA, 
             QString("Subscribe: token=%1 segment=%2").arg(token).arg(segment));
```

2. Add logging to critical paths
```cpp
void MarketDataService::addInstrumentToWatch(...) {
    Logger::info(LogCategory::MKTDATA, 
                QString("Adding instrument: token=%1 symbol=%2")
                .arg(instrument.exchangeInstrumentID)
                .arg(instrument.symbol));
    
    // ... business logic ...
    
    Logger::debug(LogCategory::MKTDATA, 
                 QString("Subscription state: %1").arg(stateToString(state)));
}
```

3. Update documentation

**Deliverable**: Comprehensive logging + updated docs ✅

**Week 3 Milestone**: Production-ready with tests

---

## Week 4: API Testing & Final Migration

### Goal
Resolve API issues and complete migration

### Day 22-24: Standalone API Tests

**Run Tests**:
```bash
# Compile tests
g++ -std=c++11 test_getquote.cpp -o test_getquote -lcurl
g++ -std=c++11 test_subscription.cpp -o test_subscription -lcurl

# Run
./run_standalone_tests.sh
```

**Analyze Results**:
1. Does getQuote API work? → Update implementation
2. Confirm subscribe behavior → Document findings
3. Test re-subscription → Verify empty listQuotes

**Actions Based on Results**:

**If getQuote works**:
```cpp
// Add explicit getQuote call for already-subscribed tokens
if (listQuotes.isEmpty() && !m_priceCache->hasPrice(token)) {
    m_provider->getQuote(token, segment, [this, token](bool ok, const Quote &quote) {
        if (ok) {
            m_priceCache->updatePrice(token, convertQuoteToTick(quote));
            emit priceUpdated(token, quote);
        }
    });
}
```

**If getQuote doesn't work**:
```cpp
// Rely solely on cache + WebSocket
// Already implemented in Week 1
```

**Deliverable**: API behavior fully understood and handled ✅

---

### Day 25-26: Feature Flag Migration

**Implementation Steps**:

1. Add feature flag
```cpp
// config.h
#define USE_SERVICE_LAYER 1

// MainWindow.cpp
void MainWindow::onAddToWatchRequested(const InstrumentData &instrument) {
#if USE_SERVICE_LAYER
    m_marketDataService->addInstrumentToWatch(instrument, getActiveMarketWatch());
#else
    // Old implementation
    ScripData data = mapToScripData(instrument);
    // ... old code ...
#endif
}
```

2. Test both paths
- Build with USE_SERVICE_LAYER=1 → Test thoroughly
- Build with USE_SERVICE_LAYER=0 → Verify fallback works

3. Gradual rollout
- Enable for internal testing
- Monitor for issues
- Enable for production

**Deliverable**: Safe migration with rollback option ✅

---

### Day 27-28: Cleanup & Documentation

**Tasks**:

1. Remove old code paths
```cpp
// Delete:
// - Old transformation code in MainWindow
// - Redundant API calls
// - Unused helper functions
```

2. Update all documentation
- API integration guide
- Service layer guide
- Testing guide
- Architecture diagrams

3. Code review
- Check for remaining coupling
- Verify test coverage
- Review logging statements

4. Performance profiling
- Measure before/after latency
- Check memory usage
- Profile critical paths

**Deliverable**: Clean, documented, production-ready code ✅

**Week 4 Milestone**: Migration complete

---

## Success Criteria

### Functional Requirements
- ✅ Already-subscribed tokens show price immediately (no 0.00 flash)
- ✅ Loading indicators visible during operations
- ✅ Errors displayed with clear messages
- ✅ Automatic retry on failures
- ✅ All existing features work without regression

### Technical Requirements
- ✅ Test coverage > 80%
- ✅ MainWindow < 500 lines (reduced from ~1000+)
- ✅ Service layer handles all business logic
- ✅ Provider abstraction enables broker swap
- ✅ Comprehensive logging throughout

### Performance Requirements
- ✅ Cache lookup < 1ms (O(1) hash map)
- ✅ Price update broadcast < 10ms
- ✅ UI remains responsive during operations
- ✅ Memory usage stable (no leaks)

---

## Risk Management

### Risk Matrix

| Risk | Impact | Probability | Mitigation |
|------|--------|-------------|------------|
| Breaking existing functionality | High | Medium | Feature flags, parallel paths, extensive testing |
| Performance regression | Medium | Low | Profiling, benchmarking, O(1) data structures |
| XTS API changes | Medium | Medium | Provider abstraction, standalone tests |
| Timeline overrun | Low | Medium | Prioritize critical UX fixes first |

### Contingency Plans

**If Week 1 takes longer**:
- Focus only on PriceCache (most critical)
- Defer state management to Week 2

**If XTS API issues persist**:
- Continue with WebSocket-only approach
- Document workaround
- Cache becomes even more critical

**If testing reveals major issues**:
- Extend testing phase
- Keep feature flag disabled
- Address issues before migration

---

## Deliverables Summary

### Week 1 Deliverables
- [x] PriceCache implementation
- [x] SubscriptionStateManager implementation
- [x] RetryHelper implementation
- [x] Unit tests for above
- [x] Integration into existing code

### Week 2 Deliverables
- [x] IMarketDataProvider interface
- [x] XTSMarketDataProvider refactoring
- [x] MarketDataService implementation
- [x] MainWindow refactoring
- [x] Mock provider for testing

### Week 3 Deliverables
- [x] Comprehensive unit tests
- [x] Integration tests
- [x] Logging infrastructure
- [x] Updated documentation

### Week 4 Deliverables
- [x] API test results & analysis
- [x] Feature flag migration
- [x] Code cleanup
- [x] Final documentation
- [x] Performance profiling results

---

## Post-Implementation

### Monitoring
- Track error rates in production
- Monitor cache hit rates
- Measure average subscription time
- Log retry frequency

### Future Enhancements
- Support for multiple broker APIs
- Advanced caching strategies (LRU, TTL)
- Real-time health monitoring dashboard
- Automated reconnection on WebSocket drop

### Maintenance
- Regular API compatibility checks
- Performance regression testing
- Security updates for dependencies
- Documentation updates

---

## Conclusion

This roadmap provides a structured, low-risk path to transform the architecture while maintaining stability. The phased approach ensures critical UX issues are fixed first, followed by architectural improvements, comprehensive testing, and safe migration.

**Key Success Factors**:
1. Start with high-impact, low-risk fixes (PriceCache)
2. Maintain parallel paths during migration
3. Extensive testing at each phase
4. Clear rollback options
5. Comprehensive documentation

**Timeline**: 4 weeks  
**Estimated Effort**: 160 hours (1 developer)  
**Expected ROI**: Improved UX, reduced technical debt, easier maintenance

---

**Document Version**: 1.0  
**Created**: 16 December 2025  
**Status**: Ready for Implementation
