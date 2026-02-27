

-rejection handling 


# Universal Strategy Wireframe - Complete Design Specification

**Project:** Trading Terminal C++  
**Date:** February 17, 2026  
**Version:** 2.0  
**Status:** Design Specification  
**Purpose:** Define universal base strategy architecture to support 90%+ of trading strategies

---

## Table of Contents

1. [Introduction](#introduction)
2. [Architecture Philosophy](#architecture-philosophy)
3. [Component 1: Strategy Metadata](#component-1-strategy-metadata)
4. [Component 2: Market Context](#component-2-market-context)
5. [Component 3: Pre-Entry Scanning](#component-3-pre-entry-scanning)
6. [Component 4: Entry Logic](#component-4-entry-logic)
7. [Component 5: Runtime Position Management](#component-5-runtime-position-management)
8. [Component 6: Exit Logic](#component-6-exit-logic)
9. [Component 7: Risk Management](#component-7-risk-management)
10. [Component 8: Logging & Monitoring](#component-8-logging--monitoring)
11. [Component 9: Backtesting & Optimization](#component-9-backtesting--optimization)
12. [Component 10: Execution Details](#component-10-execution-details)
13. [Complete Code Structure](#complete-code-structure)
14. [Real-World Strategy Examples](#real-world-strategy-examples)
15. [Implementation Guidelines](#implementation-guidelines)

---

## Introduction

### What is a Universal Strategy Wireframe?

A **Universal Strategy Wireframe** is a comprehensive base architecture that provides all the building blocks needed to implement any trading strategy. Instead of writing custom code for each strategy, traders can configure pre-built components to create strategies ranging from simple RSI trades to complex multi-leg options strategies.

### Why Do We Need This?

#### Without Universal Wireframe:
```
New Strategy Request
        ↓
Write 500-1000 lines of C++ code
        ↓
Handle entry, exit, risk management manually
        ↓
Debug, test, recompile
        ↓
5-10 days per strategy
```

#### With Universal Wireframe:
```
New Strategy Request
        ↓
Configure pre-built components (JSON/Form)
        ↓
Set entry conditions, exit rules, risk params
        ↓
Validate, test, deploy
        ↓
15-30 minutes per strategy
```

### Design Goals

1. **Flexibility**: Support 90%+ of trading strategies without code changes
2. **Simplicity**: Non-programmers can create strategies via forms/JSON
3. **Safety**: Built-in risk management and validation
4. **Performance**: Execute conditions in <50ms per tick
5. **Scalability**: Support 100+ concurrent strategies
6. **Maintainability**: Single codebase, minimal technical debt

### Strategy Coverage

| Strategy Type | Coverage | How |
|---------------|----------|-----|
| **Equity Trading** | ✅ 100% | Single-symbol, indicators, conditions |
| **Futures Trading** | ✅ 100% | Trend following, reversal, spreads |
| **Options Trading** | ✅ 95% | Multi-leg, Greeks, strike selection |
| **Intraday/Scalping** | ✅ 100% | Time-based, pivot, VWAP |
| **Pairs Trading** | ✅ 100% | Multi-symbol, correlation |
| **Volatility Strategies** | ✅ 90% | VIX, straddles, IV-based |
| **Arbitrage** | ✅ 85% | Calendar, inter-exchange |
| **Custom Logic** | ⚠️ 70% | Script-based (Phase 3) |

---

## Architecture Philosophy

### Component-Based Design

The wireframe is divided into **10 independent components**, each handling a specific aspect of trading:

```
┌─────────────────────────────────────────────────────────────────┐
│                    UNIVERSAL STRATEGY BASE                       │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ┌────────────────┐  ┌────────────────┐  ┌────────────────┐   │
│  │  1. Metadata   │  │  2. Market     │  │  3. Pre-Entry  │   │
│  │  Who/What/Why  │  │  Context       │  │  Scanning      │   │
│  └────────────────┘  └────────────────┘  └────────────────┘   │
│                                                                  │
│  ┌────────────────┐  ┌────────────────┐  ┌────────────────┐   │
│  │  4. Entry      │  │  5. Position   │  │  6. Exit       │   │
│  │  Logic         │  │  Management    │  │  Logic         │   │
│  └────────────────┘  └────────────────┘  └────────────────┘   │
│                                                                  │
│  ┌────────────────┐  ┌────────────────┐  ┌────────────────┐   │
│  │  7. Risk       │  │  8. Logging    │  │  9. Backtest   │   │
│  │  Management    │  │  & Monitoring  │  │  & Optimize    │   │
│  └────────────────┘  └────────────────┘  └────────────────┘   │
│                                                                  │
│  ┌────────────────┐                                             │
│  │  10. Execution │                                             │
│  │  Details       │                                             │
│  └────────────────┘                                             │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### Lifecycle Flow

```
Strategy Initialization
        ↓
[1. Load Metadata] → Who created this? What type?
        ↓
[2. Setup Market Context] → What symbols, instruments?
        ↓
[3. Pre-Entry Scanning] → Is market suitable now?
        ↓
        ├─ No → Wait
        ↓
        Yes
        ↓
[4. Entry Logic] → Should we enter? When? How much?
        ↓
        ├─ No → Keep scanning
        ↓
        Yes → Place Entry Order
        ↓
[5. Position Management] → Monitor position
        ├─ Update stop loss (trailing)
        ├─ Check targets (partial exits)
        ├─ Adjust hedge
        ├─ Scale position (pyramid/average)
        ↓
[6. Exit Logic] → Should we exit?
        ├─ Stop loss hit → Exit
        ├─ Target reached → Exit
        ├─ Signal reversal → Exit
        ├─ Time limit → Exit
        ├─ Risk limit breach → Exit
        ↓
        Exit → Close Position
        ↓
[7. Risk Management] → Check portfolio limits
        ├─ Daily loss limit reached? → Kill switch
        ├─ Max positions reached? → Block new entries
        ↓
[8. Logging] → Record everything
        ↓
Strategy Complete
```

---

## Component 1: Strategy Metadata

### Purpose
Identify and describe the strategy for management, tracking, and categorization.

### Structure

```cpp
struct StrategyMetadata {
    // Basic identification
    QString strategyId;          // Unique ID: "RSI_REVERSAL_001"
    QString name;                // Human-readable: "RSI Reversal Strategy"
    QString description;         // "Buy when RSI < 30, Sell when RSI > 70"
    QString version;             // "1.0", "2.5", "3.0-beta"
    QString author;              // "John Doe", "Trading Team"
    QDateTime createdDate;       // When was this created?
    QDateTime lastModified;      // When was this last edited?
    
    // Classification
    QString category;            // See categories below
    QString direction;           // "Bullish", "Bearish", "Neutral"
    int complexityLevel;         // 1=Beginner, 2=Intermediate, 3=Advanced, 4=Expert
    QString timeframe;           // "1m", "5m", "15m", "1h", "1d"
    
    // Requirements
    QStringList requiredIndicators;     // ["RSI", "SMA", "MACD"]
    QStringList requiredDataSources;    // ["TICK", "CANDLE_5M", "DEPTH"]
    QStringList requiredServices;       // ["IndicatorService", "GreeksCalculator"]
    
    // Performance tracking
    double expectedWinRate;      // 0.65 = 65% win rate
    double expectedRiskReward;   // 2.5 = 1:2.5 risk-reward
    double maxDrawdownExpected;  // 0.15 = 15% drawdown
    
    // Usage permissions
    bool isPublic;               // Can others use this?
    bool allowModification;      // Can others modify this?
    QStringList tags;            // ["intraday", "options", "scalping"]
};
```

### Categories

| Category | Description | Examples |
|----------|-------------|----------|
| `Intraday` | Close before market close | Scalping, ORB, VWAP |
| `Positional` | Hold days to weeks | Swing, Breakout |
| `Options_Buying` | Long options | Call/Put buying |
| `Options_Selling` | Short options | Straddle sell, Iron Condor |
| `Futures_Directional` | Directional futures | Trend following, Momentum |
| `Pairs_Trading` | Multi-symbol neutral | HDFC-ICICI pairs |
| `Arbitrage` | Risk-free profits | Calendar spread, Inter-exchange |
| `Volatility` | VIX/IV based | Vol crush, VIX spike |
| `Algorithmic` | Fully automated | Mean reversion, Algo |

### Example Usage

```cpp
// RSI Intraday Strategy Metadata
StrategyMetadata rsiMeta {
    .strategyId = "RSI_INTRADAY_V1",
    .name = "RSI Oversold Reversal",
    .description = "Buy when RSI(14) < 30 on 5-min chart, exit when RSI > 70",
    .version = "1.0",
    .author = "Rajesh Kumar",
    .createdDate = QDateTime::currentDateTime(),
    .category = "Intraday",
    .direction = "Neutral",  // Can go long or short
    .complexityLevel = 1,    // Beginner level
    .timeframe = "5m",
    .requiredIndicators = {"RSI"},
    .requiredDataSources = {"CANDLE_5M"},
    .expectedWinRate = 0.60,
    .expectedRiskReward = 2.0,
    .isPublic = true,
    .tags = {"RSI", "Reversal", "Intraday"}
};
```

---

## Component 2: Market Context

### Purpose
Define **WHAT** to trade - symbols, instruments, multi-leg configurations.

### Key Features

1. **Single vs Multi-Symbol**: Support both simple and complex strategies
2. **Dynamic Strike Selection**: 7 methods for options strike selection
3. **Multi-Leg Support**: Iron Condor, Butterfly, Spreads
4. **Pairs Trading**: Correlation-based multi-symbol strategies

### Core Structure

```cpp
struct MarketContext {
    // ═══════════════════════════════════════════════════════
    // STRATEGY TYPE IDENTIFICATION
    // ═══════════════════════════════════════════════════════
    
    enum StrategyType {
        SINGLE_SYMBOL,        // Simple: 1 symbol, 1 direction
        MULTI_SYMBOL,         // Pairs, Long-Short, Correlation
        MULTI_LEG_OPTIONS,    // Iron Condor, Butterfly, Straddle
        CALENDAR_SPREAD,      // Same underlying, different expiry
        INTER_EXCHANGE_ARB,   // NSE vs BSE arbitrage
        BASKET_TRADING        // Multiple uncorrelated symbols
    };
    StrategyType type;
    
    bool isMultiSymbol;       // Quick check flag
    
    // ═══════════════════════════════════════════════════════
    // SYMBOL CONFIGURATIONS
    // ═══════════════════════════════════════════════════════
    
    // For single-symbol strategies (most common)
    SymbolConfig primarySymbol;
    
    // For multi-symbol strategies
    QVector<SymbolConfig> symbols;
    
    // Symbol relationships (for pairs trading)
    struct SymbolRelationship {
        QString symbol1Alias;    // "Stock1"
        QString symbol2Alias;    // "Stock2"
        double hedgeRatio;       // 1.42 for HDFC-ICICI
        double correlation;      // 0.85
        double spreadMean;       // Historical mean spread
        double spreadStdDev;     // Standard deviation
    };
    QVector<SymbolRelationship> relationships;
};
```

### Symbol Configuration

```cpp
struct SymbolConfig {
    // ═══════════════════════════════════════════════════════
    // BASIC INSTRUMENT DETAILS
    // ═══════════════════════════════════════════════════════
    
    QString symbol;           // "NIFTY", "BANKNIFTY", "RELIANCE"
    QString alias;            // "Stock1", "NiftyFut", "CallLeg1"
    
    int segment;              // 1=NSECM, 2=NSEFO, 11=BSECM, 12=BSEFO
    QString exchange;         // "NSE", "BSE", "MCX", "NCDEX"
    
    enum InstrumentType {
        EQUITY,               // Stock (cash market)
        FUTIDX,               // Index Futures
        FUTSTK,               // Stock Futures
        OPTIDX,               // Index Options
        OPTSTK,               // Stock Options
        COMMODITY_FUT,        // Commodity Futures
        COMMODITY_OPT,        // Commodity Options
        CURRENCY_FUT          // Currency Futures
    };
    InstrumentType instrumentType;
    
    // ═══════════════════════════════════════════════════════
    // POSITION DETAILS
    // ═══════════════════════════════════════════════════════
    
    enum Action {
        BUY,                  // Long position
        SELL,                 // Short position
        BUY_TO_CLOSE,         // Close short
        SELL_TO_CLOSE         // Close long
    };
    Action action;
    
    int quantity;             // Number of shares/lots
    double ratio;             // For ratio spreads (1.0, 0.5, 2.0)
    double weight;            // For portfolio strategies (0.2 = 20%)
    
    // ═══════════════════════════════════════════════════════
    // FUTURES-SPECIFIC
    // ═══════════════════════════════════════════════════════
    
    QDate futuresExpiry;      // Expiry date for futures
    bool autoRollover;        // Auto-rollover to next month?
    int rolloverDaysBefore;   // Rollover 5 days before expiry
    
    // ═══════════════════════════════════════════════════════
    // OPTIONS-SPECIFIC (ADVANCED STRIKE SELECTION)
    // ═══════════════════════════════════════════════════════
    
    enum OptionType {
        CALL,                 // Call option (CE)
        PUT                   // Put option (PE)
    };
    OptionType optionType;
    
    QDate optionExpiry;       // Expiry date for options
    
    // Strike selection (see detailed section below)
    StrikeSelection strikeSelection;
    
    // ═══════════════════════════════════════════════════════
    // ORDER PREFERENCES
    // ═══════════════════════════════════════════════════════
    
    enum ProductType {
        INTRADAY,             // MIS (Margin Intraday Square-off)
        DELIVERY,             // CNC (Cash and Carry)
        NORMAL                // NRML (Normal)
    };
    ProductType productType;
    
    enum OrderType {
        MARKET,
        LIMIT,
        STOP_LOSS,
        STOP_LOSS_MARKET
    };
    OrderType orderType;
    
    double limitPrice;        // For limit orders
    double triggerPrice;      // For stop-loss orders
};
```

### Strike Selection System (Enhanced)

```cpp
struct StrikeSelection {
    // ═══════════════════════════════════════════════════════
    // SELECTION METHOD
    // ═══════════════════════════════════════════════════════
    
    enum SelectionMethod {
        STATIC,              // Fixed strike (manual selection)
        ATM_RELATIVE,        // Relative to ATM (ATM+1, ATM-1)
        PRICE_BASED,         // Nearest to specific price
        DELTA_BASED,         // Strike with specific delta
        PREMIUM_BASED,       // Strike with specific premium
        PERCENTAGE_OTM,      // X% out of the money
        DYNAMIC_SPREAD       // Based on volatility/ATR
    };
    SelectionMethod method;
    
    // ═══════════════════════════════════════════════════════
    // METHOD 1: STATIC STRIKE SELECTION
    // ═══════════════════════════════════════════════════════
    // Use case: Manual selection, specific known level
    // Example: Always sell 24000 CE (strong resistance)
    
    double staticStrike;     // e.g., 24000, 48000, 18500
    
    // ═══════════════════════════════════════════════════════
    // METHOD 2: ATM RELATIVE SELECTION (MOST POPULAR)
    // ═══════════════════════════════════════════════════════
    // Use case: Dynamic strike based on current market
    // Example: Always sell ATM+2 strike (200 points above ATM)
    
    int atmOffset;           // 0=ATM, +1=ATM+1, -1=ATM-1, +2=ATM+2
    int strikeInterval;      // 50 for NIFTY, 100 for BANKNIFTY, 250 for NIFTY50
    
    enum ATMCalculation {
        SPOT_PRICE,          // Use spot/underlying price
        FUTURES_PRICE,       // Use current month futures price
        ROUNDED_SPOT,        // Round spot to nearest strike
        VWAP                 // Use VWAP as ATM reference
    };
    ATMCalculation atmBase;
    
    // Examples:
    // NIFTY at 24075
    // ATM = 24100 (rounded to nearest 50)
    // atmOffset = -2, strikeInterval = 50
    // Selected strike = 24100 - (2 × 50) = 24000 PE
    
    // ═══════════════════════════════════════════════════════
    // METHOD 3: PRICE-BASED SELECTION
    // ═══════════════════════════════════════════════════════
    // Use case: Find strike nearest to specific target price
    // Example: Want to sell strike closest to 24050
    
    double targetPrice;      // e.g., 24050
    bool roundUp;            // true = round up, false = round down
    
    // Example:
    // targetPrice = 24075, roundUp = true
    // Available strikes: 24000, 24050, 24100
    // Selected: 24100 (rounded up from 24075)
    
    // ═══════════════════════════════════════════════════════
    // METHOD 4: DELTA-BASED SELECTION (PROFESSIONAL)
    // ═══════════════════════════════════════════════════════
    // Use case: Options Greeks-based selection
    // Example: Sell 0.25 delta option (safer, far OTM)
    
    double targetDelta;      // 0.25, 0.30, 0.50, 0.75
    double deltaTolerance;   // ±0.05 acceptable range
    
    // Delta interpretation:
    // 0.50 = ATM (50% probability of expiring ITM)
    // 0.30 = Slightly OTM (30% probability ITM)
    // 0.25 = Far OTM (25% probability ITM) - safer for selling
    // 0.75 = ITM (75% probability ITM) - for buying
    
    // ═══════════════════════════════════════════════════════
    // METHOD 5: PREMIUM-BASED SELECTION
    // ═══════════════════════════════════════════════════════
    // Use case: Target specific premium collection/cost
    // Example: Sell option with 100-150 rupees premium
    
    double minPremium;       // Minimum premium to collect
    double maxPremium;       // Maximum premium willing to pay
    bool preferHigherPremium; // When multiple strikes qualify
    
    // Example (Option Selling):
    // minPremium = 100, maxPremium = 150
    // Scan all strikes, find those with premium 100-150
    // Select the one with maximum probability of profit
    
    // ═══════════════════════════════════════════════════════
    // METHOD 6: PERCENTAGE OTM SELECTION
    // ═══════════════════════════════════════════════════════
    // Use case: Select strike X% away from current price
    // Example: Sell 5% OTM put (safe distance)
    
    double percentageOTM;    // 5.0 = 5% out of money
    bool fromSpot;           // true = from spot, false = from ATM
    
    // Example:
    // NIFTY spot = 24000
    // percentageOTM = 5.0 (for PUT)
    // 5% below = 24000 × 0.95 = 22800
    // Nearest strike = 22800 PE
    
    // ═══════════════════════════════════════════════════════
    // METHOD 7: DYNAMIC SPREAD (VOLATILITY-BASED)
    // ═══════════════════════════════════════════════════════
    // Use case: Adjust strike width based on market volatility
    // Example: Iron Condor with wider wings in high volatility
    
    double volatilityMultiplier;  // Strike width = volatility × multiplier
    
    enum VolatilityMeasure {
        ATR,                 // Average True Range
        STDDEV,              // Standard Deviation
        IMPLIED_VOL,         // Implied Volatility (IV)
        HISTORICAL_VOL       // Realized Volatility
    };
    VolatilityMeasure volMeasure;
    
    bool useIVSkew;          // Consider IV skew in selection
    double ivSkewWeight;     // 0.0 to 1.0
    
    // Example (Iron Condor):
    // ATR = 200 points, multiplier = 1.5
    // Put strike = ATM - (200 × 1.5) = ATM - 300
    // Call strike = ATM + (200 × 1.5) = ATM + 300
    
    // ═══════════════════════════════════════════════════════
    // REAL-TIME ADJUSTMENT
    // ═══════════════════════════════════════════════════════
    
    bool adjustOnEntry;      // Recalculate strike at entry time
    bool adjustIntraday;     // Can adjust strike during day
    int maxAdjustments;      // Max adjustments per day (prevent churn)
    
    QTime adjustmentTimes[5]; // Specific times for adjustment
    int numAdjustmentTimes;
};
```

### Real-World Examples

#### Example 1: Simple NIFTY Futures (Single Symbol)

```cpp
MarketContext simpleContext {
    .type = SINGLE_SYMBOL,
    .isMultiSymbol = false,
    .primarySymbol = {
        .symbol = "NIFTY",
        .alias = "NiftyFut",
        .segment = 2,              // NSEFO
        .exchange = "NSE",
        .instrumentType = FUTIDX,
        .action = BUY,
        .quantity = 50,            // 1 lot
        .productType = INTRADAY,
        .orderType = MARKET
    }
};
```

#### Example 2: Pairs Trading (HDFC + ICICI)

```cpp
MarketContext pairsContext {
    .type = MULTI_SYMBOL,
    .isMultiSymbol = true,
    .symbols = {
        // Long HDFC
        {
            .symbol = "HDFCBANK",
            .alias = "Stock1",
            .segment = 2,
            .exchange = "NSE",
            .instrumentType = FUTSTK,
            .action = BUY,
            .quantity = 1250,
            .ratio = 1.0,
            .productType = NORMAL
        },
        // Short ICICI
        {
            .symbol = "ICICIBANK",
            .alias = "Stock2",
            .segment = 2,
            .exchange = "NSE",
            .instrumentType = FUTSTK,
            .action = SELL,
            .quantity = 1775,      // Adjusted for hedge ratio
            .ratio = 1.42,         // Hedge ratio
            .productType = NORMAL
        }
    },
    .relationships = {
        {
            .symbol1Alias = "Stock1",
            .symbol2Alias = "Stock2",
            .hedgeRatio = 1.42,
            .correlation = 0.85,
            .spreadMean = 17.5,
            .spreadStdDev = 8.2
        }
    }
};
```

#### Example 3: Iron Condor (Multi-Leg Options)

```cpp
MarketContext ironCondorContext {
    .type = MULTI_LEG_OPTIONS,
    .isMultiSymbol = false,
    .symbols = {
        // Leg 1: Sell Put
        {
            .symbol = "NIFTY",
            .alias = "SellPut",
            .segment = 2,
            .instrumentType = OPTIDX,
            .optionType = PUT,
            .action = SELL,
            .quantity = 50,
            .strikeSelection = {
                .method = ATM_RELATIVE,
                .atmOffset = -4,        // ATM - 200 points
                .strikeInterval = 50,
                .atmBase = SPOT_PRICE,
                .adjustOnEntry = true
            },
            .productType = NORMAL
        },
        // Leg 2: Buy Put (protection)
        {
            .symbol = "NIFTY",
            .alias = "BuyPut",
            .segment = 2,
            .instrumentType = OPTIDX,
            .optionType = PUT,
            .action = BUY,
            .quantity = 50,
            .strikeSelection = {
                .method = ATM_RELATIVE,
                .atmOffset = -6,        // ATM - 300 points
                .strikeInterval = 50,
                .atmBase = SPOT_PRICE
            },
            .productType = NORMAL
        },
        // Leg 3: Sell Call
        {
            .symbol = "NIFTY",
            .alias = "SellCall",
            .segment = 2,
            .instrumentType = OPTIDX,
            .optionType = CALL,
            .action = SELL,
            .quantity = 50,
            .strikeSelection = {
                .method = ATM_RELATIVE,
                .atmOffset = +4,        // ATM + 200 points
                .strikeInterval = 50,
                .atmBase = SPOT_PRICE
            },
            .productType = NORMAL
        },
        // Leg 4: Buy Call (protection)
        {
            .symbol = "NIFTY",
            .alias = "BuyCall",
            .segment = 2,
            .instrumentType = OPTIDX,
            .optionType = CALL,
            .action = BUY,
            .quantity = 50,
            .strikeSelection = {
                .method = ATM_RELATIVE,
                .atmOffset = +6,        // ATM + 300 points
                .strikeInterval = 50,
                .atmBase = SPOT_PRICE
            },
            .productType = NORMAL
        }
    }
};
```

#### Example 4: Delta-Based Option Selling

```cpp
SymbolConfig deltaSell {
    .symbol = "BANKNIFTY",
    .instrumentType = OPTIDX,
    .optionType = PUT,
    .action = SELL,
    .quantity = 25,
    .strikeSelection = {
        .method = DELTA_BASED,
        .targetDelta = 0.25,        // Sell 0.25 delta (far OTM)
        .deltaTolerance = 0.03,     // Accept 0.22-0.28 delta
        .adjustOnEntry = true       // Recalculate at entry
    }
};
```

#### Example 5: Premium Collection Strategy

```cpp
SymbolConfig premiumSell {
    .symbol = "NIFTY",
    .instrumentType = OPTIDX,
    .optionType = PUT,
    .action = SELL,
    .quantity = 50,
    .strikeSelection = {
        .method = PREMIUM_BASED,
        .minPremium = 80.0,         // At least ₹80
        .maxPremium = 120.0,        // Max ₹120
        .preferHigherPremium = true // Prefer higher if safe
    }
};
```

---

## Component 3: Pre-Entry Scanning

### Purpose
Determine if market conditions are **suitable** for strategy execution before looking for entry signals.

### Why Pre-Entry Scanning?

Without pre-entry scanning:
- ❌ Enter during news volatility (gets whipsawed)
- ❌ Trade during low liquidity (high slippage)
- ❌ Miss unfavorable market conditions

With pre-entry scanning:
- ✅ Only trade during favorable conditions
- ✅ Avoid high-impact events
- ✅ Ensure sufficient liquidity

### Structure

```cpp
struct ScanningRules {
    // ═══════════════════════════════════════════════════════
    // TIME-BASED FILTERS
    // ═══════════════════════════════════════════════════════
    
    bool enabled;                // Is scanning active?
    
    // Trading window
    QTime tradingStartTime;      // 9:15 AM
    QTime tradingEndTime;        // 3:15 PM
    
    // Days of week
    QVector<Qt::DayOfWeek> tradingDays;  // Mon-Fri
    
    // Avoid specific time ranges
    struct TimeExclusion {
        QTime startTime;
        QTime endTime;
        QString reason;          // "Opening volatility", "Lunch hour"
    };
    QVector<TimeExclusion> excludedTimeRanges;
    
    // Example exclusions:
    // 9:15-9:25 = Opening range volatility
    // 12:00-12:30 = Low liquidity lunch hour
    // 3:20-3:30 = Closing volatility
    
    // ═══════════════════════════════════════════════════════
    // MARKET CONDITION FILTERS
    // ═══════════════════════════════════════════════════════
    
    // Trend requirement
    bool requireTrendingMarket;
    enum TrendDirection {
        UPTREND,
        DOWNTREND,
        SIDEWAYS,
        ANY
    };
    TrendDirection requiredTrend;
    
    double minADX;               // Minimum ADX for trend strength
                                 // ADX > 25 = strong trend
                                 // ADX < 20 = weak trend/choppy
    
    // Volatility requirement
    bool requireHighVolatility;
    bool requireLowVolatility;
    
    double minVIX;               // Min India VIX (15 for vol strategies)
    double maxVIX;               // Max VIX (30 to avoid panic)
    
    double minATR;               // Minimum Average True Range
    double maxATR;               // Maximum ATR (avoid excessive moves)
    
    // Volume requirement
    double minVolumeRatio;       // Current / 20-day avg (>1.5 for breakout)
    double minAbsoluteVolume;    // Minimum absolute volume
    
    // Liquidity requirement (for options)
    double minOpenInterest;      // Min OI for options
    double minBidAskSpread;      // Max acceptable spread
    
    // ═══════════════════════════════════════════════════════
    // TECHNICAL FILTERS
    // ═══════════════════════════════════════════════════════
    
    // Price relative to moving averages
    struct TechnicalFilter {
        QString indicator;       // "SMA_50", "EMA_200"
        QString operator_;       // ">", "<", ">=", "<="
        double value;
    };
    QVector<TechnicalFilter> technicalFilters;
    
    // Example filters:
    // Price > SMA(200) = Only trade in long-term uptrend
    // RSI > 40 && RSI < 60 = Only trade in neutral RSI zone
    
    // ═══════════════════════════════════════════════════════
    // FUNDAMENTAL FILTERS (for stocks)
    // ═══════════════════════════════════════════════════════
    
    struct FundamentalFilter {
        QString metric;          // "PE_RATIO", "DEBT_EQUITY", "ROE"
        QString operator_;
        double value;
    };
    QVector<FundamentalFilter> fundamentalFilters;
    
    // Example filters:
    // P/E < 25 = Avoid overvalued stocks
    // Debt/Equity < 1.0 = Only trade low-debt companies
    
    // ═══════════════════════════════════════════════════════
    // EVENT-BASED FILTERS
    // ═══════════════════════════════════════════════════════
    
    // Avoid trading around events
    bool avoidEarningsDay;       // Don't trade on earnings announcement
    int daysBeforeEarnings;      // Avoid X days before
    int daysAfterEarnings;       // Avoid X days after
    
    bool avoidExpiryDay;         // Don't trade on F&O expiry
    bool avoidExpiryWeek;        // Avoid entire expiry week
    
    bool avoidDividendExDate;    // Avoid ex-dividend date
    
    bool avoidFOMC;              // Avoid Fed meetings
    bool avoidRBIPolicy;         // Avoid RBI policy days
    bool avoidBudget;            // Avoid budget day
    
    bool avoidHolidays;          // No trading before long weekends
    int daysBeforeHoliday;
    
    // Custom event calendar
    struct EventExclusion {
        QDate date;
        QString eventName;
        bool avoidEntireDuray;
    };
    QVector<EventExclusion> customEvents;
    
    // ═══════════════════════════════════════════════════════
    // MARKET-WIDE FILTERS
    // ═══════════════════════════════════════════════════════
    
    // NIFTY/BANKNIFTY conditions (for stock trading)
    bool requireIndexTrending;
    QString indexSymbol;         // "NIFTY", "BANKNIFTY"
    TrendDirection indexTrend;
    
    // Market breadth
    double minAdvanceDeclineRatio; // Advances / Declines > 1.5
    
    // ═══════════════════════════════════════════════════════
    // RE-SCAN SETTINGS
    // ═══════════════════════════════════════════════════════
    
    int rescanIntervalSeconds;   // Re-check conditions every X seconds
    int maxFailedScans;          // Give up after X failed scans
    bool alertOnScanStart;       // Alert when conditions met
};
```

### Example Scanning Rules

#### Example 1: Breakout Strategy Scanning

```cpp
ScanningRules breakoutScan {
    .enabled = true,
    .tradingStartTime = QTime(9, 30),   // Avoid first 15 min
    .tradingEndTime = QTime(15, 0),     // Stop entries by 3 PM
    .tradingDays = {Qt::Monday, Qt::Tuesday, Qt::Wednesday, 
                    Qt::Thursday, Qt::Friday},
    
    // Exclude volatile opening
    .excludedTimeRanges = {
        {QTime(9, 15), QTime(9, 30), "Opening volatility"}
    },
    
    // Need trending market
    .requireTrendingMarket = true,
    .minADX = 25.0,              // Strong trend required
    
    // Need volume surge
    .minVolumeRatio = 1.5,       // Volume 1.5x average
    
    // Technical filters
    .technicalFilters = {
        {"Price", ">", "SMA_200"},    // Above 200-day MA
    },
    
    // Avoid high-impact events
    .avoidEarningsDay = true,
    .avoidExpiryDay = false,     // Breakout works on expiry too
    .avoidRBIPolicy = true,      // Too unpredictable
    
    .rescanIntervalSeconds = 60  // Re-check every minute
};
```

#### Example 2: Option Selling Scanning

```cpp
ScanningRules optionSellScan {
    .enabled = true,
    .tradingStartTime = QTime(10, 0),   // Let market settle
    .tradingEndTime = QTime(14, 30),    // Close early
    
    // Need low volatility
    .requireLowVolatility = true,
    .maxVIX = 20.0,              // Don't sell in panic
    .maxATR = 150.0,             // NIFTY ATR < 150
    
    // Need sideways market (mean reversion)
    .requiredTrend = SIDEWAYS,
    .minADX = 0,
    .maxADX = 20,                // ADX < 20 = choppy
    
    // Avoid all major events
    .avoidEarningsDay = true,
    .avoidExpiryDay = true,      // Too risky on expiry
    .avoidRBIPolicy = true,
    .avoidBudget = true,
    .avoidFOMC = true,
    
    .rescanIntervalSeconds = 300 // Re-check every 5 minutes
};
```

---

## Component 4: Entry Logic

### Purpose
Determine **WHEN** to enter a trade and **HOW MUCH** position size.

### Key Decisions

1. **Entry Conditions**: Technical indicators, patterns, price action
2. **Entry Timing**: Immediate, limit order, confirmation wait
3. **Entry Price**: Market, limit, stop-loss
4. **Position Sizing**: Fixed, risk-based, volatility-based

### Structure

```cpp
struct EntryRules {
    // ═══════════════════════════════════════════════════════
    // ENTRY CONDITIONS (WHAT TRIGGERS ENTRY)
    // ═══════════════════════════════════════════════════════
    
    // Long entry conditions
    ConditionGroup longEntryConditions;
    
    // Short entry conditions
    ConditionGroup shortEntryConditions;
    
    // Can enter both long and short? (for neutral strategies)
    bool allowSimultaneousLongShort;
    
    // Re-entry after exit
    bool allowReEntry;
    int minMinutesBetweenEntries;   // Cooldown period
    int maxEntriesPerDay;
    
    // ═══════════════════════════════════════════════════════
    // ENTRY TIMING (WHEN TO PLACE ORDER)
    // ═══════════════════════════════════════════════════════
    
    enum EntryTiming {
        IMMEDIATE,           // Market order as soon as condition met
        LIMIT,               // Limit order at specific price
        STOP_LIMIT,          // Trigger above/below level
        END_OF_CANDLE,       // Wait for candle close confirmation
        PULLBACK,            // Wait for pullback to support
        CONFIRMATION,        // Wait X bars for confirmation
        RETEST              // Wait for retest of level
    };
    EntryTiming timingType;
    
    int confirmationBars;        // Wait this many bars
    double pullbackPercent;      // Enter at X% pullback
    
    // ═══════════════════════════════════════════════════════
    // ENTRY PRICE (AT WHAT PRICE)
    // ═══════════════════════════════════════════════════════
    
    enum EntryPrice {
        MARKET_PRICE,        // Best available price now
        BID_PRICE,           // At current bid
        ASK_PRICE,           // At current ask
        MID_PRICE,           // (Bid + Ask) / 2
        VWAP,                // Volume-weighted average price
        CUSTOM_OFFSET,       // Entry price + offset
        LIMIT_ORDER,         // Specific limit price
        SUPPORT_RESISTANCE   // Technical level
    };
    EntryPrice priceType;
    
    double priceOffset;          // +15 points, -20 points
    double limitPrice;           // For manual limit orders
    
    // Slippage tolerance
    double maxSlippagePercent;   // Max 0.1% slippage
    double maxSlippagePoints;    // Max 10 points slippage
    
    // ═══════════════════════════════════════════════════════
    // POSITION SIZING (HOW MUCH TO BUY/SELL)
    // ═══════════════════════════════════════════════════════
    
    PositionSizingRule sizingRule;
    
    // Maximum limits
    double maxPositionValue;     // Max ₹10 lakh per position
    int maxLots;                 // Max 5 lots
    int maxContracts;            // Max 250 contracts (options)
    
    // ═══════════════════════════════════════════════════════
    // MULTI-LEG ENTRY COORDINATION
    // ═══════════════════════════════════════════════════════
    
    enum MultiLegEntryMode {
        SIMULTANEOUS,        // All legs at once (spread order)
        SEQUENTIAL,          // One by one
        LEGGING_IN,          // Enter gradually
        WAIT_FOR_FILL        // Wait for each leg to fill
    };
    MultiLegEntryMode multiLegMode;
    
    int legEntryDelayMs;         // Delay between legs (ms)
    double maxLegSlippage;       // Max slippage per leg
    
    // What to do if partial fill?
    bool cancelUnfilledLegs;     // Cancel rest if one fails
    bool adjustQuantity;         // Reduce quantity to match fills
    
    // ═══════════════════════════════════════════════════════
    // ENTRY VALIDATION
    // ═══════════════════════════════════════════════════════
    
    // Pre-entry checks
    bool checkMarginBeforeEntry;
    bool checkPositionLimits;
    bool checkRiskLimits;
    bool checkCorrelation;       // Avoid highly correlated positions
    
    double maxCorrelationAllowed; // 0.8 = max 80% correlation
};
```

### Condition System

```cpp
struct ConditionGroup {
    enum LogicOperator {
        AND,                 // All conditions must be true
        OR,                  // At least one must be true
        XOR,                 // Exactly one must be true
        NAND,                // Not all true
        NOR                  // None true
    };
    LogicOperator logicOperator;
    
    // Simple conditions
    QVector<Condition> conditions;
    
    // Nested groups (for complex logic)
    QVector<ConditionGroup> nestedGroups;
    
    // Example: (RSI < 30 AND Volume > 2x) OR (MACD crossover)
};

struct Condition {
    enum ConditionType {
        INDICATOR,           // Technical indicator
        PRICE,               // Price action
        TIME,                // Time-based
        POSITION_COUNT,      // Number of positions
        PNL,                 // Profit/Loss
        CANDLE_PATTERN,      // Candlestick pattern
        ORDER_BOOK,          // Market depth
        CUSTOM_SCRIPT        // User-defined script
    };
    ConditionType type;
    
    // For indicator conditions
    QString indicator;       // "RSI", "MACD", "SMA_50"
    QString operator_;       // ">", "<", "==", "!=", ">=", "<=", "crosses_above", "crosses_below"
    QVariant value;          // 30, 70, "SMA_20", etc.
    QString field;           // "close", "high", "low", "open"
    
    // For time conditions
    QTime timeStart;
    QTime timeEnd;
    
    // For pattern conditions
    QString patternName;     // "HAMMER", "DOJI", "ENGULFING"
    
    // For order book conditions
    double bidAskRatio;
    double bidVolume;
    double askVolume;
};
```

### Position Sizing System

```cpp
struct PositionSizingRule {
    enum SizingMethod {
        FIXED_QUANTITY,      // Always buy X shares
        FIXED_CAPITAL,       // Always invest ₹X amount
        RISK_BASED,          // Risk X% of capital
        VOLATILITY_BASED,    // Adjust for volatility (ATR)
        KELLY_CRITERION,     // Kelly formula
        EQUAL_WEIGHT,        // Equal $ weight across all
        PYRAMIDING,          // Add to winners
        PORTFOLIO_PERCENT    // X% of total portfolio
    };
    SizingMethod method;
    
    // ─────────────────────────────────────────────────────
    // FIXED QUANTITY
    // ─────────────────────────────────────────────────────
    // Example: Always buy 50 shares
    double fixedQuantity;
    
    // ─────────────────────────────────────────────────────
    // FIXED CAPITAL
    // ─────────────────────────────────────────────────────
    // Example: Always invest ₹1 lakh worth
    double fixedCapital;
    
    // ─────────────────────────────────────────────────────
    // RISK-BASED SIZING (MOST PROFESSIONAL)
    // ─────────────────────────────────────────────────────
    // Position size = (Account * RiskPercent) / Stop Loss Distance
    // Example: Risk 1% of ₹10 lakh = ₹10,000
    //          Stop loss 50 points away
    //          Position size = 10000 / 50 = 200 shares
    
    double riskPercent;      // 0.01 = 1% of capital at risk
    double accountBalance;   // Total capital
    
    // ─────────────────────────────────────────────────────
    // VOLATILITY-BASED SIZING (ATR)
    // ─────────────────────────────────────────────────────
    // Adjust size based on volatility
    // Less volatile = bigger size
    // More volatile = smaller size
    
    double atrMultiplier;    // Position size = Capital / (ATR × Multiplier)
    int atrPeriod;           // 14-day ATR
    
    // ─────────────────────────────────────────────────────
    // KELLY CRITERION (ADVANCED)
    // ─────────────────────────────────────────────────────
    // Optimal sizing based on win rate and risk-reward
    // Kelly% = W - ((1-W) / R)
    // W = Win rate, R = Avg Win / Avg Loss
    
    double kellyFraction;    // 0.25 = use 25% of Kelly (safer)
    double winRate;          // Historical win rate
    double avgWin;           // Average winning trade
    double avgLoss;          // Average losing trade
    
    // ─────────────────────────────────────────────────────
    // PYRAMIDING (ADD TO WINNERS)
    // ─────────────────────────────────────────────────────
    
    bool allowPyramiding;
    int maxPyramidLevels;    // Max 3 additions
    double pyramidTrigger;   // Add after each 1% profit
    double pyramidSizeRatio; // Each addition = 0.5 × original
    
    // ─────────────────────────────────────────────────────
    // CONSTRAINTS
    // ─────────────────────────────────────────────────────
    
    int minQuantity;         // Minimum order size
    int maxQuantity;         // Maximum order size
    double maxPositionSize;  // Max position value (₹)
    int lotSize;             // Exchange lot size (50 for NIFTY)
    
    // Round to lot size
    bool roundToLotSize;     // Always trade in full lots
};
```

### Entry Examples

#### Example 1: Simple RSI Entry

```cpp
EntryRules rsiEntry {
    .longEntryConditions = {
        .logicOperator = AND,
        .conditions = {
            {
                .type = INDICATOR,
                .indicator = "RSI_14",
                .operator_ = "<",
                .value = 30.0
            },
            {
                .type = POSITION_COUNT,
                .operator_ = "==",
                .value = 0        // No existing positions
            },
            {
                .type = TIME,
                .timeStart = QTime(9, 30),
                .timeEnd = QTime(15, 0)
            }
        }
    },
    
    .timingType = END_OF_CANDLE,     // Wait for candle close
    .priceType = MARKET_PRICE,       // Market order
    
    .sizingRule = {
        .method = RISK_BASED,
        .riskPercent = 0.01,         // Risk 1% of capital
        .accountBalance = 1000000,   // ₹10 lakh
        .roundToLotSize = true
    },
    
    .maxEntriesPerDay = 3
};
```

#### Example 2: Breakout Entry with Confirmation

```cpp
EntryRules breakoutEntry {
    .longEntryConditions = {
        .logicOperator = AND,
        .conditions = {
            {
                .type = PRICE,
                .operator_ = "crosses_above",
                .value = "52_WEEK_HIGH"
            },
            {
                .type = INDICATOR,
                .indicator = "VOLUME",
                .operator_ = ">",
                .value = "2.0 * AVG_VOLUME_20"
            },
            {
                .type = INDICATOR,
                .indicator = "RSI_14",
                .operator_ = ">",
                .value = 50.0         // Not overbought
            }
        }
    },
    
    .timingType = CONFIRMATION,
    .confirmationBars = 2,            // Wait 2 candles
    
    .priceType = LIMIT_ORDER,
    .limitPrice = "BREAKOUT_HIGH + 5", // 5 points above breakout
    
    .sizingRule = {
        .method = FIXED_CAPITAL,
        .fixedCapital = 100000        // ₹1 lakh per trade
    },
    
    .maxSlippagePercent = 0.2         // Max 0.2% slippage
};
```

#### Example 3: Multi-Leg Iron Condor Entry

```cpp
EntryRules ironCondorEntry {
    .longEntryConditions = {
        .logicOperator = AND,
        .conditions = {
            {
                .type = INDICATOR,
                .indicator = "VIX",
                .operator_ = ">",
                .value = 18.0          // High IV for good premium
            },
            {
                .type = INDICATOR,
                .indicator = "ADX",
                .operator_ = "<",
                .value = 20.0          // Sideways market
            }
        }
    },
    
    .timingType = IMMEDIATE,
    .priceType = LIMIT_ORDER,
    
    // Multi-leg coordination
    .multiLegMode = SIMULTANEOUS,     // All legs at once
    .legEntryDelayMs = 0,
    .maxLegSlippage = 5.0,            // Max 5 points per leg
    .cancelUnfilledLegs = true,       // Cancel all if any fails
    
    .sizingRule = {
        .method = FIXED_QUANTITY,
        .fixedQuantity = 50            // 1 lot (all legs same)
    },
    
    .checkMarginBeforeEntry = true
};
```

---

## Component 5: Runtime Position Management

### Purpose
Manage open positions dynamically - adjust stops, take profits, scale positions, hedge risks.

This is the **most complex and critical** component, as it determines how the strategy behaves after entry.

### Structure

```cpp
struct PositionManagement {
    // ═══════════════════════════════════════════════════════
    // SUB-COMPONENTS
    // ═══════════════════════════════════════════════════════
    
    StopLossRule stopLoss;
    TargetRule target;
    TrailingStopRule trailingStop;
    ScalingRule scaling;
    TimeBasedRule timeRule;
    HedgeRule hedging;
    GreeksManagement greeksManagement;   // For options
    
    // ═══════════════════════════════════════════════════════
    // UPDATE FREQUENCY
    // ═══════════════════════════════════════════════════════
    
    enum UpdateFrequency {
        EVERY_TICK,          // Update on every tick (high CPU)
        EVERY_SECOND,        // Update every second
        EVERY_CANDLE,        // Update on candle close
        MANUAL               // Manual triggers only
    };
    UpdateFrequency updateFrequency;
};
```

### 5.1 Stop Loss Management

```cpp
struct StopLossRule {
    enum StopType {
        NONE,                // No stop loss (risky!)
        FIXED_POINTS,        // 50 points from entry
        FIXED_PERCENT,       // 2% from entry
        ATR_BASED,           // 2 × ATR from entry
        SUPPORT_RESISTANCE,  // Technical level
        TIME_BASED,          // Close after X minutes
        VOLATILITY_BASED,    // Based on IV changes
        CHANDELIER_STOP,     // Highest high - (ATR × multiplier)
        PARABOLIC_SAR        // SAR indicator
    };
    StopType type;
    
    // ─────────────────────────────────────────────────────
    // FIXED STOP LOSS
    // ─────────────────────────────────────────────────────
    
    double fixedPoints;      // 50 points
    double fixedPercent;     // 0.02 = 2%
    
    // ─────────────────────────────────────────────────────
    // ATR-BASED STOP LOSS (PROFESSIONAL)
    // ─────────────────────────────────────────────────────
    // Adjusts for volatility
    // Low volatility = tighter stop
    // High volatility = wider stop
    
    double atrMultiplier;    // 2.0 = 2 × ATR
    int atrPeriod;           // 14-day ATR
    
    // ─────────────────────────────────────────────────────
    // TIME-BASED STOP LOSS
    // ─────────────────────────────────────────────────────
    
    int timeMinutes;         // Close after 120 minutes
    
    // ─────────────────────────────────────────────────────
    // DYNAMIC ADJUSTMENT
    // ─────────────────────────────────────────────────────
    
    // Move to breakeven
    bool moveToBreakeven;
    double moveToBreakevenAfterProfit; // Move after 1% profit
    
    // Lock in profit
    bool lockProfit;
    double lockProfitAfterPercent;     // Lock 50% after 2% profit
    double lockProfitPercentage;       // Lock at 0.5% profit level
    
    // Tighten stop over time
    bool tightenOverTime;
    struct TightenRule {
        int afterMinutes;
        double newStopPercent;
    };
    QVector<TightenRule> tightenRules;
    
    // Example tightening:
    // After 30 minutes: 2% → 1.5%
    // After 60 minutes: 1.5% → 1%
    
    // ─────────────────────────────────────────────────────
    // HARD VS SOFT STOP
    // ─────────────────────────────────────────────────────
    
    enum StopMode {
        HARD_STOP,           // Exit immediately when hit
        SOFT_STOP,           // Wait for candle close
        MENTAL_STOP          // Alert only, manual exit
    };
    StopMode stopMode;
    
    // ─────────────────────────────────────────────────────
    // ORDER PLACEMENT
    // ─────────────────────────────────────────────────────
    
    bool placeStopLossOrder;     // Place SL order with broker
    bool useMarketOrder;         // Market vs SL order
    double stopLossSlippage;     // Max slippage tolerance
};
```

### 5.2 Target Management

```cpp
struct TargetRule {
    enum TargetType {
        NONE,                // No target (trailing only)
        FIXED_POINTS,        // 100 points profit
        FIXED_PERCENT,       // 2% profit
        RISK_REWARD_RATIO,   // 1:2, 1:3 (if risk 50, target 100)
        ATR_BASED,           // 3 × ATR
        FIBONACCI,           // 1.27, 1.618 extensions
        PREVIOUS_HIGH_LOW,   // Previous swing high/low
        MULTIPLE_TARGETS,    // Scale out at multiple levels
        RESISTANCE_LEVEL     // Technical resistance
    };
    TargetType type;
    
    // ─────────────────────────────────────────────────────
    // SINGLE TARGET
    // ─────────────────────────────────────────────────────
    
    double fixedPoints;      // 100 points
    double fixedPercent;     // 0.02 = 2%
    double riskRewardRatio;  // 2.5 = 1:2.5
    double atrMultiplier;    // 3.0 = 3 × ATR
    
    // ─────────────────────────────────────────────────────
    // MULTIPLE TARGETS (SCALE OUT)
    // ─────────────────────────────────────────────────────
    
    struct PartialTarget {
        double targetLevel;   // 1%, 2%, 3%
        double exitPercent;   // Exit 33%, 33%, 34%
        bool moveStopToBreakeven; // Move SL after partial exit?
    };
    QVector<PartialTarget> partialTargets;
    
    // Example scaling:
    // 1% profit: Exit 33% of position
    // 2% profit: Exit another 33%
    // 3% profit: Exit remaining 34%
    
    // ─────────────────────────────────────────────────────
    // TARGET ADJUSTMENT
    // ─────────────────────────────────────────────────────
    
    bool adjustTargetDynamically;
    double targetAdjustmentFactor;  // Adjust based on volatility
    
    // Re-entry after target
    bool allowReEntryAfterTarget;
    int reEntryCooldownMinutes;
};
```

### 5.3 Trailing Stop

```cpp
struct TrailingStopRule {
    bool enabled;
    
    enum TrailType {
        FIXED_POINTS,        // Trail by 20 points
        FIXED_PERCENT,       // Trail by 0.5%
        ATR_BASED,           // Trail by 1 × ATR
        PARABOLIC_SAR,       // SAR as trailing stop
        MOVING_AVERAGE,      // EMA as trailing stop
        HIGHEST_HIGH,        // Trail below highest high
        CHANDELIER_STOP,     // Highest high - (ATR × multiplier)
        SWING_LOW            // Below recent swing low
    };
    TrailType type;
    
    // ─────────────────────────────────────────────────────
    // ACTIVATION TRIGGER
    // ─────────────────────────────────────────────────────
    
    double triggerProfit;    // Activate after 1% profit
    bool activateImmediately; // Start trailing from entry
    
    // ─────────────────────────────────────────────────────
    // TRAIL AMOUNT
    // ─────────────────────────────────────────────────────
    
    double trailPoints;      // 20 points behind price
    double trailPercent;     // 0.5% behind price
    double atrMultiplier;    // 1.5 × ATR behind price
    
    // ─────────────────────────────────────────────────────
    // UPDATE FREQUENCY
    // ─────────────────────────────────────────────────────
    
    int trailFrequency;      // Update every X ticks/seconds
    
    enum TrailDirection {
        UPWARD_ONLY,         // Only move stop up (long)
        DOWNWARD_ONLY,       // Only move stop down (short)
        BOTH_DIRECTIONS      // Can move both ways (risky)
    };
    TrailDirection direction;
    
    // ─────────────────────────────────────────────────────
    // BREAKEVEN TRAIL
    // ─────────────────────────────────────────────────────
    
    bool trailToBreakeven;
    double breakEvenBuffer;  // Trail to entry + 5 points
};
```

### 5.4 Position Scaling

```cpp
struct ScalingRule {
    // ═══════════════════════════════════════════════════════
    // PYRAMIDING (ADD TO WINNERS)
    // ═══════════════════════════════════════════════════════
    
    bool allowPyramiding;
    int maxPyramidLevels;         // Max 3 additions
    double pyramidTriggerProfit;  // Add after each 1% profit
    double pyramidSizeRatio;      // 0.5 = half of original size
    
    // Pyramiding example:
    // Entry: 100 shares at ₹2,000
    // After 1% profit: Add 50 shares (100 × 0.5)
    // After 2% profit: Add 25 shares (50 × 0.5)
    // Total: 175 shares
    // Avg price improves with profit
    
    // Stop loss adjustment after pyramid
    bool moveStopAfterPyramid;
    double stopMovement;           // Move to breakeven or +0.5%
    
    // ═══════════════════════════════════════════════════════
    // AVERAGING (ADD TO LOSERS - RISKY!)
    // ═══════════════════════════════════════════════════════
    
    bool allowAveraging;
    int maxAverageLevels;          // Max 2 additions
    double averageTriggerLoss;     // Add after 2% loss
    double averageSizeRatio;       // 1.0 = same size
    
    // Averaging example:
    // Entry: 100 shares at ₹2,000 (₹2,00,000)
    // After 2% loss: Add 100 shares at ₹1,960 (₹1,96,000)
    // Avg price: ₹1,980 (breakeven closer)
    // Risk: Increased exposure if keeps falling
    
    double maxAverageLoss;         // Stop averaging after 5% total loss
    
    // ═══════════════════════════════════════════════════════
    // PARTIAL EXITS (REDUCE EXPOSURE)
    // ═══════════════════════════════════════════════════════
    
    bool allowPartialExit;
    
    struct PartialExitRule {
        double trigger;            // Exit 50% after 1.5% profit
        double exitPercent;        // Percentage to exit
        bool moveStopToBreakeven;  // Move SL after exit
    };
    QVector<PartialExitRule> partialExitRules;
};
```

### 5.5 Time-Based Management

```cpp
struct TimeBasedRule {
    // ═══════════════════════════════════════════════════════
    // MAXIMUM HOLD TIME
    // ═══════════════════════════════════════════════════════
    
    bool hasMaxHoldTime;
    int maxHoldMinutes;      // 120 minutes (2 hours)
    int maxHoldDays;         // 5 days (for swing trades)
    
    enum ExitBehavior {
        EXIT_AT_MARKET,      // Market order exit
        EXIT_AT_LIMIT,       // Try to exit at favorable price
        ALERT_ONLY           // Just alert, don't exit
    };
    ExitBehavior timeLimitBehavior;
    
    // ═══════════════════════════════════════════════════════
    // MARKET CLOSE EXIT (INTRADAY)
    // ═══════════════════════════════════════════════════════
    
    bool exitBeforeClose;
    QTime exitTime;          // 3:15 PM (15 min before close)
    bool forceExitAtClose;   // Exit no matter what
    
    // ═══════════════════════════════════════════════════════
    // EXPIRY MANAGEMENT (OPTIONS)
    // ═══════════════════════════════════════════════════════
    
    bool exitBeforeExpiry;
    int daysBeforeExpiry;    // Exit 2 days before expiry
    QTime expiryDayExitTime; // Exit at 2:00 PM on expiry day
    
    // ═══════════════════════════════════════════════════════
    // STAGNATION EXIT
    // ═══════════════════════════════════════════════════════
    
    bool exitOnStagnation;
    int stagnationMinutes;   // Exit if no 0.5% move in 30 min
    double stagnationThreshold; // Define "no movement" as < 0.5%
    
    // ═══════════════════════════════════════════════════════
    // WEEKEND EXIT
    // ═══════════════════════════════════════════════════════
    
    bool exitBeforeWeekend;
    QTime fridayExitTime;    // Exit at 3:00 PM on Friday
};
```

### 5.6 Hedge Management

```cpp
struct HedgeRule {
    bool useHedge;
    
    enum HedgeType {
        NONE,
        FUTURES_HEDGE,       // Hedge with opposite futures
        OPTIONS_HEDGE,       // Buy protective put/call
        DELTA_NEUTRAL,       // Maintain delta neutral (options)
        DYNAMIC_HEDGE,       // Adjust based on PnL
        PAIR_HEDGE           // Hedge with correlated


 instrument
    };
    HedgeType type;
    
    // ─────────────────────────────────────────────────────
    // HEDGE TRIGGER
    // ─────────────────────────────────────────────────────
    
    double hedgeTriggerLoss;     // Hedge after 3% loss
    double hedgeTriggerProfit;   // Hedge profits after 5% gain
    
    // ─────────────────────────────────────────────────────
    // HEDGE RATIO
    // ─────────────────────────────────────────────────────
    
    double hedgeRatio;           // 0.5 = 50% hedge, 1.0 = 100%
    
    // ─────────────────────────────────────────────────────
    // HEDGE REMOVAL
    // ─────────────────────────────────────────────────────
    
    bool removeHedgeOnProfit;
    double removeHedgeAtProfit;  // Remove hedge after 2% profit
    
    // ─────────────────────────────────────────────────────
    // DELTA HEDGING (OPTIONS)
    // ─────────────────────────────────────────────────────
    
    double targetDelta;          // Maintain delta near 0
    double deltaThreshold;       // Rehedge if delta > 0.2
    int rehedgeFrequency;        // Check every 5 minutes
};
```

### 5.7 Greeks Management (Options Only)

```cpp
struct GreeksManagement {
    bool enabled;
    
    // ═══════════════════════════════════════════════════════
    // DELTA MANAGEMENT
    // ═══════════════════════════════════════════════════════
    
    bool manageDelta;
    double targetDelta;          // 0.0 for delta neutral
    double deltaThreshold;       // Rebalance if |delta| > 0.2
    
    enum DeltaHedgeMethod {
        FUTURES,                 // Hedge with futures
        OPTIONS,                 // Hedge with opposite options
        BOTH                     // Use both
    };
    DeltaHedgeMethod deltaHedgeMethod;
    
    // ═══════════════════════════════════════════════════════
    // GAMMA MANAGEMENT
    // ═══════════════════════════════════════════════════════
    
    bool manageGamma;
    double maxGamma;             // Max gamma exposure
    bool reduceGammaNearExpiry;  // Reduce gamma risk near expiry
    
    // ═══════════════════════════════════════════════════════
    // THETA MANAGEMENT
    // ═══════════════════════════════════════════════════════
    
    bool manageTheta;
    double targetTheta;          // Target daily theta decay
    bool exitIfThetaTooLow;      // Exit if theta < threshold
    
    // ═══════════════════════════════════════════════════════
    // VEGA MANAGEMENT
    // ═══════════════════════════════════════════════════════
    
    bool manageVega;
    double maxVega;              // Max vega exposure
    
    // Exit on IV changes
    bool exitOnIVSpike;
    double ivSpikeThreshold;     // Exit if IV increases by 50%
    
    bool exitOnIVCrush;
    double ivCrushThreshold;     // Exit if IV decreases by 30%
    
    // ═══════════════════════════════════════════════════════
    // UPDATE FREQUENCY
    // ═══════════════════════════════════════════════════════
    
    int updateFrequencySeconds;  // Recalculate Greeks every 60s
};
```

---

## Component 6: Exit Logic

### Purpose
Define all conditions that trigger position closure.

### Structure

```cpp
struct ExitRules {
    // ═══════════════════════════════════════════════════════
    // EXIT TRIGGERS (MULTIPLE CAN BE ACTIVE)
    // ═══════════════════════════════════════════════════════
    
    bool exitOnStopLoss;
    bool exitOnTarget;
    bool exitOnTrailingStop;
    bool exitOnSignalReversal;
    bool exitOnTimeLimit;
    bool exitOnMarketClose;
    bool exitOnRiskLimit;
    bool exitOnVolatilitySpike;
    bool exitOnCorrelationBreak;    // For pairs trading
    
    // ═══════════════════════════════════════════════════════
    // SIGNAL REVERSAL EXIT
    // ═══════════════════════════════════════════════════════
    
    ConditionGroup reversalConditions;
    
    // Example: Exit long if:
    // - RSI crosses above 70 (overbought)
    // - MACD crosses below signal
    // - Price closes below 20-day EMA
    
    // ═══════════════════════════════════════════════════════
    // RISK LIMIT BREACH
    // ═══════════════════════════════════════════════════════
    
    double maxDailyLoss;         // ₹20,000
    double maxWeeklyLoss;        // ₹50,000
    double maxMonthlyLoss;       // ₹1,50,000
    
    int maxConsecutiveLosses;    // Exit if 5 losses in a row
    
    // ═══════════════════════════════════════════════════════
    // PORTFOLIO LIMITS
    // ═══════════════════════════════════════════════════════
    
    double maxDrawdownPercent;   // Exit if portfolio down 15%
    double maxPortfolioRisk;     // Exit if total risk > 10%
    
    // ═══════════════════════════════════════════════════════
    // EMERGENCY EXIT (KILL SWITCH)
    // ═══════════════════════════════════════════════════════
    
    bool enableKillSwitch;
    
    struct KillSwitchTrigger {
        enum TriggerType {
            MANUAL,              // User-activated
            LOSS_THRESHOLD,      // Specific loss amount
            VIX_SPIKE,           // VIX > 40
            MARKET_CRASH,        // Market down >3% in hour
            CONNECTION_LOSS,     // Lost connection to broker
            MARGIN_CALL          // Margin below threshold
        };
        TriggerType type;
        double threshold;
    };
    QVector<KillSwitchTrigger> killSwitchTriggers;
    
    // ═══════════════════════════════════════════════════════
    // EXIT EXECUTION
    // ═══════════════════════════════════════════════════════
    
    enum ExitOrderType {
        MARKET_ORDER,            // Exit immediately
        LIMIT_ORDER,             // Try to exit at better price
        STOP_LOSS_MARKET         // Stop-loss order
    };
    ExitOrderType exitOrderType;
    
    double exitLimitPrice;       // For limit orders
    int exitTimeoutSeconds;      // Cancel if not filled in X seconds
    
    // Multi-leg exit coordination
    enum MultiLegExitMode {
        SIMULTANEOUS,            // All legs at once
        SEQUENTIAL,              // One by one
        LEGGING_OUT,             // Exit gradually
        MARKET_MAKER_EXIT        // Close at bid/ask
    };
    MultiLegExitMode multiLegExitMode;
};
```

---

## Component 7: Risk Management

### Purpose
Portfolio-wide risk controls and limits.

### Structure

```cpp
struct RiskManagement {
    // ═══════════════════════════════════════════════════════
    // ACCOUNT LIMITS
    // ═══════════════════════════════════════════════════════
    
    double totalCapital;             // ₹10,00,000
    double maxCapitalPerStrategy;    // 20% per strategy
    double maxCapitalPerTrade;       // 2% per trade
    double reserveCapital;           // Keep 20% as reserve
    
    // ═══════════════════════════════════════════════════════
    // POSITION LIMITS
    // ═══════════════════════════════════════════════════════
    
    int maxPositionsPerStrategy;     // 3 positions max
    int maxPositionsTotal;           // 10 across all strategies
    int maxPositionsPerSymbol;       // 2 positions in same symbol
    
    // ═══════════════════════════════════════════════════════
    // LOSS LIMITS
    // ═══════════════════════════════════════════════════════
    
    double maxLossPerTrade;          // ₹5,000
    double maxDailyLoss;             // ₹20,000
    double maxWeeklyLoss;            // ₹50,000
   double maxMonthlyLoss;           // ₹1,50,000
    
    // Circuit breakers
    bool pauseTradingOnDailyLoss;
    bool pauseTradingOnWeeklyLoss;
    int resumeTradingAfterMinutes;   // Resume after 60 min
    
    // ═══════════════════════════════════════════════════════
    // WIN LIMITS (PSYCHOLOGY)
    // ═══════════════════════════════════════════════════════
    
    double dailyProfitTarget;        // ₹30,000
    bool stopAfterTargetReached;     // Lock profits for the day
    bool reduceRiskAfterTarget;      // Trade smaller after target
    
    // ═══════════════════════════════════════════════════════
    // TRADE FREQUENCY
    // ═══════════════════════════════════════════════════════
    
    int maxTradesPerDay;             // 10 trades max
    int maxTradesPerHour;            // 3 trades per hour
    int minMinutesBetweenTrades;     // 5 min cooldown
    int minMinutesAfterLoss;         // 10 min cooldown after loss
    
    // ═══════════════════════════════════════════════════════
    // LEVERAGE LIMITS
    // ═══════════════════════════════════════════════════════
    
    double maxLeverage;              // 5x max
    double marginUtilizationLimit;   // 80% of available margin
    double emergencyMarginLevel;     // Alert at 90%
    
    // ═══════════════════════════════════════════════════════
    // CORRELATION LIMITS
    // ═══════════════════════════════════════════════════════
    
    bool checkCorrelation;
    double maxCorrelationAllowed;    // 0.8 = max 80% correlation
    int maxCorrelatedPositions;      // Max 3 correlated positions
    
    // ═══════════════════════════════════════════════════════
    // VOLATILITY LIMITS
    // ═══════════════════════════════════════════════════════
    
    double maxVIXForEntry;           // Don't enter if VIX > 30
    double minVIXForVolatility;      // Need VIX > 15 for vol strategies
    double maxATRMultiplier;         // Max ATR-based position size
    
    // ═══════════════════════════════════════════════════════
    // DRAWDOWN MANAGEMENT
    // ═══════════════════════════════════════════════════════
    
    double maxDrawdownPercent;       // 20% max drawdown
    bool reducePositionOnDrawdown;   // Reduce size if underwater
    double drawdownRecoveryTarget;   // Recover 50% before normal size
    
    // ═══════════════════════════════════════════════════════
    // TRADE LOOP PREVENTION & ORDER CONTROL
    // ═══════════════════════════════════════════════════════
    
    // --- Turnover Limits (Prevent Runaway Trading) ---
    double maxFutureTurnover;        // ₹50,00,000 per day (buy+sell combined)
    double maxOptionPremiumTurnover; // ₹10,00,000 per day (premium paid+received)
    QString turnoverResetTime;       // "09:15" - reset daily turnover at market open
    bool haltOnTurnoverBreach;       // Stop all trading if limit breached
    
    // --- Order Count Limits ---
    int maxOrdersPerDay;             // 500 orders max per day
    int maxOrdersPerHour;            // 100 orders max per hour
    int maxOrdersPerMinute;          // 20 orders max per minute
    int maxOrdersPerSecond;          // 5 orders max per second (flash crash prevention)
    int maxOrdersPerSymbol;          // 50 orders per symbol per day
    
    // --- Order Rate Limiting ---
    int minMillisecondsBetweenOrders; // 100ms min gap between orders
    int maxConsecutiveOrders;        // 3 consecutive orders then mandatory 5-sec pause
    int consecutivePauseSeconds;     // 5 seconds pause after consecutive orders
    
    // --- Order Acknowledgment Requirements ---
    bool requireOrderAck;            // **CRITICAL**: No new orders until previous order acknowledged
    int orderAckTimeoutMs;           // 5000ms - timeout for order acknowledgment
    int maxPendingOrders;            // 10 - max orders awaiting acknowledgment
    bool haltOnAckTimeout;           // Halt trading if order ack times out
    bool haltOnPendingLimit;         // Halt if too many pending orders
    
    // --- Order Rejection Handling ---
    int maxConsecutiveRejections;    // 5 rejections then halt
    int rejectionCooldownMinutes;    // 10 min cooldown after rejection limit
    bool haltOnRMSRejection;         // Immediate halt on RMS rejection
    bool haltOnExchangeRejection;    // Immediate halt on exchange rejection
    QString[] criticalRejectionCodes; // {"RMS-MARGIN", "RMS-LIMIT", "EXCH-FREEZE"} - critical codes trigger immediate halt
    
    // --- Position Modification Limits (Prevent Loop Modifications) ---
    int maxModificationsPerOrder;    // 3 modifications per order
    int maxModificationsPerPosition; // 10 modifications per position lifetime
    int minSecondsBetweenModifications; // 5 sec between modifications
    
    // --- Re-Entry Prevention (Stop Loss Loops) ---
    int maxReEntryAttempts;          // 2 - max re-entries after stop loss
    int reEntryCooldownMinutes;      // 15 min cooldown after stop loss
    bool blockSameSymbolAfterSL;     // Block same symbol for rest of day after SL
    
    // --- Circuit Breaker Triggers ---
    double rapidDrawdownPercent;     // 2% loss in 5 min triggers halt
    int rapidDrawdownMinutes;        // 5 minutes window
    double volatilitySpikePercent;   // 10% price move in 1 min triggers pause
    int volatilitySpikeSeconds;      // 60 seconds window
    bool haltOnMarketCircuit;        // Halt if exchange circuit filters triggered
    
    // --- Error & Anomaly Detection ---
    int maxOrderErrorsPerMinute;     // 5 errors per minute triggers review
    int maxNetworkErrorsPerMinute;   // 3 network errors triggers pause
    bool haltOnRepeatedErrors;       // Halt if same error occurs 5 times
    bool haltOnAnomalousPnL;         // Halt if PnL swings > 5% in 1 minute
    
    // --- Order State Tracking ---
    bool trackPendingOrders;         // Track all pending orders
    bool trackFailedOrders;          // Track failed order history
    bool enforceOrderSequencing;     // Wait for order completion before next
    int maxRetryAttempts;            // 3 retries for failed orders
    int retryDelaySeconds;           // 2 seconds between retries
    
    // --- Capital Deployment Rate ---
    double maxCapitalPerMinute;      // ₹2,00,000 max capital deployment per minute
    double maxCapitalPerHour;        // ₹10,00,000 max capital per hour
    bool enforceGradualDeployment;   // Prevent dumping all capital at once
    
    // --- Kill Switch & Emergency Controls ---
    bool enableKillSwitch;           // Enable emergency kill switch
    QString killSwitchHotkey;        // "Ctrl+Shift+K" - emergency stop all
    bool killSwitchSquareOffAll;     // Square off all positions on kill switch
    int killSwitchSquareOffSeconds;  // 30 sec to liquidate all positions
    
    // --- Recovery & Resume Controls ---
    bool requireManualResume;        // Require manual intervention to resume after halt
    int autoResumeAfterMinutes;      // 0 = no auto resume, >0 = resume after N minutes
    bool notifyAdminOnHalt;          // Send alert to admin when trading halted
    QString adminContactEmail;       // Admin email for critical alerts
    QString adminContactPhone;       // Admin phone for SMS alerts
};
```

### Trade Loop Prevention - Rationale & Examples

**Why These Controls Matter:**

1. **Turnover Limits**: Prevent strategy bugs from burning through unlimited capital
   - Example: Bug causes buy-sell loop → hits ₹50L turnover → system halts → capital protected

2. **Order Acknowledgment**: Critical for preventing duplicate orders
   - Without: Send order → no ack → assume failed → send again → double position
   - With: Send order → wait for ack/timeout → then decide next action

3. **Order Rate Limiting**: Prevents accidental flash crashes
   - Example: Loop error sends 1000 orders/sec → market impact → losses
   - With limit: Max 5 orders/sec → abnormal behavior detected early

4. **Rejection Handling**: Prevents retry loops when RMS/Exchange rejects
   - Example: Insufficient margin → order rejected → strategy retries infinitely
   - With limit: 5 rejections → halt → investigate issue

5. **Circuit Breakers**: Detect abnormal market/strategy behavior
   - Example: 2% loss in 5 minutes (should take hours normally) → algo malfunction suspected → halt

**Practical Configuration Examples:**

```cpp
// Conservative Setup (Capital Protection Focus)
riskConfig.maxFutureTurnover = 25'00'000;        // ₹25L daily
riskConfig.maxOptionPremiumTurnover = 5'00'000;  // ₹5L daily
riskConfig.maxOrdersPerDay = 200;
riskConfig.requireOrderAck = true;               // **MANDATORY**
riskConfig.orderAckTimeoutMs = 3000;
riskConfig.maxPendingOrders = 5;
riskConfig.haltOnAckTimeout = true;
riskConfig.maxConsecutiveRejections = 3;
riskConfig.haltOnRMSRejection = true;            // Immediate halt
riskConfig.requireManualResume = true;           // No auto-resume

// Aggressive Setup (High Frequency Scalping)
riskConfig.maxFutureTurnover = 100'00'000;       // ₹1Cr daily
riskConfig.maxOptionPremiumTurnover = 20'00'000; // ₹20L daily
riskConfig.maxOrdersPerDay = 1000;
riskConfig.maxOrdersPerMinute = 50;
riskConfig.minMillisecondsBetweenOrders = 100;   // 100ms min gap
riskConfig.requireOrderAck = true;               // Still mandatory
riskConfig.maxPendingOrders = 20;
riskConfig.maxConsecutiveRejections = 10;
riskConfig.rapidDrawdownPercent = 1.0;           // Tighter control
riskConfig.autoResumeAfterMinutes = 5;           // Auto-resume quickly
```

**Order Acknowledgment Flow:**

```
┌─────────────────────────────────────────────────────────────┐
│ Strategy Engine Order Flow (With Acknowledgment Control)   │
└─────────────────────────────────────────────────────────────┘

1. Strategy signals: BUY NIFTY 24000 CE
   ↓
2. Check: requireOrderAck = true?
   ↓ YES
3. Check: Any pending orders without ack?
   ↓ NO (proceed) / YES (WAIT or REJECT)
   ↓
4. Send order to Broker API
   ↓
5. Add to pendingOrders queue {orderId: "ORD123", sentTime: now()}
   ↓
6. Start acknowledgment timer (orderAckTimeoutMs = 5000ms)
   ↓
7a. Receive ACK from RMS/Exchange within 5 sec?
    ↓ YES → Remove from pendingOrders → Allow next order
    ↓ NO  → Timeout → Log error → Halt if haltOnAckTimeout=true
    ↓
7b. Receive REJECTION from RMS/Exchange?
    ↓ YES → Increment rejectionCount → Check maxConsecutiveRejections
           → If critical code → IMMEDIATE HALT
           → Else continue with cooldown
```

**Additional Recommendations:**

Beyond what you've asked, consider adding:

1. **Symbol-Level Circuit Breakers**
   - Max loss per symbol per day
   - Prevents one bad symbol from killing entire portfolio

2. **Time-of-Day Restrictions**
   - No new positions in last 15 min of trading
   - Reduced limits during high-volatility periods (9:15-9:30, 3:15-3:30)

3. **P&L Velocity Monitoring**
   - Alert if P&L changes faster than expected
   - Example: Scalping strategy normally makes ₹500/min, suddenly ₹5000/min → potential error

4. **Order Size Anomaly Detection**
   - Alert if order size deviates significantly from average
   - Example: Normally trade 100 qty, suddenly place 10,000 qty → likely bug

5. **Correlation Monitoring**
   - Prevent multiple strategies from overlapping on same symbol/direction
   - Example: 3 strategies all buy NIFTY → 3x leverage unintentionally

6. **Heartbeat Monitoring**
   - Strategy must send heartbeat every N seconds
   - No heartbeat → assume crashed → square off positions

These controls work together to create multiple safety layers preventing infinite loops while allowing legitimate high-frequency trading.

---

## Component 8: Logging & Monitoring

```cpp
struct LoggingConfig {
    enum LogLevel {
        DEBUG,           // Everything (verbose)
        INFO,            // Important events
        WARNING,         // Potential issues
        ERROR            // Failures only
    };
    LogLevel level;
    
    // What to log
    bool logEveryTick;
    bool logConditionEvaluation;
    bool logOrderPlacement;
    bool logPnLUpdates;
    bool logRiskMetrics;
    bool logGreeks;              // For options
    
    // Performance tracking
    bool trackExecutionLatency;
    bool trackSlippage;
    bool trackFillRates;
    bool trackWinRate;
    
    // Alerts
    bool sendEmailAlerts;
    bool sendSMSAlerts;
    bool playAudioAlerts;
    QString webhookURL;          // Slack/Discord webhook
    
    // Alert triggers
    double alertOnProfitPercent; // Alert at 5% profit
    double alertOnLossPercent;   // Alert at 2% loss
    bool alertOnEntry;
    bool alertOnExit;
    bool alertOnRiskLimitApproach; // Alert at 80% of daily loss
    
    // ═══════════════════════════════════════════════════════
    // COMPLIANCE METRICS (SEBI Algo Trading Requirements)
    // ═══════════════════════════════════════════════════════
    
    // --- Order-to-Trade Ratio (OTR) ---
    // SEBI mandates OTR < 100:1 (100 orders for every 1 trade executed)
    bool trackOrderToTradeRatio = true;
    double maxOrderToTradeRatio = 100.0;          // Max 100:1 allowed
    double warningOrderToTradeRatio = 80.0;       // Warning at 80:1
    bool haltOnOTRBreach = true;                  // Halt if ratio exceeded
    QString otrCalculationWindow = "DAILY";       // DAILY/HOURLY/REALTIME
    
    // --- Message Traffic Ratio (MTR) ---
    // Ratio of messages sent vs orders placed (includes quotes, depth requests)
    bool trackMessageTrafficRatio = true;
    double maxMessagesPerOrder = 50.0;            // Max 50 messages per order
    double warningMessagesPerOrder = 40.0;        // Warning at 40 messages per order
    
    // --- Order Modification Ratio ---
    // Ratio of modifications to original orders
    bool trackModificationRatio = true;
    double maxModificationsPerOrder = 5.0;        // Max 5 modifications per order
    double avgModificationsPerOrder = 2.0;        // Target average 2 modifications
    bool logExcessiveModifications = true;        // Log if exceeded
    
    // --- Order Cancellation Ratio ---
    // Percentage of orders canceled vs placed
    bool trackCancellationRatio = true;
    double maxCancellationRatio = 0.90;           // Max 90% cancellation rate
    double warningCancellationRatio = 0.80;       // Warning at 80%
    QString cancellationWindow = "HOURLY";        // Track per hour
    
    // --- Fill Rate (Inverse of OTR) ---
    bool trackFillRate = true;
    double minFillRate = 0.10;                    // Min 10% fill rate (1 in 10 orders)
    double targetFillRate = 0.50;                 // Target 50% fill rate
    bool alertOnLowFillRate = true;
    
    // --- Order Velocity ---
    // Orders per second/minute tracking
    bool trackOrderVelocity = true;
    int maxOrdersPerSecond = 5;                   // Max 5 orders/sec
    int maxOrdersPerMinute = 100;                 // Max 100 orders/min
    int avgOrdersPerHour = 500;                   // Average 500 orders/hour
    
    // --- Risk-Reward Compliance ---
    bool trackRiskRewardRatio = true;
    double minRiskRewardRatio = 1.0;              // Min 1:1 (risk ₹100 to make ₹100)
    double avgRiskRewardRatio = 2.0;              // Target 2:1 (risk ₹100 to make ₹200)
    bool rejectTradesWithPoorRR = false;          // Reject if RR < min?
    
    // --- Win Rate Tracking ---
    bool trackWinRate = true;
    double targetWinRate = 0.50;                  // Target 50% win rate
    double minWinRate = 0.35;                     // Min 35% win rate
    int winRateCalculationPeriod = 100;           // Last 100 trades
    bool alertOnWinRateDecline = true;
    
    // --- Trade Frequency Compliance ---
    bool trackTradeFrequency = true;
    int maxTradesPerDay = 100;                    // Max 100 trades per day
    int maxTradesPerHour = 20;                    // Max 20 trades per hour
    int minMinutesBetweenTrades = 1;              // Min 1 min between trades
    
    // --- Latency Metrics ---
    bool trackOrderPlacementLatency = true;
    int maxOrderPlacementLatencyMs = 100;         // Max 100ms from signal to order
    int avgOrderPlacementLatencyMs = 50;          // Target 50ms average
    bool trackAcknowledgmentLatency = true;
    int maxAckLatencyMs = 5000;                   // Max 5 sec for order ack
    
    // --- Market Impact Metrics ---
    bool trackMarketImpact = true;
    double maxPriceImpactPercent = 0.10;          // Max 0.1% price impact
    double avgPriceImpactPercent = 0.05;          // Target 0.05% average
    
    // --- Slippage Tracking ---
    bool trackSlippagePercent = true;
    double maxSlippagePercent = 0.50;             // Max 0.5% slippage
    double avgSlippagePercent = 0.15;             // Target 0.15% average
    bool alertExcessiveSlippage = true;
    
    // --- Position Concentration ---
    bool trackPositionConcentration = true;
    double maxSymbolConcentration = 0.30;         // Max 30% capital in one symbol
    double maxSectorConcentration = 0.50;         // Max 50% capital in one sector
    bool enforceConcentrationLimits = true;
    
    // --- Reporting & Audit ---
    bool generateDailyComplianceReport = true;
    bool generateWeeklyPerformanceReport = true;
    QString complianceReportPath = "./reports/compliance/";
    bool exportTradeLogForAudit = true;
    QString auditLogFormat = "CSV";               // CSV/JSON/XML
    
    // --- Real-Time Compliance Dashboard ---
    bool enableComplianceDashboard = true;
    int dashboardUpdateIntervalSec = 10;          // Update every 10 seconds
    bool showRealTimeOTR = true;
    bool showRealTimeFillRate = true;
    bool showRealTimeSlippage = true;
    bool showRealTimeWinRate = true;
};

### Compliance Metrics - Detailed Explanation

#### Order-to-Trade Ratio (OTR)

**Definition:** Ratio of orders placed to trades executed

**Formula:**
```
OTR = Total Orders Placed / Total Trades Executed
```

**SEBI Requirement:** OTR < 100:1 (maximum 100 orders for every 1 executed trade)

**Example:**
```
Orders Placed: 250
Trades Executed: 5
OTR = 250 / 5 = 50:1 ✅ (Within limit)

Orders Placed: 1000
Trades Executed: 5
OTR = 1000 / 5 = 200:1 ❌ (Exceeded limit - triggers halt)
```

**Why It Matters:**
- Prevents algo "spam" (placing/canceling orders rapidly)
- Ensures genuine trading intent (not market manipulation)
- Exchange resource protection (reduces order processing load)

**Implementation:**
```cpp
struct OTRTracker {
    int ordersPlacedToday = 0;
    int tradesExecutedToday = 0;
    
    void onOrderPlaced() {
        ordersPlacedToday++;
        checkOTR();
    }
    
    void onTradeExecuted() {
        tradesExecutedToday++;
        checkOTR();
    }
    
    void checkOTR() {
        if (tradesExecutedToday == 0) return; // Avoid division by zero
        
        double currentOTR = (double)ordersPlacedToday / tradesExecutedToday;
        
        if (currentOTR > maxOrderToTradeRatio) {
            log("CRITICAL: OTR " + currentOTR + " exceeded limit " + maxOrderToTradeRatio);
            haltAllTrading();
            alertCompliance("OTR breach");
        } else if (currentOTR > warningOrderToTradeRatio) {
            log("WARNING: OTR " + currentOTR + " approaching limit");
            alertTrader("OTR warning");
        }
    }
};
```

---

#### Fill Rate (Inverse of OTR)

**Definition:** Percentage of orders that result in trades

**Formula:**
```
Fill Rate = (Total Trades Executed / Total Orders Placed) * 100%
```

**Target:** > 10% (at least 1 in 10 orders should fill)  
**Good:** > 50% (1 in 2 orders fills)  
**Excellent:** > 70%

**Example:**
```
Orders Placed: 100
Trades Executed: 50
Fill Rate = (50 / 100) * 100% = 50% ✅ (Good)

Orders Placed: 1000
Trades Executed: 5
Fill Rate = (5 / 1000) * 100% = 0.5% ❌ (Very poor - investigate)
```

**Low Fill Rate Causes:**
- Orders placed too far from market price (passive strategy)
- Excessive order modifications/cancellations
- Illiquid instruments chosen
- Algorithm malfunction (placing invalid orders)

---

#### Order Modification Ratio

**Definition:** Average modifications per order

**Formula:**
```
Modification Ratio = Total Modifications / Total Orders Placed
```

**Target:** < 5 modifications per order  
**Typical:** 2-3 modifications per order

**Example:**
```
Orders Placed: 100
Modifications: 180
Ratio = 180 / 100 = 1.8 ✅ (Good)

Orders Placed: 100
Modifications: 700
Ratio = 700 / 100 = 7.0 ❌ (Excessive - potential spam)
```

**Why It Matters:**
- Excessive modifications = market manipulation suspicion
- High exchange load (each modification = message)
- May indicate algo issue (constantly adjusting prices)

---

#### Order Cancellation Ratio

**Definition:** Percentage of orders canceled (not filled)

**Formula:**
```
Cancellation Ratio = (Orders Canceled / Orders Placed) * 100%
```

**Acceptable:** < 50%  
**Warning:** 50-80%  
**Critical:** > 80%

**Example:**
```
Orders Placed: 100
Orders Canceled: 30
Cancellation Ratio = (30 / 100) * 100% = 30% ✅ (Good)

Orders Placed: 100
Orders Canceled: 95
Cancellation Ratio = (95 / 100) * 100% = 95% ❌ (Excessive)
```

---

#### Message Traffic Ratio (MTR)

**Definition:** Ratio of total API messages to orders placed

**Formula:**
```
MTR = (Quotes Requested + Depth Requests + Order Status Queries + ...) / Orders Placed
```

**Target:** < 50 messages per order  
**Typical:** 10-30 messages per order

**What Counts as Message:**
- Market data subscription
- Quote request
- Order book depth request
- Order placement
- Order modification
- Order cancellation
- Order status query

---

#### Compliance Dashboard (Real-Time)

```
┌─────────────────────────────────────────────────────────────┐
│           SEBI COMPLIANCE DASHBOARD - REAL-TIME             │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  Order-to-Trade Ratio (OTR):                                │
│    Current:   45.2 : 1    ████████░░ 45%  ✅ COMPLIANT     │
│    Limit:     100.0 : 1                                     │
│    Warning:   80.0 : 1                                      │
│                                                              │
│  Fill Rate:                                                  │
│    Current:   2.21%       ██░░░░░░░░ 22%  ⚠️  LOW          │
│    Target:    10.0%                                         │
│    Good:      50.0%                                         │
│                                                              │
│  Order Velocity:                                             │
│    Last Min:  12 orders   ████░░░░░░ 12%  ✅ OK            │
│    Limit:     100 orders/min                                │
│    Last Sec:  2 orders    ████░░░░░░ 40%  ✅ OK            │
│    Limit:     5 orders/sec                                  │
│                                                              │
│  Modification Ratio:                                         │
│    Avg:       2.8 mods/order  ✅ NORMAL                     │
│    Limit:     5.0 mods/order                                │
│    Today:     340 modifications                             │
│                                                              │
│  Cancellation Ratio:                                         │
│    Today:     67.5%       █████████░░ 68%  ⚠️  HIGH        │
│    Warning:   80.0%                                         │
│                                                              │
│  Win Rate (Last 100 Trades):                                │
│    Current:   52.0%       █████░░░░░ 52%  ✅ GOOD          │
│    Target:    50.0%                                         │
│    Min:       35.0%                                         │
│                                                              │
│  Slippage:                                                   │
│    Avg:       0.12%       ████░░░░░░ 24%  ✅ GOOD          │
│    Target:    0.15%                                         │
│    Max:       0.50%                                         │
│                                                              │
│  Risk-Reward Ratio:                                          │
│    Avg:       2.3 : 1     ███████░░░ 76%  ✅ EXCELLENT     │
│    Target:    2.0 : 1                                       │
│    Min:       1.0 : 1                                       │
│                                                              │
│  Status:  ✅ COMPLIANT  |  Last Updated: 14:32:15           │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

---

```

---

## Component 9: Backtesting & Optimization

```cpp
struct BacktestConfig {
    QDate startDate;
    QDate endDate;
    double initialCapital;
    
    // Realism settings
    double commissionPerTrade;   // ₹20 per trade
    double slippagePercent;      // 0.1% slippage
    double impactCost;           // 0.05% impact cost
    
    // Metrics to calculate
    bool calculateSharpeRatio;
    bool calculateMaxDrawdown;
    bool calculateWinRate;
    bool calculateProfitFactor;
    bool calculateExpectancy;
    bool calculateCAGR;
    
    // Optimization
    QVector<ParameterRange> parametersToOptimize;
    QString optimizationMetric;  // "Sharpe", "ProfitFactor", "CAGR"
};

struct ParameterRange {
    QString parameterName;       // "rsi_period", "stop_loss_percent"
   double minValue;
    double maxValue;
    double stepSize;
};
```

---

## Component 10: Execution Details

```cpp
struct ExecutionConfig {
    enum OrderType {
        MARKET,
        LIMIT,
        STOP_LOSS,
        STOP_LOSS_MARKET,
        BRACKET_ORDER,
        COVER_ORDER,
        OCO_ORDER            // One-cancels-other
    };
    OrderType defaultOrderType;
    
    enum ProductType {
        INTRADAY,            // MIS
        DELIVERY,            // CNC
        NORMAL               // NRML
    };
    ProductType productType;
    
    // Execution quality
    int maxRetries;              // Retry 3 times
    int retryDelayMs;            // Wait 100ms between retries
    double maxSlippagePercent;   // Max 0.2% slippage
    bool useIcebergOrders;       // Hide large orders
    
    // Smart order routing
    bool splitLargeOrders;
    int maxOrderSize;            // Split if > 500 qty
    
    // Timing
    bool avoidOpeningRange;      // Don't trade 9:15-9:25
    bool avoidClosingRange;      // Don't trade 3:20-3:30
};
```

---

## Complete Code Structure

```cpp
class UniversalStrategyBase {
public:
    // All 10 components
    StrategyMetadata metadata;
    MarketContext marketContext;
    ScanningRules scanningRules;
    EntryRules entryRules;
    PositionManagement positionManagement;
    ExitRules exitRules;
    RiskManagement riskManagement;
    LoggingConfig loggingConfig;
    BacktestConfig backtestConfig;
    ExecutionConfig executionConfig;
    
    // Lifecycle methods
    virtual void init();
    virtual void start();
    virtual void stop();
    virtual void pause();
    virtual void resume();
    
    // Event handlers
    virtual void onTick(const Tick& tick);
    virtual void onCandle(const Candle& candle);
    virtual void onOrderUpdate(const Order& order);
    virtual void onPositionUpdate(const Position& position);
    
    // State queries
    bool isScanning() const;
    bool hasPosition() const;
    double getCurrentPnL() const;
    StrategyState getState() const;
};
```

---

## Real-World Strategy Examples

### Example 1: RSI Oversold Strategy (Complete)

```cpp
UniversalStrategyBase rsiStrategy {
    .metadata = {
        .name = "RSI Oversold Reversal",
        .category = "Intraday",
        .complexityLevel = 1,
        .timeframe = "5m"
    },
    
    .marketContext = {
        .type = SINGLE_SYMBOL,
        .primarySymbol = {
            .symbol = "NIFTY",
            .instrumentType = FUTIDX,
            .action = BUY,
            .quantity = 50
        }
    },
    
    .entryRules = {
        .longEntryConditions = {AND, {
            {INDICATOR, "RSI_14", "<", 30},
            {POSITION_COUNT, "==", 0}
        }},
        .sizingRule = {RISK_BASED, .riskPercent = 0.01}
    },
    
    .positionManagement = {
        .stopLoss = {FIXED_PERCENT, .fixedPercent = 0.01},
        .target = {FIXED_PERCENT, .fixedPercent = 0.02},
        .timeRule = {.exitBeforeClose = true, .exitTime = QTime(15, 15)}
    }
};
```

---

## Implementation Guidelines

### Phase 1: Core Framework (4 weeks)
- Implement all 10 components as C++ structs
- Create parsing/validation logic
- Build execution engine

### Phase 2: UI Builder (4 weeks)
- Form-based strategy builder
- Visual configuration
- Real-time validation

### Phase 3: Advanced Features (4 weeks)
- Backtesting engine
- Greeks calculations
- Multi-leg coordination

---

**Document Version:** 2.0  
**Last Updated:** February 17, 2026  
**Status:** Design Complete - Ready for Implementation
