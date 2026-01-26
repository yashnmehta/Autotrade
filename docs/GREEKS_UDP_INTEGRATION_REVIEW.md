# Greeks Calculation in UDP Reader - Comprehensive Review

**Date:** January 19, 2026  
**Purpose:** Comprehensive audit of Greeks integration with UDP broadcast pipeline  
**Status:** Phase 1 complete, validation implementation pending

---

## Executive Summary

The Greeks calculation system is **fully integrated** with the UDP broadcast service and implements:
- ✅ Real-time IV and Greeks calculation using Black-Scholes
- ✅ 4 distinct trigger mechanisms (option trades, spot moves, time decay, illiquid updates)
- ✅ Smart caching and throttling to optimize performance
- ✅ Support for NSE and BSE (FO and CM segments)
- ✅ Thread safety mechanisms (Phase 1)

**Key Findings:**
- **Strengths:** Robust architecture, proper trigger points, efficient caching
- **Issues:** Missing input validation, unclear cache invalidation, incomplete error handling
- **Priority Fix:** Implement `validateGreeksInputs()` (30 min)

---

## 1. What's Already Implemented ✅

### 1.1 Core Calculation Pipeline

**Location:** `src/services/GreeksCalculationService.cpp`

The service implements the complete Greeks calculation pipeline:

```cpp
calculateForToken(token, segment) {
    1. Fetch contract data (strike, expiry, option type)
    2. Get option price from price stores (LTP, Bid, Ask)
    3. Get underlying price (futures/cash/index)
    4. Calculate time to expiry (trading days + intraday)
    5. Solve Implied Volatility (Newton-Raphson)
       - LTP IV (market price)
       - Bid IV (bid price) 
       - Ask IV (ask price)
    6. Calculate Greeks (Black-Scholes)
       - Delta, Gamma, Vega, Theta, Rho
    7. Cache results with metadata
    8. Emit signal with GreeksResult
}
```

**Files Involved:**
- [GreeksCalculationService.cpp](src/services/GreeksCalculationService.cpp) (733 lines)
- [GreeksCalculationService.h](include/services/GreeksCalculationService.h) (332 lines)

### 1.2 UDP Integration Points

**Location:** `src/services/UdpBroadcastService.cpp`

Four integration call sites trigger Greeks calculation:

#### **Point 1: NSE FO Option Price Update** (Line 380)
```cpp
// Callback after NSE FO price packet processed
auto& greeksService = GreeksCalculationService::instance();
if (greeksService.isEnabled() && data->ltp > 0) {
    // Called when option trades
    greeksService.onPriceUpdate(token, data->ltp, 2 /*NSEFO*/);
    
    // If this token is a future, update all dependent options
    greeksService.onUnderlyingPriceUpdate(token, data->ltp, 2 /*NSEFO*/);
}
```

**Trigger:** Option LTP changes in NSE FO  
**Effect:** Recalculates IV and Greeks for that specific option

#### **Point 2: NSE CM Underlying Price Update** (Line 443)
```cpp
// Callback after NSE CM (cash market) price update
auto& greeksService = GreeksCalculationService::instance();
if (greeksService.isEnabled() && data->ltp > 0) {
    // Update all options with this underlying
    greeksService.onUnderlyingPriceUpdate(token, data->ltp, 1 /*NSECM*/);
}
```

**Trigger:** Spot price changes for indices/stocks  
**Effect:** Recalculates Greeks for ALL options on that underlying (using cached IV)

#### **Point 3: BSE FO Option Price Update** (Line 605)
```cpp
// Similar to NSE FO
auto& greeksService = GreeksCalculationService::instance();
if (greeksService.isEnabled() && data->ltp > 0) {
    greeksService.onPriceUpdate(token, data->ltp, 12 /*BSEFO*/);
    greeksService.onUnderlyingPriceUpdate(token, data->ltp, 12 /*BSEFO*/);
}
```

**Trigger:** Option trades in BSE FO  
**Effect:** Same as NSE FO, for BSE options

#### **Point 4: BSE CM Underlying Price Update** (Line 651)
```cpp
// BSE cash market underlying updates
auto& greeksService = GreeksCalculationService::instance();
if (greeksService.isEnabled() && data->ltp > 0) {
    greeksService.onUnderlyingPriceUpdate(token, data->ltp, 11 /*BSECM*/);
}
```

**Trigger:** BSE spot price changes  
**Effect:** Updates Greeks for BSE options

### 1.3 Trigger Mechanisms

The service has **4 distinct trigger paths**:

#### **A. Option Price Update (Direct Trade)**
```cpp
void onPriceUpdate(uint32_t token, double ltp, int segment) {
    // Check throttle (default 1000ms)
    // Check price change threshold (0.1%)
    // Calculate full IV + Greeks
    calculateForToken(token, segment);
}
```
**When:** Option trades, LTP changes  
**Action:** Full calculation with fresh IV

#### **B. Underlying Price Update (Spot Movement)**
```cpp
void onUnderlyingPriceUpdate(uint32_t underlyingToken, double ltp, int segment) {
    // Find all options linked to this underlying
    QList<uint32_t> options = m_underlyingToOptions.values(underlyingToken);
    
    for (uint32_t token : options) {
        // Only update LIQUID options (traded in last 30s)
        if (timeSinceLastTrade < 30s) {
            // Reuse cached IV, recalc Greeks with new spot
            calculateForToken(token, segment);
        }
        // Illiquid options handled by timer
    }
}
```
**When:** Spot/futures price changes  
**Action:** Selective update (liquid options only), reuses cached IV

#### **C. Time Tick (Theta Decay)**
```cpp
void onTimeTick() {
    // Timer fires every 60 seconds
    forceRecalculateAll(); // Recalc all cached tokens
}
```
**When:** Every 60 seconds  
**Action:** Updates all options to reflect passage of time (theta decay)

#### **D. Illiquid Options Background Processing**
```cpp
void processIlliquidUpdates() {
    // Timer fires every 30 seconds
    // Find options NOT traded in last 30s
    for (token : m_cache) {
        if (timeSinceLastTrade > 30s) {
            // Force update even if illiquid
            calculateForToken(token, segment);
        }
    }
}
```
**When:** Every 30 seconds  
**Action:** Ensures illiquid options stay updated with spot movements

### 1.4 Configuration System

**Location:** `configs/config.ini`

