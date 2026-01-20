# ATM Watch Mechanism - Design & Architecture

## Overview

ATM (At-The-Money) watch is a specialized market watch feature for options traders that automatically tracks and displays option contracts with strike prices closest to the current underlying asset price. This document analyzes different mechanisms for ATM calculation and proposes an optimal architecture.

---

## 1. Core Requirements

### 1.1 Functional Requirements

1. **Automatic Strike Selection**
   - Identify strikes closest to current underlying price
   - Support configurable strike range (e.g., ±5 strikes from ATM)
   - Handle both Call and Put options

2. **Real-time Updates**
   - Update ATM strikes as underlying price moves
   - Efficient recalculation on price changes
   - Minimize unnecessary UI updates

3. **Multi-Expiry Support**
   - Display ATM strikes for multiple expiry dates
   - Support near-month, next-month, and quarterly expiries
   - Allow user to select specific expiries

4. **Symbol Support**
   - Work with all option-enabled underlyings (NIFTY, BANKNIFTY, stocks, etc.)
   - Handle different strike intervals (50, 100, etc.)
   - Support both NSE and BSE options

### 1.2 Performance Requirements

1. **Low Latency**
   - ATM recalculation < 1ms
   - UI update latency < 50ms
   - No blocking of price feed processing

2. **Memory Efficiency**
   - Efficient storage of strike metadata
   - Lazy loading of option chain data
   - Unsubscribe from out-of-range strikes

3. **Scalability**
   - Support multiple simultaneous ATM watches
   - Handle 100+ symbols per ATM watch
   - Efficient when multiple users watch same underlying

---

## 2. ATM Calculation Mechanisms - Comparison

### 2.1 Mechanism A: On-Demand Calculation

**Approach:**
- Calculate ATM strikes only when user requests or UI refreshes
- No continuous monitoring of underlying price
- Query price cache when calculation needed

**Advantages:**
- Simple implementation
- No background processing overhead
- Minimal memory footprint

**Disadvantages:**
- Higher UI latency (calculation on every refresh)
- May miss intermediate price movements
- Not suitable for real-time trading
- Inefficient for high-frequency updates

**Verdict:** ❌ Not suitable for real-time trading terminal

---

### 2.2 Mechanism B: Polling-Based Updates

**Approach:**
- Poll underlying price at regular intervals (e.g.,  atm strike recalculation to be done once every 2 minutes)

- Recalculate ATM strikes if price has changed significantly
- Update UI when strikes change

**Advantages:**
- Predictable update frequency
- Easy to implement with QTimer
- Configurable polling interval

**Disadvantages:**
- Delayed updates (up to polling interval)
- Unnecessary calculations when price hasn't changed
- Wastes CPU cycles during market lull
- Not event-driven

**Verdict:** ⚠️ Acceptable fallback, but not optimal

---

### 2.3 Mechanism C: Event-Driven with Price Thresholds

**Approach:**
- Subscribe to underlying price updates via PriceCache
- Trigger recalculation only when price crosses threshold
- Threshold = half of strike interval (e.g., 25 for 50-interval strikes)
- Emit signals for UI updates only when strikes change

**Advantages:**
- True real-time updates
- Minimal CPU usage (only on significant price changes)
- Event-driven, no polling overhead
- Leverages existing PriceCache infrastructure
- Optimal for trading scenarios

**Disadvantages:**
- More complex implementation
- Needs careful threshold management
- Requires proper subscription lifecycle

**Verdict:** ✅ **RECOMMENDED** - Best balance of performance and accuracy

---

### 2.4 Mechanism D: Hybrid (Event + Periodic Validation)

**Approach:**
- Primary: Event-driven updates (Mechanism C)
- Secondary: Periodic validation (every 5-10 seconds)
- Validation ensures no missed updates due to race conditions

**Advantages:**
- Combines best of event-driven and polling
- Self-healing if events are missed
- High confidence in correctness

**Disadvantages:**
- Slightly more complex
- Small periodic overhead

**Verdict:** ✅ **RECOMMENDED for Production** - Most robust

---

## 3. Recommended Architecture: Hybrid Event-Driven

### 3.1 Component Design

