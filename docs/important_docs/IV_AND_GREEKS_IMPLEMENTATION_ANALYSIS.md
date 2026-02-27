# IV and Greeks Calculation Implementation Analysis

**Document Version:** 1.0  
**Date:** January 21, 2026  
**Author:** GitHub Copilot (Claude Sonnet 4.5)  
**Status:** Comprehensive Analysis & Design Specification

---

## Table of Contents

1. [Executive Summary](#executive-summary)
2. [Current State Analysis](#current-state-analysis)
3. [Mathematical Foundations](#mathematical-foundations)
4. [Architecture Design](#architecture-design)
5. [Implementation Strategy](#implementation-strategy)
6. [Performance Considerations](#performance-considerations)
7. [Integration Points](#integration-points)
8. [Testing & Validation](#testing--validation)
9. [Production Roadmap](#production-roadmap)
10. [Appendices](#appendices)

---

## Executive Summary

### Purpose
This document provides a comprehensive analysis and design specification for implementing **Implied Volatility (IV)** and **Options Greeks** calculations within the Trading Terminal C++ application. The system must handle real-time calculations for thousands of option contracts with sub-millisecond latency requirements.

### Key Findings

#### Current State
- ✅ **Basic Greeks Calculator exists** ([Greeks.h](include/repository/Greeks.h), [Greeks.cpp](src/repository/Greeks.cpp))
- ✅ **Black-Scholes implementation present** (Delta, Gamma, Theta, Vega, Rho)
- ✅ **Data structures support Greeks** ([ContractData.h](include/repository/ContractData.h) has IV/Delta/Gamma/Vega/Theta/Rho fields)
- ❌ **No IV calculation implementation** (critical gap)
- ❌ **Not integrated with live market data** (broadcast libraries isolated)
- ❌ **No automatic refresh mechanism** (time decay, price updates)
- ❌ **Missing input parameters management** (risk-free rate, dividend yield)

#### Critical Gaps
1. **Implied Volatility Solver:** Newton-Raphson or Brent's method needed
2. **Real-Time Integration:** Link UDP broadcast → Greeks calculation → UI updates
3. **Performance Optimization:** Batch processing for option chains
4. **Input Parameter Management:** Risk-free rate, dividend yields, calendar days
5. **Underlying Price Resolution:** Futures vs. Cash market price selection

### Recommendations

**Priority 1 (Immediate):**
- Implement Newton-Raphson IV solver with analytical vega
- Create GreeksCalculationService to orchestrate calculations
- Integrate with UdpBroadcastService for real-time updates
- Add configuration for risk-free rate and calculation parameters

**Priority 2 (Short-term):**
- Implement batch calculation for option chains
- Add caching mechanism with TTL for Greeks
- Create Greeks update throttling (avoid recalculating on every tick)
- Add calendar integration for accurate time-to-expiry

**Priority 3 (Medium-term):**
- Advanced IV interpolation across strikes (volatility smile/skew)
- Historical IV tracking and visualization
- Alternative pricing models (Binomial, Monte Carlo)
- Dividend adjustment support

---

## Current State Analysis

### Existing Infrastructure

#### 1. Data Structures

##### ContractData (Repository Layer)
Location: [include/repository/ContractData.h](include/repository/ContractData.h)

```cpp
struct ContractData {
    // ... existing fields ...
    
    // ===== MARGIN & GREEKS (F&O only) =====
    double spanMargin;              // SPAN margin
    double aelMargin;               // AEL margin
    double iv;                      // Implied volatility ⚠️ NOT CALCULATED
    double delta;                   // Delta (options) ⚠️ NOT CALCULATED
    double gamma;                   // Gamma (options) ⚠️ NOT CALCULATED
    double vega;                    // Vega (options) ⚠️ NOT CALCULATED
    double theta;                   // Theta (options) ⚠️ NOT CALCULATED
    double rho;                     // Rho (options) ⚠️ NOT CALCULATED
    
    // Contract specifications
    double strikePrice;             // Strike price
    QString optionType;             // CE/PE/XX
    QString expiryDate;             // DDMMMYYYY format
    int64_t assetToken;             // Underlying asset token
    double ltp;                     // Last traded price
};
```

**Status:** Structure ready, fields present but not populated.

##### UnifiedState (Broadcast Layer)
Location: [include/data/UnifiedPriceState.h](include/data/UnifiedPriceState.h)

```cpp
struct UnifiedState {
    // Market data
    double ltp;
    double open, high, low, close;
    uint64_t volume;
    int64_t openInterest;
    int64_t openInterestChange;
    double impliedVolatility;       // ⚠️ Field exists, not populated
    
    // Contract metadata
    double strikePrice;
    char optionType[3];             // CE/PE/XX
    char expiryDate[16];            // DDMMMYYYY
    int64_t assetToken;             // Underlying token
};
```

**Status:** Real-time structure, updated by UDP broadcast parsers.

#### 2. Greeks Calculator (Basic Implementation)

Location: [include/repository/Greeks.h](include/repository/Greeks.h), [src/repository/Greeks.cpp](src/repository/Greeks.cpp)

```cpp
struct OptionGreeks {
    double delta;
    double gamma;
    double theta;
    double vega;
    double rho;
    double price;  // Theoretical price from Black-Scholes
};

class GreeksCalculator {
public:
    static OptionGreeks calculate(
        double S,      // Spot price
        double K,      // Strike
        double T,      // Time to expiry (years)
        double r,      // Risk-free rate (decimal)
        double sigma,  // Volatility (decimal)
        bool isCall
    );
};
```

**Current Implementation Analysis:**
- ✅ Black-Scholes formula correctly implemented
- ✅ Normal CDF/PDF using `std::erfc` (fast, accurate)
- ✅ Greeks formulas mathematically correct
- ✅ Theta annualized to daily decay
- ✅ Vega scaled to 1% volatility change
- ❌ Requires **known volatility** (σ) - no IV solver
- ❌ No integration with live market data
- ❌ Hard-coded parameters (r = 0.05 in test code)
- ❌ No batch processing support

**Test Usage Example:**
Location: [src/ui/MainWindow.cpp](src/ui/MainWindow.cpp) lines 585-620

```cpp
// Hard-coded test example
double S = 18100.0;     // NIFTY spot
double K = 18000.0;     // Strike
double T = 7.0 / 365.0; // 7 days to expiry
double r = 0.05;        // 5% risk-free rate
double sigma = 0.15;    // 15% volatility (assumed)

OptionGreeks g = GreeksCalculator::calculate(S, K, T, r, sigma, true);
```

**Limitations:** This is a demonstration, not production-ready integration.

#### 3. Broadcast Libraries (Data Source)

##### NSE FO Price Store
Location: [lib/cpp_broacast_nsefo/include/nsefo_price_store.h](lib/cpp_broacast_nsefo/include/nsefo_price_store.h)

```cpp
class PriceStore {
    static constexpr uint32_t MIN_TOKEN = 35000;
    static constexpr uint32_t MAX_TOKEN = 250000;
    
    // Direct array access, O(1) lookup
    std::vector<UnifiedTokenState*> store_;
    
    const UnifiedTokenState* getUnifiedState(uint32_t token) const;
};
```

**Capabilities:**
- ✅ Real-time price updates from UDP multicast
- ✅ Zero-copy architecture for performance
- ✅ Thread-safe (shared_mutex)
- ✅ Stores contract metadata (strike, expiry, option type)
- ❌ No Greeks calculation hooks
- ❌ No IV calculation

##### NSE CM Price Store (Underlying Prices)
Location: [lib/cpp_broadcast_nsecm/include/nsecm_price_store.h](lib/cpp_broadcast_nsecm/include/nsecm_price_store.h)

Similar architecture for equity/cash market (underlying prices).

#### 4. UDP Broadcast Service (Real-Time Engine)

Location: [src/services/UdpBroadcastService.cpp](src/services/UdpBroadcastService.cpp)

```cpp
class UdpBroadcastService {
    // Receives UDP packets → Parses → Updates PriceStores → Emits signals
    
    void handleNseFoTouchline(const nsefo::TouchlineData& data);
    void handleNseFoDepth(const nsefo::MarketDepthData& data);
    void handleNseFoTicker(const nsefo::TickerData& data);
};
```

**Current Flow:**
```
UDP Multicast Packet
    ↓
MulticastReceiver::recv()
    ↓
Parse Message (7200/7208/7202/etc)
    ↓
Update PriceStore (ltp, depth, OI)
    ↓
Emit signal → TokenPublisher
    ↓
MarketWatchWindow updates UI
```

**Missing Step:** Greeks calculation not triggered anywhere in this flow.

---

## Mathematical Foundations

### Black-Scholes Model (European Options)

#### Assumptions
1. **European Exercise:** Can only be exercised at expiry (NSE/BSE options are European-style)
2. **No Dividends:** For index options (NIFTY, BANKNIFTY) - requires adjustment for stock options
3. **Log-Normal Distribution:** Underlying price follows geometric Brownian motion
4. **Constant Volatility:** Volatility remains constant over option life (simplification)
5. **Frictionless Market:** No transaction costs, taxes (in practice, add adjustments)

#### Call Option Price (C)

$$
C = S \cdot N(d_1) - K \cdot e^{-rT} \cdot N(d_2)
$$

#### Put Option Price (P)

$$
P = K \cdot e^{-rT} \cdot N(-d_2) - S \cdot N(-d_1)
$$

#### Intermediate Variables

$$
d_1 = \frac{\ln(S/K) + (r + \sigma^2/2) \cdot T}{\sigma \sqrt{T}}
$$

$$
d_2 = d_1 - \sigma \sqrt{T}
$$

Where:
- **S** = Spot price (underlying asset)
- **K** = Strike price
- **T** = Time to expiry (years)
- **r** = Risk-free interest rate (decimal)
- **σ** = Implied volatility (decimal)
- **N(x)** = Cumulative standard normal distribution

### The Greeks (Sensitivity Measures)

#### Delta (Δ) - Price Sensitivity

$$
\Delta_{\text{call}} = N(d_1)
$$

$$
\Delta_{\text{put}} = N(d_1) - 1
$$

**Interpretation:** Change in option price for ₹1 change in underlying.  
**Range:** Call [0, 1], Put [-1, 0]  
**Usage:** Hedge ratio, directional exposure

#### Gamma (Γ) - Delta Sensitivity

$$
\Gamma = \frac{N'(d_1)}{S \cdot \sigma \cdot \sqrt{T}}
$$

Where $N'(x)$ is the standard normal PDF.

**Interpretation:** Rate of change of Delta.  
**Usage:** Convexity risk, delta-hedging frequency

#### Vega (ν) - Volatility Sensitivity

$$
\nu = S \cdot \sqrt{T} \cdot N'(d_1)
$$

Typically scaled to 1% volatility change: $\nu / 100$

**Interpretation:** Change in option price for 1% change in IV.  
**Usage:** Volatility exposure, IV trading

#### Theta (Θ) - Time Decay

$$
\Theta_{\text{call}} = -\frac{S \cdot N'(d_1) \cdot \sigma}{2\sqrt{T}} - r \cdot K \cdot e^{-rT} \cdot N(d_2)
$$

$$
\Theta_{\text{put}} = -\frac{S \cdot N'(d_1) \cdot \sigma}{2\sqrt{T}} + r \cdot K \cdot e^{-rT} \cdot N(-d_2)
$$

Typically annualized to daily: $\Theta / 365$

**Interpretation:** Option value decay per day (all else equal).  
**Usage:** Time decay exposure, calendar spreads

#### Rho (ρ) - Interest Rate Sensitivity

$$
\rho_{\text{call}} = K \cdot T \cdot e^{-rT} \cdot N(d_2)
$$

$$
\rho_{\text{put}} = -K \cdot T \cdot e^{-rT} \cdot N(-d_2)
$$

**Interpretation:** Change in option price for 1% change in interest rate.  
**Usage:** Interest rate risk (less relevant in India)

### Implied Volatility (IV) Calculation

**Problem:** Given market price of option, solve for σ in Black-Scholes equation.

$$
\text{Market Price} = \text{BS}(S, K, T, r, \sigma, \text{type})
$$

Solve for σ (no closed-form solution → numerical methods required).

#### Newton-Raphson Method (Recommended)

**Algorithm:**
1. Start with initial guess: $\sigma_0$ (e.g., 0.20 = 20%)
2. Iterate:
   $$
   \sigma_{n+1} = \sigma_n - \frac{\text{BS}(\sigma_n) - \text{Market Price}}{\text{Vega}(\sigma_n)}
   $$
3. Stop when $|\sigma_{n+1} - \sigma_n| < \epsilon$ (e.g., $\epsilon = 10^{-6}$)

**Advantages:**
- Fast convergence (quadratic, typically 3-5 iterations)
- Analytical derivative (Vega) available in Black-Scholes
- Industry standard

**Implementation Considerations:**
```cpp
double calculateImpliedVolatility(
    double marketPrice,
    double S, double K, double T, double r,
    bool isCall,
    double initialGuess = 0.20,    // 20% starting IV
    double tolerance = 1e-6,       // Convergence threshold
    int maxIterations = 100
) {
    double sigma = initialGuess;
    
    for (int i = 0; i < maxIterations; ++i) {
        OptionGreeks greeks = GreeksCalculator::calculate(S, K, T, r, sigma, isCall);
        double priceDiff = greeks.price - marketPrice;
        
        // Check convergence
        if (std::abs(priceDiff) < tolerance) {
            return sigma;
        }
        
        // Avoid division by zero
        if (greeks.vega < 1e-10) {
            break;  // Vega too small, IV calculation unreliable
        }
        
        // Newton-Raphson update
        sigma = sigma - priceDiff / greeks.vega;
        
        // Clamp sigma to reasonable bounds [0.01, 5.0]
        sigma = std::max(0.01, std::min(5.0, sigma));
    }
    
    return -1.0;  // Convergence failed
}
```

**Edge Cases to Handle:**
- **Deep ITM/OTM:** Low vega → slow convergence
- **Near Expiry:** T → 0 → numerical instability
- **Zero Market Price:** Options trading at 0.05 (tick size)
- **Intrinsic Value:** Market price < intrinsic → negative time value

#### Alternative: Brent's Method
- More robust for difficult cases
- Slower than Newton-Raphson
- Use as fallback if Newton fails

---

## Architecture Design

### High-Level System Architecture

```
┌─────────────────────────────────────────────────────────────────────┐
│                      TRADING TERMINAL APPLICATION                    │
├─────────────────────────────────────────────────────────────────────┤
│                                                                       │
│  ┌────────────────┐         ┌──────────────────┐                   │
│  │ MarketWatch UI │◄────────┤  FeedHandler     │                   │
│  │ (QTableView)   │         │  (Subscription)  │                   │
│  └────────────────┘         └──────────────────┘                   │
│         ▲                            ▲                               │
│         │ Greeks                     │ Ticks                         │
│         │ Display                    │                               │
│         │                            │                               │
│  ┌──────┴─────────────────────────────┴────────────┐               │
│  │    GreeksCalculationService (NEW)               │               │
│  │  ┌──────────────────────────────────────────┐   │               │
│  │  │ • Triggered by price updates             │   │               │
│  │  │ • Resolves underlying price              │   │               │
│  │  │ • Calculates IV (Newton-Raphson)         │   │               │
│  │  │ • Calculates Greeks (Black-Scholes)      │   │               │
│  │  │ • Batch processing for option chains     │   │               │
│  │  │ • Caching with TTL                       │   │               │
│  │  │ • Throttling to avoid excessive calcs    │   │               │
│  │  └──────────────────────────────────────────┘   │               │
│  └──────────────────────┬───────────────────────────┘               │
│                         │                                            │
│                         ▼                                            │
│  ┌────────────────────────────────────────────────┐                │
│  │     IVCalculator & GreeksCalculator            │                │
│  │  ┌──────────────────────────────────────────┐  │                │
│  │  │ • Black-Scholes pricing                  │  │                │
│  │  │ • Greeks analytical formulas             │  │                │
│  │  │ • Newton-Raphson IV solver               │  │                │
│  │  │ • SIMD optimizations (future)            │  │                │
│  │  └──────────────────────────────────────────┘  │                │
│  └─────────────────────┬──────────────────────────┘                │
│                        │                                             │
│                        ▼ Read contract & market data                │
│  ┌────────────────────────────────────────────────────────┐        │
│  │         NSEFORepository & ContractData                 │        │
│  │  (Master file data: strikes, expiries, lot sizes)      │        │
│  └────────────────────────────────────────────────────────┘        │
│                                                                      │
└──────────────────────────────┬───────────────────────────────────┘
                               │
                               ▼
┌─────────────────────────────────────────────────────────────────────┐
│                   UDP BROADCAST LAYER (C++ Libraries)                │
├─────────────────────────────────────────────────────────────────────┤
│                                                                       │
│  ┌────────────────────────────────────────────────────────────┐    │
│  │  UdpBroadcastService                                       │    │
│  │  ┌──────────────────────────────────────────────────────┐  │    │
│  │  │ • Receives UDP multicast packets                     │  │    │
│  │  │ • Parses NSE FO/CM, BSE FO/CM messages              │  │    │
│  │  │ • Updates price stores                               │  │    │
│  │  │ • Emits signals to FeedHandler                       │  │    │
│  │  │ • ⚠️ NEW: Trigger GreeksCalculationService          │  │    │
│  │  └──────────────────────────────────────────────────────┘  │    │
│  └────────────────────────────────────────────────────────────┘    │
│                           │                                          │
│                           ▼                                          │
│  ┌─────────────────────────────────────────────────────────┐       │
│  │         nsefo::PriceStore (Unified Token State)         │       │
│  │  • ~90K tokens (35000-250000)                           │       │
│  │  • Zero-copy array, O(1) access                         │       │
│  │  • ltp, depth, OI, contract metadata                    │       │
│  │  • ⚠️ NEW: Add IV/Greeks fields or separate store?     │       │
│  └─────────────────────────────────────────────────────────┘       │
│                           │                                          │
│  ┌─────────────────────────────────────────────────────────┐       │
│  │         nsecm::PriceStore (Underlying Prices)           │       │
│  │  • Equity/index prices (NIFTY, BANKNIFTY, stocks)       │       │
│  │  • Required for spot price (S) in Black-Scholes         │       │
│  └─────────────────────────────────────────────────────────┘       │
│                                                                      │
└──────────────────────────────────────────────────────────────────┘
                               │
                               ▼
                    NSE/BSE Exchange UDP Feed
                    (Multicast 233.1.2.5:34331, etc.)
```

### Component Design

#### 1. IVCalculator Class (New)

**Purpose:** Dedicated IV calculation with Newton-Raphson solver.

**Location:** `include/repository/IVCalculator.h`, `src/repository/IVCalculator.cpp`

```cpp
class IVCalculator {
public:
    struct IVResult {
        double impliedVolatility;
        int iterations;
        bool converged;
        double finalError;
    };
    
    /**
     * @brief Calculate implied volatility from market price
     * 
     * @param marketPrice Market price of the option
     * @param S Spot price of underlying
     * @param K Strike price
     * @param T Time to expiry (years)
     * @param r Risk-free rate (decimal)
     * @param isCall True for call, false for put
     * @param initialGuess Starting volatility guess (default 0.20)
     * @param tolerance Convergence tolerance (default 1e-6)
     * @param maxIterations Maximum iterations (default 100)
     * @return IVResult with IV and convergence status
     */
    static IVResult calculate(
        double marketPrice,
        double S, double K, double T, double r,
        bool isCall,
        double initialGuess = 0.20,
        double tolerance = 1e-6,
        int maxIterations = 100
    );
    
    /**
     * @brief Calculate IV using Brent's method (fallback)
     * Slower but more robust for edge cases
     */
    static IVResult calculateBrent(
        double marketPrice,
        double S, double K, double T, double r,
        bool isCall
    );
    
    /**
     * @brief Validate if IV calculation is feasible
     * Checks: T > 0, price > intrinsic, vega > threshold
     */
    static bool isCalculable(
        double marketPrice,
        double S, double K, double T, double r,
        bool isCall
    );
    
private:
    static double intrinsicValue(double S, double K, bool isCall);
    static double initialGuessFromMoneyness(double S, double K, double T);
};
```

#### 2. GreeksCalculationService Class (New)

**Purpose:** Orchestrate IV and Greeks calculations, integrate with real-time data flow.

**Location:** `include/services/GreeksCalculationService.h`, `src/services/GreeksCalculationService.cpp`

```cpp
class GreeksCalculationService : public QObject {
    Q_OBJECT
    
public:
    explicit GreeksCalculationService(QObject* parent = nullptr);
    
    struct CalculationConfig {
        double riskFreeRate = 0.065;      // 6.5% for India (RBI repo rate)
        double dividendYield = 0.0;       // For stock options (future)
        bool autoRecalculate = true;       // Recalc on price updates
        int throttleMs = 1000;             // Min time between recalcs
        bool batchCalculation = true;      // Process option chains together
        int maxConcurrentTokens = 5000;    // Memory limit
    };
    
    /**
     * @brief Initialize the service with configuration
     */
    void initialize(const CalculationConfig& config);
    
    /**
     * @brief Calculate Greeks for a single option token
     * Called when option price updates
     */
    void calculateForToken(uint32_t optionToken, int exchangeSegment);
    
    /**
     * @brief Calculate Greeks for an entire option chain
     * More efficient than individual calculations
     */
    void calculateForChain(
        const QString& underlying,
        const QString& expiry,
        int exchangeSegment
    );
    
    /**
     * @brief Get cached Greeks result
     */
    std::optional<GreeksResult> getCachedGreeks(uint32_t token) const;
    
    /**
     * @brief Force recalculation (e.g., time tick, manual trigger)
     */
    void forceRecalculate();
    
signals:
    void greeksCalculated(uint32_t token, const GreeksResult& result);
    void chainCalculated(const QString& underlying, const QString& expiry);
    void calculationFailed(uint32_t token, const QString& reason);
    
public slots:
    void onPriceUpdate(uint32_t token, double ltp, int exchangeSegment);
    void onUnderlyingPriceUpdate(uint32_t underlyingToken, double ltp);
    void onTimeTick();  // Called every minute for time decay updates
    
private:
    struct TokenCache {
        GreeksResult result;
        int64_t lastCalculationTime;
        double lastPrice;
        double lastUnderlyingPrice;
    };
    
    // Resolve underlying price for an option
    double getUnderlyingPrice(const ContractData* option, int exchangeSegment);
    
    // Calculate time to expiry in years
    double calculateTimeToExpiry(const QString& expiryDate);
    
    // Determine if recalculation is needed
    bool shouldRecalculate(
        uint32_t token,
        double currentPrice,
        double currentUnderlyingPrice
    );
    
    // Batch processing helper
    void processBatch(const QVector<uint32_t>& tokens);
    
    CalculationConfig m_config;
    QHash<uint32_t, TokenCache> m_cache;
    QTimer* m_timeTickTimer;
    NSEFORepository* m_nsefoRepo;
    NSECMRepository* m_nsecmRepo;
    BSEFORepository* m_bsefoRepo;
};

struct GreeksResult {
    uint32_t token;
    double impliedVolatility;
    double delta;
    double gamma;
    double vega;
    double theta;
    double rho;
    double theoreticalPrice;
    
    // Metadata
    bool ivConverged;
    int ivIterations;
    int64_t calculationTimestamp;
};
```

#### 3. Integration with UdpBroadcastService

**Location:** [src/services/UdpBroadcastService.cpp](src/services/UdpBroadcastService.cpp)

**Modification:** Add hook to trigger Greeks calculation on price updates.

```cpp
void UdpBroadcastService::handleNseFoTouchline(const nsefo::TouchlineData& data) {
    // ... existing price store update ...
    
    // NEW: Trigger Greeks calculation for options
    if (data.instrumentType == 2) {  // Options only
        m_greeksService->calculateForToken(data.token, UDP::ExchangeSegment::NSEFO);
    }
}
```

#### 4. Configuration Management

**Location:** [configs/config.ini](configs/config.ini)

Add new section:

```ini
[GREEKS_CALCULATION]
# Risk-free rate (Indian Treasury Bill / RBI Repo Rate)
risk_free_rate = 0.065

# Dividend yield for stock options (0 for indices)
dividend_yield = 0.0

# Auto-calculate on price updates
auto_calculate = true

# Throttle (milliseconds between recalculations per token)
throttle_ms = 1000

# Batch calculation (process option chains together)
batch_calculation = true

# IV solver settings
iv_initial_guess = 0.20
iv_tolerance = 0.000001
iv_max_iterations = 100

# Time tick interval (seconds) for theta decay updates
time_tick_interval = 60

# Enable Greeks calculation (global on/off)
enabled = true
```

---

## Implementation Strategy

### Phase 1: Core IV & Greeks Engine (Week 1-2)

#### Tasks
1. **Create IVCalculator Class**
   - Implement Newton-Raphson solver
   - Add Brent's method fallback
   - Unit tests with known IV values
   - Edge case handling (deep ITM/OTM, near expiry)

2. **Extend GreeksCalculator**
   - Refactor to use struct inputs (avoid long parameter lists)
   - Add validation checks
   - Optimize math operations (consider SIMD)

3. **Create GreeksCalculationService**
   - Basic single-token calculation
   - Integration with repositories
   - Configuration management
   - Caching mechanism

4. **Unit Testing**
   - Black-Scholes pricing validation
   - IV calculation convergence tests
   - Greeks accuracy tests (compare against reference implementations)
   - Performance benchmarks

#### Deliverables
- ✅ IVCalculator.h/cpp
- ✅ Enhanced GreeksCalculator
- ✅ GreeksCalculationService.h/cpp
- ✅ Unit test suite (>90% coverage)

### Phase 2: Real-Time Integration (Week 3-4)

#### Tasks
1. **UDP Broadcast Integration**
   - Hook GreeksCalculationService into UdpBroadcastService
   - Add signal/slot connections
   - Implement throttling to avoid excessive calculations

2. **Underlying Price Resolution**
   - Implement logic to fetch underlying price from NSE CM store
   - Handle futures vs. cash market logic (use futures as underlying for options)
   - Cache underlying prices for option chains

3. **Time-to-Expiry Calculation**
   - Parse expiry date strings (DDMMMYYYY format)
   - Calculate actual trading days (exclude weekends/holidays)
   - Implement calendar integration (NSE/BSE holiday list)
   - Handle intraday time decay

4. **Batch Processing**
   - Implement option chain batch calculation
   - Optimize for strike ladder (30-50 strikes per underlying)
   - Parallel processing for independent chains

#### Deliverables
- ✅ UdpBroadcastService modifications
- ✅ Underlying price resolution logic
- ✅ Calendar/expiry calculation module
- ✅ Batch processing implementation
- ✅ Integration tests

### Phase 3: UI Integration & Visualization (Week 5)

#### Tasks
1. **MarketWatch Integration**
   - Add IV/Delta/Gamma/Vega/Theta columns
   - Connect GreeksCalculationService signals to MarketWatchModel
   - Implement color coding (positive/negative Greeks)
   - Add tooltips with calculation details

2. **Option Chain Window (Future Enhancement)**
   - Dedicated option chain view
   - Strike ladder with Greeks
   - Volatility smile visualization
   - Greeks heatmap

3. **Configuration UI**
   - Settings dialog for risk-free rate
   - Toggle auto-calculation on/off
   - Adjust throttling parameters

#### Deliverables
- ✅ MarketWatchModel updates
- ✅ UI column additions
- ✅ Settings dialog
- ✅ User documentation

### Phase 4: Optimization & Production Hardening (Week 6-7)

#### Tasks
1. **Performance Optimization**
   - Profile Greeks calculation overhead
   - Implement SIMD vectorization for batch processing
   - Optimize cache hit rates
   - Reduce memory allocations

2. **Error Handling**
   - Handle edge cases gracefully (expired options, zero prices)
   - Log failed IV convergence
   - Add health monitoring

3. **Testing**
   - Load testing (10K+ option tokens)
   - Latency measurements (target <100µs per calculation)
   - Accuracy validation against Bloomberg/Reuters

4. **Documentation**
   - API documentation
   - User guide
   - Troubleshooting guide

#### Deliverables
- ✅ Performance benchmarks
- ✅ Production-ready error handling
- ✅ Comprehensive documentation
- ✅ Deployment guide

---

## Performance Considerations

### Calculation Overhead

#### Single Option Greeks Calculation
**Estimated Time:** ~5-10 microseconds
- Black-Scholes: ~2 µs
- IV Newton-Raphson (3-5 iterations): ~3-5 µs
- Total: ~7 µs per option

#### Option Chain (50 strikes)
**Estimated Time:** ~350 µs (serial) → ~50 µs (SIMD batch)
- 25 Calls + 25 Puts
- Underlying price shared
- Time-to-expiry shared
- Risk-free rate shared

#### Full Market (5000 options)
**Estimated Time:** ~50 ms (serial) → ~5 ms (optimized)
- Acceptable for 1-second update intervals
- Critical: Throttle to avoid recalculating unchanged options

### Optimization Strategies

#### 1. Caching
```cpp
struct CacheEntry {
    double lastPrice;
    double lastUnderlyingPrice;
    int64_t lastCalculationTime;
    GreeksResult result;
};

bool shouldRecalculate(uint32_t token, double currentPrice, double currentUnderlying) {
    auto& cache = m_cache[token];
    
    // Time-based throttle (min 1 second between calcs)
    if (now() - cache.lastCalculationTime < 1000ms) {
        return false;
    }
    
    // Price change threshold (>0.1% change)
    double priceChange = std::abs(currentPrice - cache.lastPrice) / cache.lastPrice;
    double underlyingChange = std::abs(currentUnderlying - cache.lastUnderlyingPrice) / cache.lastUnderlyingPrice;
    
    if (priceChange < 0.001 && underlyingChange < 0.001) {
        return false;  // No significant change
    }
    
    return true;
}
```

#### 2. Batch Processing with SIMD
```cpp
// Process 4 options simultaneously using AVX2
void calculateBatchSIMD(
    const double* ltps,           // [ltp1, ltp2, ltp3, ltp4]
    const double* strikes,        // [K1, K2, K3, K4]
    double S,                     // Single underlying price
    double T, double r,           // Shared parameters
    GreeksResult* results         // Output array
) {
    // Load 4 option prices into SIMD register
    __m256d vLtp = _mm256_loadu_pd(ltps);
    __m256d vK = _mm256_loadu_pd(strikes);
    
    // Vectorized IV calculation (Newton-Raphson loop)
    // ... SIMD implementation ...
    
    // Store results
    _mm256_storeu_pd(&results[0].impliedVolatility, vIV);
}
```

**Expected Speedup:** 3-4x for batch processing.

#### 3. Selective Calculation
- **Priority 1:** ATM options (most liquid, frequently traded)
- **Priority 2:** Near-month expiries
- **Priority 3:** Deep ITM/OTM (less sensitive to price changes)

#### 4. Thread Pool for Option Chains
```cpp
QThreadPool* m_threadPool;

void calculateForChain(const QString& underlying, const QString& expiry) {
    auto contracts = m_nsefoRepo->getContractsBySymbolAndExpiry(underlying, expiry, 2);
    
    // Split into batches of 10 options each
    for (int i = 0; i < contracts.size(); i += 10) {
        auto batch = contracts.mid(i, 10);
        
        // Submit to thread pool
        QtConcurrent::run([this, batch]() {
            for (const auto& contract : batch) {
                this->calculateForToken(contract.exchangeInstrumentID, NSEFO);
            }
        });
    }
}
```

---

## Integration Points

### 1. UdpBroadcastService → GreeksCalculationService

**Trigger:** Option price update (message 7200/7208)

```cpp
void UdpBroadcastService::handleNseFoTouchline(const nsefo::TouchlineData& data) {
    // Update price store
    nsefo::g_nseFoPriceStore.updateTouchline(unifiedData);
    
    // Convert and emit tick
    UDP::MarketTick tick = convertNseFoTouchline(data);
    FeedHandler::instance()->emitTick(tick);
    
    // NEW: Trigger Greeks calculation
    if (data.instrumentType == 2 && m_greeksService) {
        m_greeksService->onPriceUpdate(data.token, data.ltp, UDP::ExchangeSegment::NSEFO);
    }
}
```

### 2. GreeksCalculationService → MarketWatchModel

**Signal/Slot Connection:**

```cpp
// In MarketWatchWindow::setupConnections()
connect(greeksService, &GreeksCalculationService::greeksCalculated,
        this, [this](uint32_t token, const GreeksResult& result) {
    int row = m_model->findScripByToken(token);
    if (row >= 0) {
        m_model->updateGreeks(row, result);
    }
});
```

### 3. Configuration → GreeksCalculationService

**Load from config.ini:**

```cpp
void GreeksCalculationService::loadConfiguration() {
    QSettings settings("configs/config.ini", QSettings::IniFormat);
    settings.beginGroup("GREEKS_CALCULATION");
    
    m_config.riskFreeRate = settings.value("risk_free_rate", 0.065).toDouble();
    m_config.dividendYield = settings.value("dividend_yield", 0.0).toDouble();
    m_config.autoRecalculate = settings.value("auto_calculate", true).toBool();
    m_config.throttleMs = settings.value("throttle_ms", 1000).toInt();
    m_config.batchCalculation = settings.value("batch_calculation", true).toBool();
    
    settings.endGroup();
}
```

### 4. NSEFORepository → Contract Metadata

**Fetch contract details for Greeks calculation:**

```cpp
double GreeksCalculationService::calculateTimeToExpiry(const QString& expiryDate) {
    // Parse "27JAN2026" format
    QDate expiry = QDate::fromString(expiryDate, "ddMMMyyyy");
    QDate today = QDate::currentDate();
    
    // Calculate trading days (exclude weekends/holidays)
    int tradingDays = calculateTradingDays(today, expiry);
    
    // Convert to years (252 trading days per year in India)
    double T = tradingDays / 252.0;
    
    return std::max(T, 0.0001);  // Avoid T=0 (use 1 day minimum)
}
```

---

## Testing & Validation

### Unit Tests

#### 1. Black-Scholes Pricing
```cpp
TEST(GreeksCalculatorTest, CallPricingAccuracy) {
    // Known values from option pricing textbook
    double S = 100.0, K = 100.0, T = 1.0, r = 0.05, sigma = 0.20;
    
    OptionGreeks result = GreeksCalculator::calculate(S, K, T, r, sigma, true);
    
    // Expected call price: 10.45 (from reference)
    EXPECT_NEAR(result.price, 10.45, 0.01);
}

TEST(GreeksCalculatorTest, DeltaRange) {
    // Delta should be [0,1] for calls, [-1,0] for puts
    double S = 100.0, K = 100.0, T = 0.5, r = 0.05, sigma = 0.25;
    
    auto callGreeks = GreeksCalculator::calculate(S, K, T, r, sigma, true);
    auto putGreeks = GreeksCalculator::calculate(S, K, T, r, sigma, false);
    
    EXPECT_GE(callGreeks.delta, 0.0);
    EXPECT_LE(callGreeks.delta, 1.0);
    EXPECT_GE(putGreeks.delta, -1.0);
    EXPECT_LE(putGreeks.delta, 0.0);
}
```

#### 2. IV Convergence
```cpp
TEST(IVCalculatorTest, NewtonRaphsonConvergence) {
    // Generate synthetic option price
    double S = 18000.0, K = 18000.0, T = 30.0/365.0, r = 0.06;
    double trueIV = 0.18;  // 18% volatility
    
    OptionGreeks syntheticOption = GreeksCalculator::calculate(S, K, T, r, trueIV, true);
    
    // Calculate IV from market price
    IVResult result = IVCalculator::calculate(
        syntheticOption.price, S, K, T, r, true
    );
    
    EXPECT_TRUE(result.converged);
    EXPECT_NEAR(result.impliedVolatility, trueIV, 0.0001);  // 0.01% tolerance
    EXPECT_LT(result.iterations, 10);  // Should converge quickly
}

TEST(IVCalculatorTest, EdgeCaseDeepITM) {
    // Deep ITM option: S=20000, K=15000
    double S = 20000.0, K = 15000.0, T = 30.0/365.0, r = 0.06;
    double marketPrice = 5050.0;  // Near intrinsic (5000)
    
    IVResult result = IVCalculator::calculate(marketPrice, S, K, T, r, true);
    
    // Should still converge (but might take more iterations)
    EXPECT_TRUE(result.converged);
    EXPECT_GT(result.impliedVolatility, 0.0);
}
```

#### 3. Time-to-Expiry Calculation
```cpp
TEST(TimeToExpiryTest, TradingDaysCalculation) {
    // Test: Monday to Friday (5 trading days)
    QDate start = QDate(2026, 1, 26);  // Monday
    QDate end = QDate(2026, 1, 30);    // Friday
    
    int days = calculateTradingDays(start, end);
    EXPECT_EQ(days, 5);
}

TEST(TimeToExpiryTest, ExcludeWeekends) {
    // Test: Monday to next Monday (5 trading days, skip Sat/Sun)
    QDate start = QDate(2026, 1, 26);
    QDate end = QDate(2026, 2, 2);
    
    int days = calculateTradingDays(start, end);
    EXPECT_EQ(days, 5);
}

TEST(TimeToExpiryTest, HandleHolidays) {
    // Test: Include NSE holidays (Republic Day, Holi, etc.)
    // Requires holiday calendar integration
}
```

### Integration Tests

#### 1. End-to-End Calculation Flow
```cpp
TEST_F(GreeksIntegrationTest, UdpToGreeksToUI) {
    // 1. Simulate UDP packet for NIFTY 24000 CE
    nsefo::TouchlineData touchline;
    touchline.token = 42000;
    touchline.ltp = 150.50;
    touchline.instrumentType = 2;  // Option
    
    // 2. Trigger UDP handler
    udpService->handleNseFoTouchline(touchline);
    
    // 3. Wait for Greeks calculation
    QSignalSpy spy(greeksService, &GreeksCalculationService::greeksCalculated);
    ASSERT_TRUE(spy.wait(1000));  // 1 second timeout
    
    // 4. Verify Greeks result
    auto args = spy.takeFirst();
    uint32_t token = args.at(0).toUInt();
    GreeksResult result = qvariant_cast<GreeksResult>(args.at(1));
    
    EXPECT_EQ(token, 42000);
    EXPECT_GT(result.impliedVolatility, 0.0);
    EXPECT_GT(result.delta, 0.0);
}
```

#### 2. Batch Processing Performance
```cpp
TEST_F(GreeksPerformanceTest, OptionChainCalculation) {
    // Load 50 NIFTY options (25 CE + 25 PE)
    auto contracts = nsefoRepo->getContractsBySymbolAndExpiry("NIFTY", "27FEB2026", 2);
    ASSERT_EQ(contracts.size(), 50);
    
    // Measure batch calculation time
    auto start = std::chrono::high_resolution_clock::now();
    
    greeksService->calculateForChain("NIFTY", "27FEB2026", NSEFO);
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    // Should complete in <500 microseconds
    EXPECT_LT(duration.count(), 500);
}
```

### Validation Against Reference Data

#### Bloomberg Terminal Comparison
- Sample 20 NIFTY options
- Calculate IV and Greeks in our system
- Compare against Bloomberg values
- Acceptable tolerance: ±0.5% for IV, ±2% for Greeks

#### NSE Official Data
- NSE publishes theoretical prices and IV for selected options
- Use as reference for validation
- Ensure consistent pricing with exchange methodology

---

## Production Roadmap

### Phase 1: MVP (Weeks 1-4)
**Goal:** Basic IV and Greeks calculation with manual triggers

**Deliverables:**
- ✅ IVCalculator with Newton-Raphson
- ✅ Enhanced GreeksCalculator
- ✅ Basic GreeksCalculationService
- ✅ Manual calculation trigger in UI
- ✅ Display IV/Delta/Gamma in MarketWatch
- ✅ Configuration management

**Acceptance Criteria:**
- User can manually calculate Greeks for selected options
- IV converges in <10 iterations for 95% of cases
- Greeks displayed correctly in UI
- No crashes or memory leaks

### Phase 2: Real-Time Integration (Weeks 5-7)
**Goal:** Automatic Greeks calculation on price updates

**Deliverables:**
- ✅ UDP broadcast integration
- ✅ Underlying price resolution
- ✅ Time-to-expiry calculation
- ✅ Throttling and caching
- ✅ Batch processing for option chains
- ✅ Performance optimization (target <1ms per calculation)

**Acceptance Criteria:**
- Greeks update within 1 second of price change
- System handles 5000+ active options
- CPU overhead <10% on 4-core machine
- No UI lag during calculations

### Phase 3: Advanced Features (Weeks 8-10)
**Goal:** Enhanced Greeks capabilities and visualizations

**Deliverables:**
- ✅ Volatility smile/skew visualization
- ✅ Historical IV tracking
- ✅ Greeks heatmap for option chains
- ✅ Custom Greeks calculations (user-defined params)
- ✅ Export Greeks data to CSV/Excel
- ✅ Greeks alerts (e.g., high vega, large gamma)

**Acceptance Criteria:**
- Option chain window fully functional
- Volatility smile accurately displayed
- Historical IV charts render smoothly
- Users can export Greeks data

### Phase 4: Production Hardening (Weeks 11-12)
**Goal:** Production-ready deployment with monitoring

**Deliverables:**
- ✅ Comprehensive error handling
- ✅ Health monitoring and logging
- ✅ Performance benchmarks and SLA
- ✅ User documentation
- ✅ Admin guide and troubleshooting
- ✅ Load testing results

**Acceptance Criteria:**
- 99.9% uptime for Greeks calculation service
- Error rate <0.1% (failed IV convergence)
- Latency P99 <100ms for single calculation
- Complete documentation published

---

## Appendices

### Appendix A: Risk-Free Rate Sources (India)

**Recommended Sources:**
1. **RBI Repo Rate:** Current benchmark (6.5% as of Jan 2026)
2. **91-Day Treasury Bill:** Short-term risk-free rate
3. **MIBOR (Mumbai Interbank Offered Rate):** Money market rate

**Configuration Strategy:**
- Use RBI repo rate as default
- Allow user override in config
- Update quarterly or when RBI changes rates

### Appendix B: Trading Days Calendar (NSE/BSE)

**NSE Holidays 2026:**
- January 26 (Republic Day)
- March 14 (Holi)
- March 30 (Good Friday)
- April 2 (Ram Navami)
- April 14 (Dr. Ambedkar Jayanti)
- May 1 (Maharashtra Day)
- August 15 (Independence Day)
- August 19 (Janmashtami)
- October 2 (Gandhi Jayanti)
- October 24 (Dussehra)
- November 1 (Diwali Balipratipada)
- November 12 (Diwali)
- November 13 (Diwali Laxmi Pujan)
- November 14 (Diwali Balipratipada)
- December 25 (Christmas)

**Implementation:**
```cpp
bool isNSETradingDay(const QDate& date) {
    // Weekend check
    if (date.dayOfWeek() == Qt::Saturday || date.dayOfWeek() == Qt::Sunday) {
        return false;
    }
    
    // Holiday list (load from config or database)
    static QSet<QDate> holidays = {
        QDate(2026, 1, 26),  // Republic Day
        QDate(2026, 3, 14),  // Holi
        // ... add all holidays ...
    };
    
    return !holidays.contains(date);
}

int calculateTradingDays(const QDate& start, const QDate& end) {
    int count = 0;
    QDate current = start;
    
    while (current <= end) {
        if (isNSETradingDay(current)) {
            count++;
        }
        current = current.addDays(1);
    }
    
    return count;
}
```

### Appendix C: Volatility Surface Modeling

**Future Enhancement:** Beyond single-IV calculation, implement full volatility surface.

**Approach:**
1. **Collect IV for all strikes and expiries**
2. **Fit parametric model:**
   - SVI (Stochastic Volatility Inspired): Common in equity markets
   - SABR: More complex, better fit for illiquid strikes
3. **Interpolate/Extrapolate:**
   - Use model to estimate IV for untradedstrikes
   - Apply no-arbitrage constraints

**Benefits:**
- More accurate pricing for illiquid options
- Consistent Greeks across strike ladder
- Identify arbitrage opportunities

### Appendix D: Performance Benchmarks

**Target Metrics:**

| Metric | Target | Stretch Goal |
|--------|--------|--------------|
| Single Option IV Calculation | <10 µs | <5 µs |
| Single Option Greeks | <5 µs | <2 µs |
| Option Chain (50 strikes) | <500 µs | <100 µs (SIMD) |
| Full Market (5000 options) | <50 ms | <10 ms |
| CPU Overhead (idle) | <5% | <2% |
| Memory Overhead | <100 MB | <50 MB |
| IV Convergence Rate | >98% | >99.5% |
| Convergence Iterations | <10 avg | <5 avg |

**Measurement Tools:**
- Google Benchmark library
- Intel VTune Profiler
- Valgrind (memory leaks)
- perf (Linux profiling)

### Appendix E: Alternative Pricing Models

**Beyond Black-Scholes:**

1. **Binomial Tree Model**
   - Handles American options (early exercise)
   - More flexible (discrete time steps)
   - Slower than Black-Scholes

2. **Monte Carlo Simulation**
   - Path-dependent options (Asian, Barrier)
   - Stochastic volatility models
   - Computationally expensive

3. **Heston Model**
   - Stochastic volatility
   - Better fit for volatility smile
   - Complex calibration

**Recommendation:** Start with Black-Scholes (European options on NSE/BSE), add Binomial if American options support needed.

### Appendix F: Error Handling Scenarios

| Error Condition | Detection | Handling Strategy |
|-----------------|-----------|-------------------|
| IV convergence failure | Iterations > maxIterations | Log warning, use last known IV, flag in UI |
| T ≤ 0 (expired option) | T calculation | Set Greeks to 0, display "EXPIRED" |
| Market price < intrinsic | Price validation | Log warning, attempt IV calculation anyway |
| Missing underlying price | Price store lookup | Skip calculation, log error |
| Zero LTP | Price validation | Skip calculation (option not traded) |
| Vega = 0 | Division check | Fallback to Brent's method |
| Extreme moneyness (S/K) | Ratio check | Use wider bounds for IV solver |
| Negative time value | Intrinsic comparison | Log warning, possible data error |

---

## Conclusion

Implementing IV and Greeks calculations is a critical enhancement for the Trading Terminal, transforming it from a basic price viewer to a professional-grade options trading platform. The proposed architecture leverages the existing real-time UDP broadcast infrastructure while maintaining performance and scalability.

**Key Success Factors:**
1. **Robust IV Solver:** Newton-Raphson with proper edge case handling
2. **Real-Time Integration:** Seamless connection to UDP data flow
3. **Performance Optimization:** Caching, throttling, batch processing
4. **User Experience:** Clear visualization, configurable parameters
5. **Production Readiness:** Comprehensive testing, monitoring, documentation

**Next Steps:**
1. Review and approve this design document
2. Begin Phase 1 implementation (IVCalculator)
3. Set up continuous integration for automated testing
4. Schedule weekly progress reviews

**Estimated Timeline:** 12 weeks to full production deployment

---

**Document Control:**
- **Last Updated:** January 21, 2026
- **Review Status:** Draft for Review
- **Approvers:** Development Lead, Architect, Product Owner

**Related Documents:**
- [Greeks.h Implementation](include/repository/Greeks.h)
- [ContractData Structure](include/repository/ContractData.h)
- [UDP Broadcast Architecture](docs/UDP_BROADCAST_ARCHITECTURE.md)
- [Performance Benchmarking Plan](docs/PERFORMANCE_BENCHMARKS.md)
