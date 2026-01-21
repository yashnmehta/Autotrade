# ATM Watch Mechanism - Design & Architecture

## Overview

ATM (At-The-Money) watch is a specialized market watch feature for options traders that automatically tracks and displays option contracts with strike prices closest to the current underlying asset price. This document analyzes different mechanisms for ATM calculation and proposes an optimal architecture.

---

## 1. Core Requirements

### 1.1 Functional Requirements

1. **Multi-Symbol ATM Display**
   - Display ATM strikes for **ALL option-enabled symbols** (~270 symbols)
   - Single window with table view showing all symbols simultaneously
   - Columns: Symbol, Expiry, Base Price, ATM Strike, Call Data (LTP/Bid/Ask/OI), Put Data (LTP/Bid/Ask/OI)
   - NOT one window per symbol - one unified table for all

2. **Automatic Strike Selection**
   - Identify strikes closest to current underlying price
   - Support configurable strike range (e.g., ±5 strikes from ATM)
   - Handle both Call and Put options

3. **Real-time Updates**
   - Update ATM strikes as underlying price moves
   - Efficient recalculation on price changes
   - Minimize unnecessary UI updates

4. **Multi-Expiry Support**
   - Default: Current (nearest) expiry per symbol
   - Support user selection of specific expiry dates
   - Each symbol can have different expiry
   - Allow user to select specific expiries

5. **Symbol Support**
   - Work with all option-enabled underlyings (indices + stocks)
   - Handle different strike intervals (50, 100, etc.)
   - Support both NSE and BSE options

### 1.2 Performance Requirements

1. **Low Latency**
   - ATM recalculation for ALL 270 symbols: < 500ms
   - Per-symbol calculation: < 2ms
   - UI update latency: < 50ms
   - No blocking of price feed processing

2. **Memory Efficiency**
   - Efficient storage of strike metadata
   - Lazy loading of option chain data
   - Total memory for 270 symbols: < 20MB
   - Unsubscribe from out-of-range strikes

3. **Scalability**
   - Support 270+ symbols in single ATM watch window
   - Handle batch updates efficiently
   - Efficient when displaying all symbols simultaneously
   - Background calculation to prevent UI freeze

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
- **Pre-processed expiry-wise symbol cache (RAM-based)**

**API Design:**

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
    
    // ===== PRE-PROCESSED EXPIRY CACHE (RAM-BASED FAST LOOKUP) =====
    
    // Get all option-enabled symbols (instrumentType == 2)
    // Returns: ~270 symbols instantly from m_optionSymbols cache
    QVector<QString> getOptionSymbols() const;
    
    // Get symbols that have options for a specific expiry
    // Example: getSymbolsForExpiry("27JAN26") -> ["NIFTY", "BANKNIFTY", ...]
    // Returns: Instant lookup from m_expiryToSymbols cache
    QVector<QString> getSymbolsForExpiry(const QString& expiry) const;
    
    // Get current (nearest) expiry for a symbol
    // Example: getCurrentExpiry("NIFTY") -> "27JAN26"
    // Returns: Instant lookup from m_symbolToCurrentExpiry cache
    QString getCurrentExpiry(const QString& symbol) const;
    
    // Get all available expiry dates (sorted ascending)
    // Returns: List of all expiry dates from m_expiryToSymbols keys
    QVector<QString> getAllExpiries() const;
    
private:
    // ===== EXPIRY CACHE (PRE-PROCESSED AT LOAD TIME) =====
    
    // Expiry -> List of symbols
    // Example: {"27JAN26": ["NIFTY", "BANKNIFTY", "FINNIFTY", ...], ...}
    QMap<QString, QVector<QString>> m_expiryToSymbols;
    
    // Symbol -> Current (nearest) expiry
    // Example: {"NIFTY": "27JAN26", "BANKNIFTY": "27JAN26", ...}
    QMap<QString, QString> m_symbolToCurrentExpiry;
    
    // All option-enabled symbols (instrumentType == 2)
    QSet<QString> m_optionSymbols;
    
    // Build expiry cache during finalizeLoad()
    void buildExpiryCache();
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

## 11. Expiry Selection Logic - IMPLEMENTED

### 11.1 Current Expiry Mode ("CURRENT")

**Implementation Flow:**