```
┌─────────────────────────────────────────────────────────────┐
│                      ATMWatchManager                        │
│  - Manages multiple ATM watch instances                     │
│  - Handles lifecycle and resource management                │
└─────────────────────────────────────────────────────────────┘
                              │
                              │ creates/manages
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                      ATMWatchInstance                       │
│  - One per underlying symbol                                │
│  - Maintains current ATM state                              │
│  - Configuration: strikes range, expiries, CE/PE            │
└─────────────────────────────────────────────────────────────┘
                              │
                ┌─────────────┼─────────────┐
                │             │             │
                ▼             ▼             ▼
┌──────────────────┐  ┌──────────────┐  ┌──────────────┐
│  ATMCalculator   │  │  PriceCache  │  │ MasterRepo   │
│  - Strike logic  │  │  - Prices    │  │ - Metadata   │
│  - Threshold     │  │  - Subscribe │  │ - Strikes    │
└──────────────────┘  └──────────────┘  └──────────────┘
```

### 3.2 Class Structure

```cpp
// Core ATM watch instance for a single underlying
class ATMWatchInstance : public QObject {
    Q_OBJECT
    
public:
    struct Config {
        QString underlyingSymbol;      // e.g., "NIFTY"
        QVector<QString> expiries;     // e.g., ["27FEB25", "27MAR25"]
        int strikeRange = 5;           // ±5 strikes from ATM
        bool includeCE = true;
        bool includePE = true;
        int updateIntervalMs = 5000;   // Periodic validation
    };
    
    struct ATMState {
        double underlyingPrice = 0.0;
        int atmStrike = 0;             // Nearest strike
        QVector<int> strikes;          // All strikes in range
        QDateTime lastUpdate;
        bool isValid = false;
    };
    
    ATMWatchInstance(const Config& config, QObject* parent = nullptr);
    ~ATMWatchInstance();
    
    // Start/stop monitoring
    void start();
    void stop();
    
    // Query current state
    ATMState getCurrentState() const;
    QVector<TokenInfo> getWatchTokens() const;
    
    // Configuration
    void updateConfig(const Config& config);
    
signals:
    void atmStrikesChanged(const ATMState& newState);
    void underlyingPriceUpdated(double price);
    void error(const QString& message);
    
private slots:
    void onUnderlyingPriceUpdate(const QString& symbol, double ltp);
    void onPeriodicValidation();
    
private:
    void initialize();
    void subscribeToUnderlying();
    void unsubscribeAll();
    void recalculateATM(double price);
    bool shouldRecalculate(double newPrice) const;
    QVector<int> findATMStrikes(double price) const;
    void updateSubscriptions(const QVector<int>& newStrikes);
    
    Config m_config;
    ATMState m_currentState;
    QTimer* m_validationTimer;
    
    // Threshold for triggering recalculation
    double m_priceThreshold = 0.0;  // Calculated based on strike interval
    double m_lastCalculatedPrice = 0.0;
    
    // Strike metadata cache
    struct StrikeMetadata {
        int strikePrice;
        int strikeInterval;
        QVector<QString> availableExpiries;
    };
    QMap<QString, StrikeMetadata> m_strikeCache;  // symbol -> metadata
};


// Manager for multiple ATM watch instances
class ATMWatchManager : public QObject {
    Q_OBJECT
    
public:
    static ATMWatchManager* instance();
    
    // Create/destroy ATM watch instances
    ATMWatchInstance* createWatch(const ATMWatchInstance::Config& config);
    void removeWatch(const QString& watchId);
    ATMWatchInstance* getWatch(const QString& watchId);
    
    // Lifecycle
    void startAll();
    void stopAll();
    
signals:
    void watchCreated(const QString& watchId);
    void watchRemoved(const QString& watchId);
    
private:
    ATMWatchManager(QObject* parent = nullptr);
    static ATMWatchManager* s_instance;
    
    QMap<QString, ATMWatchInstance*> m_watches;
    QMutex m_mutex;
};


// Pure calculator (stateless utility)
class ATMCalculator {
public:
    struct CalculationParams {
        double underlyingPrice;
        int strikeInterval;        // e.g., 50, 100
        int rangeCount = 5;        // ±5 strikes
    };
    
    struct CalculationResult {
        int atmStrike;             // Exact or nearest ATM strike
        QVector<int> allStrikes;   // All strikes in range
        double priceToAtmDistance; // Distance from price to ATM
    };
    
    // Core calculation logic
    static CalculationResult calculateATM(const CalculationParams& params);
    
    // Helper functions
    static int findNearestStrike(double price, int interval);
    static QVector<int> generateStrikeRange(int atmStrike, int interval, int range);
    static double calculateThreshold(int strikeInterval);
    
private:
    ATMCalculator() = delete;  // Static utility class
};
```

