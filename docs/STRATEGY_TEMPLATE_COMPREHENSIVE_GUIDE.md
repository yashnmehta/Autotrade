# 📘 Strategy Template — Comprehensive Guide

> **Version**: 2.0 | **Updated**: February 2026
> **Covers**: Template creation, dynamic formulas, deployment, and 12 real-world strategy scenarios

---

## Table of Contents

1. [System Overview](#1-system-overview)
2. [Core Concepts](#2-core-concepts)
3. [Step-by-Step: Creating a Template](#3-step-by-step-creating-a-template)
4. [Step-by-Step: Deploying a Template](#4-step-by-step-deploying-a-template)
5. [Formula Language Reference](#5-formula-language-reference)
6. [Strategy Scenarios](#6-strategy-scenarios)
   - [Scenario 1: RSI Mean Reversion](#scenario-1-rsi-mean-reversion-equity)
   - [Scenario 2: VWAP Breakout Scalper](#scenario-2-vwap-breakout-scalper)
   - [Scenario 3: ATR-Based Adaptive SL/Target](#scenario-3-atr-based-adaptive-stoptarget)
   - [Scenario 4: MACD + Bollinger Combo](#scenario-4-macd--bollinger-band-squeeze)
   - [Scenario 5: Option Selling — IV Rank Filter](#scenario-5-option-selling-with-iv-rank-filter)
   - [Scenario 6: Straddle with Delta Hedge](#scenario-6-short-straddle-with-delta-hedge-trigger)
   - [Scenario 7: Pair Trade / Cash-Futures Arbitrage](#scenario-7-pair-trade--cash-futures-spread)
   - [Scenario 8: ORB (Opening Range Breakout)](#scenario-8-opening-range-breakout-orb)
   - [Scenario 9: Multi-Timeframe EMA Crossover](#scenario-9-multi-timeframe-ema-crossover)
   - [Scenario 10: Premium Decay Collector](#scenario-10-premium-decay-collector-theta-scalper)
   - [Scenario 11: Index Futures — Intraday Momentum](#scenario-11-index-futures-intraday-momentum)
   - [Scenario 12: Multi-Leg Iron Condor](#scenario-12-multi-leg-iron-condor-with-adjustment)
7. [Tips & Best Practices](#7-tips--best-practices)
8. [Troubleshooting](#8-troubleshooting)

---

## 1. System Overview

The Strategy Template system allows you to create **reusable strategy blueprints** that are:

- **Symbol-agnostic** — Define logic once, deploy on any instrument
- **Parameter-driven** — Users tune thresholds at deploy time
- **Formula-powered** — Dynamic expressions that update in real-time with market data
- **Condition-tree based** — Flexible AND/OR/nested condition logic

```
┌───────────────────────────┐      ┌──────────────────────┐      ┌──────────────────────┐
│     TEMPLATE BUILDER      │      │    DEPLOY DIALOG     │      │    LIVE RUNTIME      │
│                           │      │                      │      │                      │
│  Define:                  │─────▶│  Bind:               │─────▶│  Every Tick:         │
│  • Symbol Slots           │      │  • REF_1 → NIFTY     │      │  • Eval formulas     │
│  • Indicators             │      │  • TRADE_1 → BANK FUT│      │  • Eval conditions   │
│  • Parameters + Formulas  │      │  • RSI_PERIOD = 14   │      │  • Check risk limits │
│  • Entry/Exit Conditions  │      │  • Override / keep ƒ  │      │  • Place orders      │
│  • Risk Defaults          │      │  • Qty, SL, Target   │      │  • Log + emit signal │
└───────────────────────────┘      └──────────────────────┘      └──────────────────────┘
```

### One Template, Many Deployments

```
Template: "RSI Mean Reversion"
    │
    ├── Deploy #1:  REF_1=NIFTY,  TRADE_1=NIFTY FUT,  RSI_PERIOD=14, THRESHOLD=30
    ├── Deploy #2:  REF_1=BANKNIFTY, TRADE_1=BN FUT,   RSI_PERIOD=10, THRESHOLD=25
    └── Deploy #3:  REF_1=RELIANCE,  TRADE_1=RELIANCE,  RSI_PERIOD=21, THRESHOLD=35
```

---

## 2. Core Concepts

### 2.1 Symbol Slots

Every template defines **symbol slots** — placeholders for instruments.

| Slot Type | Purpose | Example |
|-----------|---------|---------|
| **Reference** | Drives indicators & conditions, NO orders | NIFTY 50 index |
| **Trade** | Orders are placed on this instrument | NIFTY FEB FUT |

**Segments**:
- `NSE_CM` (0) — NSE Cash/Equity
- `NSE_FO` (1) — NSE Futures & Options
- `BSE_CM` (2) — BSE Cash/Equity
- `BSE_FO` (3) — BSE Futures & Options

### 2.2 Indicators

Indicators compute technical values on candle history of a symbol slot.

| Indicator | Params | Outputs |
|-----------|--------|---------|
| RSI | period | value (0-100) |
| SMA | period | value |
| EMA | period | value |
| MACD | fast, slow, signal | macd, signal, hist |
| BBANDS | period, stddev | upper, middle, lower |
| ATR | period | value |
| STOCH | K-period, D-period, slowing | %K, %D |
| ADX | period | adx, +DI, -DI |

**Key**: Indicator periods can be fixed (`14`) or parameterized (`{{RSI_PERIOD}}`).

### 2.3 Parameters

Parameters are values the user fills in at deploy time.

| Type | Example | Purpose |
|------|---------|---------|
| `Int` | RSI_PERIOD = 14 | Integer values, often indicator periods |
| `Double` | OFFSET_PCT = 0.5 | Decimal values, often thresholds |
| `Bool` | USE_TRAILING = true | Toggle flags |
| `String` | EXPIRY = "27FEB26" | Text values |
| `Expression` | SL_LEVEL = `ATR(REF_1,14)*2.5` | **Dynamic formula!** |

### 2.4 Expression Parameters (Dynamic Formulas)

This is the most powerful feature. An `Expression` parameter contains a formula that:

- ✅ **Updates on every tick** with live market data
- ✅ **References other parameters** (e.g. `RSI_PERIOD`)
- ✅ **Calls market data** (e.g. `LTP(REF_1)`, `VWAP(REF_1)`)
- ✅ **Calls indicators** (e.g. `RSI(REF_1, 14)`, `ATR(REF_1, 14)`)
- ✅ **Calls greeks** (e.g. `IV(TRADE_1)`, `DELTA(TRADE_1)`)
- ✅ **Uses conditional logic** (e.g. `IV(TRADE_1) > 25 ? 0.02 : 0.01`)

At deploy time, the user sees the formula and can either:
- **Keep it** → evaluated live at runtime
- **Override** → replace with a fixed value

### 2.5 Conditions

Conditions are **trees** of comparisons:

```
AND
├── Leaf: RSI_1 < {{RSI_THRESHOLD}}
├── Leaf: LTP(REF_1) > ƒ(VWAP(REF_1) * 1.005)
└── OR
    ├── Leaf: VOLUME(REF_1) > 1000000
    └── Leaf: CHANGE_PCT(REF_1) > 0.5
```

**Operand Types**:

| Type | What It Is | Example |
|------|-----------|---------|
| `Price` | Live OHLCV of a symbol | `REF_1.ltp`, `TRADE_1.high` |
| `Indicator` | Computed indicator value | `RSI_1`, `MACD_1.signal` |
| `Constant` | Fixed number | `30`, `70`, `22000` |
| `Parameter` | Deploy-time value | `{{RSI_THRESHOLD}}` |
| `Formula` | **Live expression** | `ATR(REF_1,14)*2.5` |
| `Greek/IV` | Option greeks | `TRADE_1.iv`, `TRADE_1.delta` |
| `Spread` | Bid-ask spread | `TRADE_1.bid_ask_spread` |
| `Total` | Portfolio-level | `mtm_total`, `net_delta` |

**Operators**: `>`, `>=`, `<`, `<=`, `==`, `!=`, `crosses_above`, `crosses_below`

### 2.6 Risk Defaults

| Setting | Default | Description |
|---------|---------|-------------|
| Stop-Loss % | 1.0% | Exit if price drops X% from entry |
| Target % | 2.0% | Exit if price rises X% from entry |
| Trailing Stop | Off | Activate trail after X% profit |
| Time Exit | Off | Force close at specified time |
| Max Daily Trades | 10 | Limit trades per day |
| Max Daily Loss | ₹5,000 | Stop trading if loss exceeds this |

---

## 3. Step-by-Step: Creating a Template

### 3.1 Open the Template Builder

**Menu**: `Strategies → Template Builder` (or click "New Template" button)

### 3.2 Fill in Identity

```
Name:        RSI Mean Reversion
Description: Buy when RSI is oversold, sell when overbought.
             Adaptive stop-loss using ATR.
Mode:        Indicator Based
```

### 3.3 Define Symbol Slots

Click **"+ Add Symbol"** for each slot:

| # | ID | Label | Role | Segment |
|---|-----|-------|------|---------|
| 1 | REF_1 | Reference Index | Reference | NSE_CM |
| 2 | TRADE_1 | Trade Instrument | Trade | NSE_FO |

**Why separate Reference and Trade?**
- You might watch NIFTY 50 spot for RSI signals (Reference)
- But trade NIFTY Futures for execution (Trade)
- This lets you use index data for signals while trading derivatives

### 3.4 Define Indicators

Click **"+ Add Indicator"** for each:

| ID | Type | Symbol | Period | Price Field |
|----|------|--------|--------|-------------|
| RSI_1 | RSI | REF_1 | `{{RSI_PERIOD}}` | close |
| ATR_1 | ATR | REF_1 | `{{ATR_PERIOD}}` | close |

**Notice**: Period is `{{RSI_PERIOD}}` — this means the user sets it at deploy time. If you want a fixed period, just type `14`.

### 3.5 Define Parameters

Click **"+ Add Param"** for each:

| Name | Label | Type | Default | Min | Max | Description |
|------|-------|------|---------|-----|-----|-------------|
| RSI_PERIOD | RSI Period | Int | 14 | 5 | 50 | Lookback period for RSI |
| ATR_PERIOD | ATR Period | Int | 14 | 5 | 50 | Lookback period for ATR |
| RSI_OVERSOLD | RSI Buy Level | Double | 30 | 10 | 45 | RSI oversold threshold |
| RSI_OVERBOUGHT | RSI Sell Level | Double | 70 | 55 | 90 | RSI overbought threshold |
| ATR_MULT | ATR SL Multiplier | Double | 2.5 | 1.0 | 5.0 | SL = ATR × this multiplier |
| SL_LEVEL | Stop-Loss (₹) | **Expression** | `ATR(REF_1, ATR_PERIOD) * ATR_MULT` | — | — | Dynamic SL based on volatility |
| TARGET_LEVEL | Target (₹) | **Expression** | `ATR(REF_1, ATR_PERIOD) * ATR_MULT * 1.5` | — | — | Target = 1.5x SL distance |

**Key Insight**: `SL_LEVEL` and `TARGET_LEVEL` are **Expression** types. They auto-update with every tick — when volatility is high, SL widens; when calm, SL tightens.

### 3.6 Define Entry Conditions

Click **"Entry Conditions"** tab and build the condition tree:

```
AND
├── Leaf 1:
│   Left:  [Indicator]  RSI_1
│   Op:    <
│   Right: [Parameter]  {{RSI_OVERSOLD}}
│
└── Leaf 2:
    Left:  [Price]  REF_1.ltp
    Op:    >
    Right: [Formula]  SMA(REF_1, 50)
```

**Translation**: "Enter when RSI is oversold AND price is above the 50-period SMA (trend filter)"

### 3.7 Define Exit Conditions

```
OR
├── Leaf 1:
│   Left:  [Indicator]  RSI_1
│   Op:    >
│   Right: [Parameter]  {{RSI_OVERBOUGHT}}
│
└── Leaf 2:
    Left:  [Formula]  ABS(LTP(TRADE_1) - ENTRY_PRICE)
    Op:    >
    Right: [Parameter]  {{SL_LEVEL}}
```

**Translation**: "Exit when RSI is overbought OR loss exceeds the dynamic ATR-based stop-loss"

### 3.8 Set Risk Defaults

```
Stop-Loss:       1.5% (backup %, the formula-based SL is primary)
Target:          3.0%
Trailing Stop:   ON (trigger at 1.5%, trail by 0.5%)
Time Exit:       ON at 15:15
Max Daily Trades: 5
Max Daily Loss:  ₹10,000
```

### 3.9 Save

Click **"Save Template"**. The template is stored as JSON and appears in the strategy type dropdown.

---

## 4. Step-by-Step: Deploying a Template

### 4.1 Open Create Strategy Dialog

**Menu**: `Strategies → New Strategy` → Select your template name from the Type dropdown.

### 4.2 Bind Symbols

The UI shows symbol binding widgets for each slot:

```
┌─ REF_1: Reference Index ────────────────────────────────┐
│  Search: [NIFTY____________] → NIFTY 50 (26000, NSE_CM) │
│  Qty: N/A (Reference only)                               │
└──────────────────────────────────────────────────────────┘

┌─ TRADE_1: Trade Instrument ─────────────────────────────┐
│  Search: [NIFTY____________] → NIFTY FEB FUT (49508)     │
│  Qty: [50___]  Expiry: 27FEB26                           │
└──────────────────────────────────────────────────────────┘
```

### 4.3 Fill Parameters

The UI dynamically renders inputs:

```
RSI Period:           [14___]      (Int spinner, range 5-50)
ATR Period:           [14___]      (Int spinner, range 5-50)
RSI Buy Level:        [30.00]      (Double spinner, range 10-45)
RSI Sell Level:       [70.00]      (Double spinner, range 55-90)
ATR SL Multiplier:    [2.50_]      (Double spinner, range 1.0-5.0)

Stop-Loss (₹):        ƒ = ATR(REF_1, ATR_PERIOD) * ATR_MULT
                       [  ] Override with fixed value: [_____]

Target (₹):           ƒ = ATR(REF_1, ATR_PERIOD) * ATR_MULT * 1.5
                       [  ] Override with fixed value: [_____]
```

**For Expression params**: You see the formula badge. Leave it as-is for live evaluation, or check "Override" to use a fixed ₹ value.

### 4.4 Review Conditions

The dialog shows a live preview of conditions with current parameter values:

```
ENTRY:
  RSI_1 < ⚙ RSI_OVERSOLD = 30.00   AND
  REF_1.ltp > ƒ SMA(REF_1, 50)

EXIT:
  RSI_1 > ⚙ RSI_OVERBOUGHT = 70.00   OR
  ƒ ABS(LTP(TRADE_1) - ENTRY_PRICE) > ⚙ SL_LEVEL = ƒ(ATR(...))
```

### 4.5 Review Risk Settings & Deploy

Adjust risk settings if needed (they inherit from template defaults), then click **"Create"**.

The system creates a `StrategyInstance` with `strategyType = "template:<uuid>"` and the `StrategyFactory` creates a `TemplateStrategy` runtime.

---

## 5. Formula Language Reference

### 5.1 Market Data Functions

| Function | Args | Returns | Example |
|----------|------|---------|---------|
| `LTP(sym)` | symbol slot ID | Last Traded Price | `LTP(REF_1)` |
| `OPEN(sym)` | symbol slot ID | Today's open | `OPEN(REF_1)` |
| `HIGH(sym)` | symbol slot ID | Today's high | `HIGH(TRADE_1)` |
| `LOW(sym)` | symbol slot ID | Today's low | `LOW(REF_1)` |
| `CLOSE(sym)` | symbol slot ID | Previous close | `CLOSE(REF_1)` |
| `VOLUME(sym)` | symbol slot ID | Today's volume | `VOLUME(REF_1)` |
| `BID(sym)` | symbol slot ID | Best bid price | `BID(TRADE_1)` |
| `ASK(sym)` | symbol slot ID | Best ask price | `ASK(TRADE_1)` |
| `CHANGE_PCT(sym)` | symbol slot ID | % change from prev close | `CHANGE_PCT(REF_1)` |

### 5.2 Indicator Functions

| Function | Args | Returns | Example |
|----------|------|---------|---------|
| `RSI(sym, period)` | symbol, int | RSI value (0-100) | `RSI(REF_1, 14)` |
| `SMA(sym, period)` | symbol, int | Simple Moving Avg | `SMA(REF_1, 20)` |
| `EMA(sym, period)` | symbol, int | Exponential Moving Avg | `EMA(REF_1, 9)` |
| `ATR(sym, period)` | symbol, int | Average True Range | `ATR(REF_1, 14)` |
| `VWAP(sym)` | symbol | Vol-Weighted Avg Price | `VWAP(REF_1)` |
| `BBANDS_UPPER(sym, period)` | symbol, int | Bollinger Upper Band | `BBANDS_UPPER(REF_1, 20)` |
| `BBANDS_LOWER(sym, period)` | symbol, int | Bollinger Lower Band | `BBANDS_LOWER(REF_1, 20)` |
| `MACD(sym, fast, slow)` | symbol, int, int | MACD Line | `MACD(REF_1, 12, 26)` |
| `MACD_SIGNAL(sym, f, s, sig)` | sym, int, int, int | Signal Line | `MACD_SIGNAL(REF_1, 12, 26, 9)` |

### 5.3 Options Greeks

| Function | Returns | Example |
|----------|---------|---------|
| `IV(sym)` | Implied Volatility | `IV(TRADE_1)` |
| `DELTA(sym)` | Option Delta | `DELTA(TRADE_1)` |
| `GAMMA(sym)` | Option Gamma | `GAMMA(TRADE_1)` |
| `THETA(sym)` | Daily Theta Decay (₹) | `THETA(TRADE_1)` |
| `VEGA(sym)` | Vega (per 1% IV change) | `VEGA(TRADE_1)` |

### 5.4 Portfolio Functions

| Function | Returns | Example |
|----------|---------|---------|
| `MTM()` | Total Mark-to-Market P&L | `MTM() > 5000` |
| `NET_PREMIUM()` | Net premium collected/paid | `NET_PREMIUM()` |
| `NET_DELTA()` | Portfolio delta | `ABS(NET_DELTA()) > 50` |

### 5.5 Math Functions

| Function | Description | Example |
|----------|-------------|---------|
| `ABS(x)` | Absolute value | `ABS(CHANGE_PCT(REF_1))` |
| `MAX(a, b)` | Maximum of two | `MAX(LTP(REF_1), SMA(REF_1, 20))` |
| `MIN(a, b)` | Minimum of two | `MIN(BID(TRADE_1), ASK(TRADE_1))` |
| `ROUND(x)` | Round to nearest int | `ROUND(LTP(REF_1) / 100) * 100` |
| `FLOOR(x)` | Round down | `FLOOR(IV(TRADE_1))` |
| `CEIL(x)` | Round up | `CEIL(ATR(REF_1, 14))` |
| `SQRT(x)` | Square root | `SQRT(VOLUME(REF_1))` |
| `LOG(x)` | Natural logarithm | `LOG(LTP(REF_1))` |
| `POW(a, b)` | Power | `POW(2, 10)` |
| `CLAMP(x, lo, hi)` | Clamp to range | `CLAMP(RSI(REF_1, 14), 20, 80)` |

### 5.6 Operators

| Category | Operators |
|----------|-----------|
| Arithmetic | `+`, `-`, `*`, `/`, `%`, `^` |
| Comparison | `>`, `>=`, `<`, `<=`, `==`, `!=` |
| Logical | `&&` (and), `||` (or), `!` (not) |
| Ternary | `condition ? trueVal : falseVal` |

### 5.7 Formula Examples

```
# Adaptive stop-loss based on volatility
ATR(REF_1, 14) * 2.5

# VWAP-relative entry level (0.5% above VWAP)
VWAP(REF_1) * (1 + OFFSET_PCT / 100)

# Conditional spread based on IV regime
IV(TRADE_1) > 30 ? ATR(REF_1, 14) * 3 : ATR(REF_1, 14) * 2

# Round to nearest strike price (100-point intervals)
ROUND(LTP(REF_1) / 100) * 100

# Dynamic position size based on risk budget
FLOOR(RISK_AMOUNT / (ATR(REF_1, 14) * 2.5))

# Percentage distance from Bollinger upper band
(BBANDS_UPPER(REF_1, 20) - LTP(REF_1)) / LTP(REF_1) * 100

# Weighted indicator combo
RSI(REF_1, 14) * 0.4 + (100 - ADX(REF_1, 14)) * 0.6

# Bid-ask spread as percentage
(ASK(TRADE_1) - BID(TRADE_1)) / LTP(TRADE_1) * 100
```

---

## 6. Strategy Scenarios

---

### Scenario 1: RSI Mean Reversion (Equity)

**Logic**: Buy oversold, sell overbought, with a trend filter.

#### Template Definition

| Section | Configuration |
|---------|--------------|
| **Mode** | Indicator Based |
| **Symbols** | REF_1 (Reference, NSE_CM), TRADE_1 (Trade, NSE_CM) |
| **Indicators** | RSI_1: RSI on REF_1, period=`{{RSI_PERIOD}}` |
|  | SMA_1: SMA on REF_1, period=`{{TREND_PERIOD}}` |
| **Parameters** | RSI_PERIOD (Int, 14), TREND_PERIOD (Int, 50), RSI_OVERSOLD (Double, 30), RSI_OVERBOUGHT (Double, 70) |

#### Entry Condition
```
AND
├── RSI_1  <  {{RSI_OVERSOLD}}           ← RSI below 30
└── REF_1.ltp  >  SMA_1                  ← Price above 50-SMA (uptrend)
```

#### Exit Condition
```
OR
├── RSI_1  >  {{RSI_OVERBOUGHT}}         ← RSI above 70
└── REF_1.ltp  <  SMA_1                  ← Price fell below trend
```

#### Risk
```
SL: 2.0%  |  Target: 4.0%  |  Time Exit: 15:15  |  Max Trades: 3
```

#### Deploy Examples
- **RELIANCE**: REF_1=RELIANCE, TRADE_1=RELIANCE, RSI=14, Oversold=30
- **HDFC BANK**: REF_1=HDFCBANK, TRADE_1=HDFCBANK, RSI=10, Oversold=25

---

### Scenario 2: VWAP Breakout Scalper

**Logic**: Enter when price breaks above VWAP by a configurable offset, exit on reversion.

#### Template Definition

| Section | Configuration |
|---------|--------------|
| **Mode** | Indicator Based |
| **Symbols** | REF_1 (Reference, NSE_FO), TRADE_1 (Trade, NSE_FO) |
| **Indicators** | (none needed — VWAP accessed via formula) |
| **Parameters** | OFFSET_PCT (Double, 0.3, "% above VWAP to trigger entry") |
|  | REVERSION_PCT (Double, 0.1, "% below VWAP to exit") |
|  | ENTRY_LEVEL (Expression, `VWAP(REF_1) * (1 + OFFSET_PCT / 100)`) |
|  | EXIT_LEVEL (Expression, `VWAP(REF_1) * (1 - REVERSION_PCT / 100)`) |
|  | MIN_VOLUME (Double, 500000, "Minimum volume filter") |

#### Entry Condition
```
AND
├── REF_1.ltp  >  ƒ(VWAP(REF_1) * (1 + OFFSET_PCT / 100))    ← Price broke above VWAP+offset
├── ƒ(VOLUME(REF_1))  >  {{MIN_VOLUME}}                        ← Volume confirmation
└── ƒ(CHANGE_PCT(REF_1))  >  0                                 ← Positive momentum
```

#### Exit Condition
```
OR
├── REF_1.ltp  <  ƒ(VWAP(REF_1) * (1 - REVERSION_PCT / 100))  ← Price fell below VWAP
└── ƒ(CHANGE_PCT(REF_1))  <  -0.5                              ← Sharp reversal
```

#### Risk
```
SL: 0.5%  |  Target: 1.0%  |  Trailing: ON (0.3% trigger, 0.2% trail)
Time Exit: 15:10  |  Max Trades: 8
```

#### Why Formula is Powerful Here
The `ENTRY_LEVEL` and `EXIT_LEVEL` are Expression params — VWAP changes throughout the day, so these levels **move dynamically**. A fixed constant would miss the whole point.

---

### Scenario 3: ATR-Based Adaptive Stop/Target

**Logic**: Standard trend-following entry, but stop-loss and target dynamically adapt to market volatility using ATR.

#### Template Definition

| Section | Configuration |
|---------|--------------|
| **Mode** | Indicator Based |
| **Symbols** | REF_1 (Reference, NSE_CM), TRADE_1 (Trade, NSE_FO) |
| **Indicators** | EMA_FAST: EMA on REF_1, period=`{{FAST_PERIOD}}` |
|  | EMA_SLOW: EMA on REF_1, period=`{{SLOW_PERIOD}}` |
|  | ATR_1: ATR on REF_1, period=`{{ATR_PERIOD}}` |
| **Parameters** | FAST_PERIOD (Int, 9) |
|  | SLOW_PERIOD (Int, 21) |
|  | ATR_PERIOD (Int, 14) |
|  | ATR_SL_MULT (Double, 2.0, "SL = ATR × this") |
|  | ATR_TGT_MULT (Double, 3.0, "Target = ATR × this") |
|  | DYNAMIC_SL (Expression, `ATR(REF_1, ATR_PERIOD) * ATR_SL_MULT`) |
|  | DYNAMIC_TARGET (Expression, `ATR(REF_1, ATR_PERIOD) * ATR_TGT_MULT`) |

#### Entry Condition
```
AND
├── EMA_FAST  crosses_above  EMA_SLOW     ← Golden cross
└── ƒ(VOLUME(REF_1))  >  500000           ← Volume confirmation
```

#### Exit Condition
```
OR
├── EMA_FAST  crosses_below  EMA_SLOW     ← Death cross
└── (risk system handles SL/Target using DYNAMIC_SL and DYNAMIC_TARGET)
```

#### Key Formula Insight
```
When ATR = 50 pts (calm market):   SL = 100 pts, Target = 150 pts
When ATR = 150 pts (volatile):     SL = 300 pts, Target = 450 pts
```
The strategy **automatically widens stops in volatile markets** and **tightens in calm markets**. This is impossible with fixed percentage stops.

---

### Scenario 4: MACD + Bollinger Band Squeeze

**Logic**: Enter when MACD turns positive AND price breaks above Bollinger upper band (volatility expansion after squeeze).

#### Template Definition

| Section | Configuration |
|---------|--------------|
| **Mode** | Indicator Based |
| **Symbols** | REF_1 (Reference, NSE_CM) |
| **Indicators** | MACD_1: MACD on REF_1, fast=`{{MACD_FAST}}`, slow=`{{MACD_SLOW}}` |
|  | BB_1: BBANDS on REF_1, period=`{{BB_PERIOD}}` |
| **Parameters** | MACD_FAST (Int, 12), MACD_SLOW (Int, 26), BB_PERIOD (Int, 20) |
|  | BB_SQUEEZE_PCT (Double, 2.0, "BB bandwidth < this % = squeeze") |
|  | BANDWIDTH (Expression, `(BBANDS_UPPER(REF_1, BB_PERIOD) - BBANDS_LOWER(REF_1, BB_PERIOD)) / SMA(REF_1, BB_PERIOD) * 100`) |

#### Entry Condition
```
AND
├── MACD_1.macd  >  0                               ← MACD bullish
├── MACD_1.hist  >  0                               ← Histogram positive
└── REF_1.ltp  >  ƒ(BBANDS_UPPER(REF_1, BB_PERIOD))  ← Price breaks upper band
```

#### Exit Condition
```
OR
├── MACD_1.macd  <  0                               ← MACD turned bearish
└── REF_1.ltp  <  ƒ(SMA(REF_1, BB_PERIOD))          ← Price dropped below middle band
```

---

### Scenario 5: Option Selling with IV Rank Filter

**Logic**: Sell options only when IV is elevated (high IV rank), with delta-based position sizing.

#### Template Definition

| Section | Configuration |
|---------|--------------|
| **Mode** | Option Multi-Leg |
| **Symbols** | REF_1 (Reference, NSE_CM — underlying index) |
|  | TRADE_CE (Trade, NSE_FO — Call option) |
|  | TRADE_PE (Trade, NSE_FO — Put option) |
| **Indicators** | (none — IV accessed directly via Greeks) |
| **Parameters** | IV_MIN_THRESHOLD (Double, 18.0, "Minimum IV to sell options") |
|  | DELTA_LIMIT (Double, 0.25, "Max delta for strike selection") |
|  | PREMIUM_FLOOR (Double, 50.0, "Minimum premium to collect") |
|  | IV_SPREAD (Expression, `IV(TRADE_CE) - IV(TRADE_PE)`, "CE-PE IV skew") |
|  | TOTAL_THETA (Expression, `ABS(THETA(TRADE_CE)) + ABS(THETA(TRADE_PE))`, "Daily theta collected") |

#### Entry Condition
```
AND
├── ƒ(IV(TRADE_CE))  >  {{IV_MIN_THRESHOLD}}          ← IV is elevated
├── ƒ(ABS(DELTA(TRADE_CE)))  <  {{DELTA_LIMIT}}        ← OTM enough
├── ƒ(ABS(DELTA(TRADE_PE)))  <  {{DELTA_LIMIT}}        ← OTM enough
└── ƒ(LTP(TRADE_CE) + LTP(TRADE_PE))  >  {{PREMIUM_FLOOR}}  ← Enough premium
```

#### Exit Condition
```
OR
├── ƒ(MTM())  <  -10000                               ← Max loss ₹10K
├── ƒ(LTP(TRADE_CE) + LTP(TRADE_PE))  <  20           ← Premium decayed enough
└── ƒ(ABS(DELTA(TRADE_CE)))  >  0.45                  ← Strike getting ATM (danger)
```

#### Deploy Example
```
REF_1    → NIFTY 50 (spot index)
TRADE_CE → NIFTY 27FEB26 23500 CE
TRADE_PE → NIFTY 27FEB26 22500 PE
IV Min   → 20 (only sell when INDIA VIX > 20)
Delta    → 0.20
```

---

### Scenario 6: Short Straddle with Delta Hedge Trigger

**Logic**: Sell ATM straddle, hedge when portfolio delta exceeds threshold.

#### Template Definition

| Section | Configuration |
|---------|--------------|
| **Mode** | Option Multi-Leg |
| **Symbols** | REF_1 (Reference, NSE_CM), TRADE_CE (Trade, NSE_FO), TRADE_PE (Trade, NSE_FO) |
| **Parameters** | MAX_DELTA (Double, 50.0, "Hedge when net delta exceeds this") |
|  | PROFIT_TARGET_RS (Double, 5000.0, "Close at this profit") |
|  | MAX_LOSS_RS (Double, 15000.0, "Close at this loss") |
|  | PORTFOLIO_DELTA (Expression, `DELTA(TRADE_CE) * QTY_CE + DELTA(TRADE_PE) * QTY_PE`) |
|  | TOTAL_PREMIUM (Expression, `LTP(TRADE_CE) + LTP(TRADE_PE)`) |

#### Entry Condition
```
AND
├── ƒ(IV(TRADE_CE))  >  15                            ← Minimum IV
└── ƒ(ABS(DELTA(TRADE_CE)) - ABS(DELTA(TRADE_PE)))  <  0.1   ← Near ATM
```

#### Exit Condition
```
OR
├── ƒ(MTM())  >  {{PROFIT_TARGET_RS}}                ← Target hit
├── ƒ(MTM())  <  ƒ(-1 * MAX_LOSS_RS)                ← Stop hit
└── ƒ(ABS(NET_DELTA()))  >  {{MAX_DELTA}}             ← Delta blowup → hedge
```

---

### Scenario 7: Pair Trade / Cash-Futures Spread

**Logic**: Trade the basis (futures premium) between spot and futures. Enter when spread widens, exit when it normalizes.

#### Template Definition

| Section | Configuration |
|---------|--------------|
| **Mode** | Spread |
| **Symbols** | REF_SPOT (Reference, NSE_CM — Spot), REF_FUT (Reference, NSE_FO — Near-month future) |
|  | TRADE_1 (Trade, NSE_FO — same future) |
| **Parameters** | NORMAL_BASIS_PCT (Double, 0.3, "Normal basis in %") |
|  | ENTRY_DEVIATION (Double, 0.5, "Enter when basis deviates this much from normal") |
|  | CURRENT_BASIS (Expression, `(LTP(REF_FUT) - LTP(REF_SPOT)) / LTP(REF_SPOT) * 100`) |
|  | BASIS_DEVIATION (Expression, `ABS(CURRENT_BASIS - NORMAL_BASIS_PCT)`) |

#### Entry Condition
```
AND
├── ƒ(BASIS_DEVIATION)  >  {{ENTRY_DEVIATION}}       ← Spread has widened
└── ƒ(VOLUME(REF_FUT))  >  100000                    ← Enough liquidity
```

#### Exit Condition
```
ƒ(BASIS_DEVIATION)  <  0.1                           ← Spread normalized
```

#### Formula Insight
```
Spot = 23,000 | Future = 23,150 → Basis = 0.65%
Normal Basis = 0.30% | Deviation = 0.35%
Entry at: deviation > 0.50% → future is expensive, sell it

As expiry approaches, basis converges → profit on the convergence
```

---

### Scenario 8: Opening Range Breakout (ORB)

**Logic**: Capture the first N-minute range high/low, enter on breakout with volume confirmation.

#### Template Definition

| Section | Configuration |
|---------|--------------|
| **Mode** | Indicator Based |
| **Symbols** | REF_1 (Reference, NSE_FO), TRADE_1 (Trade, NSE_FO) |
| **Parameters** | ORB_HIGH (Double, 0, "Set manually to first-15min high") |
|  | ORB_LOW (Double, 0, "Set manually to first-15min low") |
|  | BUFFER_PTS (Double, 5, "Points above/below range for entry") |
|  | ORB_RANGE (Expression, `ORB_HIGH - ORB_LOW`, "Range width") |
|  | SL_LEVEL (Expression, `ORB_RANGE * 0.5`, "SL = half the range") |
|  | MIN_RANGE (Double, 30, "Minimum range to trade") |

#### Entry Condition (Long)
```
AND
├── REF_1.ltp  >  ƒ(ORB_HIGH + BUFFER_PTS)           ← Breakout above range
├── ƒ(ORB_RANGE)  >  {{MIN_RANGE}}                   ← Range is meaningful
└── ƒ(VOLUME(REF_1))  >  500000                      ← Volume confirmation
```

#### Exit Condition
```
OR
├── REF_1.ltp  <  ƒ(ORB_HIGH - SL_LEVEL)             ← SL hit
└── (time exit at 15:10)
```

**Note**: `ORB_HIGH` and `ORB_LOW` are set manually after the first 15 minutes. Future enhancement: auto-capture from candle data.

---

### Scenario 9: Multi-Timeframe EMA Crossover

**Logic**: Use daily EMA for trend direction, 5-min EMA for entry timing.

#### Template Definition

| Section | Configuration |
|---------|--------------|
| **Mode** | Indicator Based |
| **Symbols** | REF_1 (Reference, NSE_CM) |
| **Indicators** | EMA_9: EMA, period=9, timeframe=5min |
|  | EMA_21: EMA, period=21, timeframe=5min |
|  | EMA_50D: EMA, period=50, timeframe=Daily |
| **Parameters** | FAST_EMA (Int, 9), SLOW_EMA (Int, 21), TREND_EMA (Int, 50) |

#### Entry Condition
```
AND
├── REF_1.ltp  >  EMA_50D                            ← Daily trend is UP
├── EMA_9  crosses_above  EMA_21                     ← 5-min golden cross
└── ƒ(RSI(REF_1, 14))  >  50                         ← Momentum confirmation
```

#### Exit Condition
```
OR
├── EMA_9  crosses_below  EMA_21                     ← 5-min death cross
└── REF_1.ltp  <  EMA_50D                            ← Daily trend broken
```

---

### Scenario 10: Premium Decay Collector (Theta Scalper)

**Logic**: Sell OTM options in the last few days before expiry when theta decay accelerates.

#### Template Definition

| Section | Configuration |
|---------|--------------|
| **Mode** | Option Multi-Leg |
| **Symbols** | REF_1 (Reference, NSE_CM), TRADE_1 (Trade, NSE_FO — OTM option) |
| **Parameters** | MAX_PREMIUM (Double, 30, "Sell only cheap options") |
|  | MIN_THETA_PCT (Double, 3.0, "Daily theta as % of premium") |
|  | THETA_RATIO (Expression, `ABS(THETA(TRADE_1)) / LTP(TRADE_1) * 100`) |
|  | SAFE_DISTANCE (Expression, `ABS(LTP(REF_1) - ROUND(LTP(REF_1)/100)*100) / LTP(REF_1) * 100`) |

#### Entry Condition
```
AND
├── ƒ(LTP(TRADE_1))  <  {{MAX_PREMIUM}}              ← Option is cheap
├── ƒ(THETA_RATIO)  >  {{MIN_THETA_PCT}}              ← Theta is aggressive
├── ƒ(SAFE_DISTANCE)  >  1.5                          ← Enough distance from ATM
└── ƒ(ABS(DELTA(TRADE_1)))  <  0.15                   ← Deep OTM
```

#### Exit Condition
```
OR
├── ƒ(LTP(TRADE_1))  <  2                             ← Premium decayed to near-zero
├── ƒ(ABS(DELTA(TRADE_1)))  >  0.35                   ← Getting too close to ATM
└── ƒ(MTM())  <  -5000                                ← Max loss
```

---

### Scenario 11: Index Futures — Intraday Momentum

**Logic**: Ride strong momentum moves with adaptive trailing stops.

#### Template Definition

| Section | Configuration |
|---------|--------------|
| **Mode** | Indicator Based |
| **Symbols** | REF_1 (Reference, NSE_CM — NIFTY Spot), TRADE_1 (Trade, NSE_FO — NIFTY FUT) |
| **Indicators** | RSI_1: RSI on REF_1, period=14 |
|  | ATR_1: ATR on REF_1, period=14 |
|  | EMA_20: EMA on REF_1, period=20, timeframe=5min |
| **Parameters** | MOMENTUM_THRESHOLD (Double, 0.3, "Min % change for momentum") |
|  | TRAIL_MULT (Double, 1.5, "Trail distance = ATR × this") |
|  | TRAIL_DISTANCE (Expression, `ATR(REF_1, 14) * TRAIL_MULT`) |
|  | ENTRY_STRENGTH (Expression, `ABS(CHANGE_PCT(REF_1)) * VOLUME(REF_1) / 1000000`) |

#### Entry Condition
```
AND
├── ƒ(ABS(CHANGE_PCT(REF_1)))  >  {{MOMENTUM_THRESHOLD}}  ← Strong move
├── ƒ(ENTRY_STRENGTH)  >  50                               ← Volume-weighted strength
├── RSI_1  >  50                                            ← Bullish momentum
└── REF_1.ltp  >  EMA_20                                   ← Above 20-EMA
```

#### Exit Condition
```
OR
├── ƒ(CHANGE_PCT(REF_1))  <  -0.2                     ← Reversal
├── REF_1.ltp  <  EMA_20                               ← Fell below EMA
└── (trailing stop using TRAIL_DISTANCE)
```

---

### Scenario 12: Multi-Leg Iron Condor with Adjustment

**Logic**: Sell iron condor (OTM call spread + OTM put spread), adjust when one leg is threatened.

#### Template Definition

| Section | Configuration |
|---------|--------------|
| **Mode** | Option Multi-Leg |
| **Symbols** | REF_1 (Reference, NSE_CM — NIFTY Spot) |
|  | TRADE_SC (Trade, NSE_FO — Short Call, OTM) |
|  | TRADE_BC (Trade, NSE_FO — Long Call, further OTM) |
|  | TRADE_SP (Trade, NSE_FO — Short Put, OTM) |
|  | TRADE_BP (Trade, NSE_FO — Long Put, further OTM) |
| **Parameters** | IV_MIN (Double, 15, "Min IV to enter") |
|  | ADJUSTMENT_DELTA (Double, 0.35, "Adjust when short leg delta exceeds this") |
|  | TARGET_PREMIUM_PCT (Double, 50, "Exit when X% of premium captured") |
|  | TOTAL_PREMIUM (Expression, `LTP(TRADE_SC) - LTP(TRADE_BC) + LTP(TRADE_SP) - LTP(TRADE_BP)`) |
|  | MAX_SHORT_DELTA (Expression, `MAX(ABS(DELTA(TRADE_SC)), ABS(DELTA(TRADE_SP)))`) |
|  | CURRENT_WIDTH (Expression, `(LTP(REF_1) - ROUND(LTP(REF_1)/100)*100)`) |

#### Entry Condition
```
AND
├── ƒ(IV(TRADE_SC))  >  {{IV_MIN}}                    ← IV elevated
├── ƒ(ABS(DELTA(TRADE_SC)))  <  0.20                  ← Call is OTM
├── ƒ(ABS(DELTA(TRADE_SP)))  <  0.20                  ← Put is OTM
└── ƒ(TOTAL_PREMIUM)  >  30                            ← Enough premium
```

#### Exit Condition
```
OR
├── ƒ(TOTAL_PREMIUM)  <  ƒ(TOTAL_PREMIUM * (100 - TARGET_PREMIUM_PCT) / 100)
│                                                       ← X% premium captured
├── ƒ(MAX_SHORT_DELTA)  >  {{ADJUSTMENT_DELTA}}        ← Leg threatened
├── ƒ(MTM())  <  -15000                                ← Max loss
└── ƒ(MTM())  >  10000                                 ← Profit target
```

---

## 7. Tips & Best Practices

### Template Design Tips

| Tip | Why |
|-----|-----|
| **Always use Reference + Trade symbols** | Keeps signal generation separate from execution |
| **Parameterize everything** | Users can tune without editing the template |
| **Use Expression for SL/Target** | Fixed % stops don't adapt to market conditions |
| **Add volume filters** | Prevents false signals in low-liquidity periods |
| **Set conservative risk defaults** | Users can loosen, but safe defaults prevent blowups |

### Formula Best Practices

| Practice | Example |
|----------|---------|
| **Use named params in formulas** | `ATR(REF_1, ATR_PERIOD) * SL_MULT` not `ATR(REF_1, 14) * 2.5` |
| **Use ternary for regime switching** | `IV(T) > 30 ? 3.0 : 2.0` |
| **Clamp outputs** | `CLAMP(RSI(REF_1,14), 20, 80)` prevents extreme values |
| **Test formulas at deploy time** | The validation badge (✓/✗) catches syntax errors |
| **Keep formulas readable** | Break complex logic into multiple Expression params |

### Condition Tree Tips

| Tip | Example |
|-----|---------|
| **Use AND for entry (strict)** | All conditions must be true |
| **Use OR for exit (generous)** | Any exit signal should close |
| **Nest for complex logic** | `AND( RSI<30, OR( Vol>1M, Change>0.5% ) )` |
| **Use Formula operands for dynamic levels** | Compare price against live VWAP, not fixed value |

### Common Mistakes to Avoid

| ❌ Don't | ✅ Do |
|----------|------|
| Use fixed SL of 1% on all instruments | Use `ATR * multiplier` for adaptive SL |
| Compare against a constant like `22000` | Compare against `SMA(REF_1, 50)` — it moves |
| Ignore volume | Add `VOLUME(REF_1) > threshold` to filters |
| Use same params for all instruments | Deploy with different params per instrument |
| Forget time exit | Always enable 15:15 exit for intraday |

---

## 8. Troubleshooting

### Template Builder Issues

| Issue | Solution |
|-------|----------|
| "Save" button disabled | Ensure Name, at least 1 symbol, and at least 1 entry condition are set |
| Indicator not showing in condition dropdown | Make sure the indicator is added in the Indicators tab first |
| Parameter reference `{{NAME}}` not resolving | Check the param name matches exactly (case-sensitive) |
| Expression formula shows ✗ error | Check syntax — all function names are UPPERCASE, use parentheses |

### Deploy-Time Issues

| Issue | Solution |
|-------|----------|
| Symbol not found | Ensure master file is loaded and token exists for the selected segment |
| Expression shows "ƒ = ?" | Formula syntax error — check for missing parentheses or unknown functions |
| Can't change a parameter | The template author locked it (`locked = true`) |

### Runtime Issues

| Issue | Solution |
|-------|----------|
| Strategy shows "Error" state | Check console for "ERROR: Template not found" — template may have been deleted |
| Formula returning 0.0 | Indicator may not have enough candle history yet — wait for warmup |
| Conditions never trigger | Lower thresholds or check that symbols are receiving live data |
| Orders not placing | Check that trade symbols are bound and exchange connection is active |

### Formula Errors

| Error | Meaning | Fix |
|-------|---------|-----|
| `Unknown function: XYZ` | Function name not recognized | Check spelling, must be UPPERCASE |
| `Unknown parameter: ABC` | Parameter name not defined | Add it to the Parameters tab |
| `Division by zero` | Denominator evaluated to 0 | Use `MAX(denom, 0.01)` to prevent |
| `Unexpected token` | Syntax error | Check for missing operators or parentheses |

---

## Appendix: Quick Reference Card

### Template Creation Checklist

```
□ 1. Name & Description
□ 2. Mode (Indicator / Option Multi-Leg / Spread)
□ 3. Symbol Slots (Reference + Trade)
□ 4. Indicators (with parameterized periods)
□ 5. Parameters (Int / Double / Bool / Expression)
□ 6. Entry Conditions (AND/OR tree)
□ 7. Exit Conditions (AND/OR tree)
□ 8. Risk Defaults (SL, Target, Trailing, Time Exit)
□ 9. Save & Test Deploy
```

### Formula Cheat Sheet

```
PRICE:    LTP(sym)  OPEN(sym)  HIGH(sym)  LOW(sym)  CLOSE(sym)  BID(sym)  ASK(sym)
VOLUME:   VOLUME(sym)  CHANGE_PCT(sym)
INDIC:    RSI(sym,p)  SMA(sym,p)  EMA(sym,p)  ATR(sym,p)  VWAP(sym)
BANDS:    BBANDS_UPPER(sym,p)  BBANDS_LOWER(sym,p)
MACD:     MACD(sym,f,s)  MACD_SIGNAL(sym,f,s,sig)
GREEKS:   IV(sym)  DELTA(sym)  GAMMA(sym)  THETA(sym)  VEGA(sym)
FOLIO:    MTM()  NET_PREMIUM()  NET_DELTA()
MATH:     ABS(x)  MAX(a,b)  MIN(a,b)  ROUND(x)  FLOOR(x)  CEIL(x)
          SQRT(x)  LOG(x)  POW(a,b)  CLAMP(x,lo,hi)
LOGIC:    &&  ||  !  ? :
ARITH:    +  -  *  /  %  ^
COMPARE:  >  >=  <  <=  ==  !=
```

---

*This guide covers the full capability of the Strategy Template system. Start with Scenario 1 (RSI Mean Reversion) to learn the basics, then progress to Scenarios 5-12 for options and advanced formula usage.*