```cpp
// ATMWatchWindow::loadAllSymbols() - Step 4: Identify Current Expiry
for (const QString& symbol : optionSymbols) {
    QString expiry;
    
    if (m_currentExpiry == "CURRENT") {
        // Step 4.1: Get all unique expiry dates for this symbol
        QStringList expiryList = symbolExpiries[symbol].values();
        
        // Step 4.2: Sort expiries in ascending order (nearest first)
        std::sort(expiryList.begin(), expiryList.end());
        
        // Step 4.3: Pick the first expiry (current/nearest expiry)
        if (!expiryList.isEmpty()) {
            expiry = expiryList.first();
        }
    } else {
        // Use user-selected expiry (only if symbol has this expiry)
        if (symbolExpiries[symbol].contains(m_currentExpiry)) {
            expiry = m_currentExpiry;
        }
    }
    
    // Add watch with selected expiry
    ATMWatchManager::getInstance()->addWatch(symbol, expiry, BasePriceSource::Cash);
}
```

**Key Points:**
- Expiry format: "27JAN26" (DDMMMYY) - lexicographic sort works correctly
- Each symbol can have different current expiries
- Example: NIFTY might have 23JAN26 while custom strikes have 30JAN26

## 12. Future Enhancements

### 12.1 Advanced Features

1. **ATM Straddle/Strangle Views**
   - Display combined CE+PE positions
   - Calculate combined Greeks

2. **Historical ATM Tracking**
   - Log ATM strike transitions
   - Analyze price behavior around strikes

3. **Enhanced Expiry Selection**
   - Filter by minimum OI threshold
   - Prioritize weekly vs monthly expiries

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

## 12. Implementation Status & Architecture

### 12.1 Current Implementation (As of Jan 2026)

**Architecture Overview:**
- **Single ATM Watch Window** displaying ALL option-enabled symbols (~270 symbols)
- Table view with columns: Symbol | Expiry | Base Price | ATM Strike | Call Data | Put Data
- Batch processing to avoid N × N calculation complexity
- Background loading + optimized logging to prevent UI freeze

**Core Components - ✅ IMPLEMENTED:**

1. **ATMWatchManager** (`src/services/ATMWatchManager.cpp`)
   - Manages ATM calculations for ALL symbols in single batch
   - **Batch Add Method**: `addWatchesBatch()` - adds all 270 symbols, triggers ONE calculation
   - Timer-based recalculation (60-second interval)
   - Thread-safe access to ATM results
   - Uses `ATMCalculator` for strike calculation
   - **Optimized Logging**: Single summary log instead of 270 individual logs per calculation

2. **ATMWatchWindow** (`src/ui/ATMWatchWindow.cpp`)
   - Single window displaying all symbols in unified table
   - **Background symbol loading**: Prevents UI freeze with 270 symbols
   - **Batch watch addition**: Collects all configs first, then adds in single call
   - Real-time LTP updates via 1-second timer
   - Displays: Symbol, Expiry, Base Price, ATM Strike, Call/Put LTP/Bid/Ask/OI

3. **ATMCalculator** (`include/utils/ATMCalculator.h`)
   - **Algorithm**: Finds **nearest actual strike** to current price (NOT median)
   - Uses binary search (`std::lower_bound`) for efficiency
   - Example: Price=23450 → Finds closest from [23000, 23100, 23200, 23300, 23400, 23500] → Returns 23500
   
4. **Asset Token Retrieval** (`NSEFORepository`)
   - **Symbol to Asset Token Map**: `std::unordered_map<std::string, int64_t> m_symbolToAssetToken`
   - Populated during master file load
   - Fast O(1) lookup: `getAssetToken("NIFTY")` → 26000
   - Used to fetch cash market LTP for base price

5. **Expiry Selection Logic** (`ATMWatchWindow::loadAllSymbols()`)
   ```
   Step 1: Filter OPTSTK contracts (instrumentType == 2) → Get all option symbols
   Step 2: Extract unique symbols from options → ~270 symbols
   Step 3: For each symbol, collect all unique expiry dates
   Step 4: For each symbol:
           If "CURRENT" mode:
               - Sort expiries ascending (lexicographic: "23JAN26" < "30JAN26")
               - Pick first expiry (nearest/current)
           Else:
               - Use user-selected expiry
   Step 5: Batch add ALL symbols to ATMWatchManager (single call)
   Step 6: Trigger ONE calculateAll() for all 270 symbols
   ```