### 3.3 Data Flow

```
1. User Creates ATM Watch
   └─> ATMWatchManager::createWatch()
       └─> ATMWatchInstance created with config
           └─> ATMWatchInstance::start()
               ├─> Subscribe to underlying via PriceCache
               ├─> Calculate initial strike metadata
               ├─> Calculate initial ATM strikes
               ├─> Subscribe to option strikes
               └─> Start validation timer

2. Underlying Price Update (Event-Driven)
   └─> PriceCache::priceUpdated signal
       └─> ATMWatchInstance::onUnderlyingPriceUpdate()
           ├─> Check if price crossed threshold
           ├─> If yes: recalculateATM()
           │   ├─> ATMCalculator::calculateATM()
           │   ├─> Compare with current strikes
           │   ├─> If changed: updateSubscriptions()
           │   └─> Emit atmStrikesChanged()
           └─> If no: just emit underlyingPriceUpdated()

3. Periodic Validation (Safety Net)
   └─> QTimer timeout every 5 seconds
       └─> ATMWatchInstance::onPeriodicValidation()
           ├─> Fetch current underlying price
           ├─> Recalculate ATM
           ├─> If drift detected: correct state
           └─> Emit signals if changed

4. User Changes Config
   └─> ATMWatchInstance::updateConfig()
       ├─> Unsubscribe from old strikes
       ├─> Recalculate with new params
       └─> Subscribe to new strikes

5. Cleanup
   └─> ATMWatchInstance::stop() or destructor
       ├─> Unsubscribe from all symbols
       └─> Stop validation timer
```

---

## 4. Threshold Calculation Strategy

### 4.1 Principle

The threshold determines when to recalculate ATM strikes. It should be:
- **Large enough** to avoid excessive recalculations
- **Small enough** to catch strike transitions early

### 4.2 Recommended Formula

```cpp
double ATMCalculator::calculateThreshold(int strikeInterval) {
    // Threshold = 40% of strike interval
    // This ensures recalculation well before crossing to next strike
    return strikeInterval * 0.4;
}
```

**Examples:**
- NIFTY (interval=50): threshold = 20 points
- BANKNIFTY (interval=100): threshold = 40 points
- Stock options (interval=10): threshold = 4 points

### 4.3 Dynamic Threshold Adjustment

```cpp
void ATMWatchInstance::recalculateATM(double price) {
    // Find strike interval for this underlying
    int interval = getStrikeInterval(m_config.underlyingSymbol);
    
    // Update threshold
    m_priceThreshold = ATMCalculator::calculateThreshold(interval);
    
    // Calculate ATM
    ATMCalculator::CalculationParams params;
    params.underlyingPrice = price;
    params.strikeInterval = interval;
    params.rangeCount = m_config.strikeRange;
    
    auto result = ATMCalculator::calculateATM(params);
    
    // Check if strikes changed
    if (result.atmStrike != m_currentState.atmStrike ||
        result.allStrikes != m_currentState.strikes) {
        
        // Update state
        m_currentState.atmStrike = result.atmStrike;
        m_currentState.strikes = result.allStrikes;
        m_currentState.underlyingPrice = price;
        m_currentState.lastUpdate = QDateTime::currentDateTime();
        m_currentState.isValid = true;
        
        // Update subscriptions
        updateSubscriptions(result.allStrikes);
        
        // Notify UI
        emit atmStrikesChanged(m_currentState);
    }
    
    m_lastCalculatedPrice = price;
}
```

---

## 5. Strike Subscription Management

### 5.1 Challenges

1. **Dynamic subscription set**: As price moves, need to subscribe to new strikes and unsubscribe from old ones
2. **Multi-expiry handling**: Each strike may have multiple expiries
3. **Resource efficiency**: Avoid subscribing to too many symbols
4. **Race conditions**: Handle subscription during rapid price movements

