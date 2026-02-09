# ATM Watch - Advanced Implementation Plan

**Document Version:** 2.0  
**Date:** February 8, 2026  
**Status:** Strategic Roadmap for Advanced Features  
**Based On:** P1-P3 Complete (8.5/10), IV/Greeks Analysis

---

## Executive Summary

### Current State Assessment

ATM Watch has completed **Phase 1-3 implementation** achieving **8.5/10 production-ready** status:

✅ **Foundation (P1-P3 Complete)**:
- **1-Minute ATM Calculation**: Event-driven + 60-second timer backup
- **Live Price Updates**: Real-time spot/future price via UDP multicast
- **Strike Range Selection**: ATM+1 to ATM+5 configurable (±N strikes)
- Event-driven architecture with threshold-based recalculation
- Thread-safe Meyer's singleton pattern
- Comprehensive error handling (5 status states)
- Incremental UI updates (no flicker)
- ATM change notifications
- Performance: 400ms for 270 symbols (beats 500ms target)
- Cache system: O(1) lookups, 43ms build, 5MB memory

⚠️ **Critical Gaps for Advanced Trading**:
- **No Greeks calatm caculation** (IV, Delta, Gamma, Theta, Vega, Rho)
- **No implied volatility solver** (Newton-Raphson needed)
- **No risk analytics** (portfolio Greeks, position sizing)
- **No historical tracking** (ATM changes, IV evolution)
- **No volatility surface** (smile/skew visualization)
- **No advanced alerts** (Greeks threshold, IV percentile)
- **Limited analytics** (no P&L projection, risk metrics)

### Vision: Professional Options Trading Platform

Transform ATM Watch from **basic ATM tracker** → **Professional options analysis workstation**

**Target Users:**
1. **Retail Options Traders** - Need Greeks, IV, risk metrics
2. **Professional Traders** - Need volatility surface, historical analytics
3. **Market Makers** - Need real-time Greeks aggregation, portfolio risk
4. **Educators** - Need visualizations, what-if scenarios

### Strategic Approach

**Phase 4-7 Roadmap** (16 weeks):
- **Phase 4**: Greeks Integration (3 weeks) - IV solver + Greeks calculation
- **Phase 5**: Risk Analytics (3 weeks) - Portfolio Greeks, position metrics
- **Phase 6**: Historical Analytics (4 weeks) - ATM tracking, IV percentiles
- **Phase 7**: Advanced Visualization (3 weeks) - Volatility surface, heatmaps
- **Phase 8**: Trading Intelligence (3 weeks) - Alerts, scanning, recommendations

---

## Table of Contents