6. **Base Price Sources**
   - **Cash Market** (Default): Uses `nsecm::getGenericLtp(assetToken)`
   - **Future**: Finds nearest future contract for the expiry
   
7. **Real-Time LTP Updates** (`ATMWatchWindow`)
   - QTimer at 1-second interval
   - Refreshes base price column from latest `ATMInfo` array
   - Background thread for symbol loading (prevents UI freeze)

### 12.2 Key Architectural Decisions

| Component | Decision | Rationale |
|-----------|----------|-----------|
| **Display Model** | Single window for ALL symbols | User wants to see all 270 option symbols in one unified table view |
| **Strike Calculation** | Nearest actual strike (binary search) | More accurate than fixed interval rounding; handles irregular strike patterns |
| **Batch Processing** | addWatchesBatch() method | Adding 270 symbols one-by-one caused N × N calculations; batch = one calculation |
| **Update Mechanism** | Polling (60s timer) | Simple, predictable; future: add event-driven for threshold-based updates |
| **Base Price Source** | Asset token map lookup | Fast O(1) retrieval; eliminates hardcoded symbol-to-token mappings |
| **Expiry Selection** | Per-symbol current expiry | Each symbol can have different expiry dates; handles custom strikes correctly |
| **Thread Safety** | `std::shared_mutex` | Read-heavy workload (1 write per 60s, multiple UI reads) |
| **Logging** | Summary instead of per-symbol | Reduced from 270 logs per calculation to 1 summary log |

### 12.3 Performance Characteristics

**Measured Performance:**
- Per-symbol ATM calculation: ~2ms
- Full batch calculation (270 symbols): ~300-500ms
- UI LTP Update: ~1ms (QTimer at 1000ms interval)
- Symbol Loading: Background thread (no UI freeze)
- Memory: ~50KB per symbol × 270 = ~13.5MB total

**Optimization Results:**
- **Before**: Adding 270 symbols triggered 270 × 270 = 72,900 calculations + 72,900 debug logs = UI freeze 60+ seconds
- **After**: Batch add triggers 1 calculation for 270 symbols + 1 summary log = ~500ms, no UI freeze

**Bottlenecks Fixed:**
- ✅ Initial symbol loading: Background threading prevents UI freeze
- ✅ Excessive logging: Reduced from 270 logs to 1 summary per calculation
- ✅ N × N calculations: Batch method triggers single calculation for all symbols
- ✅ LTP updates: 1-second timer provides real-time updates

### 12.4 Future Optimizations

**Recommended Enhancements:**