### 5.2 Subscription Strategy

```cpp
void ATMWatchInstance::updateSubscriptions(const QVector<int>& newStrikes) {
    QSet<QString> newSymbols;
    QSet<QString> oldSymbols = m_subscribedSymbols;
    
    // Build new symbol set
    for (int strike : newStrikes) {
        for (const QString& expiry : m_config.expiries) {
            if (m_config.includeCE) {
                QString ceSymbol = buildOptionSymbol(
                    m_config.underlyingSymbol, expiry, strike, "CE");
                newSymbols.insert(ceSymbol);
            }
            if (m_config.includePE) {
                QString peSymbol = buildOptionSymbol(
                    m_config.underlyingSymbol, expiry, strike, "PE");
                newSymbols.insert(peSymbol);
            }
        }
    }
    
    // Unsubscribe from removed symbols
    QSet<QString> toUnsubscribe = oldSymbols - newSymbols;
    for (const QString& symbol : toUnsubscribe) {
        PriceCache::instance()->unsubscribe(symbol, this);
    }
    
    // Subscribe to new symbols
    QSet<QString> toSubscribe = newSymbols - oldSymbols;
    for (const QString& symbol : toSubscribe) {
        PriceCache::instance()->subscribe(symbol, this);
    }
    
    m_subscribedSymbols = newSymbols;
}
```

### 5.3 Batch Subscription Optimization

```cpp
// Instead of individual subscribe calls, batch them
void ATMWatchInstance::updateSubscriptions(const QVector<int>& newStrikes) {
    QVector<QString> toSubscribe, toUnsubscribe;
    
    // ... calculate diff ...
    
    // Batch subscribe/unsubscribe
    if (!toUnsubscribe.isEmpty()) {
        PriceCache::instance()->batchUnsubscribe(toUnsubscribe, this);
    }
    if (!toSubscribe.isEmpty()) {
        PriceCache::instance()->batchSubscribe(toSubscribe, this);
    }
}
```

---

## 6. Integration with Existing Systems

### 6.1 PriceCache Integration

**Requirements:**
- Subscribe to underlying symbol for LTP updates
- Subscribe to option strikes (CE/PE) for price display
- Batch subscription API for efficiency

**Changes Needed:**

```cpp
// Add to PriceCache
class PriceCache {
public:
    // Batch operations for ATM watch
    void batchSubscribe(const QVector<QString>& symbols, QObject* subscriber);
    void batchUnsubscribe(const QVector<QString>& symbols, QObject* subscriber);
    
    // Get price without subscribing (for validation)
    double getLastPrice(const QString& symbol) const;
};
```

### 6.2 MasterRepository Integration

**Requirements:**
- Query strike interval for underlying
- Get available expiries for underlying
- Get all strikes for underlying+expiry

**Changes Needed:**

```cpp
// Add to MasterRepository
class MasterRepository {
public:
    // ATM watch specific queries
    int getStrikeInterval(const QString& underlyingSymbol) const;
    QVector<QString> getAvailableExpiries(const QString& underlyingSymbol) const;
    QVector<int> getAllStrikes(const QString& underlyingSymbol, 
                               const QString& expiry) const;
    
    // Get option symbol from components
    QString getOptionSymbol(const QString& underlying,
                           const QString& expiry,
                           int strike,
                           const QString& optionType) const;
};
```

### 6.3 UI Integration

**Market Watch Window Enhancement:**

```cpp
class MarketWatchWindow {
private slots:
    void onCreateATMWatch();
    void onATMConfigDialog();
    void onATMStrikesChanged(const ATMWatchInstance::ATMState& state);
    
private:
    ATMWatchInstance* m_atmWatch = nullptr;
    void displayATMStrikes(const QVector<TokenInfo>& tokens);
};
```

**ATM Configuration Dialog:**
```cpp
class ATMConfigDialog : public QDialog {
    // User inputs:
    // - Underlying symbol (combo box)
    // - Expiries (multi-select list)
    // - Strike range (spinbox: ±3, ±5, ±7, ±10)
    // - Include CE/PE (checkboxes)
    // - Update interval (spinbox for validation timer)
};
```

---

## 7. Performance Optimization

### 7.1 Memory Optimization