1. [Phase 4: Greeks Integration](#phase-4-greeks-integration)
2. [Phase 5: Risk Analytics](#phase-5-risk-analytics)
3. [Phase 6: Historical Analytics](#phase-6-historical-analytics)
4. [Phase 7: Advanced Visualization](#phase-7-advanced-visualization)
5. [Phase 8: Trading Intelligence](#phase-8-trading-intelligence)
6. [Architecture Evolution](#architecture-evolution)
7. [Performance Targets](#performance-targets)
8. [Testing Strategy](#testing-strategy)
9. [Deployment Roadmap](#deployment-roadmap)

---

## Phase 4: Greeks Integration

**Duration:** 3 weeks  
**Priority:** P0 (Critical) - Greeks are fundamental for options trading  
**Dependencies:** P1-P3 complete, Greeks calculator exists  
**Goal:** Real-time IV and Greeks calculation for all ATM strikes

### 4.1 Implied Volatility Solver

#### Implementation: IVCalculator Class

**Location:** `include/repository/IVCalculator.h`, `src/repository/IVCalculator.cpp`

```cpp
class IVCalculator {
public:
    struct IVResult {
        double impliedVolatility;
        int iterations;
        bool converged;
        double finalError;
        QString convergenceDetail;
    };
    
    /**
     * @brief Calculate IV using Newton-Raphson (primary method)
     * 
     * Fast convergence (3-5 iterations typical) using analytical Vega.
     * Industry standard for options pricing.
     * 
     * @param marketPrice Observed market LTP
     * @param S Underlying price (spot/future)
     * @param K Strike price
     * @param T Time to expiry (years)
     * @param r Risk-free rate (e.g., 0.065 for 6.5%)
     * @param isCall true=CE, false=PE
     * @param initialGuess Starting IV (default 0.20 = 20%)
     * @param tolerance Convergence threshold (1e-6 = 0.0001%)
     * @param maxIterations Safety limit (100)
     */
    static IVResult calculateNewtonRaphson(
        double marketPrice,
        double S, double K, double T, double r,
        bool isCall,
        double initialGuess = 0.20,
        double tolerance = 1e-6,
        int maxIterations = 100
    );
    
    /**
     * @brief Calculate IV using Brent's method (fallback)
     * 
     * More robust for edge cases (deep ITM/OTM, near expiry).
     * Slower than Newton-Raphson but guaranteed convergence.
     */
    static IVResult calculateBrent(
        double marketPrice,
        double S, double K, double T, double r,
        bool isCall,
        double lowerBound = 0.01,
        double upperBound = 5.0,
        double tolerance = 1e-6
    );
    
    /**
     * @brief Smart IV calculation (auto-select method)
     * 
     * Tries Newton-Raphson first, falls back to Brent if needed.
     * Recommended for production use.
     */
    static IVResult calculate(
        double marketPrice,
        double S, double K, double T, double r,
        bool isCall
    );
    
    /**
     * @brief Validate if IV calculation is feasible
     * 
     * Checks:
     * - T > 0 (not expired)
     * - marketPrice > intrinsic (has time value)
     * - marketPrice is reasonable (not 0.05 tick)
     */
    static bool isCalculable(
        double marketPrice,
        double S, double K, double T, double r,
        bool isCall,
        QString* reason = nullptr
    );
    
    /**
     * @brief Generate initial IV guess based on moneyness
     * 
     * ATM options: ~20% IV starting guess
     * Deep ITM/OTM: Adjust based on S/K ratio
     * Improves convergence speed
     */
    static double smartInitialGuess(double S, double K, double T);
    
private:
    static double intrinsicValue(double S, double K, bool isCall);
    static constexpr double MIN_TIME_VALUE = 0.10; // ₹0.10 minimum
    static constexpr double MIN_EXPIRY_DAYS = 1.0 / 365.0; // 1 day min
};
```

**Algorithm: Newton-Raphson**

```
Given: Market Price = P_market
Find: σ such that BS(S, K, T, r, σ) = P_market

Iteration:
1. Start with σ₀ = 0.20 (20%)
2. Calculate theoretical price: P_theo = BlackScholes(σᵢ)
3. Calculate vega: ν = ∂BS/∂σ
4. Update: σᵢ₊₁ = σᵢ - (P_theo - P_market) / ν
5. If |σᵢ₊₁ - σᵢ| < 1e-6, converged
6. Else, repeat (max 100 iterations)

Edge Cases:
- Vega ≈ 0: Switch to Brent
- σ < 0.01: Clamp to 1%
- σ > 5.0: Clamp to 500%
- No convergence: Return -1.0 (NaN)
```

**Performance Targets:**
- **Convergence**: 95%+ success rate
- **Speed**: <5 microseconds per calculation
- **Accuracy**: Within 0.01% of Bloomberg/NSE values
- **Iterations**: <10 average

#### Week 1 Tasks: IV Solver

| Day | Task | Deliverable |
|-----|------|-------------|
| Mon | Create IVCalculator.h/cpp skeleton | Header with all methods |
| Tue | Implement Newton-Raphson core | Working IV calculation |
| Wed | Implement Brent's method fallback | Robust edge case handling |
| Thu | Unit tests (50+ test cases) | >90% code coverage |
| Fri | Edge case testing (deep ITM/OTM, near expiry) | Production-ready IV solver |

### 4.2 GreeksCalculationService

**Purpose:** Orchestrate IV + Greeks calculations for ATM Watch

**Location:** `include/services/GreeksCalculationService.h`

```cpp
class GreeksCalculationService : public QObject {
    Q_OBJECT
    
public:
    struct GreeksConfig {
        double riskFreeRate = 0.065;     // 6.5% (RBI repo rate)
        double dividendYield = 0.0;      // 0% for indices
        bool autoCalculate = true;        // Calculate on price updates
        int throttleMs = 1000;            // Min 1s between calcs
        bool batchCalculation = true;     // Process chains together
        int cacheValidityMs = 5000;       // Cache for 5 seconds
        bool enableIVSolver = true;       // Calculate IV from market price
    };
    
    struct GreeksResult {
        uint32_t token;
        QString symbol;
        double strikePrice;
        bool isCall;
        
        // Market data
        double marketPrice;
        double underlyingPrice;
        double timeToExpiry;
        
        // Greeks
        double impliedVolatility;
        double delta;
        double gamma;
        double theta;  // Daily decay
        double vega;   // Per 1% IV change
        double rho;    // Per 1% rate change
        double theoreticalPrice;
        
        // Metadata
        bool ivConverged;
        int ivIterations;
        int64_t calculationTimestamp;
        QString errorMessage;
    };
    
    static GreeksCalculationService& getInstance();
    
    /**
     * @brief Initialize with configuration
     */
    void initialize(const GreeksConfig& config);
    
    /**
     * @brief Calculate Greeks for a single option token
     * 
     * Called by ATM Watch when option price updates.
     * Uses cached result if recent enough.
     */
    void calculateForToken(uint32_t token, int exchangeSegment);
    
    /**
     * @brief Calculate Greeks for ATM strike range
     * 
     * More efficient than individual calculations.
     * Optimized for ATM Watch use case (5-11 strikes).
     */
    void calculateForATMRange(
        const QString& symbol,
        const QString& expiry,
        double atmStrike,
        int rangeCount,  // ±N strikes
        int exchangeSegment
    );
    
    /**
     * @brief Get cached Greeks (if valid)
     */
    std::optional<GreeksResult> getCachedGreeks(uint32_t token) const;
    
    /**
     * @brief Force recalculation (time tick, manual refresh)
     */
    void forceRecalculate();
    
    /**
     * @brief Update risk-free rate (reload from config)
     */
    void updateRiskFreeRate(double rate);
    
signals:
    void greeksCalculated(const GreeksResult& result);
    void atmRangeCalculated(const QString& symbol, const QString& expiry);
    void calculationFailed(uint32_t token, const QString& reason);
    
public slots:
    void onOptionPriceUpdate(uint32_t token, double ltp, int exchangeSegment);
    void onUnderlyingPriceUpdate(const QString& symbol, double price);
    void onTimeTick();  // Called every minute for theta decay
    
private:
    GreeksCalculationService();
    ~GreeksCalculationService() = default;
    
    // Singleton pattern
    GreeksCalculationService(const GreeksCalculationService&) = delete;
    GreeksCalculationService& operator=(const GreeksCalculationService&) = delete;
    
    struct TokenCache {
        GreeksResult result;
        int64_t lastCalculationTime;
        double lastPrice;
        double lastUnderlyingPrice;
    };
    
    // Helper methods
    double getUnderlyingPrice(const QString& symbol, const QString& expiry);
    double calculateTimeToExpiry(const QString& expiryDate);
    bool shouldRecalculate(uint32_t token, double currentPrice, double currentUnderlying);
    void processBatch(const QVector<uint32_t>& tokens);
    
    GreeksConfig m_config;
    QHash<uint32_t, TokenCache> m_cache;
    QTimer* m_timeTickTimer;
    mutable std::shared_mutex m_mutex;
};
```

**Data Flow: Price Update → Greeks**

```
┌─────────────────────────────────────────────────────────────┐
│                  UDP BROADCAST SERVICE                       │
│  NSE FO Touchline (7200) → Option price update              │
└────────────────────┬────────────────────────────────────────┘
                     │ emit priceUpdate(token, ltp)
                     ↓
┌─────────────────────────────────────────────────────────────┐
│              ATM WATCH WINDOW (onTickUpdate)                 │
│  1. Update option LTP in table                              │
│  2. Trigger Greeks calculation (if enabled)                 │
└────────────────────┬────────────────────────────────────────┘
                     │ calculateForToken(token)
                     ↓
┌─────────────────────────────────────────────────────────────┐
│           GREEKS CALCULATION SERVICE                        │
│  ┌──────────────────────────────────────────────────────┐   │
│  │ 1. Check cache (valid for 5s?)                      │   │
│  │ 2. Get contract data (strike, expiry, type)         │   │
│  │ 3. Get underlying price (cash/future)               │   │
│  │ 4. Calculate time-to-expiry (trading days)          │   │
│  │ 5. Calculate IV (Newton-Raphson)                    │   │
│  │ 6. Calculate Greeks (Black-Scholes)                 │   │
│  │ 7. Cache result                                      │   │
│  │ 8. Emit greeksCalculated signal                     │   │
│  └──────────────────────────────────────────────────────┘   │
└────────────────────┬────────────────────────────────────────┘
                     │ emit greeksCalculated(result)
                     ↓
┌─────────────────────────────────────────────────────────────┐
│              ATM WATCH WINDOW (onGreeksUpdate)              │
│  Update table: IV, Delta, Gamma, Vega, Theta columns        │
└─────────────────────────────────────────────────────────────┘
```

#### Week 2 Tasks: Greeks Service

| Day | Task | Deliverable |
|-----|------|-------------|
| Mon | Create GreeksCalculationService skeleton | Header + singleton setup |
| Tue | Implement single-token calculation | Working calculateForToken() |
| Wed | Implement ATM range batch calculation | Optimized batch processing |
| Thu | Add caching + throttling mechanism | Production-grade performance |
| Fri | Integration tests + performance profiling | <10ms for 10 strikes |

### 4.3 ATM Watch UI Integration

**Enhancements to ATMWatchWindow.cpp**

```cpp
// Add Greeks display columns (already defined in P1-P3)
enum CallCols { 
    CALL_LTP = 0, CALL_CHG, CALL_BID, CALL_ASK, 
    CALL_VOL, CALL_OI, 
    CALL_IV,     // ← NEW: Implied Volatility
    CALL_DELTA,  // ← NEW: Delta
    CALL_GAMMA,  // ← NEW: Gamma
    CALL_VEGA,   // ← NEW: Vega
    CALL_THETA,  // ← NEW: Theta (daily decay)
    CALL_COUNT 
};

enum PutCols { 
    PUT_BID = 0, PUT_ASK, PUT_LTP, PUT_CHG, 
    PUT_VOL, PUT_OI, 
    PUT_IV,      // ← NEW: Implied Volatility
    PUT_DELTA,   // ← NEW: Delta
    PUT_GAMMA,   // ← NEW: Gamma
    PUT_VEGA,    // ← NEW: Vega
    PUT_THETA,   // ← NEW: Theta (daily decay)
    PUT_COUNT 
};

// Add symbol Greeks aggregation
enum SymbolCols { 
    SYM_NAME = 0, 
    SYM_PRICE, 
    SYM_ATM, 
    SYM_EXPIRY,
    SYM_NET_DELTA,   // ← NEW: CE Delta + PE Delta
    SYM_NET_GAMMA,   // ← NEW: CE Gamma + PE Gamma
    SYM_NET_VEGA,    // ← NEW: CE Vega + PE Vega
    SYM_COUNT 
};

// Greeks update handler
void ATMWatchWindow::onGreeksCalculated(const GreeksResult& result) {
    auto info = m_tokenToInfo.value(result.token);
    if (info.first.isEmpty()) return;
    
    int row = m_symbolToRow.value(info.first, -1);
    if (row < 0) return;
    
    bool isCall = info.second;
    
    if (isCall) {
        // Update Call table
        m_callModel->setData(m_callModel->index(row, CALL_IV), 
            QString::number(result.impliedVolatility * 100, 'f', 2));  // Show as %
        m_callModel->setData(m_callModel->index(row, CALL_DELTA), 
            QString::number(result.delta, 'f', 3));
        m_callModel->setData(m_callModel->index(row, CALL_GAMMA), 
            QString::number(result.gamma, 'f', 4));
        m_callModel->setData(m_callModel->index(row, CALL_VEGA), 
            QString::number(result.vega, 'f', 2));
        m_callModel->setData(m_callModel->index(row, CALL_THETA), 
            QString::number(result.theta, 'f', 2));
            
        // Color coding
        m_callModel->item(row, CALL_DELTA)->setForeground(
            result.delta > 0.5 ? Qt::darkGreen : Qt::darkRed
        );
    } else {
        // Update Put table (similar logic)
        // ...
    }
    
    // Update aggregated Greeks in symbol table
    updateSymbolGreeks(info.first);
}

// Calculate net Greeks for the symbol (CE + PE combined)
void ATMWatchWindow::updateSymbolGreeks(const QString& symbol) {
    int row = m_symbolToRow.value(symbol, -1);
    if (row < 0) return;
    
    // Get CE and PE tokens
    auto atmInfo = ATMWatchManager::getInstance().getATMInfo(symbol);
    auto ceGreeks = GreeksCalculationService::getInstance().getCachedGreeks(atmInfo.callToken);
    auto peGreeks = GreeksCalculationService::getInstance().getCachedGreeks(atmInfo.putToken);
    
    if (ceGreeks && peGreeks) {
        double netDelta = ceGreeks->delta + peGreeks->delta;  // CE positive, PE negative
        double netGamma = ceGreeks->gamma + peGreeks->gamma;  // Both positive
        double netVega = ceGreeks->vega + peGreeks->vega;     // Both positive
        
        m_symbolModel->setData(m_symbolModel->index(row, SYM_NET_DELTA), 
            QString::number(netDelta, 'f', 3));
        m_symbolModel->setData(m_symbolModel->index(row, SYM_NET_GAMMA), 
            QString::number(netGamma, 'f', 4));
        m_symbolModel->setData(m_symbolModel->index(row, SYM_NET_VEGA), 
            QString::number(netVega, 'f', 2));
    }
}
```

**Visual Enhancements:**

1. **Color Coding**:
   ```cpp
   // Delta color: Deep ITM (green) → ATM (yellow) → Deep OTM (red)
   QColor deltaColor(double delta, bool isCall) {
       if (isCall) {
           if (delta > 0.7) return QColor(0, 100, 0);      // Dark green
           if (delta > 0.3) return QColor(255, 165, 0);    // Orange
           return QColor(139, 0, 0);                        // Dark red
       } else {
           if (delta < -0.7) return QColor(0, 100, 0);
           if (delta < -0.3) return QColor(255, 165, 0);
           return QColor(139, 0, 0);
       }
   }
   
   // Theta color: Always red (decay is bad for buyers)
   QColor thetaColor(double theta) {
       return theta < -10 ? QColor(139, 0, 0) : QColor(255, 0, 0);
   }
   ```

2. **Tooltips**:
   ```cpp
   QString greeksTooltip(const GreeksResult& result) {
       return QString(
           "Symbol: %1 %2 %3\n"
           "Market Price: ₹%4\n"
           "Theoretical: ₹%5\n"
           "---\n"
           "IV: %6% (%7 iterations)\n"
           "Delta: %8 (₹1 move = ₹%8 option change)\n"
           "Gamma: %9 (delta change per ₹1)\n"
           "Vega: %10 (₹1 change per 1% IV)\n"
           "Theta: %11 (daily decay)\n"
           "Time to Expiry: %12 days"
       ).arg(result.symbol)
        .arg(result.strikePrice)
        .arg(result.isCall ? "CE" : "PE")
        .arg(result.marketPrice, 0, 'f', 2)
        .arg(result.theoreticalPrice, 0, 'f', 2)
        .arg(result.impliedVolatility * 100, 0, 'f', 2)
        .arg(result.ivIterations)
        .arg(result.delta, 0, 'f', 3)
        .arg(result.gamma, 0, 'f', 5)
        .arg(result.vega, 0, 'f', 2)
        .arg(result.theta, 0, 'f', 2)
        .arg(result.timeToExpiry * 365, 0, 'f', 1);
   }
   ```

3. **Greeks Toggle Button**:
   ```cpp
   QPushButton* m_greeksToggleBtn = new QPushButton("Show Greeks");
   m_greeksToggleBtn->setCheckable(true);
   m_greeksToggleBtn->setChecked(true);
   
   connect(m_greeksToggleBtn, &QPushButton::toggled, [this](bool checked) {
       // Show/hide Greeks columns
       for (int col = CALL_IV; col <= CALL_THETA; ++col) {
           m_callTable->setColumnHidden(col, !checked);
       }
       for (int col = PUT_IV; col <= PUT_THETA; ++col) {
           m_putTable->setColumnHidden(col, !checked);
       }
   });
   ```

#### Week 3 Tasks: UI Integration

| Day | Task | Deliverable |
|-----|------|-------------|
| Mon | Add Greeks columns to tables | Header + model setup |
| Tue | Implement onGreeksCalculated handler | Real-time Greeks display |
| Wed | Add color coding + tooltips | Professional UI |
| Thu | Add Greeks toggle + settings dialog | User control |
| Fri | Integration testing + bug fixes | Production-ready UI |

### 4.4 Configuration Management

**Add to configs/config.ini:**

```ini
[GREEKS_CALCULATION]
# Risk-free rate (Indian Treasury Bill / RBI Repo Rate)
# As of Feb 2026: 6.5%
risk_free_rate = 0.065

# Dividend yield for stock options (0 for indices)
dividend_yield = 0.0

# Enable Greeks calculation (global on/off)
enabled = true

# Auto-calculate on price updates
auto_calculate = true

# Throttle (milliseconds between recalculations per token)
# Lower = more responsive, Higher = less CPU usage
throttle_ms = 1000

# Cache validity (milliseconds)
# Greeks are valid for this duration without recalculation
cache_validity_ms = 5000

# Batch calculation for option chains
batch_calculation = true

# IV Solver Settings
iv_initial_guess = 0.20
iv_tolerance = 0.000001
iv_max_iterations = 100
iv_enable_brent_fallback = true

# Time tick interval (seconds) for theta decay updates
# Greeks are recalculated every N seconds for time decay
time_tick_interval = 60

# Performance settings
max_concurrent_calculations = 500
enable_simd_optimization = false  # Future: AVX2 batch processing
```

**Load in GreeksCalculationService::initialize():**

```cpp
void GreeksCalculationService::initialize() {
    QSettings settings("configs/config.ini", QSettings::IniFormat);
    settings.beginGroup("GREEKS_CALCULATION");
    
    m_config.riskFreeRate = settings.value("risk_free_rate", 0.065).toDouble();
    m_config.autoCalculate = settings.value("auto_calculate", true).toBool();
    m_config.throttleMs = settings.value("throttle_ms", 1000).toInt();
    m_config.cacheValidityMs = settings.value("cache_validity_ms", 5000).toInt();
    m_config.enableIVSolver = settings.value("enabled", true).toBool();
    
    settings.endGroup();
    
    // Start time tick timer for theta decay updates
    int tickInterval = settings.value("GREEKS_CALCULATION/time_tick_interval", 60).toInt();
    m_timeTickTimer->start(tickInterval * 1000);
    
    qInfo() << "[GreeksService] Initialized with r=" << m_config.riskFreeRate
            << ", throttle=" << m_config.throttleMs << "ms";
}
```

### 4.5 Phase 4 Deliverables

**Code Artifacts:**
- ✅ `IVCalculator.h/cpp` - Newton-Raphson + Brent's method (300 lines)
- ✅ `GreeksCalculationService.h/cpp` - Orchestration service (500 lines)
- ✅ `ATMWatchWindow.cpp` - Greeks UI integration (200 lines added)
- ✅ `config.ini` - Configuration section (30 lines)

**Tests:**
- ✅ Unit tests: `test_iv_calculator.cpp` (>50 test cases)
- ✅ Integration tests: `test_greeks_service.cpp` (20 test cases)
- ✅ UI tests: Manual testing checklist (30 items)

**Documentation:**
- ✅ API documentation (Doxygen comments)
- ✅ User guide: "Using Greeks in ATM Watch"
- ✅ Technical spec: "IV Solver Algorithm"
- ✅ Performance benchmarks

**Success Metrics:**
- ✅ IV convergence rate: >95%
- ✅ Calculation speed: <10ms for 10 strikes
- ✅ Accuracy: Within 0.5% of Bloomberg values
- ✅ CPU overhead: <5% idle, <15% under load
- ✅ UI responsiveness: No lag during calculations

---

## Phase 5: Risk Analytics

**Duration:** 3 weeks  
**Priority:** P1 (High) - Portfolio risk management  
**Dependencies:** Phase 4 complete  
**Goal:** Aggregate Greeks for portfolio risk assessment

### 5.1 Portfolio Greeks Manager

**Purpose:** Track and aggregate Greeks across all ATM positions

```cpp
class PortfolioGreeksManager : public QObject {
    Q_OBJECT
    
public:
    struct Position {
        uint32_t token;
        QString symbol;
        double strikePrice;
        bool isCall;
        int quantity;           // +ve = long, -ve = short
        double entryPrice;
        double currentPrice;
        
        // Greeks per contract
        double delta;
        double gamma;
        double vega;
        double theta;
        
        // Position Greeks (Greeks × quantity)
        double positionDelta;   // delta × qty
        double positionGamma;   // gamma × qty
        double positionVega;    // vega × qty
        double positionTheta;   // theta × qty
        
        // P&L
        double unrealizedPnL;   // (current - entry) × qty
        double deltaPnL;        // Estimated P&L for ₹1 underlying move
    };
    
    struct PortfolioSummary {
        // Aggregate Greeks
        double totalDelta;      // Net directional exposure
        double totalGamma;      // Net convexity risk
        double totalVega;       // Net volatility exposure
        double totalTheta;      // Net time decay (daily)
        
        // Risk metrics
        double netExposure;     // Sum of position values
        double margin;          // Total margin required
        double unrealizedPnL;   // Total P&L
        
        // Position counts
        int longCalls;
        int shortCalls;
        int longPuts;
        int shortPuts;
        int totalPositions;
        
        // Risk scores
        double deltaRisk;       // |totalDelta| / total positions
        double gammaRisk;       // totalGamma score
        double vegaRisk;        // totalVega / total positions
        double thetaDecay;      // Daily decay amount
    };
    
    static PortfolioGreeksManager& getInstance();
    
    /**
     * @brief Add position to portfolio
     */
    void addPosition(const Position& pos);
    
    /**
     * @brief Update position quantity (add/reduce/close)
     */
    void updatePosition(uint32_t token, int deltaQty);
    
    /**
     * @brief Remove position
     */
    void removePosition(uint32_t token);
    
    /**
     * @brief Get all positions
     */
    QVector<Position> getPositions() const;
    
    /**
     * @brief Get portfolio summary with aggregated Greeks
     */
    PortfolioSummary getSummary() const;
    
    /**
     * @brief Calculate P&L for hypothetical underlying move
     * 
     * @param underlyingMove ₹ move in underlying (e.g., +100, -50)
     * @return Estimated portfolio P&L
     */
    double estimatePnL(double underlyingMove) const;
    
    /**
     * @brief Export portfolio to CSV
     */
    bool exportToCSV(const QString& filePath) const;
    
signals:
    void portfolioUpdated();
    void riskAlertTriggered(const QString& message);
    
public slots:
    void onGreeksUpdated(const GreeksResult& result);
    void onPriceUpdated(uint32_t token, double ltp);
    
private:
    PortfolioGreeksManager();
    ~PortfolioGreeksManager() = default;
    
    QHash<uint32_t, Position> m_positions;
    mutable std::shared_mutex m_mutex;
    
    void checkRiskLimits();  // Trigger alerts if limits breached
};
```

### 5.2 Portfolio Greeks Window

**New UI Window:** `PortfolioGreeksWindow`

**Features:**
1. **Positions Table**:
   - Symbol, Strike, Type (CE/PE)
   - Quantity, Entry, Current, P&L
   - Greeks per contract
   - Position Greeks (Greeks × Qty)

2. **Summary Panel**:
   ```
   ┌─────────────────────────────────────────────────────┐
   │ Portfolio Greeks Summary                             │
   ├─────────────────────────────────────────────────────┤
   │ Total Delta:    +145.2  (Moderately Bullish)        │
   │ Total Gamma:    +12.8   (Long Gamma - Positive)     │
   │ Total Vega:     +320.5  (Long Volatility)           │
   │ Total Theta:    -450.2  (Daily decay: ₹450/day)     │
   ├─────────────────────────────────────────────────────┤
   │ Net Exposure:   ₹2,35,000                           │
   │ Unrealized P&L: +₹12,500 (5.3%)                     │
   │ Estimated P&L for ₹100 move: +₹14,520               │
   └─────────────────────────────────────────────────────┘
   ```

3. **Risk Gauges**:
   - Delta gauge: -1.0 (bearish) → 0 (neutral) → +1.0 (bullish)
   - Gamma meter: Convexity risk
   - Vega meter: Volatility exposure
   - Theta decay: Daily cost

4. **What-If Scenario**:
   ```cpp
   // User input: Underlying move
   QDoubleSpinBox* m_underlyingMoveSpinBox;
   QLabel* m_estimatedPnLLabel;
   
   void onUnderlyingMoveChanged(double move) {
       double pnl = PortfolioGreeksManager::getInstance().estimatePnL(move);
       m_estimatedPnLLabel->setText(QString("Estimated P&L: ₹%1").arg(pnl, 0, 'f', 2));
   }
   ```

### 5.3 Position Sizing Calculator

**Tool for determining optimal position size based on Greeks**

```cpp
struct PositionSizeResult {
    int recommendedQty;
    double positionDelta;
    double capitalRequired;
    double maxLoss;       // Max loss if underlying moves X%
    double riskReward;
    QString reasoning;
};

class PositionSizingCalculator {
public:
    /**
     * @brief Calculate optimal position size for delta-neutral strategy
     * 
     * Example: User wants delta-neutral portfolio
     * Current delta: +100
     * Add NIFTY 24500 PE (delta -0.4) → Need 250 contracts
     */
    static PositionSizeResult calculateDeltaNeutral(
        double currentPortfolioDelta,
        const GreeksResult& newOptionGreeks
    );
    
    /**
     * @brief Calculate position size based on risk capital
     * 
     * Example: Risk ₹10,000, Option premium ₹150, Stop loss at ₹100
     * Max quantity: 10000 / (150 - 100) = 200 contracts
     */
    static PositionSizeResult calculateByRiskCapital(
        double riskCapital,
        double optionPremium,
        double stopLoss
    );
    
    /**
     * @brief Calculate position size to achieve target Greeks
     * 
     * Example: Want +50 vega exposure
     * Option vega = 15
     * Required: 50 / 15 = ~4 contracts
     */
    static PositionSizeResult calculateByTargetGreeks(
        double targetDelta,
        double targetGamma,
        double targetVega,
        const GreeksResult& optionGreeks
    );
};
```

### 5.4 Risk Alerts System

**Real-time alerts for risk limit breaches**

```cpp
struct RiskLimit {
    QString name;
    double threshold;
    bool enabled;
    QString alertMessage;
};

class RiskAlertsManager : public QObject {
    Q_OBJECT
    
public:
    // Default limits
    struct Limits {
        double maxDelta = 500.0;         // Max directional exposure
        double maxGamma = 100.0;         // Max convexity risk
        double maxVega = 1000.0;         // Max volatility exposure
        double maxDailyTheta = 2000.0;   // Max daily decay
        double maxPositionLoss = 50000.0;// Max loss per position
        double maxPortfolioDrawdown = 0.10; // 10% max drawdown
    };
    
    void setLimits(const Limits& limits);
    void checkLimits(const PortfolioSummary& summary);
    
signals:
    void riskAlert(const QString& type, const QString& message, int severity);
    // severity: 0=info, 1=warning, 2=critical
};
```

**Example Alerts:**
- ⚠️ **Warning**: Portfolio delta exceeded +500 (currently +524)
- 🔴 **Critical**: Daily theta decay >₹2000 (currently ₹2,345)
- ℹ️ **Info**: Portfolio is delta-neutral (delta = -12)
- 🔴 **Critical**: Position NIFTY 24000 CE down 25% (stop loss triggered)

### 5.5 Phase 5 Deliverables

**Code Artifacts:**
- ✅ `PortfolioGreeksManager.h/cpp` (400 lines)
- ✅ `PortfolioGreeksWindow.h/cpp` (600 lines)
- ✅ `PositionSizingCalculator.h/cpp` (200 lines)
- ✅ `RiskAlertsManager.h/cpp` (150 lines)

**Success Metrics:**
- ✅ Accurate Greeks aggregation (verified against manual calculation)
- ✅ Real-time portfolio updates (<100ms latency)
- ✅ Risk alerts trigger correctly
- ✅ What-if P&L within 5% of actual (backtested)

---

## Phase 6: Historical Analytics

**Duration:** 4 weeks  
**Priority:** P1 (High) - Historical analysis for strategy development  
**Dependencies:** Phase 4-5 complete  
**Goal:** Track ATM changes, IV evolution, Greeks history

### 6.1 Historical Data Storage

**Database Schema:**

```sql
-- SQLite database: trading_terminal.db

CREATE TABLE atm_history (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp INTEGER NOT NULL,  -- Unix timestamp
    symbol TEXT NOT NULL,
    expiry TEXT NOT NULL,
    underlying_price REAL,
    atm_strike REAL,
    call_ltp REAL,
    put_ltp REAL,
    call_iv REAL,
    put_iv REAL,
    call_delta REAL,
    put_delta REAL,
    call_gamma REAL,
    put_gamma REAL,
    call_vega REAL,
    put_vega REAL,
    call_theta REAL,
    put_theta REAL,
    INDEX idx_symbol_timestamp (symbol, timestamp),
    INDEX idx_symbol_expiry (symbol, expiry)
);

CREATE TABLE iv_history (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp INTEGER NOT NULL,
    token INTEGER NOT NULL,
    symbol TEXT NOT NULL,
    strike REAL NOT NULL,
    is_call BOOLEAN NOT NULL,
    expiry TEXT NOT NULL,
    underlying_price REAL,
    option_ltp REAL,
    implied_volatility REAL,
    delta REAL,
    gamma REAL,
    vega REAL,
    theta REAL,
    INDEX idx_token_timestamp (token, timestamp),
    INDEX idx_symbol_strike_expiry (symbol, strike, expiry)
);

CREATE TABLE portfolio_snapshots (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp INTEGER NOT NULL,
    total_delta REAL,
    total_gamma REAL,
    total_vega REAL,
    total_theta REAL,
    net_exposure REAL,
    unrealized_pnl REAL,
    positions_json TEXT,  -- JSON array of positions
    INDEX idx_timestamp (timestamp)
);
```

### 6.2 Historical Data Collector

```cpp
class HistoricalDataCollector : public QObject {
    Q_OBJECT
    
public:
    enum RecordingMode {
        Continuous,      // Record every N seconds
        OnChange,        // Record only when ATM changes
        OnDemand         // Manual snapshots only
    };
    
    static HistoricalDataCollector& getInstance();
    
    /**
     * @brief Start recording historical data
     */
    void startRecording(RecordingMode mode = OnChange, int intervalSeconds = 60);
    
    /**
     * @brief Stop recording
     */
    void stopRecording();
    
    /**
     * @brief Take manual snapshot
     */
    void takeSnapshot();
    
    /**
     * @brief Query ATM history
     */
    QVector<ATMHistoryPoint> queryATMHistory(
        const QString& symbol,
        const QDateTime& startTime,
        const QDateTime& endTime
    );
    
    /**
     * @brief Query IV history for a strike
     */
    QVector<IVHistoryPoint> queryIVHistory(
        uint32_t token,
        const QDateTime& startTime,
        const QDateTime& endTime
    );
    
    /**
     * @brief Get IV percentile (current IV vs historical)
     */
    double getIVPercentile(uint32_t token, int lookbackDays = 30);
    
signals:
    void snapshotRecorded(const QDateTime& timestamp);
    
private slots:
    void onTimerTick();
    void onATMChanged(const QString& symbol, double oldStrike, double newStrike);
    
private:
    HistoricalDataCollector();
    ~HistoricalDataCollector() = default;
    
    QSqlDatabase m_db;
    QTimer* m_recordTimer;
    RecordingMode m_mode;
    bool m_recording = false;
};
```

### 6.3 Historical Charts

**IV Evolution Chart:**

```cpp
class IVChartWidget : public QWidget {
    Q_OBJECT
    
public:
    /**
     * @brief Display IV evolution over time for ATM strike
     * 
     * Features:
     * - Line chart: IV vs Time
     * - Percentile bands (25th, 50th, 75th percentile lines)
     * - Current IV marker
     * - ATM strike change annotations
     */
    void plotIVHistory(
        const QString& symbol,
        const QString& expiry,
        const QDateTime& startTime,
        const QDateTime& endTime
    );
    
    /**
     * @brief Display IV skew (IV vs Strike)
     * 
     * Volatility smile/smirk visualization
     */
    void plotIVSkew(
        const QString& symbol,
        const QString& expiry,
        const QDateTime& timestamp
    );
};
```

**ATM Movement Heatmap:**

```cpp
class ATMHeatmapWidget : public QWidget {
    Q_OBJECT
    
public:
    /**
     * @brief Display ATM strike changes as heatmap
     * 
     * X-axis: Time (hourly)
     * Y-axis: Strike prices
     * Color: ATM probability (darker = ATM longer)
     * 
     * Shows which strikes were ATM throughout the day
     */
    void plotATMHeatmap(
        const QString& symbol,
        const QDate& date
    );
};
```

### 6.4 IV Percentile Indicator

**Show current IV relative to historical range**

```cpp
struct IVPercentileData {
    double currentIV;
    double historicalMin;
    double historicalMax;
    double historicalMean;
    double historicalStdDev;
    double percentile;  // 0-100
    QString interpretation;  // "Low", "Normal", "High", "Extreme"
};

IVPercentileData getIVPercentile(uint32_t token, int lookbackDays) {
    // Query last N days of IV data
    auto history = queryIVHistory(token, lookbackDays);
    
    double current = history.last().impliedVolatility;
    
    // Calculate percentile
    int rank = 0;
    for (const auto& point : history) {
        if (point.impliedVolatility < current) rank++;
    }
    double percentile = (rank * 100.0) / history.size();
    
    // Interpretation
    QString interp;
    if (percentile < 20) interp = "Very Low";
    else if (percentile < 40) interp = "Low";
    else if (percentile < 60) interp = "Normal";
    else if (percentile < 80) interp = "High";
    else interp = "Very High";
    
    return {current, min, max, mean, stdDev, percentile, interp};
}
```

**UI Display:**

```
┌───────────────────────────────────────────────┐
│ NIFTY 24000 CE - IV Percentile                │
├───────────────────────────────────────────────┤
│ Current IV: 18.5% (67th percentile)           │
│ ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ │
│ │   │   │   │   │   │   ▲   │   │   │   │   │
│ 12% 14% 16% 18% 20% 22% 24% 26% 28% 30% 32%  │
│ Min           Mean         Current        Max │
│                                                │
│ Interpretation: HIGH                           │
│ 30-day range: 12.5% - 32.1%                   │
│ Avg: 19.2%, StdDev: 3.4%                      │
└───────────────────────────────────────────────┘
```

### 6.5 Phase 6 Deliverables

**Code Artifacts:**
- ✅ `HistoricalDataCollector.h/cpp` (500 lines)
- ✅ `IVChartWidget.h/cpp` (400 lines)
- ✅ `ATMHeatmapWidget.h/cpp` (300 lines)
- ✅ Database schema + migration scripts

**Success Metrics:**
- ✅ Recording overhead <1% CPU
- ✅ Database size <100MB per month
- ✅ Query performance <100ms for 1-month data
- ✅ Charts render <500ms

---

## Phase 7: Advanced Visualization

**Duration:** 3 weeks  
**Priority:** P2 (Medium) - Visual analytics  
**Dependencies:** Phase 6 complete  
**Goal:** Volatility surface, Greeks heatmaps, interactive charts

### 7.1 Volatility Surface Viewer

**3D volatility surface (IV vs Strike vs Time)**

```cpp
class VolatilitySurfaceWidget : public QWidget {
    Q_OBJECT
    
public:
    /**
     * @brief Display 3D volatility surface
     * 
     * X-axis: Strike price (moneyness)
     * Y-axis: Time to expiry (days)
     * Z-axis: Implied volatility (%)
     * 
     * Shows volatility smile/skew across strikes and expirations
     */
    void plotVolatilitySurface(const QString& symbol);
    
    /**
     * @brief Export surface data
     */
    bool exportToCSV(const QString& filePath);
};
```

**Features:**
- Interactive 3D rotation (Qt3D or QtDataVisualization)
- Slice views (IV vs Strike for single expiry)
- Color gradient (low IV = blue, high IV = red)
- Moneyness normalization (ATM = 100%)

### 7.2 Greeks Heatmap

**Visualize Greeks across option chain**

```cpp
class GreeksHeatmapWidget : public QTableWidget {
    Q_OBJECT
    
public:
    /**
     * @brief Display Greeks as color-coded heatmap
     * 
     * Rows: Strike prices
     * Columns: Delta | Gamma | Vega | Theta
     * Color: Value intensity (green=high, red=low)
     */
    void plotGreeksHeatmap(
        const QString& symbol,
        const QString& expiry,
        GreekType greekType = AllGreeks
    );
};
```

**Example Output:**

```
        Delta       Gamma       Vega        Theta
23000  0.95 ████   0.02 █      10.5 ██     -5.2 █
23500  0.75 ███    0.08 ███    45.2 ████   -12.8 ███
24000  0.50 ██    0.12 ████   68.5 █████  -18.5 ████
24500  0.25 █      0.08 ███    45.8 ████   -12.3 ███
25000  0.05 ░      0.02 █      11.2 ██     -5.8 █

Legend: Dark = High, Light = Low
```

### 7.3 Real-Time Greeks Sparklines

**Mini charts in ATM Watch table cells**

```cpp
class SparklineDelegate : public QStyledItemDelegate {
    Q_OBJECT
    
protected:
    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override {
        // Draw mini line chart in table cell
        // Shows last 10 values (rolling window)
    }
};

// Usage in ATM Watch
m_callTable->setItemDelegateForColumn(CALL_IV, new SparklineDelegate());
m_callTable->setItemDelegateForColumn(CALL_DELTA, new SparklineDelegate());
```

**Visual:**
```
IV: 18.5% ╱╲╱╲__
Delta: 0.52 __╱╱╲╲
Gamma: 0.08 ╱╲__╱╲
```

### 7.4 Strike Range Settings Dialog

**User-configurable ATM+N strike selection**

```cpp
class ATMWatchSettingsDialog : public QDialog {
    Q_OBJECT
    
public:
    explicit ATMWatchSettingsDialog(QWidget* parent = nullptr);
    
private:
    void setupUI();
    void loadSettings();
    void saveSettings();
    
    // UI Components
    QSpinBox* m_strikeRangeSpinBox;        // 0-10 (ATM+1 to ATM+10)
    QDoubleSpinBox* m_thresholdMultiplier;  // 0.25-1.0 (recalc threshold)
    QSpinBox* m_updateIntervalSpinBox;      // 10-300 seconds
    QCheckBox* m_autoRecalculateCheckbox;   // Enable/disable auto
    QCheckBox* m_showGreeksCheckbox;        // Show/hide Greeks columns
    QCheckBox* m_soundAlertsCheckbox;       // Enable sound alerts
    QComboBox* m_basePriceSourceCombo;      // Cash/Future
    
    // Greeks settings
    QDoubleSpinBox* m_riskFreeRateSpinBox;  // 0-10% (RBI rate)
    QCheckBox* m_enableGreeksCheckbox;      // Enable Greeks calculation
};
```

**Settings Dialog UI:**

```
┌─────────────────────────────────────────────────────────────┐
│ ATM Watch Settings                                    [X]   │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│ ┌─ Strike Selection ─────────────────────────────────────┐ │
│ │                                                         │ │
│ │ Strike Range:  [5 ▼] strikes around ATM                │ │
│ │                (Shows ATM-5 to ATM+5)                   │ │
│ │                                                         │ │
│ │ Example: NIFTY ATM=24000                                │ │
│ │ Will show: 23500, 23600, 23700, 23800, 23900,          │ │
│ │            24000, 24100, 24200, 24300, 24400, 24500     │ │
│ │                                                         │ │
│ └─────────────────────────────────────────────────────────┘ │
│                                                             │
│ ┌─ Update Settings ──────────────────────────────────────┐ │
│ │                                                         │ │
│ │ [ ✓ ] Auto-calculate on price changes                  │ │
│ │                                                         │ │
│ │ Timer Interval:  [60 ▼] seconds (backup)               │ │
│ │                                                         │ │
│ │ Threshold Multiplier: [0.50] (0.25 - 1.0)              │ │
│ │ └─ Recalculate when price moves 50% of strike interval │ │
│ │                                                         │ │
│ │ Base Price Source: [Cash ▼] (Cash/Future)              │ │
│ │                                                         │ │
│ └─────────────────────────────────────────────────────────┘ │
│                                                             │
│ ┌─ Greeks Calculation ───────────────────────────────────┐ │
│ │                                                         │ │
│ │ [ ✓ ] Enable Greeks calculation                        │ │
│ │                                                         │ │
│ │ Risk-Free Rate: [6.5] % (RBI Repo Rate)                │ │
│ │                                                         │ │
│ │ [ ✓ ] Show Greeks columns (IV, Delta, Gamma, etc.)     │ │
│ │                                                         │ │
│ └─────────────────────────────────────────────────────────┘ │
│                                                             │
│ ┌─ Alerts ───────────────────────────────────────────────┐ │
│ │                                                         │ │
│ │ [ ✓ ] Sound alerts on ATM change                       │ │
│ │ [ ✓ ] Visual flash on strike change                    │ │
│ │ [ ✓ ] System notifications                             │ │
│ │                                                         │ │
│ └─────────────────────────────────────────────────────────┘ │
│                                                             │
│                    [ Reset to Defaults ]  [ OK ]  [ Cancel ]│
└─────────────────────────────────────────────────────────────┘
```

**Implementation:**

```cpp
void ATMWatchSettingsDialog::saveSettings() {
    // Save strike range
    int rangeCount = m_strikeRangeSpinBox->value();
    ATMWatchManager::getInstance().setStrikeRangeCount(rangeCount);
    
    // Save threshold multiplier
    double multiplier = m_thresholdMultiplier->value();
    ATMWatchManager::getInstance().setThresholdMultiplier(multiplier);
    
    // Save to config file
    QSettings settings("configs/config.ini", QSettings::IniFormat);
    settings.beginGroup("ATM_WATCH");
    settings.setValue("strike_range_count", rangeCount);
    settings.setValue("threshold_multiplier", multiplier);
    settings.setValue("update_interval", m_updateIntervalSpinBox->value());
    settings.setValue("base_price_source", m_basePriceSourceCombo->currentText());
    settings.setValue("enable_greeks", m_enableGreeksCheckbox->isChecked());
    settings.setValue("risk_free_rate", m_riskFreeRateSpinBox->value() / 100.0);
    settings.setValue("sound_alerts", m_soundAlertsCheckbox->isChecked());
    settings.endGroup();
    
    qInfo() << "[ATMWatch] Settings saved: range=" << rangeCount 
            << ", threshold=" << multiplier;
}
```

### 7.5 Option Chain Window Integration

**Launch full Option Chain from ATM Watch**

**Purpose:** Allow traders to open detailed Option Chain window for any symbol directly from ATM Watch.

**Features:**
1. **Context Menu Integration**: Right-click any row → "Open Option Chain"
2. **Double-Click Action**: Double-click symbol → Open chain window
3. **Auto-Focus**: Chain opens with ATM strike centered
4. **Expiry Sync**: Uses same expiry as ATM Watch
5. **Real-Time Sync**: Chain window updates with same data feed

**Implementation:**

```cpp
class ATMWatchWindow : public QWidget {
    Q_OBJECT
    
private:
    // Add context menu support
    void setupContextMenu();
    void showContextMenu(const QPoint& pos);
    
    // Double-click handler
    void onSymbolDoubleClicked(const QModelIndex& index);
    
    // Option chain launcher
    void openOptionChain(const QString& symbol, const QString& expiry);
    
    QMap<QString, QWidget*> m_openChainWindows;  // Track open windows
};

// Context menu setup
void ATMWatchWindow::setupContextMenu() {
    m_symbolTable->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_symbolTable, &QTableView::customContextMenuRequested,
            this, &ATMWatchWindow::showContextMenu);
    
    // Double-click handler
    connect(m_symbolTable, &QTableView::doubleClicked,
            this, &ATMWatchWindow::onSymbolDoubleClicked);
}

void ATMWatchWindow::showContextMenu(const QPoint& pos) {
    QModelIndex index = m_symbolTable->indexAt(pos);
    if (!index.isValid()) return;
    
    int row = index.row();
    QString symbol = m_symbolModel->item(row, SYM_NAME)->text();
    QString expiry = m_symbolModel->item(row, SYM_EXPIRY)->text();
    
    QMenu contextMenu(this);
    
    // Open Option Chain action
    QAction* openChainAction = contextMenu.addAction(
        QIcon(":/icons/option_chain.png"), 
        "Open Option Chain"
    );
    connect(openChainAction, &QAction::triggered, [this, symbol, expiry]() {
        openOptionChain(symbol, expiry);
    });
    
    contextMenu.addSeparator();
    
    // Copy symbol
    QAction* copyAction = contextMenu.addAction("Copy Symbol");
    connect(copyAction, &QAction::triggered, [symbol]() {
        QApplication::clipboard()->setText(symbol);
    });
    
    // Remove from watch
    QAction* removeAction = contextMenu.addAction(
        QIcon(":/icons/remove.png"), 
        "Remove from Watch"
    );
    connect(removeAction, &QAction::triggered, [this, symbol]() {
        ATMWatchManager::getInstance().removeWatch(symbol);
        refreshData();
    });
    
    contextMenu.exec(m_symbolTable->viewport()->mapToGlobal(pos));
}

void ATMWatchWindow::onSymbolDoubleClicked(const QModelIndex& index) {
    int row = index.row();
    QString symbol = m_symbolModel->item(row, SYM_NAME)->text();
    QString expiry = m_symbolModel->item(row, SYM_EXPIRY)->text();
    
    openOptionChain(symbol, expiry);
}

void ATMWatchWindow::openOptionChain(const QString& symbol, const QString& expiry) {
    // Check if already open
    QString key = symbol + "_" + expiry;
    if (m_openChainWindows.contains(key)) {
        QWidget* existingWindow = m_openChainWindows[key];
        existingWindow->raise();
        existingWindow->activateWindow();
        return;
    }
    
    // Create new Option Chain window
    OptionChainWindow* chainWindow = new OptionChainWindow(nullptr);
    chainWindow->setAttribute(Qt::WA_DeleteOnClose);
    chainWindow->setWindowTitle(QString("Option Chain - %1 %2").arg(symbol, expiry));
    
    // Load data for the symbol and expiry
    chainWindow->loadOptionChain(symbol, expiry);
    
    // Get ATM strike from current data
    auto atmInfo = ATMWatchManager::getInstance().getATMInfo(symbol);
    if (atmInfo.isValid && atmInfo.atmStrike > 0) {
        chainWindow->centerOnStrike(atmInfo.atmStrike);
        chainWindow->highlightATMStrike(atmInfo.atmStrike);
    }
    
    // Track window
    m_openChainWindows[key] = chainWindow;
    
    // Remove from tracking when closed
    connect(chainWindow, &QObject::destroyed, [this, key]() {
        m_openChainWindows.remove(key);
    });
    
    // Show window
    chainWindow->resize(1200, 800);
    chainWindow->show();
    
    qInfo() << "[ATMWatch] Opened option chain for" << symbol << expiry;
}
```

**Option Chain Window Enhancements:**

```cpp
class OptionChainWindow : public QWidget {
    Q_OBJECT
    
public:
    /**
     * @brief Load option chain for specific symbol and expiry
     */
    void loadOptionChain(const QString& symbol, const QString& expiry);
    
    /**
     * @brief Center view on specific strike
     */
    void centerOnStrike(double strike);
    
    /**
     * @brief Highlight ATM strike (visual marker)
     */
    void highlightATMStrike(double atmStrike);
    
    /**
     * @brief Auto-update when ATM changes in ATM Watch
     */
    void updateATMHighlight(double newATMStrike);
    
private slots:
    void onATMChanged(const QString& symbol, double oldStrike, double newStrike);
};

// Connect to ATM Watch Manager for live updates
void OptionChainWindow::setupConnections() {
    connect(&ATMWatchManager::getInstance(), 
            &ATMWatchManager::atmStrikeChanged,
            this, 
            &OptionChainWindow::onATMChanged);
}

void OptionChainWindow::onATMChanged(const QString& symbol, 
                                     double oldStrike, 
                                     double newStrike) {
    if (symbol == m_currentSymbol) {
        // Update ATM highlight
        highlightATMStrike(newStrike);
        
        // Optional: Flash animation
        flashATMRow(newStrike, 2000);  // 2 seconds
        
        qDebug() << "[OptionChain]" << symbol << "ATM changed:" 
                 << oldStrike << "→" << newStrike;
    }
}
```

**Visual Design:**

```
┌─────────────────────────────────────────────────────────────────────────────┐
│ Option Chain - NIFTY 27FEB2026                                      [_][□][X]│
├─────────────────────────────────────────────────────────────────────────────┤
│ Underlying: NIFTY  Price: 23,987.50 ↑ +125.30 (+0.52%)                     │
│ ATM Strike: 24000  Expiry: 19 days   Total OI: 45.2M                       │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  ────────────────── CALLS ──────────────────┼─────────────────── PUTS ───────│
│  OI  Vol   IV    Δ    LTP  Chg   Bid  Ask  │  Strike │ Bid  Ask  LTP  Chg   │
│ ───────────────────────────────────────────┼─────────────────────────────────│
│ 125K 2.5K 18.2% 0.85  680  +45   675  685  │  23500  │ 15   20   18   -2    │
│ 180K 4.2K 17.8% 0.75  520  +38   515  525  │  23600  │ 25   30   28   -3    │
│ 245K 6.8K 17.5% 0.68  385  +32   380  390  │  23700  │ 38   43   40   -4    │
│ 320K 9.5K 17.2% 0.60  275  +25   270  280  │  23800  │ 55   60   58   -5    │
│ 425K 12K  16.9% 0.52  190  +18   185  195  │  23900  │ 78   83   80   -8    │
│═520K═15K═16.5%═0.50══135══+12═══130══140══│══24000══│═105══110══108══-10═══│← ATM
│ 425K 11K  16.8% 0.48   95  +8     90  100  │  24100  │ 145  150  148  -12   │
│ 320K 8.2K 17.1% 0.40   65  +5     60   70  │  24200  │ 195  200  198  -15   │
│ 245K 5.5K 17.4% 0.32   42  +3     38   46  │  24300  │ 260  265  262  -20   │
│ 180K 3.8K 17.8% 0.25   28  +2     24   32  │  24400  │ 340  345  342  -25   │
│ 125K 2.1K 18.2% 0.18   18  +1     14   22  │  24500  │ 435  440  438  -32   │
│                                                                             │
│  [Launched from ATM Watch]  [Sync ATM Highlight: ON]  [Auto-Update: ON]     │
└─────────────────────────────────────────────────────────────────────────────┘
```

**Configuration (config.ini):**

```ini
[OPTION_CHAIN]
# Integration with ATM Watch
enable_atm_watch_integration = true
auto_highlight_atm = true
flash_on_atm_change = true
flash_duration_ms = 2000

# Window behavior
allow_multiple_chains = true
reuse_existing_window = true
```

### 7.6 Interactive Option Chain Visualizer

**Enhanced option chain with visual Greeks**

```cpp
class OptionChainVisualizer : public QWidget {
    Q_OBJECT
    
public:
    /**
     * @brief Interactive option chain with visualizations
     * 
     * Features:
     * - Strike ladder (vertical)
     * - CE/PE side-by-side
     * - Greeks bars (visual magnitude)
     * - IV smile overlay
     * - ATM marker (highlight)
     * - OI profile (horizontal bars)
     */
    void displayChain(const QString& symbol, const QString& expiry);
};
```

### 7.7 Phase 7 Deliverables

**Code Artifacts:**
- ✅ `VolatilitySurfaceWidget.h/cpp` (500 lines)
- ✅ `GreeksHeatmapWidget.h/cpp` (350 lines)
- ✅ `SparklineDelegate.h/cpp` (200 lines)
- ✅ `ATMWatchSettingsDialog.h/cpp` (400 lines) - **NEW**
- ✅ `ATMWatchWindow.cpp` - Context menu + Option Chain integration (300 lines added)
- ✅ `OptionChainWindow.cpp` - ATM sync enhancements (200 lines added)
- ✅ `OptionChainVisualizer.h/cpp` (600 lines)

**Success Metrics:**
- ✅ Charts render <1 second
- ✅ Smooth interaction (60 FPS)
- ✅ Low memory footprint (<50MB for charts)
- ✅ Option Chain opens in <500ms
- ✅ Settings persist across sessions
- ✅ ATM+1 to ATM+10 strike selection working

---

## Phase 8: Trading Intelligence

**Duration:** 3 weeks  
**Priority:** P2 (Medium) - Advanced trading features  
**Dependencies:** Phase 4-7 complete  
**Goal:** Automated scanning, alerts, strategy recommendations

### 8.1 Greeks-Based Scanner

**Find options matching Greeks criteria**

```cpp
struct ScanCriteria {
    // Greeks filters
    double minDelta = 0.0;
    double maxDelta = 1.0;
    double minGamma = 0.0;
    double minVega = 0.0;
    double maxTheta = 0.0;
    
    // IV filters
    double minIV = 0.0;
    double maxIV = 5.0;
    double ivPercentileMin = 0.0;
    double ivPercentileMax = 100.0;
    
    // Price filters
    double minLTP = 0.0;
    double maxLTP = 999999.0;
    double minOI = 0;
    double minVolume = 0;
    
    // Moneyness
    enum Moneyness { Any, ITM, ATM, OTM };
    Moneyness moneyness = Any;
};

struct ScanResult {
    uint32_t token;
    QString symbol;
    double strike;
    bool isCall;
    QString expiry;
    
    double ltp;
    double iv;
    double delta;
    double gamma;
    double vega;
    double theta;
    
    double ivPercentile;
    QString reason;  // Why this option matched
};

class GreeksScanner {
public:
    /**
     * @brief Scan all options matching criteria
     */
    static QVector<ScanResult> scan(const ScanCriteria& criteria);
    
    /**
     * @brief Pre-defined scans
     */
    static QVector<ScanResult> scanHighVega();        // Options with high vega
    static QVector<ScanResult> scanLowIVPercentile(); // IV in bottom 20%
    static QVector<ScanResult> scanHighGamma();       // High gamma (near ATM)
    static QVector<ScanResult> scanDeepITM();         // Deep ITM with high delta
};
```

**Pre-Defined Scans:**

1. **High Vega Opportunities** (IV trading):
   - Vega > 50
   - IV percentile < 30 (low IV, room to rise)
   - ATM or near ATM strikes

2. **Gamma Scalping Candidates**:
   - Gamma > 0.05
   - Delta between 0.4 - 0.6 (near ATM)
   - High liquidity (OI > 10,000)

3. **Theta Decay Sellers**:
   - Theta < -10 (high decay)
   - IV percentile > 70 (sell high IV)
   - Time to expiry < 7 days

4. **Deep ITM Hedges**:
   - Delta > 0.9 (calls) or < -0.9 (puts)
   - Low vega (less IV risk)
   - Tight bid-ask spread

### 8.2 Smart Alerts System

**Advanced alert conditions**

```cpp
class SmartAlertsManager : public QObject {
    Q_OBJECT
    
public:
    struct AlertCondition {
        QString name;
        uint32_t token;
        
        enum Type {
            IVPercentile,     // IV crosses percentile threshold
            DeltaChange,      // Delta moves beyond range
            ATMTransition,    // Option becomes ATM
            GreeksThreshold,  // Any Greek crosses threshold
            PriceAction       // Option price movement
        };
        
        Type type;
        double threshold;
        bool enabled;
    };
    
    /**
     * @brief Add alert condition
     */
    void addAlert(const AlertCondition& alert);
    
    /**
     * @brief Pre-defined alert templates
     */
    AlertCondition createIVPercentileAlert(uint32_t token, double percentile);
    AlertCondition createATMTransitionAlert(const QString& symbol);
    AlertCondition createGammaExplosionAlert(uint32_t token, double threshold);
    
signals:
    void alertTriggered(const AlertCondition& alert, const QString& message);
};
```

**Example Alerts:**

1. **IV Percentile Alert**:
   - "NIFTY 24000 CE IV entered bottom 10% (12.5% IV, 8th percentile) - BUY signal"

2. **ATM Transition Alert**:
   - "NIFTY 23500 CE became ATM (underlying moved to 23,510) - High gamma zone"

3. **Gamma Explosion Alert**:
   - "NIFTY 24000 PE gamma spiked to 0.15 (was 0.08) - Hedging opportunity"

4. **Theta Acceleration Alert**:
   - "NIFTY 24500 CE theta decay accelerated to -₹25/day (5 days to expiry) - Close or roll"

### 8.3 Strategy Recommender

**AI-powered strategy suggestions based on Greeks and market conditions**

```cpp
enum Strategy {
    LongStraddle,      // Buy ATM CE + PE (low IV, expect move)
    ShortStraddle,     // Sell ATM CE + PE (high IV, expect range)
    BullCallSpread,    // Buy ITM CE, sell OTM CE (bullish)
    BearPutSpread,     // Buy ITM PE, sell OTM PE (bearish)
    IronCondor,        // Sell CE/PE spread (high IV, range-bound)
    CalendarSpread,    // Sell near expiry, buy far expiry (theta play)
    DeltaNeutral,      // CE + PE combo, net delta ≈ 0
    RatioSpread        // Unequal legs (gamma/vega play)
};

struct StrategyRecommendation {
    Strategy strategy;
    QString name;
    QString description;
    
    // Legs
    struct Leg {
        uint32_t token;
        QString symbol;
        double strike;
        bool isCall;
        int quantity;
        QString action;  // "BUY" or "SELL"
    };
    QVector<Leg> legs;
    
    // Expected Greeks
    double netDelta;
    double netGamma;
    double netVega;
    double netTheta;
    
    // P&L expectation
    double maxProfit;
    double maxLoss;
    double breakeven;
    
    // Reasoning
    QString rationale;
    double confidenceScore;  // 0-100
};

class StrategyRecommender {
public:
    /**
     * @brief Recommend strategies based on market conditions
     */
    static QVector<StrategyRecommendation> recommendStrategies(
        const QString& symbol,
        const QString& expiry
    );
    
private:
    static double getIVRegime();  // Low/Medium/High IV environment
    static double getTrendStrength();  // Bullish/Neutral/Bearish
    static double getRangeExpectation();  // Expected move
};
```

**Example Recommendation:**

```
┌────────────────────────────────────────────────────────────┐
│ Strategy: BULL CALL SPREAD                                 │
├────────────────────────────────────────────────────────────┤
│ Rationale:                                                 │
│ - NIFTY trend: Bullish (RSI 65, above 20 EMA)            │
│ - IV Regime: Medium (45th percentile)                     │
│ - Expected move: +2% (±100 points)                        │
│                                                            │
│ Setup:                                                     │
│ - BUY  1 NIFTY 24000 CE @ ₹180 (Delta 0.52)             │
│ - SELL 1 NIFTY 24500 CE @ ₹60  (Delta 0.25)             │
│                                                            │
│ Greeks:                                                    │
│ - Net Delta: +0.27 (moderately bullish)                   │
│ - Net Theta: -₹5/day (low decay)                         │
│ - Net Vega: +12 (slight long volatility)                  │
│                                                            │
│ Risk/Reward:                                               │
│ - Max Profit: ₹380 (if NIFTY > 24500)                    │
│ - Max Loss: ₹120 (if NIFTY < 24000)                      │
│ - Breakeven: 24,120                                        │
│ - Risk/Reward: 1:3.17                                      │
│                                                            │
│ Confidence: 78/100 (HIGH)                                  │
└────────────────────────────────────────────────────────────┘
```

### 8.4 Greeks Backtester

**Test Greeks-based strategies on historical data**

```cpp
class GreeksBacktester {
public:
    struct BacktestConfig {
        QDate startDate;
        QDate endDate;
        QString symbol;
        Strategy strategy;
        double initialCapital;
    };
    
    struct BacktestResult {
        double totalReturn;
        double sharpeRatio;
        double maxDrawdown;
        int totalTrades;
        int winRate;
        QVector<double> equity_curve;
        QString summary;
    };
    
    /**
     * @brief Run backtest on historical Greeks data
     */
    static BacktestResult runBacktest(const BacktestConfig& config);
};
```

### 8.5 Phase 8 Deliverables

**Code Artifacts:**
- ✅ `GreeksScanner.h/cpp` (400 lines)
- ✅ `SmartAlertsManager.h/cpp` (350 lines)
- ✅ `StrategyRecommender.h/cpp` (600 lines)
- ✅ `GreeksBacktester.h/cpp` (500 lines)

**Success Metrics:**
- ✅ Scanner finds opportunities in <1 second
- ✅ Alerts trigger within 500ms of condition met
- ✅ Strategy recommendations 70%+ accuracy
- ✅ Backtest runs at 100x+ real-time speed

---

## Architecture Evolution

### System Architecture (Post Phase 4-8)

```
┌───────────────────────────────────────────────────────────────┐
│                    ATM WATCH WINDOW (UI)                      │
│  ┌─────────────────────────────────────────────────────────┐  │
│  │ Call Table | Symbol Table | Put Table                   │  │
│  │ LTP IV Δ Γ | Name Price   | LTP IV Δ Γ                  │  │
│  │        θ ν  | ATM  Expiry  |        θ ν                  │  │
│  └─────────────────────────────────────────────────────────┘  │
│  ┌─────────────────────────────────────────────────────────┐  │
│  │ Charts: IV Evolution | Volatility Surface | Heatmaps    │  │
│  └─────────────────────────────────────────────────────────┘  │
└──────────────────────┬────────────────────────────────────────┘
                       │
        ┌──────────────┴──────────────┬──────────────┬──────────────┐
        ↓                             ↓              ↓              ↓
┌─────────────────┐  ┌─────────────────────┐  ┌─────────────┐  ┌─────────────┐
│ ATM Watch Mgr   │  │ Greeks Calc Service │  │ Portfolio   │  │ Historical  │
│ (P1-P3)         │  │ (P4)                │  │ Greeks Mgr  │  │ Data Coll.  │
│                 │  │                     │  │ (P5)        │  │ (P6)        │
│ • Event-driven  │  │ • IV Calculator     │  │             │  │             │
│ • Incremental   │  │ • Greeks calc       │  │ • Portfolio │  │ • ATM hist  │
│ • Threshold     │  │ • Caching           │  │   summary   │  │ • IV hist   │
│ • Strike range  │  │ • Throttling        │  │ • Risk      │  │ • Snapshots │
└─────────────────┘  └─────────────────────┘  │   alerts    │  └─────────────┘
        │                     │                │ • Position  │         │
        │                     │                │   sizing    │         │
        └──────────┬──────────┴────────────────┴─────────────┴─────────┘
                   │
        ┌──────────┴──────────────────────────────────────┐
        ↓                                                  ↓
┌──────────────────────┐                     ┌──────────────────────┐
│ Repository Manager    │                     │ UDP Broadcast Svc    │
│ (Data Layer)          │                     │ (Real-Time Data)     │
│                       │                     │                      │
│ • Expiry cache (O(1)) │                     │ • Price updates      │
│ • Strike lookups      │                     │ • Greeks trigger     │
│ • Token resolution    │                     │                      │
└──────────────────────┘                     └──────────────────────┘
```

### Performance Targets

| Component | Metric | Target | Stretch |
|-----------|--------|--------|---------|
| **IV Calculation** | Latency | <10 µs | <5 µs |
| **Greeks Calculation** | Latency | <5 µs | <2 µs |
| **ATM Range (10 strikes)** | Latency | <10 ms | <5 ms |
| **Portfolio Aggregation** | Latency | <50 ms | <20 ms |
| **Historical Query** | Latency | <100 ms | <50 ms |
| **Scanner (full market)** | Latency | <1 s | <500 ms |
| **Chart Rendering** | Latency | <1 s | <500 ms |
| **CPU Overhead (idle)** | Usage | <5% | <2% |
| **CPU Overhead (active)** | Usage | <15% | <10% |
| **Memory Footprint** | Usage | <200 MB | <150 MB |
| **Database Size** | Per month | <100 MB | <50 MB |

---

## Testing Strategy

### Unit Tests (Phase 4-8)

| Phase | Component | Test Cases | Coverage Target |
|-------|-----------|------------|-----------------|
| P4 | IVCalculator | 50+ | >90% |
| P4 | GreeksCalculationService | 30+ | >85% |
| P5 | PortfolioGreeksManager | 25+ | >85% |
| P5 | PositionSizingCalculator | 20+ | >90% |
| P6 | HistoricalDataCollector | 15+ | >80% |
| P7 | Visualization widgets | 10+ | >70% |
| P8 | GreeksScanner | 20+ | >85% |
| P8 | StrategyRecommender | 15+ | >75% |

**Total:** 200+ unit tests

### Integration Tests

1. **End-to-End Greeks Flow** (P4):
   - UDP price → IV calculation → Greeks display
   - Expected: <100ms latency

2. **Portfolio Risk Update** (P5):
   - Add position → Greeks aggregate → Risk alert
   - Expected: <50ms latency

3. **Historical Data Pipeline** (P6):
   - ATM change → Record snapshot → Query → Chart
   - Expected: <500ms total

4. **Scanner Performance** (P8):
   - Scan 5000 options → Filter by criteria → Return results
   - Expected: <1 second

### Performance Tests

1. **Stress Test**: 10,000 Greeks calculations per second
2. **Memory Leak Test**: 8-hour continuous operation
3. **Database Scaling**: 1 year of historical data (>1GB)
4. **Concurrent Users**: 10 simultaneous ATM Watch instances

### Validation Tests

1. **Accuracy Validation**: Compare IV/Greeks against Bloomberg
   - Tolerance: ±0.5% for IV, ±2% for Greeks

2. **Convergence Test**: IV solver success rate
   - Target: >95% convergence

3. **Historical Accuracy**: Backtest P&L estimation
   - Tolerance: ±5% error

---

## Deployment Roadmap

### Phase 4: Greeks Integration (3 weeks)

**Week 1:** IV Solver
- Day 1-2: IVCalculator implementation
- Day 3-4: Unit tests + edge cases
- Day 5: Documentation + code review

**Week 2:** Greeks Service
- Day 1-2: GreeksCalculationService core
- Day 3: Caching + throttling
- Day 4: Integration with UDP/ATM Watch
- Day 5: Performance profiling

**Week 3:** UI Integration
- Day 1-2: Greeks columns + display
- Day 3: Color coding + tooltips
- Day 4: Configuration + settings
- Day 5: Testing + bug fixes

**Deliverables:**
- ✅ Real-time IV and Greeks in ATM Watch
- ✅ Sub-10ms latency for 10 strikes
- ✅ >95% IV convergence rate
- ✅ User documentation

---

### Phase 5: Risk Analytics (3 weeks)

**Week 1:** Portfolio Manager
- Day 1-3: PortfolioGreeksManager implementation
- Day 4-5: Position tracking + aggregation

**Week 2:** Portfolio Window
- Day 1-3: PortfolioGreeksWindow UI
- Day 4: Risk gauges + summary panel
- Day 5: What-if scenario calculator

**Week 3:** Risk Alerts
- Day 1-2: RiskAlertsManager implementation
- Day 3: Alert UI + notifications
- Day 4-5: Position sizing calculator

**Deliverables:**
- ✅ Portfolio Greeks aggregation
- ✅ Real-time risk monitoring
- ✅ Risk alerts system
- ✅ Position sizing tools

---

### Phase 6: Historical Analytics (4 weeks)

**Week 1:** Database Setup
- Day 1-2: Database schema design
- Day 3-4: HistoricalDataCollector implementation
- Day 5: Migration scripts + testing

**Week 2:** Data Recording
- Day 1-3: Continuous recording implementation
- Day 4-5: Query optimization

**Week 3:** IV Analytics
- Day 1-3: IV percentile calculator
- Day 4-5: IV history charts

**Week 4:** ATM Analytics
- Day 1-3: ATM movement tracking
- Day 4-5: ATM heatmap + dashboard

**Deliverables:**
- ✅ Historical data storage (SQLite)
- ✅ IV percentile tracking
- ✅ ATM movement history
- ✅ Historical charts

---

### Phase 7: Advanced Visualization (3 weeks)

**Week 1:** Volatility Surface
- Day 1-3: 3D surface implementation
- Day 4-5: Interactive controls + export

**Week 2:** Heatmaps
- Day 1-2: Greeks heatmap widget
- Day 3-4: ATM heatmap implementation
- Day 5: Color schemes + legends

**Week 3:** Settings Dialog & Option Chain Integration
- Day 1-2: ATM Watch Settings Dialog (strike range, threshold, alerts)
- Day 3: Sparkline delegate
- Day 4-5: Option Chain window integration (context menu, double-click, ATM sync)

**Deliverables:**
- ✅ Volatility surface viewer
- ✅ Greeks heatmaps
- ✅ Real-time sparklines
- ✅ Settings dialog (ATM+1 to +5 selection) - **NEW**
- ✅ Option Chain integration (launch from ATM Watch) - **NEW**
- ✅ Enhanced option chain view with ATM sync

---

### Phase 8: Trading Intelligence (3 weeks)

**Week 1:** Scanner
- Day 1-3: GreeksScanner implementation
- Day 4-5: Pre-defined scans + UI

**Week 2:** Smart Alerts
- Day 1-3: SmartAlertsManager
- Day 4-5: Alert templates + UI

**Week 3:** Strategy Recommender
- Day 1-4: StrategyRecommender implementation
- Day 5: Backtester + validation

**Deliverables:**
- ✅ Greeks-based scanner
- ✅ Smart alerts system
- ✅ Strategy recommendations
- ✅ Backtesting framework

---

## Summary & Next Steps

### Transformation Journey

**Current State (Post P1-P3):**
- ✅ Basic ATM tracking (8.5/10 production-ready)
- ✅ Event-driven architecture
- ✅ Excellent cache system
- ❌ No Greeks/IV calculation
- ❌ No risk analytics
- ❌ No historical tracking

**Future State (Post P4-P8):**
- ✅ Professional options analysis platform
- ✅ Real-time Greeks and IV
- ✅ Portfolio risk management
- ✅ Historical analytics + percentiles
- ✅ Advanced visualizations
- ✅ Trading intelligence (scanner, alerts, strategies)

**Rating Evolution:**
- Current: 8.5/10 (basic ATM watch)
- Post Phase 4: 9.0/10 (+ Greeks)
- Post Phase 5: 9.2/10 (+ risk analytics)
- Post Phase 6: 9.5/10 (+ historical data)
- Post Phase 7: 9.7/10 (+ visualizations)
- Post Phase 8: 9.8/10 (+ trading intelligence)

### Estimated Timeline

| Phase | Duration | Dependencies | Cumulative |
|-------|----------|--------------|------------|
| Phase 4: Greeks Integration | 3 weeks | P1-P3 | 3 weeks |
| Phase 5: Risk Analytics | 3 weeks | P4 | 6 weeks |
| Phase 6: Historical Analytics | 4 weeks | P4-P5 | 10 weeks |
| Phase 7: Advanced Visualization | 3 weeks | P6 | 13 weeks |
| Phase 8: Trading Intelligence | 3 weeks | P4-P7 | 16 weeks |

**Total:** 16 weeks (~4 months) for complete transformation

### Recommended Approach

**Option A: Sequential (Conservative)**
- Complete Phase 4 → Deploy → Phase 5 → Deploy → ...
- **Pros**: Lower risk, early value delivery
- **Cons**: Longer to full feature set (6 months)

**Option B: Parallel (Aggressive)**
- Phase 4 + Phase 6 (parallel, different teams)
- Phase 5 + Phase 7 (parallel)
- **Pros**: Faster completion (3 months)
- **Cons**: Higher complexity, resource intensive

**Option C: Hybrid (Recommended)**
- Phase 4 (critical path, 3 weeks)
- Phase 5 + Phase 6 (parallel, 4 weeks)
- Phase 7 + Phase 8 (parallel, 3 weeks)
- **Timeline**: 10 weeks (2.5 months)
- **Resource**: 2 developers

### Immediate Next Steps

1. **Review Core Features** (User Requested):
   - ✅ **1-Minute Calculation**: Already implemented (P2 - 60s timer + event-driven)
   - ✅ **ATM+1 to ATM+5 Selection**: Already implemented (P3 - strike range)
   - ✅ **Live Spot/Future Price**: Already implemented (P2 - real-time UDP)
   - ⚠️ **Settings Dialog**: Needs UI (Phase 7 - Week 3)
   - ⚠️ **Option Chain Launch**: Needs implementation (Phase 7 - Week 3)

2. **Week 1**: Start Phase 4 (IV Calculator)
3. **Review**: Architecture design approval
4. **Setup**: Testing framework + CI/CD
5. **Team**: Allocate 2 developers for 16 weeks
6. **Stakeholders**: Weekly demo + feedback sessions

### Quick Wins (Can Implement Now)

**Before Phase 4, these features can be added immediately:**

1. **Settings Dialog** (2-3 days):
   - Strike range selector (ATM+1 to ATM+10)
   - Timer interval adjustment
   - Threshold multiplier
   - Base price source (Cash/Future)
   - Alert preferences

2. **Option Chain Integration** (2-3 days):
   - Context menu: "Open Option Chain"
   - Double-click launches chain
   - ATM strike highlighting
   - Real-time sync with ATM changes

3. **Enhanced Status Bar** (1 day):
   - Show "ATM calculated 15s ago"
   - Display "Next update in 45s"
   - Show active settings (e.g., "ATM+5, Cash price, 60s timer")

**Estimated Time**: 1 week for all three quick wins
**Benefit**: Immediate user value before starting Phase 4

---

**Document Version**: 2.0  
**Last Updated**: February 8, 2026  
**Author**: GitHub Copilot (Claude Sonnet 4.5)  
**Status**: Strategic Roadmap - Ready for Implementation  
**Approval Required**: Development Lead, Architect, Product Owner

**Related Documents:**
- [ATM Watch Comprehensive Analysis](ATM_WATCH_COMPREHENSIVE_ANALYSIS.md)
- [IV and Greeks Implementation Analysis](../importsant_docs/IV_AND_GREEKS_IMPLEMENTATION_ANALYSIS.md)
- [P1-P3 Implementation Summary](ATM_WATCH_P1_IMPLEMENTATION.md)
- [Greeks Calculator](../../include/repository/Greeks.h)