```ini
[Greeks]
enabled=true
auto_calculate=true
risk_free_rate=0.065        # RBI repo rate (6.5%)
dividend_yield=0.0          # 0 for indices
throttle_ms=1000            # Min time between recalcs per token
iv_initial_guess=0.20       # Starting IV for Newton-Raphson
iv_tolerance=1e-6           # Convergence criteria
iv_max_iterations=100       # Max iterations
time_tick_interval_sec=60   # Theta decay timer
illiquid_update_interval_sec=30  # Background timer
illiquid_threshold_sec=30   # Definition of "illiquid"
```

**Loaded in:** [GreeksCalculationService.cpp#L68](src/services/GreeksCalculationService.cpp#L68)

### 1.5 Data Structures

#### **GreeksResult** (Output)
```cpp
struct GreeksResult {
    uint32_t token;
    int exchangeSegment;
    
    // Calculated values
    double impliedVolatility;  // LTP IV
    double bidIV, askIV;       // IV skew
    double delta, gamma, vega, theta, rho;
    double theoreticalPrice;
    
    // Metadata
    bool ivConverged;
    int ivIterations;
    int64_t calculationTimestamp;
    
    // Input snapshot
    double spotPrice, strikePrice, timeToExpiry, optionPrice;
};
```

#### **GreeksValidationResult** (Phase 1 Addition)
```cpp
struct GreeksValidationResult {
    bool valid;
    QString errorMessage;
    GreeksResult result;  // Only if valid
    
    // Detailed checks
    bool contractFound;
    bool isOption;
    bool hasValidAssetToken;
    bool hasUnderlyingPrice;
    bool notExpired;
    bool marketPriceValid;
};
```

**Status:** Structure defined, implementation pending

### 1.6 Caching Strategy

**Cache Structure:**
```cpp
struct CacheEntry {
    GreeksResult result;
    int64_t lastCalculationTime;    // Throttle check
    double lastPrice;                // Option LTP
    double lastUnderlyingPrice;      // Spot price
    int64_t lastTradeTimestamp;      // For illiquid detection
};

QHash<uint32_t, CacheEntry> m_cache;  // token -> entry
```

**Cache Behavior:**
1. **On Option Trade:** Calculate fresh IV, cache result
2. **On Spot Move:** Reuse cached IV if option price unchanged
3. **Cache Lookup:** O(1) hash lookup by token
4. **No TTL:** Cache never expires (time handled by theta timer)

**Smart IV Reuse (Lines 256-272):**
```cpp
if (m_cache.contains(token)) {
    const auto& entry = m_cache[token];
    // If option price UNCHANGED, reuse cached IV
    if (abs(entry.lastPrice - currentPrice) < 0.0001) {
        usedIV = entry.result.impliedVolatility;
        usingCachedIV = true;
        // Recalc Greeks with new spot + cached IV
    }
}
```

### 1.7 Underlying Price Resolution

**Logic:** [GreeksCalculationService.cpp#L500-L600](src/services/GreeksCalculationService.cpp#L500)

```cpp
double getUnderlyingPrice(uint32_t optionToken, int segment) {
    1. Get contract data → assetToken (underlying)
    2. If assetToken <= 0, lookup by symbol name (for indices)
    
    // For NSE FO options:
    3a. Try futures price in NSEFO (if underlying is a future)
    3b. Use nsecm::getGenericLtp(assetToken)
        - Handles equities (NSECM PriceStore)
        - Handles indices (IndexStore with NIFTY/BANKNIFTY mappings)
    
    // For BSE FO options:
    4a. Try futures price in BSEFO
    4b. Fall back to BSECM cash market
    
    return price or 0.0
}
```

**Key Feature:** Unified function handles stocks, futures, AND indices

### 1.8 Time to Expiry Calculation

**Trading Days Logic:**
```cpp
double calculateTimeToExpiry(QDate expiry) {
    1. Calculate trading days from today to expiry
       - Skip weekends
       - Skip NSE holidays (hardcoded 2026 list)
    
    2. Add intraday component
       - If before 3:30 PM: add fraction of today
       - secondsRemaining / (24 * 60 * 60)
    
    3. Convert to years: (days + intraday) / 252
    
    4. Return max(T, 0.0001)  // Avoid division by zero
}
```

**Holidays Loaded:** [GreeksCalculationService.cpp#L695](src/services/GreeksCalculationService.cpp#L695)

### 1.9 Thread Safety (Phase 1)

**Addition:** [RepositoryManager.h#L393](include/repository/RepositoryManager.h#L393)

```cpp
class RepositoryManager {
    mutable QReadWriteLock m_repositoryLock;  // NEW
    
    const ContractData* getContractByToken(int segment, uint32_t token) const {
        QReadLocker locker(&m_repositoryLock);  // Thread-safe read
        // ... lookup logic
    }
};
```

**Purpose:** Protects concurrent access from:
- Main thread (UI updates)
- UDP threads (price updates)
- Greeks timers (background processing)

### 1.10 Initialization Safety (Phase 1)

**Addition:** [RepositoryManager.h#L392](include/repository/RepositoryManager.h#L392)

```cpp
signals:
    void repositoryLoaded();  // NEW signal
```

**Usage:** Greeks service can wait for this signal before starting calculations

**Problem Solved:** Previously, UDP packets could arrive before master files were parsed, causing crashes

---

## 2. What's Proper and Correct ✅

### 2.1 Trigger Timing

✅ **Option Trades → Immediate Update**  
Correct. When an option trades, we need fresh IV immediately.

✅ **Spot Moves → Selective Update**  
Correct. Only updates liquid options immediately. Illiquid options handled by background timer to avoid wasted CPU.

✅ **Time Tick → All Options**  
Correct. Theta decay affects all options equally, so periodic full recalc is appropriate.

✅ **Illiquid Processing → Background**  
Correct. Ensures stale options (not traded recently) stay updated when spot moves.

### 2.2 IV Solver

✅ **Newton-Raphson Method**  
Industry standard for IV calculation. Fast convergence (typically 3-5 iterations).

✅ **Convergence Criteria**  
Tolerance of 1e-6 (0.0001%) is appropriate for financial calculations.

✅ **Max Iterations: 100**  
Reasonable safety limit. Most convergence happens in <10 iterations.

### 2.3 Black-Scholes Implementation

✅ **Greeks Calculation**  
Using standard Black-Scholes formulas via `GreeksCalculator::calculate()`.

✅ **Risk-Free Rate: 6.5%**  
Matches current RBI repo rate (Jan 2026).

✅ **Dividend Yield: 0%**  
Correct for index options (NIFTY/BANKNIFTY). Should be configurable per underlying for stock options.

### 2.4 Data Flow

✅ **UDP Packet → Price Store → Greeks Trigger**  
Correct architecture. Greeks service doesn't parse packets directly, uses price stores.

✅ **Direct Price Store Access**  
Efficient. No redundant API calls.

```cpp
const auto* state = nsefo::g_nseFoPriceStore.getUnifiedState(token);
double ltp = state->ltp;
```

### 2.5 Cache Design

✅ **Hash Map Lookup**  
O(1) access time for cached results.

✅ **Metadata Storage**  
Tracks last calc time, last prices for throttling logic.

✅ **IV Reuse Strategy**  
Correct. If option price unchanged, IV hasn't changed (by definition of IV).

### 2.6 Integration Architecture

✅ **Singleton Pattern**  
Appropriate for service. Single instance shared across UDP threads.

✅ **Qt Signals for Distribution**  
Correct. Emits `greeksCalculated()` signal for UI/logging to consume.

✅ **Non-Blocking**  
Greeks calculation doesn't block UDP packet processing.

---

## 3. What Can Be Improved ⚠️

### 3.1 **CRITICAL: Missing Input Validation**

**Problem:**  
`calculateForToken()` proceeds without validating inputs first.

**Current Code (Line 110):**
```cpp
GreeksResult GreeksCalculationService::calculateForToken(uint32_t token, int segment) {
    // NO validation here!
    // Directly fetches contract, assumes it exists
    const ContractData* contract = m_repoManager->getContractByToken(segment, token);
    if (!contract) {
        return result;  // Silent failure
    }
    // ... continues
}
```

**Issues:**
- No check if RepositoryManager is initialized
- No check if contract is an option
- No check if underlying price is available
- No check if option has expired
- No check if market price is valid
- Many silent failures with empty `GreeksResult`

**Solution (Phase 1 - Pending):**
```cpp
GreeksResult calculateForToken(uint32_t token, int segment) {
    // VALIDATE FIRST
    double optionPrice = getOptionPrice(token, segment);
    GreeksValidationResult validation = validateGreeksInputs(token, segment, optionPrice);
    
    if (!validation.valid) {
        emit calculationFailed(token, segment, validation.errorMessage);
        return GreeksResult{};  // Empty result with error logged
    }
    
    // Proceed with calculation
    // ... existing logic
}
```

**Implementation Time:** ~30 minutes  
**Priority:** P0 (Critical)

**Structure Already Added (Phase 1):**
- [GreeksCalculationService.h#L65-L82](include/services/GreeksCalculationService.h#L65) - `GreeksValidationResult` struct
- [GreeksCalculationService.h#L172](include/services/GreeksCalculationService.h#L172) - `validateGreeksInputs()` declaration

**Implementation Guide:**
See [PHASE1_IMPLEMENTATION_SUMMARY.md](PHASE1_IMPLEMENTATION_SUMMARY.md) Section 5.2

### 3.2 Cache Invalidation Strategy Unclear

**Problem:**  
Cache has no time-based expiry. Relies on triggers to update.

**Scenario:**
1. Option trades at 10:00 AM → IV = 18%, cached
2. Market closes, no more option trades
3. Next day 9:15 AM → Spot moves, but option hasn't traded yet
4. Greeks recalculated with **yesterday's IV** (cached)

**Current Workaround:**
- Time tick timer (60s) forces recalculation
- Illiquid timer (30s) catches stale options

**Issue:**
These timers may not fire if application just started. Old IV could persist for 30-60 seconds.

**Proposed Solution:**
```cpp
// Add cache expiry
struct CacheEntry {
    GreeksResult result;
    int64_t cacheTimestamp;  // NEW
    // ... other fields
};

// In calculateForToken():
if (m_cache.contains(token)) {
    int64_t cacheAge = now - entry.cacheTimestamp;
    if (cacheAge > 3600000) {  // 1 hour
        // Force fresh IV calculation
        usingCachedIV = false;
    }
}
```

**Implementation Time:** 15 minutes  
**Priority:** P2 (Medium)

### 3.3 Throttling May Miss Rapid Price Changes

**Problem:**  
Default throttle is 1000ms per token. If option trades 3 times in 1 second, only last price used.

**Current Code (Line 398):**
```cpp
void onPriceUpdate(uint32_t token, double ltp, int segment) {
    // Check throttle
    if (elapsed < m_config.throttleMs) {
        return;  // SKIPPED
    }
    // ... calculate
}
```

**Scenario:**
```
10:30:00.100 - Option LTP: 100 → Greeks calculated (IV = 18%)
10:30:00.200 - Option LTP: 102 → SKIPPED (throttled)
10:30:00.500 - Option LTP: 105 → SKIPPED (throttled)
10:30:01.150 - Option LTP: 108 → Greeks calculated (IV = 20%)
```

Intermediate prices (102, 105) never used for IV calculation.

**Impact:**
- May miss transient IV spikes
- Bid/Ask IV skew may be stale

**Proposed Solution:**
```cpp
// Always calculate Bid/Ask IV (not throttled)
// Throttle only LTP IV

void onPriceUpdate(uint32_t token, double ltp, int segment) {
    // Always update Bid/Ask IV (fast, no throttle)
    updateBidAskIV(token, segment);
    
    // Throttle LTP IV
    if (elapsed >= m_config.throttleMs) {
        calculateForToken(token, segment);
    }
}
```

**Alternative:**
Reduce throttle to 250ms (4 updates/sec) instead of 1000ms.

**Implementation Time:** 30 minutes  
**Priority:** P2 (Medium)

### 3.4 Error Reporting Incomplete

**Problem:**  
Many error conditions log to console but don't report structured errors.

**Examples:**
```cpp
// Line 185
if (optionPrice <= 0 && bidPrice <= 0 && askPrice <= 0) {
    // qWarning() << "No market data";  // COMMENTED OUT
    emit calculationFailed(token, segment, "No market data available");
    return result;
}

// Line 203
if (underlyingPrice <= 0) {
    // qWarning() << "Underlying price not available";  // COMMENTED OUT
    emit calculationFailed(token, segment, "Underlying price not available");
    return result;
}
```

**Issues:**
- Many qWarning() calls commented out (reduce log spam)
- No way to query "Why didn't token X get Greeks?"
- UI has no feedback on calculation failures

**Proposed Solution:**
```cpp
// Add failure tracking
struct FailureStats {
    int noContract = 0;
    int notOption = 0;
    int noUnderlyingPrice = 0;
    int expired = 0;
    int ivNoConverge = 0;
};

QHash<uint32_t, FailureStats> m_failureStats;

// API to query
QString getLastFailureReason(uint32_t token) const;
FailureStats getFailureStats() const;  // Aggregated stats
```

**Implementation Time:** 45 minutes  
**Priority:** P2 (Medium)

### 3.5 No Packet Loss Detection for Underlying

**Problem:**  
If underlying price UDP packets are lost, Greeks calculation uses stale spot price.

**Current:**
```cpp
double underlyingPrice = getUnderlyingPrice(token, segment);
// NO check: "Is this price fresh?"
// Just uses whatever is in price store
```

**Scenario:**
1. NIFTY spot at 10:00 AM: 22,500 (Greeks calculated)
2. UDP packets for NIFTY lost from 10:00 to 10:05
3. NIFTY options trade at 10:03 → Greeks calculated with stale spot (22,500)
4. Actual NIFTY may be 22,550 by now

**Proposed Solution:**
```cpp
// Add timestamp check
double getUnderlyingPrice(uint32_t optionToken, int segment) {
    // ... get price
    
    // Check if price is fresh (< 5 seconds old)
    const auto* state = nsecm::g_nseCmPriceStore.getUnifiedState(underlyingToken);
    if (state) {
        int64_t priceAge = now - state->lastUpdateTime;
        if (priceAge > 5000) {  // 5 seconds
            qWarning() << "Stale underlying price for" << underlyingToken 
                       << "Age:" << priceAge << "ms";
            // Option 1: Return 0 (fail calculation)
            // Option 2: Proceed with warning
        }
        return state->ltp;
    }
}
```

**Implementation Time:** 20 minutes  
**Priority:** P2 (Medium)

### 3.6 Underlying-to-Options Mapping May Be Incomplete

**Problem:**  
Mapping built lazily (only when option is calculated first time).

**Current Code (Line 214):**
```cpp
// Register mapping if new
if (!m_cache.contains(token) && contract->assetToken > 0) {
    m_underlyingToOptions.insert(contract->assetToken, token);
}
```

**Issue:**
If an option never trades, it's never added to mapping. When spot moves, that option won't be updated.

**Scenario:**
1. 100 NIFTY call options exist
2. Only 50 have traded (in cache, mapped)
3. NIFTY spot moves → Only those 50 get Greeks updated
4. Other 50 options: Greeks not calculated until they trade

**Proposed Solution:**
```cpp
// Build mapping eagerly on startup
void buildUnderlyingMappings() {
    // Iterate all contracts in repository
    for (contract : allContracts) {
        if (contract->instrumentType == 2 && contract->assetToken > 0) {
            m_underlyingToOptions.insert(contract->assetToken, contract->token);
        }
    }
}

// Call in initialize()
void initialize(const GreeksConfig& config) {
    // ... existing init
    buildUnderlyingMappings();  // NEW
}
```

**Implementation Time:** 30 minutes  
**Priority:** P2 (Medium)

### 3.7 No Monitoring/Statistics

**Problem:**  
No visibility into Greeks service performance.

**Missing Metrics:**
- Calculation success rate
- Average IV iterations
- Cache hit rate
- Throttled updates count
- Failed calculations by reason
- Average calculation time

**Proposed Solution:**
```cpp
struct GreeksStats {
    int totalCalculations = 0;
    int successfulCalculations = 0;
    int failedCalculations = 0;
    int cacheHits = 0;
    int cacheMisses = 0;
    int throttledUpdates = 0;
    double avgIvIterations = 0.0;
    double avgCalcTimeMs = 0.0;
    QHash<QString, int> failureReasons;  // reason -> count
};

// API
GreeksStats getStats() const;
void resetStats();
```

**Implementation Time:** 1 hour  
**Priority:** P3 (Low)

### 3.8 Dividend Yield Hardcoded to 0

**Problem:**  
Dividend yield is 0 for all underlyings.

**Current:**
```cpp
// config.ini
dividend_yield=0.0  // Applied to ALL underlyings
```

**Impact:**
- Correct for index options (NIFTY, BANKNIFTY)
- Incorrect for stock options (stocks pay dividends)
- Greeks will be slightly inaccurate for high-dividend stocks

**Proposed Solution:**
```cpp
// Per-underlying dividend yield
QHash<uint32_t, double> m_dividendYields;  // underlyingToken -> yield

// Load from config or database
void loadDividendYields() {
    // Example:
    // RELIANCE (token 2885): 0.25% annual
    // TCS (token 11536): 2.5% annual
    m_dividendYields[2885] = 0.0025;
    m_dividendYields[11536] = 0.025;
}

// Use in calculation
double divYield = m_dividendYields.value(underlyingToken, 0.0);
OptionGreeks greeks = GreeksCalculator::calculate(
    spotPrice, strikePrice, T, m_config.riskFreeRate, 
    iv, isCall, divYield  // Pass per-underlying yield
);
```

**Implementation Time:** 45 minutes  
**Priority:** P3 (Low)

### 3.9 Holiday List Hardcoded

**Problem:**  
NSE holidays for 2026 are hardcoded in source code.

**Current:** [GreeksCalculationService.cpp#L695](src/services/GreeksCalculationService.cpp#L695)
```cpp
void loadNSEHolidays() {
    m_nseHolidays = {
        QDate(2026, 1, 26),  // Republic Day
        // ... hardcoded dates
    };
}
```

**Issues:**
- Needs code change every year
- No support for 2027, 2028, etc.
- Can't handle last-minute holiday changes

**Proposed Solution:**
```cpp
// Load from external file
// configs/nse_holidays.json
{
    "2026": ["2026-01-26", "2026-03-14", ...],
    "2027": ["2027-01-26", "2027-03-02", ...]
}

void loadNSEHolidays(const QString& filePath) {
    QFile file(filePath);
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    // Parse and populate m_nseHolidays
}
```

**Implementation Time:** 30 minutes  
**Priority:** P3 (Low)

---

## 4. Pending Features 🚧

### 4.1 UI Integration

**Status:** Greeks calculated, but UI integration unknown

**What's Missing:**
- Display Greeks in option chain (Delta, IV, Theta columns)
- Greeks watchlist (track specific options)
- IV surface/smile visualization
- Historical IV chart
- Greeks heatmap (Delta across strikes/expiries)

**Estimated Time:** 8-12 hours (UI work)

### 4.2 Greeks-Based Alerts

**What's Missing:**
- Alert when IV crosses threshold (e.g., IV > 30%)
- Alert on unusual IV changes (e.g., +5% in 1 minute)
- Alert on Greeks levels (e.g., Delta > 0.9)
- Delta-neutral portfolio alerts

**Estimated Time:** 4-6 hours

### 4.3 Historical IV Tracking

**What's Missing:**
- Database to store calculated IVs
- Historical IV retrieval API
- IV percentile calculation (e.g., "Current IV is at 80th percentile")
- HV vs IV comparison (realized vs implied volatility)

**Estimated Time:** 6-8 hours

### 4.4 IV Surface Calculation

**What's Missing:**
- Calculate IV for entire option chain
- Interpolate IV across strikes
- Visualize volatility smile/smirk
- Term structure (IV across expiries)

**Estimated Time:** 4-6 hours

### 4.5 Portfolio Greeks

**What's Missing:**
- Aggregate Greeks across open positions
- Portfolio delta hedging suggestions
- Portfolio vega (volatility exposure)
- Greeks P&L attribution

**Estimated Time:** 8-10 hours

### 4.6 Greeks Stress Testing

**What's Missing:**
- "What if spot moves ±5%?" scenario analysis
- "What if IV changes ±10%?" impact
- Time decay projection (Greeks in 1 day, 7 days, etc.)

**Estimated Time:** 4-6 hours

### 4.7 Custom Risk-Free Rate

**Current:** Single rate (6.5%) for all calculations

**What's Missing:**
- Term structure of interest rates
- Use different rates for different expiries
- Match RBI yield curve

**Estimated Time:** 2-3 hours

### 4.8 American Options Support

**Current:** Black-Scholes (European options)

**What's Missing:**
- Binomial tree for American options
- Early exercise detection
- American option Greeks

**Estimated Time:** 12-16 hours (significant work)

---

## 5. Missing Points / Gaps 🔴

### 5.1 No Repository Initialization Check

**Problem:**  
Greeks calculation starts immediately, but repository may not be loaded yet.

**Current:**
```cpp
// No wait for repositoryLoaded() signal
GreeksCalculationService::instance().initialize(config);
// Calculation can start immediately
```

**Gap:**
If UDP packets arrive before master files are parsed:
- `getContractByToken()` returns nullptr
- Silent failure, empty Greeks
- No way to know "repository not ready"

**Solution (Phase 1 - Implemented):**
```cpp
// In initialization code:
connect(RepositoryManager::instance(), &RepositoryManager::repositoryLoaded,
        []() {
            qInfo() << "Repository loaded, enabling Greeks calculation";
            GreeksCalculationService::instance().setEnabled(true);
        });

// Start Greeks disabled
auto& greeks = GreeksCalculationService::instance();
greeks.setEnabled(false);  // Wait for signal
greeks.initialize(config);
```

**Status:** Signal added in Phase 1, connection code not yet written  
**Implementation Time:** 10 minutes

### 5.2 No Fallback for Missing Underlying Price

**Problem:**  
If underlying price is unavailable, calculation fails silently.

**Current:**
```cpp
double underlyingPrice = getUnderlyingPrice(token, segment);
if (underlyingPrice <= 0) {
    emit calculationFailed(token, segment, "Underlying price not available");
    return GreeksResult{};  // Empty result
}
```

**Gap:**
No fallback mechanism. Options without underlying price get no Greeks.

**Proposed Fallback:**
```cpp
// 1. Try futures price
// 2. Try cash market price
// 3. Try last known price (from yesterday's close)
// 4. Try theoretical price (put-call parity)
// 5. Fail with detailed error
```

**Implementation Time:** 1 hour

### 5.3 No Corporate Action Handling

**Problem:**  
Strike adjustments after splits/bonuses not handled.

**Scenario:**
1. Stock at ₹1000, option strike ₹1000
2. Stock splits 2:1 → ₹500
3. Option strike should adjust to ₹500
4. Greeks calculation fails (strike > spot by 2x)

**Gap:**
No detection of corporate actions, no strike adjustment logic.

**Solution:**
- Maintain corporate action log (splits, bonuses, dividends)
- Adjust strikes before Greeks calculation
- Mark adjusted contracts in database

**Implementation Time:** 4-6 hours

### 5.4 No Bid/Ask Spread Validation

**Problem:**  
Wide bid/ask spreads indicate illiquidity, but not detected.

**Current:**
```cpp
// Calculates Bid IV and Ask IV, but doesn't check spread
result.bidIV = 15%;
result.askIV = 25%;  // 10% spread! (Huge)
// No warning, no flag
```

**Gap:**
UI shows Greeks without indicating reliability (wide spread = unreliable Greeks).

**Proposed Addition:**
```cpp
struct GreeksResult {
    // ... existing fields
    
    // NEW
    double bidAskSpreadPct;     // (ask - bid) / mid
    bool isLiquid;              // Spread < 5% threshold
    QString liquidityWarning;   // "Wide spread, Greeks unreliable"
};
```

**Implementation Time:** 20 minutes

### 5.5 No Pin Risk Detection

**Problem:**  
Options near strike at expiry have high pin risk, not detected.

**Scenario:**
- Expiry day, 3:00 PM
- NIFTY spot: 22,500
- 22,500 strike options → Delta ~0.5 (high risk)
- No warning to user

**Gap:**
Greeks service calculates accurate Greeks, but doesn't flag risk scenarios.

**Proposed Addition:**
```cpp
struct GreeksResult {
    // ... existing fields
    
    // NEW
    bool isPinRisk;          // Near strike on expiry day
    QString riskWarning;     // "Pin risk: Spot near strike, expiry today"
};
```

**Implementation Time:** 30 minutes

### 5.6 No Vega Normalization

**Problem:**  
Vega reported as "per 1% IV change" but not always clear.

**Current:**
```cpp
result.vega = 12.5;  // Means: ₹12.5 change per 1% IV change
// If IV goes from 18% to 19%, option price changes by ₹12.5
```

**Gap:**
Some platforms report vega per 0.01 (1 vol point), some per 1% (100 vol points).

**Proposed:**
```cpp
struct GreeksResult {
    double vega;          // Per 1% (our standard)
    double vegaPerVol;    // Per 0.01 (alternate convention)
};
```

**Implementation Time:** 10 minutes

### 5.7 No Greeks for Futures

**Problem:**  
Only options get Greeks, but futures also have Greeks (Delta = 1, others = 0).

**Current:**
```cpp
// Instrument type check
if (!isOption(contract->instrumentType)) {
    return GreeksResult{};  // Futures rejected
}
```

**Gap:**
Portfolio Greeks aggregation can't include futures positions.

**Solution:**
```cpp
if (isFuture(contract->instrumentType)) {
    // Futures Greeks (trivial case)
    result.delta = (isLong ? 1.0 : -1.0);
    result.gamma = 0.0;
    result.vega = 0.0;
    result.theta = 0.0;
    result.rho = 0.0;
    return result;
}
```

**Implementation Time:** 15 minutes

---

## 6. Code Quality Assessment

### 6.1 Strengths ✅

1. **Clean Separation of Concerns**
   - Greeks service doesn't parse UDP packets
   - Uses price stores as data source
   - Emits signals for distribution

2. **Efficient Caching**
   - O(1) hash lookup
   - Smart IV reuse
   - Metadata for throttling

3. **Configurable**
   - All parameters in config.ini
   - No hardcoded constants (except holidays)

4. **Robust IV Solver**
   - Newton-Raphson with convergence tracking
   - Max iteration safety limit

5. **Comprehensive Greeks**
   - All 5 Greeks calculated
   - Bid/Ask IV for skew analysis
   - Theoretical price for comparison

6. **Smart Throttling**
   - Per-token throttle (not global)
   - Price change threshold (0.1%)
   - Liquid vs illiquid distinction

### 6.2 Code Smells ⚠️

1. **Silent Failures**
   - Many qWarning() commented out
   - Returns empty GreeksResult on error

2. **Magic Numbers**
   - 0.0001 (price comparison threshold)
   - 0.001 (price change threshold)
   - 30000 ms (illiquid threshold)
   - Should be named constants

3. **Large Function**
   - `calculateForToken()` is 200+ lines
   - Should be broken into smaller functions:
     - `validateInputs()`
     - `fetchPriceData()`
     - `calculateIV()`
     - `calculateGreeks()`
     - `updateCache()`

4. **Commented Debug Code**
   ```cpp
   // qInfo() << "IV:" << iv;  // Many instances
   ```
   Should use proper logging levels (qDebug vs qInfo vs qWarning).

5. **No Unit Tests**
   - Complex financial calculations
   - Should have unit tests for:
     - IV convergence
     - Greeks accuracy
     - Cache behavior
     - Throttling logic

### 6.3 Documentation Quality

**Header File:**  
✅ Excellent Doxygen comments  
✅ Usage examples provided  
✅ All public methods documented

**Implementation File:**  
⚠️ Minimal comments  
⚠️ Complex logic not explained  
⚠️ No ASCII art or diagrams

---

## 7. Performance Analysis

### 7.1 Calculation Cost

**Single Option Greeks:**
1. Repository lookup: ~0.01 ms (hash lookup)
2. Price store lookup: ~0.01 ms (hash lookup)
3. IV calculation: ~0.5-2 ms (Newton-Raphson, 3-10 iterations)
4. Greeks calculation: ~0.1 ms (Black-Scholes closed-form)

**Total:** ~1-3 ms per option (typical case)

### 7.2 Spot Movement Impact

**Scenario:** NIFTY moves, 1000 options subscribed

**Without Optimization:**
- Update all 1000 options: ~1000-3000 ms (1-3 seconds!)
- Would block UDP processing

**With Current Optimization:**
- Only updates LIQUID options (traded in last 30s)
- Typical: 100-200 liquid options
- Time: ~100-600 ms (acceptable)
- Illiquid options updated by background timer

### 7.3 Throttling Effectiveness

**Assumption:** Option trades every 100ms (10 trades/sec)

**Without Throttle:**
- 10 calculations/sec per option
- 1000 options: 10,000 calculations/sec
- CPU overload

**With 1000ms Throttle:**
- 1 calculation/sec per option
- 1000 options: 1,000 calculations/sec
- Manageable load

### 7.4 Cache Hit Rate

**Typical Scenario:**
- 100 option LTP updates/sec (fast market)
- 50 underlying price updates/sec
- Cache hit rate: ~80% (IV reused when option price unchanged)
- IV calculations: 20/sec instead of 100/sec (5x reduction)

### 7.5 Memory Usage

**Cache Size:**
```
Single CacheEntry: ~200 bytes (GreeksResult + metadata)
1000 options cached: ~200 KB
10,000 options: ~2 MB (negligible)
```

**Conclusion:** Memory is not a concern.

---

## 8. Security & Risk Considerations

### 8.1 Division by Zero

✅ **Protected:**
```cpp
// Line 635
return std::max(T, 0.0001);  // Minimum time to expiry
```

### 8.2 Expired Options

⚠️ **Partially Protected:**
```cpp
if (T <= 0) {
    emit calculationFailed(token, segment, "Option expired");
    return result;
}
```

**Issue:** Checked AFTER fetching prices. Should check earlier.

### 8.3 Invalid Prices

⚠️ **Minimal Validation:**
```cpp
if (optionPrice <= 0 && bidPrice <= 0 && askPrice <= 0) {
    // Reject
}
```

**Missing:**
- No check for unreasonably high prices (e.g., option > spot)
- No check for negative prices (shouldn't happen, but...)
- No check for NaN/Infinity

### 8.4 Concurrency

✅ **Protected (Phase 1):**
- RepositoryManager uses QReadWriteLock
- Price stores use atomic operations (LockFreeStore)

⚠️ **Potential Issue:**
- m_cache access not thread-safe
- If two threads call calculateForToken() simultaneously:
  - QHash concurrent read: safe
  - QHash concurrent write: **UNSAFE**

**Proposed Fix:**
```cpp
class GreeksCalculationService {
    mutable QReadWriteLock m_cacheLock;  // NEW
    
    GreeksResult calculateForToken(uint32_t token, int segment) {
        // ... calculation
        
        // Thread-safe cache update
        {
            QWriteLocker locker(&m_cacheLock);
            m_cache[token] = entry;
        }
    }
};
```

**Implementation Time:** 30 minutes  
**Priority:** P1 (High, if Greeks called from multiple threads)

---

## 9. Testing Recommendations

### 9.1 Unit Tests (Priority P1)

**Test Cases:**

1. **IV Convergence**
   ```cpp
   testIVConvergence() {
       // Given: ATM option, spot=100, strike=100, T=30d, ltp=5
       // When: Calculate IV
       // Then: IV should converge to ~18-20%
   }
   ```

2. **Greeks Accuracy**
   ```cpp
   testGreeksAccuracy() {
       // Given: Known inputs (spot, strike, T, IV, r)
       // When: Calculate Greeks
       // Then: Match expected values (from reference calculator)
   }
   ```

3. **Cache Behavior**
   ```cpp
   testCacheReuse() {
       // Given: Option calculated once
       // When: Spot moves, option price unchanged
       // Then: IV should be reused (not recalculated)
   }
   ```

4. **Throttling**
   ```cpp
   testThrottling() {
       // Given: Option updates 3 times in 500ms
       // When: Throttle = 1000ms
       // Then: Only first update should calculate, others skipped
   }
   ```

### 9.2 Integration Tests (Priority P2)

1. **UDP → Greeks Pipeline**
   ```cpp
   testUdpIntegration() {
       // Given: UDP packet with option price
       // When: Packet processed
       // Then: greeksCalculated() signal should emit within 5ms
   }
   ```

2. **Repository Integration**
   ```cpp
   testRepositoryLookup() {
       // Given: Master files loaded
       // When: Calculate Greeks for known token
       // Then: Should fetch correct strike, expiry, option type
   }
   ```

### 9.3 Stress Tests (Priority P2)

1. **High Frequency Updates**
   ```cpp
   testHighFrequency() {
       // Given: 1000 options
       // When: All update simultaneously (spot moves)
       // Then: Should complete in < 1 second
   }
   ```

2. **Memory Leak**
   ```cpp
   testMemoryLeak() {
       // Given: Application running
       // When: Calculate Greeks for 10,000 options over 1 hour
       // Then: Memory usage should stabilize (no continuous growth)
   }
   ```

### 9.4 Edge Case Tests (Priority P3)

1. **Expired Options**
2. **Missing Underlying**
3. **Wide Bid/Ask Spread**
4. **IV Non-Convergence**
5. **Extreme Greeks (Deep ITM/OTM)**

---

## 10. Action Plan & Priorities

### Phase 1: Critical Fixes (Already in Progress) ⏳

✅ **Completed:**
- Thread safety (QReadWriteLock in RepositoryManager)
- Initialization signal (repositoryLoaded)
- Validation structure (GreeksValidationResult)

⏳ **Pending (30 minutes):**
- Implement `validateGreeksInputs()` function
- Update `calculateForToken()` to call validation first
- Test validation with missing contract, expired option, etc.

**Branch:** `feature/basic_plus` (already pushed)

### Phase 2: Input Validation & Error Handling (2-3 hours)

**Tasks:**
1. Complete `validateGreeksInputs()` implementation ✅ (from Phase 1)
2. Add failure statistics tracking
3. Improve error messages (detailed reasons)
4. Add `getLastFailureReason()` API for UI

**Testing:**
- Test with missing contracts
- Test with expired options
- Test with unavailable underlying prices

### Phase 3: Cache & Performance (1-2 hours)

**Tasks:**
1. Add cache expiry (1-hour TTL)
2. Reduce throttle to 250ms (test performance)
3. Always calculate Bid/Ask IV (no throttle)
4. Add packet loss detection for underlying prices

**Testing:**
- Benchmark calculation time (before/after)
- Monitor cache hit rate
- Test with rapid price updates

### Phase 4: Underlying Mapping & Reliability (2-3 hours)

**Tasks:**
1. Build underlying-to-options mapping eagerly on startup
2. Add stale price detection (5-second threshold)
3. Implement fallback logic for missing underlying prices
4. Add bid/ask spread validation

**Testing:**
- Test with 1000+ options
- Verify all options update on spot move
- Test with simulated packet loss

### Phase 5: Monitoring & Stats (1-2 hours)

**Tasks:**
1. Add performance statistics (success rate, avg time, etc.)
2. Add failure reason tracking
3. Expose stats via API for UI
4. Add logging levels (debug/info/warning)

**Testing:**
- Run for 1 hour, verify stats accuracy
- Test with intentional failures

### Phase 6: Unit Tests (4-6 hours)

**Tasks:**
1. Set up test framework (QtTest)
2. Write IV convergence tests
3. Write Greeks accuracy tests (vs reference)
4. Write cache behavior tests
5. Write throttling tests

**Testing:**
- Run all tests, target 95%+ pass rate

### Phase 7: Advanced Features (8-12 hours)

**Tasks:**
1. UI integration (option chain Greeks display)
2. Historical IV tracking
3. IV surface calculation
4. Greeks-based alerts
5. Portfolio Greeks aggregation

**Testing:**
- End-to-end testing with live market data

---

## 11. Summary

### What's Working Well ✅

1. **Complete Greeks Pipeline** - IV + all 5 Greeks calculated
2. **Smart Trigger System** - 4 distinct paths (trade, spot, time, illiquid)
3. **Efficient Caching** - IV reuse, O(1) lookup, metadata tracking
4. **Proper Architecture** - Singleton, signals, non-blocking
5. **Configuration** - All parameters externalized
6. **Thread Safety** - Phase 1 fixes implemented

### Critical Issues 🔴

1. **Missing Input Validation** - Calculation proceeds without checks (P0)
2. **Silent Failures** - Many errors unreported (P1)
3. **Cache Invalidation** - No time-based expiry (P2)

### Quick Wins (< 1 hour each) 🎯

1. Implement `validateGreeksInputs()` (~30 min)
2. Add cache expiry (~15 min)
3. Repository initialization check (~10 min)
4. Bid/ask spread validation (~20 min)
5. Stale price detection (~20 min)

### Long-Term Improvements 🚀

1. UI Integration (Greeks display in option chain)
2. Historical IV tracking & database
3. Unit test suite
4. Portfolio Greeks aggregation
5. Greeks-based alerting system

---

## 12. References

**Key Files:**
- [GreeksCalculationService.cpp](src/services/GreeksCalculationService.cpp) - Main implementation (733 lines)
- [GreeksCalculationService.h](include/services/GreeksCalculationService.h) - Interface (332 lines)
- [UdpBroadcastService.cpp](src/services/UdpBroadcastService.cpp) - Integration points (971 lines)
- [PHASE1_IMPLEMENTATION_SUMMARY.md](PHASE1_IMPLEMENTATION_SUMMARY.md) - Phase 1 fixes guide

**Integration Points:**
- Line 380: NSE FO option price update
- Line 443: NSE CM underlying price update
- Line 605: BSE FO option price update
- Line 651: BSE CM underlying price update

**Configuration:**
- `configs/config.ini` - [Greeks] section (11 parameters)

**Related Components:**
- IVCalculator (Newton-Raphson IV solver)
- GreeksCalculator (Black-Scholes Greeks)
- RepositoryManager (contract data)
- PriceStores (nsefo, nsecm, bse)

---

## Appendix A: Full Greeks Calculation Flow

```
┌─────────────────────────────────────────────────────────────────┐
│                     UDP Packet Arrives                          │
│              (NSE FO TouchLine / Market Depth)                  │
└────────────────────────┬────────────────────────────────────────┘
                         │
                         ▼
┌─────────────────────────────────────────────────────────────────┐
│  Broadcast Receiver (nsefo_receiver.cpp)                       │
│  1. Parse packet (7201 / 7202 / 7218)                         │
│  2. Validate checksum                                          │
│  3. Update g_nseFoPriceStore (UnifiedState)                   │
└────────────────────────┬────────────────────────────────────────┘
                         │
                         ▼
┌─────────────────────────────────────────────────────────────────┐
│  UdpBroadcastService::setupNseFoCallbacks() (Line 380)        │
│  1. Fetch state from price store                              │
│  2. Convert to UDP::MarketTick                                │
│  3. Notify FeedHandler                                        │
│  4. Call GreeksCalculationService::instance()                 │
│     greeksService.onPriceUpdate(token, ltp, 2)                │
└────────────────────────┬────────────────────────────────────────┘
                         │
                         ▼
┌─────────────────────────────────────────────────────────────────┐
│  GreeksCalculationService::onPriceUpdate()                     │
│  1. Check if enabled && autoCalculate                         │
│  2. Check throttle (last calc < 1000ms ago?)                  │
│  3. Check price change (> 0.1% change?)                       │
│  4. Call calculateForToken(token, segment)                    │
└────────────────────────┬────────────────────────────────────────┘
                         │
                         ▼
┌─────────────────────────────────────────────────────────────────┐
│  GreeksCalculationService::calculateForToken()                 │
│                                                                │
│  ┌──────────────────────────────────────────────────────────┐ │
│  │ Step 1: Fetch Contract Data                             │ │
│  │  - m_repoManager->getContractByToken(segment, token)    │ │
│  │  - Get: strike, expiry, optionType, assetToken          │ │
│  └──────────────────────────────────────────────────────────┘ │
│                         │                                      │
│  ┌──────────────────────▼──────────────────────────────────┐ │
│  │ Step 2: Fetch Option Price                             │ │
│  │  - nsefo::g_nseFoPriceStore.getUnifiedState(token)     │ │
│  │  - Get: LTP, Bid, Ask                                  │ │
│  └──────────────────────────────────────────────────────────┘ │
│                         │                                      │
│  ┌──────────────────────▼──────────────────────────────────┐ │
│  │ Step 3: Fetch Underlying Price                         │ │
│  │  - getUnderlyingPrice(token, segment)                  │ │
│  │    a. Try futures price (NSEFO store)                  │ │
│  │    b. Try cash price (NSECM store)                     │ │
│  │    c. Try index price (IndexStore via getGenericLtp)   │ │
│  └──────────────────────────────────────────────────────────┘ │
│                         │                                      │
│  ┌──────────────────────▼──────────────────────────────────┐ │
│  │ Step 4: Calculate Time to Expiry                       │ │
│  │  - calculateTimeToExpiry(expiryDate)                   │ │
│  │  - Count trading days (skip weekends + NSE holidays)   │ │
│  │  - Add intraday fraction (if before 3:30 PM)           │ │
│  │  - Convert to years: (days + intraday) / 252           │ │
│  └──────────────────────────────────────────────────────────┘ │
│                         │                                      │
│  ┌──────────────────────▼──────────────────────────────────┐ │
│  │ Step 5: Check Cache for IV Reuse                       │ │
│  │  - If option price unchanged: reuse cached IV          │ │
│  │  - Else: proceed to calculate fresh IV                 │ │
│  └──────────────────────────────────────────────────────────┘ │
│                         │                                      │
│  ┌──────────────────────▼──────────────────────────────────┐ │
│  │ Step 6: Calculate Implied Volatility                   │ │
│  │  - IVCalculator::calculate() (Newton-Raphson)          │ │
│  │  - LTP IV (market price)                               │ │
│  │  - Bid IV (bid price)                                  │ │
│  │  - Ask IV (ask price)                                  │ │
│  │  - Max 100 iterations, tolerance 1e-6                  │ │
│  └──────────────────────────────────────────────────────────┘ │
│                         │                                      │
│  ┌──────────────────────▼──────────────────────────────────┐ │
│  │ Step 7: Calculate Greeks (Black-Scholes)               │ │
│  │  - GreeksCalculator::calculate(spot, strike, T, r, iv) │ │
│  │  - Delta: ∂Price/∂Spot                                 │ │
│  │  - Gamma: ∂Delta/∂Spot                                 │ │
│  │  - Vega:  ∂Price/∂IV                                   │ │
│  │  - Theta: ∂Price/∂Time                                 │ │
│  │  - Rho:   ∂Price/∂Rate                                 │ │
│  └──────────────────────────────────────────────────────────┘ │
│                         │                                      │
│  ┌──────────────────────▼──────────────────────────────────┐ │
│  │ Step 8: Update Cache                                   │ │
│  │  - m_cache[token] = {result, timestamp, prices, ...}   │ │
│  │  - m_underlyingToOptions.insert(assetToken, token)     │ │
│  └──────────────────────────────────────────────────────────┘ │
│                         │                                      │
│  ┌──────────────────────▼──────────────────────────────────┐ │
│  │ Step 9: Emit Signal                                    │ │
│  │  - emit greeksCalculated(token, segment, result)       │ │
│  └──────────────────────────────────────────────────────────┘ │
└─────────────────────────┬────────────────────────────────────────┘
                         │
                         ▼
┌─────────────────────────────────────────────────────────────────┐
│  Signal Subscribers                                            │
│  - UI: Update Greeks columns in option chain                  │
│  - Logger: Log Greeks to file                                 │
│  - Alerting: Check Greeks thresholds                          │
└─────────────────────────────────────────────────────────────────┘
```

---

**End of Review**