1. **Lazy Strike Metadata Loading**
   - Load strike data only for selected underlyings
   - Cache in ATMWatchInstance, not globally

2. **Subscription Pooling**
   - Multiple ATM watches for same underlying share subscription
   - Reference counting for price subscriptions

3. **State Diffing**
   - Compare new vs old strike lists before updating
   - Only emit signals if actual change occurred

### 7.2 CPU Optimization

1. **Threshold-Based Recalculation**
   - Skip recalculation if price within threshold
   - Reduces calculations by ~95% for typical price movements

2. **Cached Calculations**
   ```cpp
   // Cache strike interval to avoid repeated lookups
   int m_cachedStrikeInterval = 0;
   
   // Cache sorted strike list
   QVector<int> m_cachedAllStrikes;
   ```

3. **Efficient Strike Search**
   ```cpp
   int ATMCalculator::findNearestStrike(double price, int interval) {
       // O(1) calculation instead of searching
       return static_cast<int>(std::round(price / interval)) * interval;
   }
   ```

### 7.3 Concurrency Considerations

```cpp
class ATMWatchInstance : public QObject {
private:
    // Protect state during updates
    mutable QReadWriteLock m_stateLock;
    
    ATMState getCurrentState() const {
        QReadLocker lock(&m_stateLock);
        return m_currentState;
    }
    
    void recalculateATM(double price) {
        // Calculate outside lock
        auto result = ATMCalculator::calculateATM(params);
        
        // Update under write lock
        QWriteLocker lock(&m_stateLock);
        m_currentState = newState;
    }
};
```

---

## 8. Edge Cases & Error Handling

### 8.1 Edge Cases

1. **Missing Strike Interval**
   - Fallback to default (e.g., 50 for indices)
   - Log warning

2. **No Valid Strikes**
   - Underlying not option-enabled
   - Display error in UI

3. **Expiry Not Available**
   - Filter out invalid expiries
   - Warn user

4. **Price Spikes**
   - Validate price before recalculating
   - Ignore if >20% jump in single update (likely bad tick)

5. **Market Closed**
   - Continue monitoring but flag as stale
   - Resume on market open

### 8.2 Error Handling

```cpp
void ATMWatchInstance::initialize() {
    try {
        // Validate underlying exists
        if (!MasterRepository::instance()->hasSymbol(m_config.underlyingSymbol)) {
            emit error("Underlying symbol not found");
            return;
        }
        
        // Get strike interval
        m_cachedStrikeInterval = MasterRepository::instance()
            ->getStrikeInterval(m_config.underlyingSymbol);
        if (m_cachedStrikeInterval == 0) {
            emit error("Strike interval not available");
            return;
        }
        
        // Validate expiries
        auto availableExpiries = MasterRepository::instance()
            ->getAvailableExpiries(m_config.underlyingSymbol);
        for (const QString& expiry : m_config.expiries) {
            if (!availableExpiries.contains(expiry)) {
                qWarning() << "Expiry not available:" << expiry;
            }
        }
        
        // Success
        subscribeToUnderlying();
        
    } catch (const std::exception& e) {
        emit error(QString("Initialization failed: %1").arg(e.what()));
    }
}
```

---

## 9. Testing Strategy

### 9.1 Unit Tests

```cpp
class ATMCalculatorTest : public QObject {
    Q_OBJECT
    
private slots:
    void testFindNearestStrike();
    void testGenerateStrikeRange();
    void testCalculateATM();
    void testThresholdCalculation();
};

void ATMCalculatorTest::testFindNearestStrike() {
    // Strike interval 50
    QCOMPARE(ATMCalculator::findNearestStrike(18256.75, 50), 18250);
    QCOMPARE(ATMCalculator::findNearestStrike(18276.50, 50), 18300);
    
    // Strike interval 100
    QCOMPARE(ATMCalculator::findNearestStrike(45678.50, 100), 45700);
}
```

### 9.2 Integration Tests

1. **Price Update Simulation**
   - Mock PriceCache price updates
   - Verify ATM recalculation triggers

2. **Subscription Management**
   - Verify subscribe/unsubscribe calls
   - Check for subscription leaks

3. **Multi-Instance Testing**
   - Create multiple ATM watches
   - Verify isolation