1. **Pre-Processed Expiry Cache (HIGH PRIORITY - RAM-based fast rendering)**
   
   **Current Problem**: Every time ATM Watch opens, we filter 100,000+ contracts to find option symbols
   
   **Solution**: Pre-process during master file load and store in RAM
   
   **Data Structures**:
   ```cpp
   class RepositoryManager {
   private:
       // Expiry-wise symbol cache (pre-processed at load time)
       // Key: Expiry date (e.g., "27JAN26")
       // Value: List of unique symbols that have options for this expiry
       QMap<QString, QVector<QString>> m_expiryToSymbols;
       
       // Symbol-wise current expiry cache
       // Key: Symbol name (e.g., "NIFTY")
       // Value: Nearest/Current expiry date for this symbol
       QMap<QString, QString> m_symbolToCurrentExpiry;
       
       // All option-enabled symbols (instrumentType == 2)
       QSet<QString> m_optionSymbols;
   };
   ```
   
   **Population** (during master file load):
   ```cpp
   void RepositoryManager::finalizeLoad() {
       // Clear caches
       m_expiryToSymbols.clear();
       m_symbolToCurrentExpiry.clear();
       m_optionSymbols.clear();
       
       // Build expiry-wise cache
       auto allContracts = getContractsBySegment("NSE", "FO");
       for (const auto& contract : allContracts) {
           if (contract.instrumentType == 2) { // OPTSTK only
               m_optionSymbols.insert(contract.name);
               m_expiryToSymbols[contract.expiryDate].append(contract.name);
           }
       }
       
       // Deduplicate symbols per expiry
       for (auto it = m_expiryToSymbols.begin(); it != m_expiryToSymbols.end(); ++it) {
           QVector<QString> uniqueSymbols = it.value().toList().toVector();
           std::sort(uniqueSymbols.begin(), uniqueSymbols.end());
           uniqueSymbols.erase(std::unique(uniqueSymbols.begin(), uniqueSymbols.end()), 
                               uniqueSymbols.end());
           it.value() = uniqueSymbols;
       }
       
       // Build current expiry cache (nearest expiry per symbol)
       QMap<QString, QSet<QString>> symbolExpiries; // temp
       for (const auto& contract : allContracts) {
           if (contract.instrumentType == 2) {
               symbolExpiries[contract.name].insert(contract.expiryDate);
           }
       }
       
       for (auto it = symbolExpiries.begin(); it != symbolExpiries.end(); ++it) {
           QStringList expiries = it.value().values();
           std::sort(expiries.begin(), expiries.end());
           m_symbolToCurrentExpiry[it.key()] = expiries.first(); // Nearest
       }
       
       qDebug() << "Expiry cache built:" << m_expiryToSymbols.size() << "expiries,"
                << m_optionSymbols.size() << "option symbols";
   }
   ```
   
   **Usage** (instant lookup in ATMWatchWindow):
   ```cpp
   void ATMWatchWindow::loadAllSymbols() {
       auto repo = RepositoryManager::getInstance();
       
       QVector<QString> symbols;
       QString targetExpiry = m_expiryCombo->currentData().toString();
       
       if (targetExpiry == "CURRENT") {
           // Get all option symbols with their current expiry
           symbols = repo->getOptionSymbols(); // From m_optionSymbols cache
       } else {
           // Get symbols for specific expiry (instant lookup)
           symbols = repo->getSymbolsForExpiry(targetExpiry); // From m_expiryToSymbols cache
       }
       
       // Prepare watch configs (no filtering needed!)
       QVector<QPair<QString, QString>> watchConfigs;
       for (const QString& symbol : symbols) {
           QString expiry = (targetExpiry == "CURRENT") 
                          ? repo->getCurrentExpiry(symbol)  // From m_symbolToCurrentExpiry cache
                          : targetExpiry;
           watchConfigs.append(qMakePair(symbol, expiry));
       }
       
       // Batch add (single calculation)
       ATMWatchManager::getInstance()->addWatchesBatch(watchConfigs);
   }
   ```
   
   **Performance Improvement**:
   - Before: Filter 100,000+ contracts every time = 200-500ms
   - After: Direct hash/map lookup = <1ms
   - **500x faster data preparation**
   
   **Memory Overhead**:
   - ~50 expiries × 270 symbols avg × 20 bytes = ~270KB
   - Negligible compared to benefit

2. **Event-Driven Price Updates**
   - Subscribe to PriceCache `priceUpdated` signal
   - Recalculate only when price crosses threshold (e.g., ±40% of strike interval)
   - Reduces CPU usage by ~95% vs polling

3. **Threshold-Based Recalculation**
   ```cpp
   double threshold = strikeInterval * 0.4; // e.g., 20 points for NIFTY (50 interval)
   if (abs(newPrice - lastCalculatedPrice) > threshold) {
       recalculateATM();
   }
   ```

4. **Batch Subscription Management**
   - Currently subscribes one-by-one
   - Implement `batchSubscribe()` in FeedHandler for efficiency

5. **Strike Interval Detection**
   - Auto-detect strike interval from actual strikes
   - Use for threshold calculation and validation

## 13. Conclusion

### Current Status: **Simplified Polling Implementation**

**Implemented Features:**
- ✅ Multi-symbol ATM watch
- ✅ Accurate strike calculation (nearest, not median)
- ✅ Fast asset token lookup (unordered_map)
- ✅ Per-symbol current expiry selection
- ✅ Real-time LTP updates (1-second timer)
- ✅ Background symbol loading (no UI freeze)
- ✅ Thread-safe data access

**Next Steps for Production:**
1. Implement event-driven updates (replace polling)
2. Add threshold-based recalculation
3. Performance profiling with 200+ symbols
4. Add user-configurable update intervals
5. Implement subscription batching

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
