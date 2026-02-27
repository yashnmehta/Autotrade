# Custom Strategy Builder - Complete Implementation Guide

**Project:** Trading Terminal C++  
**Date:** February 17, 2026  
**Author:** Development Team  
**Status:** Planning Phase  
**Priority:** High (Post-Charting Feature)

---

## Table of Contents

1. [Executive Summary](#executive-summary)
2. [Problem Statement](#problem-statement)
3. [What is a Custom Strategy Builder?](#what-is-a-custom-strategy-builder)
4. [Why Do We Need This?](#why-do-we-need-this)
5. [Use Cases & User Personas](#use-cases--user-personas)
6. [Requirements](#requirements)
7. [Implementation Approaches](#implementation-approaches)
8. [Recommended Approach](#recommended-approach)
9. [Detailed Architecture](#detailed-architecture)
10. [Implementation Roadmap](#implementation-roadmap)
11. [Integration with Existing Code](#integration-with-existing-code)
12. [Code Examples](#code-examples)
13. [Testing Strategy](#testing-strategy)
14. [Risk Analysis](#risk-analysis)
15. [Future Enhancements](#future-enhancements)

---

## Executive Summary

### Vision
Enable non-programmers to create, test, and deploy custom trading strategies without writing C++ code, reducing strategy development time from **days to minutes**.

### Key Benefits
- 🎯 **User Empowerment**: Traders create their own strategies
- ⚡ **Rapid Prototyping**: Test ideas in minutes, not days
- 🔒 **Safety**: Sandboxed execution prevents system crashes
- 📊 **Backtesting**: Validate before risking real money
- 🚀 **Scalability**: Support 100+ concurrent strategies

### MVP Scope (4 weeks)
- Form-based strategy creation (no coding required)
- 10-15 technical indicators (SMA, RSI, MACD, etc.)
- Entry/exit rule builder
- Stop-loss/target management
- Integration with existing StrategyService

---

## Problem Statement

### Current Workflow (Without Strategy Builder)

```
Trader has idea → Contact Developer → Wait 2-3 days
                       ↓
                 Developer writes C++ code
                       ↓
                 Compile & Test → Bugs found
                       ↓
                 Fix & Recompile → Wait 1-2 days
                       ↓
                 Deploy → Doesn't work as expected
                       ↓
                 Modify → Another 2-3 days
```

**Total Time:** 5-10 days per strategy iteration  
**Risk:** High (untested code can crash application)  
**Frustration:** Extreme (constant back-and-forth)

### Ideal Workflow (With Strategy Builder)

```
Trader has idea → Open Strategy Builder → Select indicators
                       ↓
                 Set conditions (form-based)
                       ↓
                 Backtest on 6 months data → See results
                       ↓
                 Adjust parameters → Re-test
                       ↓
                 Deploy to live trading
```

**Total Time:** 15-30 minutes  
**Risk:** Low (sandboxed, validated)  
**Satisfaction:** High (self-service)

---

## What is a Custom Strategy Builder?

### Analogy: Think of It Like...

| Platform | Comparison |
|----------|------------|
| **TradingView** | Pine Script editor for indicators |
| **Amibroker** | AFL formula language |
| **MetaTrader** | MQL4/MQL5 Expert Advisors |
| **WordPress** | Page builder (visual, drag-drop) |
| **Excel** | Conditional formatting rules |

### Core Components

```
┌─────────────────────────────────────────────────────────────┐
│                  STRATEGY BUILDER UI                        │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐     │
│  │  Form-Based  │  │   Visual     │  │   Script     │     │
│  │   Builder    │  │   Editor     │  │   Editor     │     │
│  │   (Phase 1)  │  │  (Phase 2)   │  │  (Phase 3)   │     │
│  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘     │
│         │                 │                  │              │
│         └─────────────────┼──────────────────┘              │
│                           ↓                                 │
│              ┌────────────────────────┐                     │
│              │   Strategy Compiler     │                     │
│              │  - Validate rules       │                     │
│              │  - Generate code/JSON   │                     │
│              │  - Create instance      │                     │
│              └────────────┬────────────┘                     │
│                           ↓                                 │
│              ┌────────────────────────┐                     │
│              │   Strategy Engine       │                     │
│              │  - Execute on ticks     │                     │
│              │  - Evaluate conditions  │                     │
│              │  - Place orders         │                     │
│              │  - Manage positions     │                     │
│              └────────────┬────────────┘                     │
│                           ↓                                 │
│              ┌────────────────────────┐                     │
│              │   Order Executor        │                     │
│              │  - Place orders         │                     │
│              │  - Modify SL/Target     │                     │
│              │  - Track positions      │                     │
│              └─────────────────────────┘                     │
└─────────────────────────────────────────────────────────────┘
```

---

## Why Do We Need This?

### Business Perspective

#### Without Strategy Builder
- ❌ Limited to pre-coded strategies
- ❌ Developer bottleneck for every new strategy
- ❌ Hard to sell to traders (fixed strategies)
- ❌ High support cost (custom requests)
- ❌ Slow iteration (recompile for every change)

#### With Strategy Builder
- ✅ Unlimited strategy variations
- ✅ Self-service (traders independent)
- ✅ Strong selling point (competitive advantage)
- ✅ Reduced support load
- ✅ Fast A/B testing of strategies

### Technical Perspective

#### Current Architecture Problem
```cpp
// To add new strategy, must:
1. Create New C++ Class → src/strategies/MyNewStrategy.cpp
2. Inherit from StrategyBase
3. Implement onTick() logic
4. Recompile entire application (30-60 seconds)
5. Restart application (lose state)
6. Test → Find bugs → Repeat
```

#### With Strategy Builder
```json
// To add new strategy:
1. Fill form or write JSON
2. Click "Save" → Validates in real-time
3. Click "Start" → Deploys instantly
4. Monitor → Adjust parameters → Re-deploy (no restart)
```

### Competitive Analysis

| Feature | Zerodha Kite | Upstox Pro | Angel One | **Our Terminal** |
|---------|--------------|------------|-----------|-------------------|
| Custom Strategies | ❌ No | ❌ No | ❌ No | ✅ **Yes (Planned)** |
| Algo Trading | ❌ API only | ❌ API only | ❌ API only | ✅ **Built-in** |
| Backtesting | ✅ External | ❌ No | ❌ No | ✅ **Integrated** |
| Visual Builder | ❌ No | ❌ No | ❌ No | ✅ **Planned** |

**Conclusion:** This feature gives us a **significant competitive advantage**.

---

## Use Cases & User Personas

### Persona 1: Rajesh - Day Trader (Beginner)

**Background:**
- 2 years trading experience
- No programming knowledge
- Trades NIFTY futures intraday
- Uses RSI + MACD manually

**Pain Point:**
> "I miss many trades because I'm watching the chart manually. I want to automate my RSI strategy but don't know coding."

**Use Case: Simple RSI Strategy**

**Goal:** Buy when RSI < 30, Sell when RSI > 70

**Steps with Strategy Builder:**
1. Open Strategy Builder
2. Select "Simple Entry/Exit Strategy" template
3. Fill form:
   - Symbol: NIFTY FUT
   - Entry Condition: RSI(14) < 30
   - Exit Condition: RSI(14) > 70
   - Quantity: 50
   - Stop Loss: 1%
4. Click "Backtest" → See 6-month results
5. Click "Deploy" → Strategy goes live

**Expected Outcome:**
- ✅ No coding required
- ✅ Time: 10 minutes
- ✅ Confidence: High (backtested)

---

### Persona 2: Priya - Algo Trader (Intermediate)

**Background:**
- 5 years trading experience
- Basic Python knowledge
- Trades options strategies
- Uses multiple indicators

**Pain Point:**
> "I want to combine MA crossover + RSI + Volume. Writing Python scripts is tedious and I need proper execution."

**Use Case: Multi-Indicator Strategy**

**Goal:** Buy when (SMA20 > SMA50) AND (RSI < 40) AND (Volume > AvgVolume)

**Steps with Visual Flow Editor:**
```
[SMA 20] ────┐
             ├─> [Greater Than] ───┐
[SMA 50] ────┘                     │
                                   ├─> [AND] ─> [BUY]
[RSI 14] ───> [Less Than 40] ─────┤           
                                   │
[Volume] ───> [> Avg Volume] ─────┘
```

**Expected Outcome:**
- ✅ Visual representation (easy to understand)
- ✅ Complex logic without coding
- ✅ Time: 20 minutes

---

### Persona 3: Arjun - Institutional Trader (Advanced)

**Background:**
- 10+ years experience
- Expert programmer (C++, Python)
- Manages hedge fund
- Complex options strategies

**Pain Point:**
> "I need full flexibility. Visual builders are too limiting. I want to write custom logic but don't want to deal with compilation and deployment."

**Use Case: Iron Condor Auto-Deployment**

**Goal:** Deploy Iron Condor when IV > 15%, manage Greeks dynamically

**Steps with Script Editor:**
```javascript
// ChaiScript code
def should_enter() {
    var iv = implied_volatility("NIFTY");
    var spot = ltp("NIFTY");
    var delta_hedge = calculate_portfolio_delta();
    
    return (iv > 15.0) && 
           (spot > sma(200)) && 
           (abs(delta_hedge) < 0.1);
}

def on_entry() {
    var atm = nearest_strike(ltp("NIFTY"), 50);
    
    // Sell Iron Condor
    sell_option("NIFTY", atm, "CE", 25);
    buy_option("NIFTY", atm + 100, "CE", 25);
    sell_option("NIFTY", atm, "PE", 25);
    buy_option("NIFTY", atm - 100, "PE", 25);
}

def on_tick() {
    // Adjust delta hedge if needed
    var delta = portfolio_delta();
    if (abs(delta) > 0.2) {
        hedge_with_futures(delta);
    }
}
```

**Expected Outcome:**
- ✅ Full programming power
- ✅ No compilation needed
- ✅ Hot-reload (modify while running)
- ✅ Time: 30-60 minutes

---

### Persona 4: Sneha - Options Strategy Reseller

**Background:**
- Creates and sells strategies
- Needs white-label solutions
- Targets retail traders

**Pain Point:**
> "I want to create strategy templates and sell them to my clients. They should be able to deploy with one click."

**Use Case: Template Library**

**Steps:**
1. Create "Bank Nifty Short Straddle" template
2. Parameterize: Strike selection, SL%, Target%
3. Export as shareable file (.strategy)
4. Clients import and deploy in one click

**Expected Outcome:**
- ✅ Monetization opportunity
- ✅ Easy distribution
- ✅ No support burden

---

## Requirements

### 6.1 Functional Requirements

#### Must Have (MVP - Phase 1)

| Req ID | Feature | Description | Priority |
|--------|---------|-------------|----------|
| FR-01 | Strategy Creation Form | UI to input strategy parameters | P0 |
| FR-02 | Indicator Selection | Dropdown with 10-15 indicators | P0 |
| FR-03 | Condition Builder | Set entry/exit conditions | P0 |
| FR-04 | Risk Management | Set SL, Target, Position size | P0 |
| FR-05 | Strategy Deployment | Save and start strategy | P0 |
| FR-06 | Real-time Monitoring | Track PnL, positions | P0 |
| FR-07 | Stop/Pause Strategy | Lifecycle management | P0 |

**Indicators for MVP:**
1. SMA (Simple Moving Average)
2. EMA (Exponential Moving Average)
3. RSI (Relative Strength Index)
4. MACD (Moving Average Convergence Divergence)
5. Bollinger Bands
6. ATR (Average True Range)
7. Stochastic Oscillator
8. Volume
9. OBV (On Balance Volume)
10. ADX (Average Directional Index)

**Conditions for MVP:**
- Comparison: `>`, `<`, `>=`, `<=`, `==`, `!=`
- Logic: `AND`, `OR`
- Max 5 conditions per rule

#### Should Have (Phase 2)

| Req ID | Feature | Description | Priority |
|--------|---------|-------------|----------|
| FR-08 | Visual Flow Editor | Drag-drop block editor | P1 |
| FR-09 | Backtesting | Test on historical data | P1 |
| FR-10 | Strategy Templates | Pre-built strategies | P1 |
| FR-11 | Parameter Optimization | Auto-find best parameters | P1 |
| FR-12 | Multi-Leg Strategies | Options spreads | P1 |

#### Nice to Have (Phase 3)

| Req ID | Feature | Description | Priority |
|--------|---------|-------------|----------|
| FR-13 | Script Editor | ChaiScript/Lua support | P2 |
| FR-14 | Strategy Marketplace | Share/sell strategies | P2 |
| FR-15 | Machine Learning | AI-based optimization | P2 |
| FR-16 | Strategy Cloning | Copy and modify existing | P2 |

---

### 6.2 Non-Functional Requirements

#### Performance

| Metric | Target | Critical? |
|--------|--------|-----------|
| Strategy Evaluation Time | < 50ms per tick | ✅ Yes |
| Concurrent Strategies | 100+ | ✅ Yes |
| Memory per Strategy | < 10 MB | ⚠️ Important |
| UI Responsiveness | < 100ms | ⚠️ Important |
| Backtest Speed | 1 year data < 30s | ❌ Nice to have |

#### Safety & Security

| Requirement | Implementation | Priority |
|-------------|----------------|----------|
| Sandboxed Execution | No system calls, file I/O restricted | P0 |
| Input Validation | Sanitize all user inputs | P0 |
| Risk Limits | Max loss, max positions, max orders | P0 |
| Kill Switch | Emergency stop all strategies | P0 |
| Audit Trail | Log all strategy actions | P1 |

#### Scalability

| Aspect | Target |
|--------|--------|
| Database | Support 10,000+ strategies |
| Concurrent Users | 50 users creating strategies simultaneously |
| Historical Data | Store 5 years of tick data |

---

### 6.3 Technical Requirements

#### Dependencies

| Library | Purpose | Version | License |
|---------|---------|---------|---------|
| TA-Lib | Technical indicators | 0.4.0+ | BSD |
| Qt Framework | UI components | 5.15.2 | LGPL |
| SQLite | Strategy storage | 3.x | Public Domain |
| ChaiScript (Optional) | Scripting (Phase 3) | 6.1+ | BSD |

#### API Requirements

**Must integrate with:**
- ✅ `FeedHandler` - Market data subscription
- ✅ `IndicatorService` - Indicator calculations
- ✅ `OrderService` - Order placement
- ✅ `PositionService` - Position tracking
- ✅ `StrategyService` - Strategy management
- ✅ `DatabaseManager` - Persistence

---

## Implementation Approaches

### Approach 1: JSON-Based Strategy Definition ⭐ **RECOMMENDED FOR MVP**

#### Concept
Users fill a form → System generates JSON → Engine executes JSON

#### Pros
- ✅ Simplest to implement (1-2 weeks)
- ✅ No new dependencies
- ✅ Safe (no code execution)
- ✅ Easy to validate
- ✅ Can use existing UI (`CreateStrategyDialog`)

#### Cons
- ❌ Limited flexibility
- ❌ Can't do complex logic (nested conditions)
- ❌ Not suitable for advanced users

#### Example JSON Strategy
```json
{
  "strategy_id": "rsi_reversal_001",
  "name": "RSI Reversal Strategy",
  "version": "1.0",
  "symbol": "NIFTY",
  "segment": 2,
  "timeframe": "5m",
  
  "parameters": {
    "rsi_period": 14,
    "rsi_oversold": 30,
    "rsi_overbought": 70,
    "position_size": 50,
    "max_positions": 1
  },
  
  "indicators": [
    {
      "name": "RSI",
      "period": "{{rsi_period}}",
      "price_field": "close"
    }
  ],
  
  "entry_rules": {
    "long_entry": {
      "operator": "AND",
      "conditions": [
        {
          "type": "indicator",
          "indicator": "RSI",
          "operator": "<",
          "value": "{{rsi_oversold}}"
        },
        {
          "type": "position_count",
          "operator": "==",
          "value": 0
        },
        {
          "type": "time",
          "operator": "between",
          "start": "09:30:00",
          "end": "15:00:00"
        }
      ]
    },
    "short_entry": {
      "operator": "AND",
      "conditions": [
        {
          "type": "indicator",
          "indicator": "RSI",
          "operator": ">",
          "value": "{{rsi_overbought}}"
        },
        {
          "type": "position_count",
          "operator": "==",
          "value": 0
        }
      ]
    }
  },
  
  "exit_rules": {
    "target": {
      "type": "percentage",
      "value": 2.0
    },
    "stop_loss": {
      "type": "percentage",
      "value": 1.0
    },
    "time_exit": {
      "enabled": true,
      "time": "15:15:00"
    }
  },
  
  "risk_management": {
    "max_daily_loss": 5000,
    "max_daily_trades": 10,
    "trailing_stop": {
      "enabled": false,
      "trigger_profit": 1.0,
      "trail_amount": 0.5
    }
  }
}
```

#### Implementation Effort
- **Time:** 1-2 weeks
- **Complexity:** Low
- **Risk:** Low

---

### Approach 2: Visual Flow Editor 🎨 **BEST UX**

#### Concept
Drag-drop blocks → Connect with arrows → Generate code

#### Pros
- ✅ Best user experience
- ✅ Intuitive (no learning curve)
- ✅ Visual debugging (see flow)
- ✅ Can export to JSON or script

#### Cons
- ❌ Complex to build (3-4 weeks)
- ❌ Requires QGraphicsView mastery
- ❌ More bugs (UI complexity)

#### Visual Block Types

```
┌─────────────────────────────────────────────────────────┐
│                    BLOCK PALETTE                        │
├─────────────────────────────────────────────────────────┤
│  Indicator Blocks                                       │
│  ┌───────┐ ┌───────┐ ┌───────┐ ┌───────┐             │
│  │  SMA  │ │  RSI  │ │ MACD  │ │  ATR  │             │
│  └───────┘ └───────┘ └───────┘ └───────┘             │
│                                                         │
│  Comparison Blocks                                      │
│  ┌───────┐ ┌───────┐ ┌───────┐                       │
│  │   >   │ │   <   │ │  ==   │                       │
│  └───────┘ └───────┘ └───────┘                       │
│                                                         │
│  Logic Blocks                                           │
│  ┌───────┐ ┌───────┐ ┌───────┐                       │
│  │  AND  │ │  OR   │ │  NOT  │                       │
│  └───────┘ └───────┘ └───────┘                       │
│                                                         │
│  Action Blocks                                          │
│  ┌───────┐ ┌───────┐ ┌───────┐                       │
│  │  BUY  │ │ SELL  │ │ EXIT  │                       │
│  └───────┘ └───────┘ └───────┘                       │
└─────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────┐
│                    CANVAS AREA                          │
│                                                         │
│   [RSI 14] ──┐                                         │
│              ├──> [< 30] ──┐                           │
│              │             │                           │
│              │             ├──> [AND] ──> [BUY 50]     │
│              │             │                           │
│   [Price] ───┴──> [> SMA 20] ──┘                       │
│                                                         │
└─────────────────────────────────────────────────────────┘
```

#### Implementation Effort
- **Time:** 3-4 weeks
- **Complexity:** High
- **Risk:** Medium

---

### Approach 3: Script-Based (ChaiScript) 💻 **MOST FLEXIBLE**

#### Concept
Users write simple script → Engine executes

#### Pros
- ✅ Maximum flexibility
- ✅ Advanced users love it
- ✅ Can do anything (loops, functions, etc.)
- ✅ Hot-reload (modify while running)

#### Cons
- ❌ Requires scripting knowledge
- ❌ New dependency (ChaiScript library)
- ❌ Security concerns (sandboxing needed)
- ❌ Harder to debug for users

#### Example ChaiScript Strategy
```javascript
// User writes this in script editor
var signal = "";
var sma20 = 0.0;
var sma50 = 0.0;
var rsi = 0.0;

def init() {
    subscribe_indicator("SMA_20");
    subscribe_indicator("SMA_50");
    subscribe_indicator("RSI_14");
    subscribe_candles("5m", 100);
    
    log("Strategy initialized");
}

def on_candle(candle) {
    // Get indicator values
    sma20 = get_indicator("SMA_20");
    sma50 = get_indicator("SMA_50");
    rsi = get_indicator("RSI_14");
    
    // Bullish signal
    if (sma20 > sma50 && rsi < 40) {
        signal = "LONG";
        log("Bullish crossover detected, RSI: " + rsi.to_string());
    }
    // Bearish signal
    else if (sma20 < sma50 && rsi > 60) {
        signal = "SHORT";
        log("Bearish crossover detected, RSI: " + rsi.to_string());
    }
}

def should_enter_long() {
    return signal == "LONG" && 
           get_open_positions().size() == 0 &&
           is_market_open();
}

def should_enter_short() {
    return signal == "SHORT" && 
           get_open_positions().size() == 0;
}

def should_exit(position) {
    // Exit if signal reverses
    if (position.side == "BUY" && signal == "SHORT") {
        return true;
    }
    if (position.side == "SELL" && signal == "LONG") {
        return true;
    }
    
    return false;
}

def calculate_quantity(signal) {
    var capital = get_available_margin();
    var ltp = get_ltp();
    var risk_percent = 0.01; // 1% of capital
    
    return int((capital * risk_percent) / ltp);
}

def calculate_stop_loss(entry_price, side) {
    var atr = get_indicator("ATR_14");
    
    if (side == "BUY") {
        return entry_price - (2.0 * atr);
    } else {
        return entry_price + (2.0 * atr);
    }
}

def calculate_target(entry_price, side) {
    var atr = get_indicator("ATR_14");
    var rr_ratio = 2.5; // Risk-Reward ratio
    
    if (side == "BUY") {
        return entry_price + (rr_ratio * 2.0 * atr);
    } else {
        return entry_price - (rr_ratio * 2.0 * atr);
    }
}
```

#### Implementation Effort
- **Time:** 2-3 weeks
- **Complexity:** Medium-High
- **Risk:** Medium (security concerns)

---

## Recommended Approach

### 🎯 **Phase 1: JSON-Based (Start Here)**

**Why?**
1. ✅ Fastest to market (1-2 weeks)
2. ✅ Lowest risk
3. ✅ Covers 80% of use cases
4. ✅ Uses existing infrastructure
5. ✅ Easy to extend later

**Timeline:**
```
Week 1: Backend implementation
  Day 1-2: StrategyParser class
  Day 3-4: CustomStrategy class
  Day 5: Integration with StrategyService

Week 2: UI implementation
  Day 1-2: Enhanced CreateStrategyDialog
  Day 3-4: Indicator selector, condition builder
  Day 5: Testing

Week 3-4: Testing & refinement
  Week 3: Unit tests, integration tests
  Week 4: User acceptance testing, bug fixes
```

### 📈 **Phase 2: Visual Flow Editor (After MVP Success)**

**When?**
- After 100+ users actively using Phase 1
- After gathering user feedback
- After Phase 1 is stable

**Estimated Effort:** 3-4 weeks

### 💻 **Phase 3: Script Editor (For Power Users)**

**When?**
- After Phase 2 is complete
- When institutional clients request it
- When JSON approach shows limitations

**Estimated Effort:** 2-3 weeks

---

## Detailed Architecture

### System Architecture Diagram

```
┌──────────────────────────────────────────────────────────────────┐
│                         USER INTERFACE LAYER                      │
├──────────────────────────────────────────────────────────────────┤
│                                                                   │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │         StrategyBuilderDialog (NEW)                      │    │
│  │  ┌──────────────────┐  ┌──────────────────┐            │    │
│  │  │  Basic Info      │  │  Indicators      │            │    │
│  │  │  - Name          │  │  [x] SMA         │            │    │
│  │  │  - Symbol        │  │  [x] RSI         │            │    │
│  │  │  - Timeframe     │  │  [ ] MACD        │            │    │
│  │  └──────────────────┘  └──────────────────┘            │    │
│  │                                                          │    │
│  │  ┌────────────────────────────────────────────┐        │    │
│  │  │  Entry Conditions                           │        │    │
│  │  │  ┌──────────────────────────────────────┐ │        │    │
│  │  │  │ RSI(14)  [<]  [30]      [X]          │ │        │    │
│  │  │  │           [AND] [OR]                  │ │        │    │
│  │  │  │ Price    [>]  [SMA(20)] [X]          │ │        │    │
│  │  │  │           [+ Add Condition]           │ │        │    │
│  │  │  └──────────────────────────────────────┘ │        │    │
│  │  └────────────────────────────────────────────┘        │    │
│  │                                                          │    │
│  │  ┌────────────────────────────────────────────┐        │    │
│  │  │  Risk Management                            │        │    │
│  │  │  Stop Loss:  [1.0] %  Target: [2.0] %     │        │    │
│  │  │  Quantity:   [50]     Max Loss: [5000] ₹  │        │    │
│  │  └────────────────────────────────────────────┘        │    │
│  │                                                          │    │
│  │  [Validate] [Backtest] [Save] [Deploy]                 │    │
│  └─────────────────────────────────────────────────────────┘    │
│                                                                   │
└───────────────────────────────┬───────────────────────────────────┘
                                │
┌───────────────────────────────▼───────────────────────────────────┐
│                        BUSINESS LOGIC LAYER                        │
├────────────────────────────────────────────────────────────────────┤
│                                                                    │
│  ┌──────────────────────────────────────────────────────────┐    │
│  │  StrategyParser (NEW)                                     │    │
│  │  + parseJSON(QJsonObject) → StrategyDefinition           │    │
│  │  + validate(StrategyDefinition) → bool                   │    │
│  │  + generateJSON(StrategyDefinition) → QJsonObject        │    │
│  └─────────────────────────┬────────────────────────────────┘    │
│                            │                                      │
│  ┌─────────────────────────▼────────────────────────────────┐    │
│  │  CustomStrategy : StrategyBase (NEW)                      │    │
│  │  - m_definition: StrategyDefinition                       │    │
│  │  - m_indicators: QHash<QString, double>                   │    │
│  │  + init(StrategyInstance)                                │    │
│  │  + onTick(MarketTick)                                    │    │
│  │  - evaluateEntryConditions() → bool                      │    │
│  │  - evaluateExitConditions() → bool                       │    │
│  │  - calculateIndicators()                                 │    │
│  └─────────────────────────┬────────────────────────────────┘    │
│                            │                                      │
│  ┌─────────────────────────▼────────────────────────────────┐    │
│  │  StrategyService (EXISTING - EXTEND)                      │    │
│  │  + createCustomStrategy(JSON) → qint64                   │    │
│  │  + startStrategy(instanceId)                             │    │
│  │  + stopStrategy(instanceId)                              │    │
│  └────────────────────────────────────────────────────────────┘    │
│                                                                    │
└───────────────────────────────┬───────────────────────────────────┘
                                │
┌───────────────────────────────▼───────────────────────────────────┐
│                          DATA ACCESS LAYER                         │
├────────────────────────────────────────────────────────────────────┤
│                                                                    │
│  ┌──────────────────┐  ┌──────────────────┐  ┌────────────────┐ │
│  │  FeedHandler     │  │  IndicatorService│  │  OrderService  │ │
│  │  (Market Data)   │  │  (TA-Lib)        │  │  (Execution)   │ │
│  └──────────────────┘  └──────────────────┘  └────────────────┘ │
│                                                                    │
│  ┌──────────────────┐  ┌──────────────────┐                      │
│  │ PositionService  │  │ DatabaseManager  │                      │
│  │ (Tracking)       │  │ (Storage)        │                      │
│  └──────────────────┘  └──────────────────┘                      │
│                                                                    │
└────────────────────────────────────────────────────────────────────┘
```

---

### Class Diagram (Phase 1)

```cpp
// ═══════════════════════════════════════════════════════════
// MODELS
// ═══════════════════════════════════════════════════════════

struct Condition {
    enum Type { Indicator, Price, Time, PositionCount };
    
    Type type;
    QString indicator;      // "RSI", "SMA", etc.
    QString operator_;      // ">", "<", "==", "!=", ">=", "<="
    QVariant value;         // 30, 70, "{{parameter}}", etc.
    QString field;          // "close", "high", "low", "open"
};

struct ConditionGroup {
    enum Operator { AND, OR };
    
    Operator logicOperator;
    QVector<Condition> conditions;
    QVector<ConditionGroup> nestedGroups; // For complex (A AND B) OR (C AND D)
};

struct RiskParams {
    double stopLossPercent;
    double targetPercent;
    int maxPositions;
    double maxDailyLoss;
    int maxDailyTrades;
    
    bool trailingStopEnabled;
    double trailingTriggerProfit;
    double trailingAmount;
    
    bool timeBasedExitEnabled;
    QTime exitTime;
};

struct StrategyDefinition {
    QString strategyId;
    QString name;
    QString version;
    QString symbol;
    int segment;
    QString timeframe;
    
    QVariantMap parameters;
    QVector<QString> indicators;
    
    ConditionGroup longEntryRules;
    ConditionGroup shortEntryRules;
    ConditionGroup exitRules;
    
    RiskParams riskManagement;
};

// ═══════════════════════════════════════════════════════════
// PARSER
// ═══════════════════════════════════════════════════════════

class StrategyParser {
public:
    static StrategyDefinition parseJSON(const QJsonObject& json, 
                                        QString& errorMsg);
    
    static QJsonObject toJSON(const StrategyDefinition& def);
    
    static bool validate(const StrategyDefinition& def, 
                        QString& errorMsg);
    
private:
    static Condition parseCondition(const QJsonObject& json);
    static ConditionGroup parseConditionGroup(const QJsonObject& json);
    static RiskParams parseRiskParams(const QJsonObject& json);
    
    static bool validateIndicator(const QString& indicator);
    static bool validateOperator(const QString& op);
    static bool validateTimeframe(const QString& timeframe);
};

// ═══════════════════════════════════════════════════════════
// STRATEGY IMPLEMENTATION
// ═══════════════════════════════════════════════════════════

class CustomStrategy : public StrategyBase {
    Q_OBJECT
    
public:
    explicit CustomStrategy(QObject* parent = nullptr);
    ~CustomStrategy() override;
    
    void init(const StrategyInstance& instance) override;
    void start() override;
    void stop() override;
    
protected slots:
    void onTick(const UDP::MarketTick& tick) override;
    void onCandle(const Candle& candle); // NEW
    
private:
    // Core logic
    bool evaluateConditionGroup(const ConditionGroup& group);
    bool evaluateCondition(const Condition& condition);
    
    // Indicator management
    void subscribeToIndicators();
    void updateIndicators(const Candle& candle);
    double getIndicatorValue(const QString& indicator);
    
    // Position management
    void checkEntrySignals();
    void checkExitSignals();
    void placeEntryOrder(const QString& side);
    void placeExitOrder();
    
    // Risk management
    void checkRiskLimits();
    void updateTrailingStop();
    void checkTimeBasedExit();
    
    // Helper methods
    double resolveValue(const QVariant& value);
    bool compareValues(double left, const QString& op, double right);
    
private:
    StrategyDefinition m_definition;
    QHash<QString, double> m_indicators;
    QVector<Candle> m_candles;
    
    bool m_inPosition;
    QString m_positionSide;
    double m_entryPrice;
    double m_currentStopLoss;
    double m_currentTarget;
    
    int m_dailyTrades;
    double m_dailyPnL;
    QDate m_lastResetDate;
};

// ═══════════════════════════════════════════════════════════
// UI COMPONENTS
// ═══════════════════════════════════════════════════════════

class ConditionWidget : public QWidget {
    Q_OBJECT
    
public:
    explicit ConditionWidget(QWidget* parent = nullptr);
    
    Condition getCondition() const;
    void setCondition(const Condition& condition);
    
signals:
    void conditionChanged();
    void removeRequested();
    
private:
    QComboBox* m_typeCombo;
    QComboBox* m_indicatorCombo;
    QComboBox* m_operatorCombo;
    QLineEdit* m_valueEdit;
    QPushButton* m_removeBtn;
};

class StrategyBuilderDialog : public QDialog {
    Q_OBJECT
    
public:
    explicit StrategyBuilderDialog(QWidget* parent = nullptr);
    
    StrategyDefinition getStrategyDefinition() const;
    void setStrategyDefinition(const StrategyDefinition& def);
    
private slots:
    void onAddCondition();
    void onRemoveCondition(ConditionWidget* widget);
    void onValidate();
    void onBacktest();
    void onSave();
    void onDeploy();
    
private:
    void setupUI();
    void loadIndicators();
    bool validateInput();
    
private:
    // Basic info
    QLineEdit* m_nameEdit;
    QComboBox* m_symbolCombo;
    QComboBox* m_timeframeCombo;
    
    // Indicators
    QListWidget* m_indicatorList;
    
    // Entry conditions
    QVBoxLayout* m_entryConditionsLayout;
    QVector<ConditionWidget*> m_entryConditionWidgets;
    QComboBox* m_entryLogicCombo; // AND / OR
    
    // Exit conditions
    QVBoxLayout* m_exitConditionsLayout;
    QVector<ConditionWidget*> m_exitConditionWidgets;
    
    // Risk management
    QDoubleSpinBox* m_stopLossSpin;
    QDoubleSpinBox* m_targetSpin;
    QSpinBox* m_quantitySpin;
    QDoubleSpinBox* m_maxDailyLossSpin;
    QSpinBox* m_maxDailyTradesSpin;
    
    // Trailing stop
    QCheckBox* m_trailingStopCheck;
    QDoubleSpinBox* m_trailingTriggerSpin;
    QDoubleSpinBox* m_trailingAmountSpin;
    
    // Time-based exit
    QCheckBox* m_timeExitCheck;
    QTimeEdit* m_exitTimeEdit;
};
```

---

## Implementation Roadmap

### Week 1: Backend Core (Days 1-5)

#### Day 1: Data Models & Parser

**Files to create:**
1. `include/strategy/StrategyDefinition.h`
2. `include/strategy/StrategyParser.h`
3. `src/strategy/StrategyParser.cpp`

**Tasks:**
```cpp
// StrategyDefinition.h - Define all data structures
struct Condition { /* ... */ };
struct ConditionGroup { /* ... */ };
struct RiskParams { /* ... */ };
struct StrategyDefinition { /* ... */ };

// StrategyParser.cpp - Implement parsing
StrategyDefinition StrategyParser::parseJSON(const QJsonObject& json) {
    // Parse indicators
    // Parse entry conditions
    // Parse exit conditions
    // Parse risk params
}

bool StrategyParser::validate(const StrategyDefinition& def) {
    // Validate indicator names
    // Validate operators
    // Validate value ranges
    // Check for required fields
}
```

**Test:**
```cpp
// Test with sample JSON
QJsonObject json = loadFromFile("test_strategy.json");
QString error;
StrategyDefinition def = StrategyParser::parseJSON(json, error);
ASSERT_TRUE(error.isEmpty());
ASSERT_EQ(def.name, "Test Strategy");
```

---

#### Day 2-3: CustomStrategy Class

**Files to create:**
1. `include/strategies/CustomStrategy.h`
2. `src/strategies/CustomStrategy.cpp`

**Tasks:**
```cpp
// CustomStrategy.h
class CustomStrategy : public StrategyBase {
    void init(const StrategyInstance& instance) override;
    void onTick(const UDP::MarketTick& tick) override;
    
private:
    bool evaluateConditionGroup(const ConditionGroup& group);
    bool evaluateCondition(const Condition& condition);
    void updateIndicators();
    
    StrategyDefinition m_definition;
    QHash<QString, double> m_indicators;
};
```

**Implementation Steps:**

1. **Init method:**
```cpp
void CustomStrategy::init(const StrategyInstance& instance) {
    StrategyBase::init(instance);
    
    // Parse strategy definition from JSON in parameters
    QString jsonStr = m_instance.parameters["definition"].toString();
    QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
    QString error;
    m_definition = StrategyParser::parseJSON(doc.object(), error);
    
    if (!error.isEmpty()) {
        log("ERROR: " + error);
        updateState(StrategyState::Error);
        return;
    }
    
    // Subscribe to indicators
    subscribeToIndicators();
}
```

2. **OnTick handler:**
```cpp
void CustomStrategy::onTick(const UDP::MarketTick& tick) {
    if (!m_isRunning) return;
    
    // Update indicators
    updateIndicators();
    
    // Check risk limits first
    checkRiskLimits();
    
    if (!m_inPosition) {
        checkEntrySignals();
    } else {
        checkExitSignals();
        updateTrailingStop();
        checkTimeBasedExit();
    }
}
```

3. **Condition evaluation:**
```cpp
bool CustomStrategy::evaluateConditionGroup(const ConditionGroup& group) {
    if (group.conditions.isEmpty()) return false;
    
    if (group.logicOperator == ConditionGroup::AND) {
        // All conditions must be true
        for (const Condition& cond : group.conditions) {
            if (!evaluateCondition(cond)) return false;
        }
        return true;
    } else {
        // At least one condition must be true
        for (const Condition& cond : group.conditions) {
            if (evaluateCondition(cond)) return true;
        }
        return false;
    }
}

bool CustomStrategy::evaluateCondition(const Condition& condition) {
    double leftValue = 0.0;
    double rightValue = resolveValue(condition.value);
    
    switch (condition.type) {
        case Condition::Indicator:
            leftValue = getIndicatorValue(condition.indicator);
            break;
            
        case Condition::Price:
            leftValue = getLTP();
            break;
            
        case Condition::PositionCount:
            leftValue = m_instance.activePositions;
            break;
            
        case Condition::Time:
            // Handle time-based conditions
            return checkTimeCondition(condition);
    }
    
    return compareValues(leftValue, condition.operator_, rightValue);
}
```

**Test:**
```cpp
// Create test instance with JSON definition
StrategyInstance instance;
instance.parameters["definition"] = testJSON;

CustomStrategy strategy;
strategy.init(instance);
strategy.start();

// Simulate tick
UDP::MarketTick tick;
tick.ltp = 22000.0;
strategy.onTick(tick);

// Verify conditions evaluated correctly
```

---

#### Day 4: Indicator Integration

**Tasks:**
1. Subscribe to TA-Lib indicators
2. Cache indicator values
3. Update on each tick/candle

**Implementation:**
```cpp
void CustomStrategy::subscribeToIndicators() {
    for (const QString& indicator : m_definition.indicators) {
        // Subscribe to indicator service
        // Store in m_indicators hash
    }
}

void CustomStrategy::updateIndicators() {
    for (const QString& indicator : m_definition.indicators) {
        double value = IndicatorService::instance()
                          .calculate(indicator, m_definition.symbol);
        m_indicators[indicator] = value;
    }
}

double CustomStrategy::getIndicatorValue(const QString& indicator) {
    if (m_indicators.contains(indicator)) {
        return m_indicators[indicator];
    }
    
    log("Warning: Indicator " + indicator + " not found");
    return 0.0;
}
```

---

#### Day 5: Integration with StrategyService

**Files to modify:**
1. `src/services/StrategyService.cpp`

**Tasks:**
```cpp
// In StrategyService::createInstance()
StrategyBase* StrategyService::createStrategyObject(
    const QString& strategyType) {
    
    if (strategyType == "Custom") {
        return new CustomStrategy(this);
    }
    else if (strategyType == "Momentum") {
        return new MomentumStrategy(this);
    }
    // ... existing strategies
    
    return nullptr;
}
```

**Test:**
```cpp
// Create custom strategy through service
qint64 instanceId = StrategyService::instance().createInstance(
    "Test RSI Strategy",
    "Testing custom strategies",
    "Custom", // strategyType
    "NIFTY",
    "account1",
    2, // NSEFO
    1.0, // stopLoss
    2.0, // target
    22000.0, // entryPrice
    50, // quantity
    {{"definition", testJSON}}
);

ASSERT_TRUE(instanceId > 0);
StrategyService::instance().startStrategy(instanceId);
```

---

### Week 2: UI Implementation (Days 6-10)

#### Day 6-7: StrategyBuilderDialog UI

**Files to create:**
1. `include/ui/StrategyBuilderDialog.h`
2. `src/ui/StrategyBuilderDialog.cpp`
3. `ui/StrategyBuilderDialog.ui` (Qt Designer)

**UI Layout:**
```
┌────────────────────────────────────────────────────────────┐
│  Create Custom Strategy                               [X]   │
├────────────────────────────────────────────────────────────┤
│  ┌─ Basic Information ─────────────────────────────────┐   │
│  │  Name:      [________________]                       │   │
│  │  Symbol:    [NIFTY ▼]  Segment: [NSEFO ▼]          │   │
│  │  Timeframe: [5 min ▼]  Account: [Account1 ▼]       │   │
│  └───────────────────────────────────────────────────────┘   │
│                                                              │
│  ┌─ Indicators ─────────────────────────────────────────┐   │
│  │  Available:         Selected:                        │   │
│  │  ┌──────────────┐  ┌──────────────┐                │   │
│  │  │ SMA          │  │ RSI (14)     │  [Settings]    │   │
│  │  │ EMA          │  │ SMA (20)     │  [Remove]      │   │
│  │  │ RSI          │  │ SMA (50)     │                │   │
│  │  │ MACD         │  └──────────────┘                │   │
│  │  │ BB           │       [>> Add]                    │   │
│  │  └──────────────┘                                   │   │
│  └───────────────────────────────────────────────────────┘   │
│                                                              │
│  ┌─ Entry Conditions ───────────────────────────────────┐   │
│  │  Logic: (●) AND  ( ) OR                              │   │
│  │  ┌────────────────────────────────────────────────┐ │   │
│  │  │ [RSI(14) ▼] [<] [30____]         [X Remove]   │ │   │
│  │  │ [Price   ▼] [>] [SMA(20)▼]       [X Remove]   │ │   │
│  │  │                        [+ Add Condition]        │ │   │
│  │  └────────────────────────────────────────────────┘ │   │
│  └───────────────────────────────────────────────────────┘   │
│                                                              │
│  ┌─ Exit Conditions ─────────────────────────────────────┐   │
│  │  Stop Loss: [1.0__] %    Target: [2.0__] %          │   │
│  │                                                       │   │
│  │  □ Trailing Stop                                     │   │
│  │    Trigger at: [1.0__] % profit                     │   │
│  │    Trail by:   [0.5__] %                            │   │
│  │                                                       │   │
│  │  □ Time-based Exit                                   │   │
│  │    Exit at: [15:15:00]                              │   │
│  └───────────────────────────────────────────────────────┘   │
│                                                              │
│  ┌─ Risk Management ─────────────────────────────────────┐   │
│  │  Position Size:     [50___] lots/shares              │   │
│  │  Max Positions:     [1____]                          │   │
│  │  Max Daily Loss:    [5000_] ₹                        │   │
│  │  Max Daily Trades:  [10___]                          │   │
│  └───────────────────────────────────────────────────────┘   │
│                                                              │
│  ┌────────────────────────────────────────────────────────┐ │
│  │ Status: ✓ Validation passed                          │ │
│  └────────────────────────────────────────────────────────┘ │
│                                                              │
│       [Validate] [Backtest] [Save Draft] [Deploy Now]      │
└────────────────────────────────────────────────────────────┘
```

**Implementation:**
```cpp
class StrategyBuilderDialog : public QDialog {
    Q_OBJECT
    
public:
    explicit StrategyBuilderDialog(QWidget* parent = nullptr);
    
    StrategyDefinition getDefinition() const;
    
private slots:
    void onAddCondition();
    void onRemoveCondition();
    void onIndicatorSelected();
    void onValidate();
    void onBacktest();
    void onSave();
    void onDeploy();
    
private:
    void setupUI();
    void populateIndicators();
    QJsonObject buildJSON() const;
    bool validateInputs();
    
    // UI components
    QLineEdit* m_nameEdit;
    QComboBox* m_symbolCombo;
    QComboBox* m_segmentCombo;
    QComboBox* m_timeframeCombo;
    
    QListWidget* m_availableIndicators;
    QListWidget* m_selectedIndicators;
    
    QVBoxLayout* m_conditionsLayout;
    QVector<ConditionWidget*> m_conditionWidgets;
    
    QDoubleSpinBox* m_stopLossSpin;
    QDoubleSpinBox* m_targetSpin;
    // ... rest of widgets
};
```

---

#### Day 8-9: ConditionWidget Component

**Files to create:**
1. `include/ui/ConditionWidget.h`
2. `src/ui/ConditionWidget.cpp`

**Widget Layout:**
```
┌────────────────────────────────────────────────────────┐
│ [Indicator▼]  [Operator▼]  [Value____]  [X Remove]   │
│  RSI(14)         <            30                       │
└────────────────────────────────────────────────────────┘
```

**Implementation:**
```cpp
class ConditionWidget : public QWidget {
    Q_OBJECT
    
public:
    explicit ConditionWidget(QWidget* parent = nullptr);
    
    Condition getCondition() const;
    void setCondition(const Condition& cond);
    
signals:
    void conditionChanged();
    void removeRequested(ConditionWidget* self);
    
private slots:
    void onTypeChanged(int index);
    void onIndicatorChanged(int index);
    
private:
    void setupUI();
    void updateIndicatorList();
    
    QComboBox* m_typeCombo;       // Indicator, Price, Time, etc.
    QComboBox* m_indicatorCombo;  // RSI, SMA, MACD, etc.
    QComboBox* m_operatorCombo;   // >, <, ==, !=
    QLineEdit* m_valueEdit;       // 30, 70, etc.
    QPushButton* m_removeBtn;
};

void ConditionWidget::setupUI() {
    QHBoxLayout* layout = new QHBoxLayout(this);
    
    // Type combo
    m_typeCombo = new QComboBox();
    m_typeCombo->addItems({"Indicator", "Price", "Time", "Position Count"});
    layout->addWidget(m_typeCombo);
    
    // Indicator combo
    m_indicatorCombo = new QComboBox();
    updateIndicatorList();
    layout->addWidget(m_indicatorCombo);
    
    // Operator combo
    m_operatorCombo = new QComboBox();
    m_operatorCombo->addItems({">", "<", ">=", "<=", "==", "!="});
    layout->addWidget(m_operatorCombo);
    
    // Value edit
    m_valueEdit = new QLineEdit();
    m_valueEdit->setPlaceholderText("Value");
    layout->addWidget(m_valueEdit);
    
    // Remove button
    m_removeBtn = new QPushButton("X");
    m_removeBtn->setMaximumWidth(30);
    connect(m_removeBtn, &QPushButton::clicked, 
            this, [this]() { emit removeRequested(this); });
    layout->addWidget(m_removeBtn);
    
    // Signals
    connect(m_typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ConditionWidget::onTypeChanged);
}

Condition ConditionWidget::getCondition() const {
    Condition cond;
    cond.type = static_cast<Condition::Type>(m_typeCombo->currentIndex());
    cond.indicator = m_indicatorCombo->currentText();
    cond.operator_ = m_operatorCombo->currentText();
    cond.value = m_valueEdit->text().toDouble();
    return cond;
}
```

---

#### Day 10: Integration & Testing

**Tasks:**
1. Connect StrategyBuilderDialog to main window
2. Add menu item: "Strategies → Create Custom Strategy"
3. Test full workflow

**In MainWindow.cpp:**
```cpp
void MainWindow::onCreateCustomStrategy() {
    StrategyBuilderDialog dialog(this);
    
    if (dialog.exec() == QDialog::Accepted) {
        StrategyDefinition def = dialog.getDefinition();
        
        // Convert to JSON
        QJsonObject json = StrategyParser::toJSON(def);
        QString jsonStr = QJsonDocument(json).toJson();
        
        // Create strategy instance
        qint64 instanceId = StrategyService::instance().createInstance(
            def.name,
            "Custom strategy",
            "Custom",
            def.symbol,
            "account1",
            def.segment,
            def.riskManagement.stopLossPercent,
            def.riskManagement.targetPercent,
            0.0, // entryPrice (will be determined at runtime)
            50,  // quantity
            {{"definition", jsonStr}}
        );
        
        if (instanceId > 0) {
            QMessageBox::information(this, "Success",
                "Strategy created successfully!");
                
            // Auto-start if user wants
            int ret = QMessageBox::question(this, "Start Strategy?",
                "Do you want to start the strategy now?");
            if (ret == QMessageBox::Yes) {
                StrategyService::instance().startStrategy(instanceId);
            }
        }
    }
}
```

---

### Week 3-4: Testing & Refinement

#### Unit Tests
```cpp
// tests/test_strategy_parser.cpp
TEST(StrategyParser, ParseValidJSON) {
    QString json = R"({
        "name": "Test Strategy",
        "symbol": "NIFTY",
        "entry_rules": {
            "long_entry": {
                "operator": "AND",
                "conditions": [
                    {"indicator": "RSI", "operator": "<", "value": 30}
                ]
            }
        }
    })";
    
    QString error;
    StrategyDefinition def = StrategyParser::parseJSON(
        QJsonDocument::fromJson(json.toUtf8()).object(), error);
    
    EXPECT_TRUE(error.isEmpty());
    EXPECT_EQ(def.name, "Test Strategy");
    EXPECT_EQ(def.symbol, "NIFTY");
}

TEST(StrategyParser, DetectInvalidIndicator) {
    QString json = R"({
        "name": "Invalid",
        "indicators": ["INVALID_INDICATOR"]
    })";
    
    QString error;
    StrategyDefinition def = StrategyParser::parseJSON(
        QJsonDocument::fromJson(json.toUtf8()).object(), error);
    
    EXPECT_FALSE(error.isEmpty());
    EXPECT_TRUE(error.contains("Invalid indicator"));
}
```

#### Integration Tests
```cpp
// tests/test_custom_strategy.cpp
TEST(CustomStrategy, EvaluateSimpleCondition) {
    // Setup
    CustomStrategy strategy;
    StrategyInstance instance;
    instance.parameters["definition"] = createTestJSON();
    
    strategy.init(instance);
    strategy.start();
    
    // Simulate market tick with RSI < 30
    UDP::MarketTick tick;
    tick.ltp = 22000.0;
    
    // Mock indicator value
    IndicatorService::instance().setMockValue("RSI", 28.0);
    
    strategy.onTick(tick);
    
    // Verify entry signal generated
    EXPECT_TRUE(strategy.hasEntrySignal());
}
```

---

## Integration with Existing Code

### Files to Create (NEW)

```
include/
├── strategy/
│   ├── StrategyDefinition.h      ✨ NEW
│   ├── StrategyParser.h          ✨ NEW
│   └── CustomStrategy.h          ✨ NEW
├── ui/
│   ├── StrategyBuilderDialog.h   ✨ NEW
│   └── ConditionWidget.h         ✨ NEW

src/
├── strategy/
│   ├── StrategyParser.cpp        ✨ NEW
│   └── CustomStrategy.cpp        ✨ NEW
├── ui/
│   ├── StrategyBuilderDialog.cpp ✨ NEW
│   └── ConditionWidget.cpp       ✨ NEW

ui/
└── StrategyBuilderDialog.ui      ✨ NEW

tests/
├── test_strategy_parser.cpp      ✨ NEW
└── test_custom_strategy.cpp      ✨ NEW
```

### Files to Modify (EXISTING)

```
src/services/StrategyService.cpp
  + Add "Custom" strategy type handling
  + Register CustomStrategy in factory

src/ui/MainWindow.cpp
  + Add menu item: "Strategies → Create Custom"
  + Connect to StrategyBuilderDialog

CMakeLists.txt
  + Add new source files
  + Add new header files
  + Add test files
```

### CMakeLists.txt Changes

```cmake
# Add new source files
set(SOURCES
    # ... existing sources ...
    
    # Custom Strategy Builder
    src/strategy/StrategyParser.cpp
    src/strategy/CustomStrategy.cpp
    src/ui/StrategyBuilderDialog.cpp
    src/ui/ConditionWidget.cpp
)

set(HEADERS
    # ... existing headers ...
    
    # Custom Strategy Builder
    include/strategy/StrategyDefinition.h
    include/strategy/StrategyParser.h
    include/strategies/CustomStrategy.h
    include/ui/StrategyBuilderDialog.h
    include/ui/ConditionWidget.h
)

set(UI_FILES
    # ... existing UI files ...
    
    # Custom Strategy Builder
    ui/StrategyBuilderDialog.ui
)

# Tests
if(BUILD_TESTING)
    add_executable(test_strategy_parser 
        tests/test_strategy_parser.cpp
        src/strategy/StrategyParser.cpp
    )
    target_link_libraries(test_strategy_parser GTest::GTest GTest::Main Qt5::Core)
    add_test(NAME test_strategy_parser COMMAND test_strategy_parser)
    
    add_executable(test_custom_strategy
        tests/test_custom_strategy.cpp
        src/strategy/CustomStrategy.cpp
    )
    target_link_libraries(test_custom_strategy GTest::GTest GTest::Main Qt5::Core)
    add_test(NAME test_custom_strategy COMMAND test_custom_strategy)
endif()
```

---

## Code Examples

### Example 1: Simple RSI Strategy

**User creates this through UI:**
```
Name: RSI Reversal
Symbol: NIFTY
Entry: RSI(14) < 30 AND Positions == 0
Exit: RSI(14) > 70 OR Stop Loss OR Target
Stop Loss: 1%
Target: 2%
Quantity: 50
```

**Generated JSON:**
```json
{
  "strategy_id": "rsi_reversal_001",
  "name": "RSI Reversal",
  "symbol": "NIFTY",
  "segment": 2,
  "timeframe": "5m",
  
  "indicators": ["RSI_14"],
  
  "entry_rules": {
    "long_entry": {
      "operator": "AND",
      "conditions": [
        {"type": "indicator", "indicator": "RSI_14", "operator": "<", "value": 30},
        {"type": "position_count", "operator": "==", "value": 0}
      ]
    }
  },
  
  "exit_rules": {
    "target": {"type": "percentage", "value": 2.0},
    "stop_loss": {"type": "percentage", "value": 1.0}
  },
  
  "risk_management": {
    "position_size": 50,
    "max_positions": 1,
    "max_daily_loss": 5000,
    "max_daily_trades": 10
  }
}
```

**How it executes:**
```cpp
void CustomStrategy::onTick(const MarketTick& tick) {
    // Update RSI
    double rsi = IndicatorService::instance().getRSI(m_symbol, 14);
    m_indicators["RSI_14"] = rsi;
    
    if (!m_inPosition) {
        // Check entry: RSI < 30 AND positions == 0
        if (rsi < 30.0 && m_instance.activePositions == 0) {
            placeEntryOrder("BUY");
            log("Entry: RSI=" + QString::number(rsi));
        }
    } else {
        // Check exit: RSI > 70 OR SL/Target
        if (rsi > 70.0) {
            placeExitOrder();
            log("Exit: RSI overbought =" + QString::number(rsi));
        } else {
            double pnl = (tick.ltp - m_entryPrice) / m_entryPrice * 100.0;
            
            if (pnl <= -1.0) {
                placeExitOrder();
                log("Exit: Stop loss hit");
            } else if (pnl >= 2.0) {
                placeExitOrder();
                log("Exit: Target reached");
            }
        }
    }
}
```

---

### Example 2: MA Crossover with Volume Filter

**User creates:**
```
Name: MA Crossover + Volume
Entry: SMA(20) > SMA(50) AND Volume > AvgVolume(20) * 1.5
Exit: SMA(20) < SMA(50) OR Stop Loss OR Target
```

**Generated JSON:**
```json
{
  "name": "MA Crossover + Volume",
  "symbol": "BANKNIFTY",
  "indicators": ["SMA_20", "SMA_50", "VOLUME"],
  
  "entry_rules": {
    "long_entry": {
      "operator": "AND",
      "conditions": [
        {"type": "indicator", "indicator": "SMA_20", "operator": ">", "value": "SMA_50"},
        {"type": "indicator", "indicator": "VOLUME", "operator": ">", "value": "AVGVOLUME_20 * 1.5"}
      ]
    }
  }
}
```

---

### Example 3: Complex Multi-Condition

**User creates:**
```
Entry: (RSI < 30 OR MACD_HISTOGRAM > 0) AND (Price > SMA200) AND (Time between 9:30-15:00)
```

**Generated JSON (nested groups):**
```json
{
  "entry_rules": {
    "long_entry": {
      "operator": "AND",
      "groups": [
        {
          "operator": "OR",
          "conditions": [
            {"indicator": "RSI", "operator": "<", "value": 30},
            {"indicator": "MACD_HISTOGRAM", "operator": ">", "value": 0}
          ]
        },
        {
          "operator": "AND",
          "conditions": [
            {"field": "close", "operator": ">", "indicator": "SMA_200"},
            {"type": "time", "operator": "between", "start": "09:30:00", "end": "15:00:00"}
          ]
        }
      ]
    }
  }
}
```

---

## Testing Strategy

### Unit Tests (QtTest / Google Test)

```cpp
// Test 1: Parser validation
TEST(StrategyParser, ValidateIndicator) {
    EXPECT_TRUE(StrategyParser::validateIndicator("RSI"));
    EXPECT_TRUE(StrategyParser::validateIndicator("SMA"));
    EXPECT_FALSE(StrategyParser::validateIndicator("INVALID"));
}

// Test 2: Condition evaluation
TEST(CustomStrategy, EvaluateCondition) {
    Condition cond;
    cond.indicator = "RSI";
    cond.operator_ = "<";
    cond.value = 30.0;
    
    CustomStrategy strategy;
    strategy.m_indicators["RSI"] = 28.0;
    
    EXPECT_TRUE(strategy.evaluateCondition(cond));
}

// Test 3: Complex logic (AND/OR)
TEST(CustomStrategy, EvaluateANDConditionGroup) {
    ConditionGroup group;
    group.logicOperator = ConditionGroup::AND;
    
    Condition c1{.indicator = "RSI", .operator_ = "<", .value = 30};
    Condition c2{.indicator = "MACD", .operator_ = ">", .value = 0};
    group.conditions = {c1, c2};
    
    CustomStrategy strategy;
    strategy.m_indicators["RSI"] = 28.0;
    strategy.m_indicators["MACD"] = 5.0;
    
    EXPECT_TRUE(strategy.evaluateConditionGroup(group));
}
```

---

### Integration Tests

```cpp
// Test full workflow: Create → Start → Tick → Exit
TEST(IntegrationTest, FullStrategyLifecycle) {
    // 1. Create strategy definition
    StrategyDefinition def;
    def.name = "Test Strategy";
    def.symbol = "NIFTY";
    // ... set up conditions
    
    // 2. Convert to JSON
    QJsonObject json = StrategyParser::toJSON(def);
    
    // 3. Create instance through service
    qint64 id = StrategyService::instance().createInstance(
        def.name, "", "Custom", def.symbol, "acc1", 2, 
        1.0, 2.0, 0.0, 50, {{"definition", json}});
    
    ASSERT_TRUE(id > 0);
    
    // 4. Start strategy
    EXPECT_TRUE(StrategyService::instance().startStrategy(id));
    
    // 5. Simulate entry tick
    UDP::MarketTick tick;
    tick.token = 256265; // NIFTY
    tick.ltp = 22000.0;
    
    // Mock RSI < 30
    IndicatorService::instance().setMockValue("RSI", 28.0);
    
    // Trigger tick
    FeedHandler::instance().processTick(tick);
    
    // Wait for order placement
    QTest::qWait(100);
    
    // 6. Verify order placed
    auto orders = OrderService::instance().getOrders();
    EXPECT_EQ(orders.size(), 1);
    EXPECT_EQ(orders[0].side, "BUY");
    
    // 7. Simulate exit tick (RSI > 70)
    tick.ltp = 22400.0;
    IndicatorService::instance().setMockValue("RSI", 72.0);
    FeedHandler::instance().processTick(tick);
    
    QTest::qWait(100);
    
    // 8. Verify exit order
    orders = OrderService::instance().getOrders();
    EXPECT_EQ(orders.size(), 2); // Entry + Exit
    EXPECT_EQ(orders[1].side, "SELL");
    
    // 9. Stop strategy
    StrategyService::instance().stopStrategy(id);
}
```

---

### Manual Testing Checklist

```
□ Open StrategyBuilderDialog
□ Fill in strategy name
□ Select symbol (NIFTY)
□ Add indicator (RSI)
□ Add entry condition (RSI < 30)
□ Set stop loss (1%)
□ Set target (2%)
□ Click "Validate" → Should show green checkmark
□ Click "Save Draft" → Should save to database
□ Click "Deploy Now" → Should create strategy instance
□ Open Strategy Manager → Should see new strategy
□ Click "Start" → Strategy should start
□ Simulate market data → Strategy should evaluate
□ Check logs → Should see condition evaluation
□ Trigger entry condition → Should place order
□ Trigger exit condition → Should exit position
□ Click "Stop" → Strategy should stop cleanly
```

---

## Risk Analysis

### Potential Risks & Mitigation

| Risk | Impact | Probability | Mitigation |
|------|--------|-------------|------------|
| **Infinite loops in conditions** | High (CPU 100%) | Medium | Timeout on eval (50ms max), complexity limit |
| **Invalid indicator calculations** | High (wrong trades) | Low | Validation, unit tests for each indicator |
| **Memory leak (many strategies)** | Medium (crash) | Medium | Resource monitoring, max 100 strategies |
| **Order placement failures** | High (loss) | Medium | Retry logic, fallback, alerts |
| **Risk limit bypass** | Critical (huge loss) | Low | Double-check in multiple layers |
| **UI freeze on backtest** | Low (UX) | High | Run backtest in background thread |
| **Database corruption** | Medium (data loss) | Low | Transaction wrapping, backups |
| **Security: JSON injection** | Low (sandboxed) | Low | Input sanitization, schema validation |

---

### Safeguards to Implement

```cpp
// 1. Timeout protection
bool CustomStrategy::evaluateConditionGroup(
    const ConditionGroup& group) {
    
    QElapsedTimer timer;
    timer.start();
    
    // Evaluate conditions
    bool result = /* ... */;
    
    if (timer.elapsed() > 50) { // 50ms timeout
        log("ERROR: Condition evaluation timeout");
        updateState(StrategyState::Error);
        return false;
    }
    
    return result;
}

// 2. Max complexity limit
bool StrategyParser::validate(const StrategyDefinition& def) {
    int totalConditions = 
        def.longEntryRules.conditions.size() +
        def.shortEntryRules.conditions.size() +
        def.exitRules.conditions.size();
    
    if (totalConditions > 20) {
        return false; // Too complex
    }
    
    return true;
}

// 3. Daily loss circuit breaker
void CustomStrategy::checkRiskLimits() {
    if (m_dailyPnL <= -m_definition.riskManagement.maxDailyLoss) {
        log("CRITICAL: Daily loss limit hit: " + 
            QString::number(m_dailyPnL));
        
        // Force exit all positions
        placeExitOrder();
        
        // Stop strategy
        stop();
        
        // Alert user
        emit emergencyStop(m_instance.instanceId, 
                          "Daily loss limit exceeded");
    }
}

// 4. Order rate limiting
void CustomStrategy::placeEntryOrder(const QString& side) {
    // Prevent order spamming
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - m_lastOrderTime < 1000) { // 1 second cooldown
        log("Warning: Order rate limit, skipping");
        return;
    }
    
    m_lastOrderTime = now;
    
    // Place order
    // ...
}
```

---

## Future Enhancements

### Phase 2 Features (3-6 months)

1. **Backtesting Engine** ⚠️ HIGH DEMAND
   - Historical data replay
   - Performance metrics (Sharpe, drawdown)
   - Walk-forward analysis
   - Monte Carlo simulation

2. **Visual Flow Editor** 🎨 BETTER UX
   - Drag-drop blocks
   - Real-time flow visualization
   - Export to JSON/script

3. **Strategy Templates Library** 📚 USER CONVENIENCE
   - Pre-built strategies (RSI, MACD, Breakout)
   - One-click deployment
   - Parameter customization

4. **Strategy Optimizer** 🔧 ADVANCED
   - Auto-tune parameters
   - Genetic algorithm
   - Grid search
   - Parallel backtesting

5. **Multi-Leg Strategies** 🎯 OPTIONS TRADERS
   - Iron Condor, Straddle, Spread
   - Greeks-based exits
   - Dynamic hedging

---

### Phase 3 Features (6-12 months)

6. **Script Editor (ChaiScript)** 💻 POWER USERS
   - Full programming flexibility
   - Syntax highlighting (QScintilla)
   - Debugger (breakpoints, step-through)

7. **Strategy Marketplace** 💰 MONETIZATION
   - Share strategies (free/paid)
   - Rating & reviews
   - Automatic updates
   - Revenue sharing

8. **Machine Learning Integration** 🤖 CUTTING EDGE
   - Predictive models (LSTM, Random Forest)
   - Feature engineering
   - Model training UI
   - AutoML

9. **Cloud Sync** ☁️ CONVENIENCE
   - Sync strategies across devices
   - Backup to cloud
   - Collaborate with team

10. **API for External Strategies** 🔌 ECOSYSTEM
    - REST API for strategy CRUD
    - Webhook for alerts
    - OAuth for third-party integrations

---

## Glossary

| Term | Definition |
|------|------------|
| **Strategy Builder** | UI tool for creating custom trading strategies without coding |
| **Condition** | A rule like "RSI < 30" that evaluates to true/false |
| **Condition Group** | Multiple conditions combined with AND/OR logic |
| **Entry Signal** | When all entry conditions are met, trigger buy/sell |
| **Exit Signal** | When exit conditions are met, close position |
| **Indicator** | Technical indicator (SMA, RSI, MACD, etc.) calculated from price data |
| **Backtesting** | Testing strategy on historical data to evaluate performance |
| **Sandboxed Execution** | Running strategy in isolated environment (no system access) |
| **Risk Parameters** | Stop loss, target, max loss, position size, etc. |
| **Strategy Definition** | Complete specification of a strategy (JSON format) |
| **Parser** | Component that converts JSON to internal data structures |
| **ChaiScript** | Embedded scripting language for C++ (used in Phase 3) |

---

## References

### External Documentation
- [TA-Lib Documentation](https://ta-lib.org/function.html) - Technical indicators
- [Qt Documentation](https://doc.qt.io/qt-5/) - Qt framework
- [ChaiScript](http://chaiscript.com/) - Scripting engine (Phase 3)
- [TradingView Pine Script](https://www.tradingview.com/pine-script-docs/en/v5/) - Inspiration for syntax
- [Amibroker AFL](https://www.amibroker.com/guide/) - Competing product

### Internal Documentation
- [STRATEGY_BUILDER_IMPLEMENTATION_PLAN.md](STRATEGY_BUILDER_IMPLEMENTATION_PLAN.md) - Original detailed plan
- [StrategyBase.h](../include/strategies/StrategyBase.h) - Base class
- [StrategyService.h](../include/services/StrategyService.h) - Service layer

---

## Appendix A: JSON Schema

```json
{
  "$schema": "http://json-schema.org/draft-07/schema#",
  "title": "Custom Strategy Definition",
  "type": "object",
  "properties": {
    "strategy_id": {"type": "string"},
    "name": {"type": "string"},
    "version": {"type": "string"},
    "symbol": {"type": "string"},
    "segment": {"type": "integer", "enum": [1, 2, 11, 12]},
    "timeframe": {"type": "string", "enum": ["1m", "5m", "15m", "30m", "1h", "1d"]},
    
    "parameters": {
      "type": "object",
      "additionalProperties": true
    },
    
    "indicators": {
      "type": "array",
      "items": {"type": "string"}
    },
    
    "entry_rules": {
      "type": "object",
      "properties": {
        "long_entry": {"$ref": "#/definitions/conditionGroup"},
        "short_entry": {"$ref": "#/definitions/conditionGroup"}
      }
    },
    
    "exit_rules": {
      "type": "object",
      "properties": {
        "target": {"$ref": "#/definitions/exitTarget"},
        "stop_loss": {"$ref": "#/definitions/exitStopLoss"},
        "time_exit": {"$ref": "#/definitions/timeExit"}
      }
    },
    
    "risk_management": {"$ref": "#/definitions/riskParams"}
  },
  
  "required": ["name", "symbol"],
  
  "definitions": {
    "condition": {
      "type": "object",
      "properties": {
        "type": {"type": "string", "enum": ["indicator", "price", "time", "position_count"]},
        "indicator": {"type": "string"},
        "operator": {"type": "string", "enum": [">", "<", ">=", "<=", "==", "!="]},
        "value": {}
      },
      "required": ["type", "operator", "value"]
    },
    
    "conditionGroup": {
      "type": "object",
      "properties": {
        "operator": {"type": "string", "enum": ["AND", "OR"]},
        "conditions": {
          "type": "array",
          "items": {"$ref": "#/definitions/condition"}
        }
      },
      "required": ["operator", "conditions"]
    },
    
    "exitTarget": {
      "type": "object",
      "properties": {
        "type": {"type": "string", "enum": ["percentage", "points", "atr"]},
        "value": {"type": "number"}
      }
    },
    
    "exitStopLoss": {
      "type": "object",
      "properties": {
        "type": {"type": "string", "enum": ["percentage", "points", "atr", "trailing"]},
        "value": {"type": "number"}
      }
    },
    
    "timeExit": {
      "type": "object",
      "properties": {
        "enabled": {"type": "boolean"},
        "time": {"type": "string", "pattern": "^[0-2][0-9]:[0-5][0-9]:[0-5][0-9]$"}
      }
    },
    
    "riskParams": {
      "type": "object",
      "properties": {
        "position_size": {"type": "integer", "minimum": 1},
        "max_positions": {"type": "integer", "minimum": 1},
        "max_daily_loss": {"type": "number", "minimum": 0},
        "max_daily_trades": {"type": "integer", "minimum": 1},
        "trailing_stop": {
          "type": "object",
          "properties": {
            "enabled": {"type": "boolean"},
            "trigger_profit": {"type": "number"},
            "trail_amount": {"type": "number"}
          }
        }
      }
    }
  }
}
```

---

## Appendix B: Sample Strategies

### 1. RSI Oversold/Overbought
```json
{
  "name": "RSI Reversal",
  "symbol": "NIFTY",
  "indicators": ["RSI_14"],
  "entry_rules": {
    "long_entry": {
      "operator": "AND",
      "conditions": [
        {"type": "indicator", "indicator": "RSI_14", "operator": "<", "value": 30}
      ]
    },
    "short_entry": {
      "operator": "AND",
      "conditions": [
        {"type": "indicator", "indicator": "RSI_14", "operator": ">", "value": 70}
      ]
    }
  },
  "exit_rules": {
    "target": {"type": "percentage", "value": 2.0},
    "stop_loss": {"type": "percentage", "value": 1.0}
  }
}
```

### 2. Moving Average Crossover
```json
{
  "name": "MA Crossover",
  "symbol": "BANKNIFTY",
  "indicators": ["SMA_20", "SMA_50"],
  "entry_rules": {
    "long_entry": {
      "operator": "AND",
      "conditions": [
        {"type": "indicator", "indicator": "SMA_20", "operator": ">", "value": "SMA_50"}
      ]
    }
  },
  "exit_rules": {
    "conditions": [
      {"type": "indicator", "indicator": "SMA_20", "operator": "<", "value": "SMA_50"}
    ]
  }
}
```

### 3. Bollinger Band Breakout
```json
{
  "name": "BB Breakout",
  "symbol": "RELIANCE",
  "indicators": ["BB_UPPER", "BB_LOWER", "BB_MIDDLE"],
  "entry_rules": {
    "long_entry": {
      "operator": "AND",
      "conditions": [
        {"type": "price", "operator": ">", "value": "BB_UPPER"},
        {"type": "indicator", "indicator": "VOLUME", "operator": ">", "value": "AVG_VOLUME * 1.5"}
      ]
    }
  }
}
```

---

## Conclusion

This document provides a complete blueprint for implementing the Custom Strategy Builder feature. The recommended approach (JSON-based, Phase 1) can be completed in **4 weeks** and will immediately add significant value to the trading terminal.

**Next Steps:**
1. Review and approve this document
2. Create GitHub issues for each component
3. Begin implementation (Week 1, Day 1)
4. Weekly progress reviews
5. Beta testing with 5-10 users
6. Production release

**Success Metrics:**
- 50+ custom strategies created in first month
- 80%+ user satisfaction rating
- <5% strategy failure rate
- Average creation time < 30 minutes

---

**Document Version:** 1.0  
**Last Updated:** February 17, 2026  
**Status:** Ready for Implementation  
**Estimated Completion:** March 17, 2026 (4 weeks from start)