### 9.3 Performance Tests

1. **Recalculation Latency**
   - Target: <1ms for typical case
   - Measure with 1000 iterations

2. **Memory Usage**
   - Monitor with 10+ ATM watch instances
   - Check for memory leaks

3. **Subscription Churn**
   - Simulate rapid price movements
   - Verify efficient subscription updates

---

## 10. Implementation Phases

### Phase 1: Core Infrastructure (Week 1)
- [ ] Implement `ATMCalculator` (pure calculation logic)
- [ ] Implement `ATMWatchInstance` (single watch)
- [ ] Add PriceCache batch subscription API
- [ ] Add MasterRepository query methods
- [ ] Unit tests for calculator

### Phase 2: Manager & Integration (Week 2)
- [ ] Implement `ATMWatchManager`
- [ ] Integrate with PriceCache price updates
- [ ] Implement subscription management
- [ ] Integration tests

### Phase 3: UI Integration (Week 3)
- [ ] Create ATM configuration dialog
- [ ] Add "Create ATM Watch" menu option
- [ ] Display ATM strikes in market watch
- [ ] Visual indicators for ATM strike

### Phase 4: Optimization & Testing (Week 4)
- [ ] Performance profiling
- [ ] Memory optimization
- [ ] Edge case handling
- [ ] User acceptance testing

---

## 11. Future Enhancements

### 11.1 Advanced Features

1. **ATM Straddle/Strangle Views**
   - Display combined CE+PE positions
   - Calculate combined Greeks

2. **Historical ATM Tracking**
   - Log ATM strike transitions
   - Analyze price behavior around strikes

3. **Smart Expiry Selection**
   - Auto-select most liquid expiries
   - Prioritize near-term expiries

4. **Multi-Underlying ATM**
   - Compare ATM across multiple underlyings
   - E.g., NIFTY vs BANKNIFTY

### 11.2 Analytics

1. **ATM Volatility**
   - Track IV changes at ATM strikes
   - Alert on unusual volatility

2. **OI Analysis**
   - Show OI buildup at ATM strikes
   - Max pain calculation

3. **PCR (Put-Call Ratio)**
   - Calculate PCR for ATM strikes
   - Trend analysis

---

## 12. Conclusion

### Recommended Approach: **Hybrid Event-Driven (Mechanism D)**

**Key Characteristics:**
- ✅ Event-driven primary updates (low latency)
- ✅ Periodic validation for robustness
- ✅ Threshold-based recalculation (efficiency)
- ✅ Efficient subscription management
- ✅ Integrates seamlessly with existing PriceCache

**Expected Performance:**
- Recalculation latency: <1ms
- UI update latency: <50ms
- Memory per instance: ~500KB
- CPU usage: <0.1% during normal trading

**Next Steps:**
1. Review and approve architecture
2. Begin Phase 1 implementation
3. Create sample ATM watch for NIFTY
4. Iterate based on user feedback

---

## Appendix A: Sample Configuration

```ini
[ATMWatch_NIFTY]
underlying=NIFTY
expiries=27FEB25,27MAR25
strike_range=5
include_ce=true
include_pe=true
validation_interval=5000

[ATMWatch_BANKNIFTY]
underlying=BANKNIFTY
expiries=27FEB25
strike_range=7
include_ce=true
include_pe=true
validation_interval=5000
```

## Appendix B: Example Usage

```cpp
// Create ATM watch for NIFTY
ATMWatchInstance::Config config;
config.underlyingSymbol = "NIFTY";
config.expiries = {"27FEB25", "27MAR25"};
config.strikeRange = 5;
config.includeCE = true;
config.includePE = true;

auto* atmWatch = ATMWatchManager::instance()->createWatch(config);

// Connect to signals
connect(atmWatch, &ATMWatchInstance::atmStrikesChanged,
        this, &MarketWatchWindow::onATMStrikesChanged);

// Start monitoring
atmWatch->start();

// Later: query current state
auto state = atmWatch->getCurrentState();
qDebug() << "Current ATM Strike:" << state.atmStrike;
qDebug() << "Underlying Price:" << state.underlyingPrice;
```

---

**Document Version:** 1.0  
**Last Updated:** January 20, 2026  
**Author:** Trading Terminal Development Team
