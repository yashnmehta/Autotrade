# Algo Trading Without Market Orders - Complete Guide

**Project:** Trading Terminal C++  
**Date:** February 17, 2026  
**Version:** 1.0  
**Purpose:** Comprehensive guide for algo trading execution without market orders (SEBI algo compliance)

---

## Table of Contents

1. [Introduction](#introduction)
2. [Why Market Orders Are Blocked](#why-market-orders-are-blocked)
3. [Available Order Types](#available-order-types)
4. [Limit Order Strategies](#limit-order-strategies)
5. [Price Calculation Methods](#price-calculation-methods)
6. [Execution Quality Metrics](#execution-quality-metrics)
7. [Common Scenarios & Solutions](#common-scenarios--solutions)
8. [Risk Management](#risk-management)
   - 8.1 [Exchange Price Protection Mechanisms](#1-exchange-price-protection-mechanisms)
     - LPPR (Limit Price Protection Range)
     - DRP (Dynamic Price Range / Circuit Filters)
     - TER (Tick-by-Tick Execution Range)
   - 8.2 [Comprehensive Pre-Order Validation](#2-comprehensive-pre-order-validation)
   - 8.3 [Real-Time Price Range Monitoring](#3-real-time-price-range-monitoring)
   - 8.4 [Smart Price Adjustment](#4-smart-price-adjustment)
9. [Code Implementation](#code-implementation)
10. [Best Practices](#best-practices)
11. [Exchange Validation Summary](#exchange-validation-summary)

---

## Introduction

### The Problem

In India, **SEBI (Securities and Exchange Board of India)** requires all **approved algorithmic trading strategies** to:
1. **NOT use Market orders** (to prevent flash crashes)
2. Use **Limit orders only** with price validation
3. Implement **order-to-trade ratio** compliance
4. Have **pre-deployment approval** from exchange

### Impact on Algo Trading

**With Market Orders:**
```cpp
// Simple but NOT ALLOWED
Order order = {
    .type = MARKET,
    .symbol = "NIFTY24000CE",
    .side = BUY,
    .quantity = 50
};
// Exchange executes at ANY available price
// Risk: Slippage, flash crash contribution
```

**Without Market Orders (Compliant):**
```cpp
// Must use Limit orders
Order order = {
    .type = LIMIT,
    .symbol = "NIFTY24000CE",
    .side = BUY,
    .quantity = 50,
    .price = 125.50  // ← MUST specify price
};
// Challenge: What price to use?
// Risk: Order may not fill if price moves
```

### Strategy Requirements

To trade without market orders, you need:
1. **Smart price calculation** (to get fills quickly)
2. **Fill probability optimization** (avoid missing trades)
3. **Slippage control** (don't overpay for urgency)
4. **Order modification** (adjust price if not filling)
5. **Timeout handling** (cancel and retry if stale)
6. **LPPR validation** (Limit Price Protection Range - exchange price limits)
7. **DRP monitoring** (Dynamic Price Range - circuit filter tracking)
8. **TER compliance** (Tick-by-Tick Execution Range - tick size validation)

### Critical Exchange Validations

Every order must pass **three mandatory exchange validations**:

| Validation | What It Checks | Failure Impact |
|------------|----------------|----------------|
| **TER (Tick Size)** | Price is valid tick multiple (₹0.05, ₹0.10, etc.) | Exchange rejects immediately |
| **LPPR (Price Protection)** | Price within ±N% of Last Traded Price | Exchange/broker blocks order |
| **DRP (Circuit Filter)** | Price within daily circuit bands (±5%, ±10%, ±20%) | Order rejected, trading may halt |

**Example:**
```cpp
// BAD: Will be rejected by exchange
Order order = {
    .price = 125.37,  // ❌ Invalid tick (₹0.05 tick size)
    // ... other fields
};

// GOOD: Passes all validations
Order order = {
    .price = 125.35,  // ✅ Valid tick
    // TER: ✅ Multiple of ₹0.05
    // LPPR: ✅ Within ±10% of LTP (if LTP=₹120)
    // DRP: ✅ Within circuit bands (₹110-₹130)
};
```

---

## Why Market Orders Are Blocked

### Regulatory Reasons

| Issue | Market Order Risk | Why Blocked |
|-------|-------------------|-------------|
| **Flash Crashes** | Algos dump unlimited market orders → price collapses | Market orders have no price protection |
| **Market Impact** | Large market orders move prices significantly | Can destabilize markets |
| **Liquidity Drain** | Multiple algos hit market orders → drains liquidity | Exchange liquidity protection |
| **Fat Finger** | Algo error sends massive market order → huge loss | No price limit = unlimited loss |
| **Unfair Advantage** | HFT uses market orders to front-run | Level playing field for all participants |

### SEBI Circular Requirements

**SEBI/HO/MRD/MRD1/CIR/P/2019/65 (May 2019):**
- All algos must use **limit orders only**
- Must have **kill switch** (emergency stop)
- Order-to-trade ratio **< 100:1** (100 orders for 1 fill = max)
- Pre-deployment testing mandatory
- Real-time monitoring required

---

## Available Order Types

### 1. Limit Order (Primary)

**Definition:** Order with specified maximum buy price or minimum sell price

```cpp
Order limitOrder = {
    .type = LIMIT,
    .symbol = "NIFTY24000CE",
    .side = BUY,
    .quantity = 50,
    .price = 125.50,        // Will NOT pay more than this
    .validity = DAY
};
```

**Characteristics:**
- ✅ Price protection (won't overpay)
- ✅ Compliant with SEBI rules
- ⚠️ May not fill if price moves away
- ⚠️ Requires price calculation logic

---

### 2. Limit Order with Modification

**Definition:** Place limit order, modify price if not filling

```cpp
// Initial order
Order order = createLimitOrder(symbol, side, qty, price);
placeOrder(order);

// Wait 2 seconds
sleep(2000);

// Not filled? Modify price
if (!order.isFilled()) {
    double newPrice = aggressivePrice(symbol, side);
    modifyOrder(order.orderId, newPrice);
}
```

**Characteristics:**
- ✅ Higher fill rate than static limit
- ✅ Still price-protected (don't cross spread)
- ⚠️ Requires order modification API
- ⚠️ Exchange modification fees may apply

---

### 3. Iceberg Order (Large Orders)

**Definition:** Large order split into smaller visible chunks

```cpp
Order icebergOrder = {
    .type = LIMIT,
    .symbol = "NIFTY",
    .side = BUY,
    .quantity = 500,           // Total quantity
    .visibleQuantity = 50,     // Show only 50 at a time
    .price = 21500.00,
    .validity = DAY
};
```

**Characteristics:**
- ✅ Hides total order size (reduces market impact)
- ✅ Gradually fills without moving market
- ⚠️ Takes longer to fill (multiple rounds)
- ⚠️ Not all brokers support iceberg

---

### 4. Immediate or Cancel (IOC)

**Definition:** Fill immediately at limit price, cancel remainder

```cpp
Order iocOrder = {
    .type = LIMIT,
    .symbol = "NIFTY24000CE",
    .side = BUY,
    .quantity = 50,
    .price = 125.50,
    .validity = IOC            // Fill NOW or cancel
};
```

**Characteristics:**
- ✅ Fast execution (no waiting)
- ✅ No stale orders (auto-cancel)
- ⚠️ May partially fill (get 30 out of 50)
- ⚠️ Need to handle partial fills

---

### 5. Stop-Limit Order

**Definition:** Limit order activated when trigger price is hit

```cpp
Order stopLimitOrder = {
    .type = STOP_LIMIT,
    .symbol = "NIFTY",
    .side = SELL,              // Stop loss
    .quantity = 50,
    .triggerPrice = 21450,     // Activate when price hits this
    .limitPrice = 21440,       // Then sell at this or better
    .validity = DAY
};
```

**Characteristics:**
- ✅ Automated stop loss (no monitoring needed)
- ✅ Price protection (won't sell too cheap)
- ⚠️ May not fill if price gaps through
- ⚠️ Requires exchange support

---

## Limit Order Strategies

### Strategy 1: Passive (Maker)

**Goal:** Get best price, willing to wait

```cpp
double passivePrice(string symbol, Side side) {
    Quote quote = getQuote(symbol);
    
    if (side == BUY) {
        // Place at bid (join queue, wait for seller to hit us)
        return quote.bid;
    } else {
        // Place at ask (join queue, wait for buyer to hit us)
        return quote.ask;
    }
}
```

**Use Cases:**
- Large orders (minimize market impact)
- Non-urgent entries
- Profit booking (not time-critical)

**Pros:**
- ✅ Best price (at bid/ask)
- ✅ No spread cost
- ✅ May get exchange maker rebate

**Cons:**
- ❌ Low fill probability (waiting for someone to hit you)
- ❌ May miss trade if price moves away
- ❌ Queue position matters (FIFO)

---

### Strategy 2: Aggressive (Taker)

**Goal:** High fill probability, pay spread

```cpp
double aggressivePrice(string symbol, Side side) {
    Quote quote = getQuote(symbol);
    
    if (side == BUY) {
        // Place at ask (pay the spread, immediate fill)
        return quote.ask;
    } else {
        // Place at bid (pay the spread, immediate fill)
        return quote.bid;
    }
}
```

**Use Cases:**
- Stop loss exit (urgent)
- Time-critical entries (breakout, news)
- Small orders (spread cost negligible)

**Pros:**
- ✅ High fill probability (~95%)
- ✅ Fast execution (<1 second)
- ✅ Suitable for urgent orders

**Cons:**
- ❌ Pay the spread (₹0.50 to ₹5 per lot)
- ❌ Higher slippage
- ❌ Exchange taker fee (higher)

---

### Strategy 3: Mid-Point

**Goal:** Balance between price and fill rate

```cpp
double midpointPrice(string symbol, Side side) {
    Quote quote = getQuote(symbol);
    
    // Place at mid-point of spread
    double mid = (quote.bid + quote.ask) / 2.0;
    
    // Round to tick size
    return roundToTickSize(mid, getTickSize(symbol));
}
```

**Use Cases:**
- Medium urgency
- Moderate order sizes
- Balanced strategies

**Pros:**
- ✅ Better price than aggressive
- ✅ Better fill rate than passive
- ✅ Balanced approach

**Cons:**
- ⚠️ Moderate fill probability (~70%)
- ⚠️ May need modification if not filling
- ⚠️ Queue position worse than passive

---

### Strategy 4: Join Best + Offset

**Goal:** Improve queue position without crossing spread

```cpp
double joinBestPrice(string symbol, Side side, int offsetTicks = 1) {
    Quote quote = getQuote(symbol);
    double tickSize = getTickSize(symbol);
    
    if (side == BUY) {
        // Place 1 tick above current bid (better queue position)
        return quote.bid + (offsetTicks * tickSize);
    } else {
        // Place 1 tick below current ask
        return quote.ask - (offsetTicks * tickSize);
    }
}
```

**Use Cases:**
- Want faster fill than passive
- Don't want to pay full spread
- Medium-sized orders

**Pros:**
- ✅ Better queue position than passive
- ✅ Cheaper than aggressive
- ✅ Higher fill rate than passive

**Cons:**
- ⚠️ Still may not fill if spread is wide
- ⚠️ If many algos use this, all compete at same price
- ⚠️ Need to monitor and modify

---

### Strategy 5: TWAP (Time-Weighted Average Price)

**Goal:** Execute large order gradually over time

```cpp
void executeTWAP(string symbol, Side side, int totalQty, int durationMin) {
    int intervals = 10;  // Split into 10 chunks
    int qtyPerInterval = totalQty / intervals;
    int sleepMs = (durationMin * 60 * 1000) / intervals;
    
    for (int i = 0; i < intervals; i++) {
        // Place limit order for chunk
        double price = midpointPrice(symbol, side);
        Order order = createLimitOrder(symbol, side, qtyPerInterval, price);
        placeOrder(order);
        
        // Wait 2 seconds for fill, modify if needed
        sleep(2000);
        if (!order.isFilled()) {
            modifyOrder(order.orderId, aggressivePrice(symbol, side));
        }
        
        // Wait for next interval
        sleep(sleepMs);
    }
}
```

**Use Cases:**
- Very large orders (avoid market impact)
- Non-urgent execution
- Benchmark against TWAP

**Pros:**
- ✅ Minimal market impact
- ✅ Predictable execution pattern
- ✅ Good for large orders (500+ lots)

**Cons:**
- ❌ Slow execution (minutes to hours)
- ❌ Risk of price moving away during execution
- ❌ Requires monitoring and adjustment

---

### Strategy 6: VWAP (Volume-Weighted Average Price)

**Goal:** Execute in proportion to market volume

```cpp
void executeVWAP(string symbol, Side side, int totalQty, int durationMin) {
    // Get historical volume profile
    vector<double> volumeProfile = getVolumeProfile(symbol, durationMin);
    
    int intervals = volumeProfile.size();
    
    for (int i = 0; i < intervals; i++) {
        // Calculate qty based on volume weight
        int qtyForInterval = totalQty * (volumeProfile[i] / sum(volumeProfile));
        
        // Place limit order
        double price = midpointPrice(symbol, side);
        Order order = createLimitOrder(symbol, side, qtyForInterval, price);
        placeOrder(order);
        
        // Adjust if not filling
        monitorAndModify(order, maxWaitSec: 10);
        
        // Wait for next interval
        sleep(60000); // 1 min intervals
    }
}
```

**Use Cases:**
- Large orders (institutional size)
- Want to match market participation
- Benchmark performance against VWAP

**Pros:**
- ✅ Matches market rhythm (higher volume = more aggressive)
- ✅ Better execution than TWAP for volatile markets
- ✅ Industry standard benchmark

**Cons:**
- ❌ Complex to implement
- ❌ Requires historical volume data
- ❌ May miss opportunities if volume is low

---

## Price Calculation Methods

### Method 1: Order Book Analysis

```cpp
struct OrderBookLevel {
    double price;
    int quantity;
};

double calculateSmartPrice(string symbol, Side side, int orderQty) {
    // Get order book depth
    vector<OrderBookLevel> levels = getOrderBook(symbol, side, depth: 5);
    
    // Calculate cumulative quantity at each level
    int cumQty = 0;
    for (auto& level : levels) {
        cumQty += level.quantity;
        
        // If this level can absorb our order, use it
        if (cumQty >= orderQty) {
            return level.price;
        }
    }
    
    // If order is larger than visible depth, use last price + buffer
    double buffer = side == BUY ? 0.10 : -0.10; // 0.10 buffer
    return levels.back().price + buffer;
}
```

---

### Method 2: Spread-Based Pricing

```cpp
double calculateSpreadBasedPrice(string symbol, Side side, double urgency) {
    // urgency: 0.0 (passive) to 1.0 (aggressive)
    
    Quote quote = getQuote(symbol);
    double spread = quote.ask - quote.bid;
    
    if (side == BUY) {
        // bid + (urgency * spread)
        // urgency=0.0 → bid (passive)
        // urgency=0.5 → mid (balanced)
        // urgency=1.0 → ask (aggressive)
        return quote.bid + (urgency * spread);
    } else {
        return quote.ask - (urgency * spread);
    }
}
```

---

### Method 3: Volatility-Adjusted Pricing

```cpp
double calculateVolatilityAdjustedPrice(string symbol, Side side, double urgency) {
    Quote quote = getQuote(symbol);
    double atr = getATR(symbol, period: 14);
    double volatilityBuffer = atr * 0.01; // 1% of ATR
    
    double basePrice;
    if (side == BUY) {
        basePrice = quote.bid + (urgency * (quote.ask - quote.bid));
        // In high volatility, add buffer to ensure fill
        return basePrice + (volatilityBuffer * urgency);
    } else {
        basePrice = quote.ask - (urgency * (quote.ask - quote.bid));
        return basePrice - (volatilityBuffer * urgency);
    }
}
```

---

### Method 4: Time Decay Adjustment

```cpp
double calculateTimeSensitivePrice(string symbol, Side side, 
                                   int secondsSinceSignal, int maxWaitSec) {
    // As time passes, become more aggressive
    double urgency = min(1.0, (double)secondsSinceSignal / maxWaitSec);
    
    return calculateSpreadBasedPrice(symbol, side, urgency);
}

// Usage
void placeOrderWithTimeDecay(Order order, int maxWaitSec) {
    auto startTime = now();
    
    while (true) {
        int elapsed = (now() - startTime).seconds();
        
        // Calculate urgency (increases with time)
        double urgency = (double)elapsed / maxWaitSec;
        
        // Update price
        double price = calculateSpreadBasedPrice(order.symbol, order.side, urgency);
        
        if (elapsed == 0) {
            // Initial placement
            order.price = price;
            placeOrder(order);
        } else {
            // Modify price (more aggressive)
            modifyOrder(order.orderId, price);
        }
        
        // Check fill
        if (order.isFilled() || elapsed >= maxWaitSec) {
            break;
        }
        
        // Wait 2 seconds before next modification
        sleep(2000);
    }
}
```

---

## Execution Quality Metrics

### 1. Fill Rate

```cpp
double fillRate = (double)filledOrders / totalOrders * 100;
// Target: > 95%
```

**Good:** 95%+  
**Acceptable:** 90-95%  
**Poor:** < 90% (too passive, missing trades)

---

### 2. Slippage

```cpp
double slippage = executionPrice - signalPrice;
// For BUY: Positive slippage = paid more than signal price
// For SELL: Negative slippage = received less than signal price
```

**Good:** < 0.05% of price  
**Acceptable:** 0.05% - 0.15%  
**Poor:** > 0.15% (too aggressive, overpaying)

---

### 3. Time to Fill

```cpp
int timeToFill = fillTime - orderPlacementTime; // in seconds
```

**Good:** < 5 seconds  
**Acceptable:** 5-15 seconds  
**Poor:** > 15 seconds (too slow, risk of price movement)

---

### 4. Price Improvement

```cpp
double priceImprovement = signalPrice - executionPrice;
// For BUY: Positive = bought cheaper than signal
// For SELL: Positive = sold higher than signal
```

**Excellent:** > 0.10%  
**Good:** 0% - 0.10%  
**Poor:** < 0% (paid worse than signal)

---

## Common Scenarios & Solutions

### Scenario 1: Stop Loss Execution

**Problem:** Stop loss triggered, need immediate exit, but no market orders allowed

**Solution:**
```cpp
void executeStopLoss(Position pos) {
    string symbol = pos.symbol;
    Side side = pos.side == BUY ? SELL : BUY; // Opposite side
    int qty = pos.quantity;
    
    // Get current market price
    Quote quote = getQuote(symbol);
    
    // Use aggressive pricing (pay spread for immediate exit)
    double stopPrice = side == BUY ? quote.ask : quote.bid;
    
    // Add buffer for slippage (1% worse)
    double buffer = stopPrice * 0.01;
    if (side == BUY) {
        stopPrice += buffer;
    } else {
        stopPrice -= buffer;
    }
    
    // Round to tick size
    stopPrice = roundToTickSize(stopPrice, getTickSize(symbol));
    
    // Place limit order with IOC (fill immediately or cancel)
    Order order = {
        .type = LIMIT,
        .symbol = symbol,
        .side = side,
        .quantity = qty,
        .price = stopPrice,
        .validity = IOC
    };
    
    placeOrder(order);
    
    // Wait 1 second
    sleep(1000);
    
    // If not filled, modify with more aggressive price
    if (!order.isFilled()) {
        double newPrice = side == BUY ? quote.ask * 1.02 : quote.bid * 0.98;
        modifyOrder(order.orderId, roundToTickSize(newPrice, getTickSize(symbol)));
    }
    
    // Wait another second
    sleep(1000);
    
    // Still not filled? Cancel and retry
    if (!order.isFilled()) {
        cancelOrder(order.orderId);
        executeStopLoss(pos); // Recursive retry
    }
}
```

---

### Scenario 2: Entry Order Not Filling

**Problem:** Strategy generated signal, placed limit order, but price moved away

**Solution:**
```cpp
void handleUnfilledEntry(Order order, int maxWaitSec) {
    auto startTime = now();
    
    while ((now() - startTime).seconds() < maxWaitSec) {
        // Check if filled
        if (order.isFilled()) {
            log("Order filled at " + order.executionPrice);
            return;
        }
        
        // Check if signal still valid
        if (!isSignalStillValid(order.strategyId)) {
            log("Signal expired, canceling order");
            cancelOrder(order.orderId);
            return;
        }
        
        // Modify price to be more aggressive
        Quote quote = getQuote(order.symbol);
        double newPrice = order.side == BUY ? quote.bid + (quote.ask - quote.bid) * 0.5
                                             : quote.ask - (quote.ask - quote.bid) * 0.5;
        
        modifyOrder(order.orderId, roundToTickSize(newPrice, getTickSize(order.symbol)));
        
        // Wait 3 seconds
        sleep(3000);
    }
    
    // Timeout - cancel order
    log("Order timeout, canceling");
    cancelOrder(order.orderId);
}
```

---

### Scenario 3: Large Order Execution

**Problem:** Need to execute 500 lots, but will move market if placed at once

**Solution:**
```cpp
void executeLargeOrder(string symbol, Side side, int totalQty) {
    // Split into chunks (10% of typical volume)
    int avgVolume = getAverageVolume(symbol, minutes: 5);
    int chunkSize = avgVolume * 0.1;
    
    int remainingQty = totalQty;
    
    while (remainingQty > 0) {
        int thisChunk = min(chunkSize, remainingQty);
        
        // Place chunk with iceberg if supported
        Order order = {
            .type = LIMIT,
            .symbol = symbol,
            .side = side,
            .quantity = thisChunk,
            .visibleQuantity = thisChunk / 5, // Show only 20%
            .price = midpointPrice(symbol, side),
            .validity = DAY
        };
        
        placeOrder(order);
        
        // Monitor for 30 seconds
        monitorAndModify(order, maxWaitSec: 30);
        
        // Update remaining
        remainingQty -= order.filledQuantity;
        
        // Pause before next chunk (avoid detection)
        sleep(randomInt(10000, 30000)); // 10-30 sec
    }
}
```

---

## Risk Management

### 1. Exchange Price Protection Mechanisms

#### LPPR (Limit Price Protection Range)

**Definition:** Exchange-defined price range beyond which orders are automatically rejected

**NSE/BSE Implementation:**
- Orders beyond certain % from Last Traded Price (LTP) are rejected
- Prevents erroneous orders (fat finger errors)
- Range varies by instrument type

**LPPR Limits by Instrument:**

| Instrument | LPPR Range | Example (LTP = ₹100) |
|------------|------------|----------------------|
| **Equity (Cash)** | ±10% to ±20% | Buy: ₹80-₹120 |
| **Equity Futures** | ±10% | Buy: ₹90-₹110 |
| **Index Futures** | ±10% | Buy: ₹90-₹110 |
| **Options (ITM/ATM)** | ±50% to ±100% | Buy: ₹50-₹150 |
| **Options (Deep OTM)** | ±200% | Buy: ₹5-₹300 |
| **Currency** | ±2.5% | Buy: ₹97.50-₹102.50 |
| **Commodities** | Varies | Depends on commodity |

**Code Implementation:**
```cpp
struct LPPRConfig {
    double equityLPPR = 0.20;        // 20% for equity
    double futuresLPPR = 0.10;       // 10% for futures
    double optionsATMLPPR = 0.50;    // 50% for ATM options
    double optionsOTMLPPR = 2.00;    // 200% for OTM options
    double currencyLPPR = 0.025;     // 2.5% for currency
};

bool validateLPPR(Order order, double ltp) {
    LPPRConfig config;
    double lpprRange = 0.0;
    
    // Determine LPPR based on instrument type
    if (order.segment == "EQ") {
        lpprRange = config.equityLPPR;
    } else if (order.instrumentType == "FUTIDX" || order.instrumentType == "FUTSTK") {
        lpprRange = config.futuresLPPR;
    } else if (order.instrumentType == "OPTIDX" || order.instrumentType == "OPTSTK") {
        // Determine if ATM or OTM
        double strikePrice = extractStrike(order.symbol);
        double spotPrice = getSpotPrice(order.underlyingSymbol);
        double moneyness = abs(strikePrice - spotPrice) / spotPrice;
        
        if (moneyness < 0.05) {
            lpprRange = config.optionsATMLPPR; // ATM (within 5%)
        } else {
            lpprRange = config.optionsOTMLPPR; // OTM
        }
    } else if (order.segment == "CDS") {
        lpprRange = config.currencyLPPR;
    }
    
    // Calculate allowed price range
    double minAllowedPrice = ltp * (1.0 - lpprRange);
    double maxAllowedPrice = ltp * (1.0 + lpprRange);
    
    // Validate order price
    if (order.price < minAllowedPrice || order.price > maxAllowedPrice) {
        log("ERROR: LPPR violation for " + order.symbol);
        log("  LTP: " + ltp);
        log("  Order Price: " + order.price);
        log("  Allowed Range: " + minAllowedPrice + " - " + maxAllowedPrice);
        log("  LPPR: ±" + (lpprRange * 100) + "%");
        return false;
    }
    
    return true;
}
```

---

#### DRP (Dynamic Price Range / Price Bands)

**Definition:** Exchange's real-time price movement limits (circuit filters)

**NSE Price Bands:**
- **±5%**: Blue chip stocks (NIFTY 50 constituents)
- **±10%**: Most stocks
- **±20%**: Small/mid-cap stocks, volatile stocks
- **No limit**: Index futures, liquid derivatives

**Circuit Filter Stages:**

```
Stage 1: ±5% move → 15-minute trading halt
Stage 2: ±10% move → 1-hour trading halt  
Stage 3: ±20% move → Trading halted for the day
```

**DRP Implementation:**
```cpp
struct DRPInfo {
    double lowerBand;           // Lower circuit price
    double upperBand;           // Upper circuit price
    double bandPercent;         // Band percentage (0.05, 0.10, 0.20)
    bool isLowerCircuitHit;     // Currently at lower circuit?
    bool isUpperCircuitHit;     // Currently at upper circuit?
    QDateTime lastUpdated;
};

DRPInfo getDRPInfo(string symbol) {
    // Fetch from exchange API or market data feed
    DRPInfo info;
    
    double ltp = getLastPrice(symbol);
    double prevClose = getPreviousClose(symbol);
    
    // Determine band percentage based on stock classification
    if (isNifty50Stock(symbol)) {
        info.bandPercent = 0.05; // 5%
    } else if (isLargeCap(symbol)) {
        info.bandPercent = 0.10; // 10%
    } else {
        info.bandPercent = 0.20; // 20%
    }
    
    // Calculate bands (from previous close, not LTP)
    info.lowerBand = prevClose * (1.0 - info.bandPercent);
    info.upperBand = prevClose * (1.0 + info.bandPercent);
    
    // Check if circuit hit
    info.isLowerCircuitHit = (ltp <= info.lowerBand);
    info.isUpperCircuitHit = (ltp >= info.upperBand);
    
    info.lastUpdated = QDateTime::currentDateTime();
    
    return info;
}

bool validateDRP(Order order, string symbol) {
    DRPInfo drp = getDRPInfo(symbol);
    
    // Check if price within circuit limits
    if (order.price < drp.lowerBand) {
        log("ERROR: DRP violation - price below lower circuit");
        log("  Order Price: " + order.price);
        log("  Lower Band: " + drp.lowerBand);
        return false;
    }
    
    if (order.price > drp.upperBand) {
        log("ERROR: DRP violation - price above upper circuit");
        log("  Order Price: " + order.price);
        log("  Upper Band: " + drp.upperBand);
        return false;
    }
    
    // Check if circuit is hit (order may not execute)
    if (order.side == BUY && drp.isUpperCircuitHit) {
        log("WARNING: Upper circuit hit, BUY order may not execute");
        alertTrader("Upper circuit hit for " + symbol);
        // Allow order but warn
    }
    
    if (order.side == SELL && drp.isLowerCircuitHit) {
        log("WARNING: Lower circuit hit, SELL order may not execute");
        alertTrader("Lower circuit hit for " + symbol);
        // Allow order but warn
    }
    
    return true;
}
```

---

#### TER (Tick-by-Tick Execution Range)

**Definition:** Minimum price movement (tick size) allowed by exchange

**NSE Tick Size Table:**

| Price Range | Tick Size |
|-------------|-----------|
| **₹0 - ₹50** | ₹0.05 |
| **₹50 - ₹100** | ₹0.05 |
| **₹100 - ₹500** | ₹0.05 |
| **₹500 - ₹1,000** | ₹0.10 |
| **₹1,000 - ₹2,500** | ₹0.25 |
| **₹2,500 - ₹5,000** | ₹0.50 |
| **₹5,000+** | ₹1.00 |

**Options Tick Size:**
- Premium < ₹3: Tick = ₹0.05
- Premium ₹3 - ₹50: Tick = ₹0.05
- Premium > ₹50: Tick = ₹0.05

**Futures Tick Size:**
- Index Futures (NIFTY, BANKNIFTY): ₹0.05
- Stock Futures: Same as underlying stock

**TER Implementation:**
```cpp
struct TickSizeConfig {
    // Equity tick sizes
    map<pair<double, double>, double> equityTickMap = {
        {{0, 500}, 0.05},
        {{500, 1000}, 0.10},
        {{1000, 2500}, 0.25},
        {{2500, 5000}, 0.50},
        {{5000, 999999}, 1.00}
    };
    
    // Options tick size
    double optionsTickSize = 0.05;
    
    // Futures tick size
    double futuresTickSize = 0.05;
};

double getTickSize(string symbol, double price) {
    TickSizeConfig config;
    
    // Determine instrument type
    if (isOption(symbol)) {
        return config.optionsTickSize;
    }
    
    if (isFuture(symbol)) {
        return config.futuresTickSize;
    }
    
    // Equity - based on price range
    for (auto& [priceRange, tickSize] : config.equityTickMap) {
        if (price >= priceRange.first && price < priceRange.second) {
            return tickSize;
        }
    }
    
    return 0.05; // Default
}

double roundToTickSize(double price, double tickSize) {
    // Round to nearest tick
    return round(price / tickSize) * tickSize;
}

bool validateTickSize(Order order) {
    double tickSize = getTickSize(order.symbol, order.price);
    
    // Check if price is valid tick multiple
    double remainder = fmod(order.price, tickSize);
    
    if (abs(remainder) > 0.001) { // Floating point tolerance
        log("ERROR: TER violation - invalid tick size");
        log("  Price: " + order.price);
        log("  Tick Size: " + tickSize);
        log("  Remainder: " + remainder);
        
        // Auto-correct
        double correctedPrice = roundToTickSize(order.price, tickSize);
        log("  Corrected Price: " + correctedPrice);
        
        // Option 1: Auto-correct and proceed
        order.price = correctedPrice;
        return true;
        
        // Option 2: Reject order
        // return false;
    }
    
    return true;
}
```

---

### 2. Comprehensive Pre-Order Validation

**Combined LPPR + DRP + TER Validation:**

```cpp
class OrderValidator {
private:
    LPPRConfig lpprConfig_;
    TickSizeConfig tickConfig_;
    
public:
    struct ValidationResult {
        bool isValid;
        string errorCode;
        string errorMessage;
        double suggestedPrice;  // If auto-correction possible
    };
    
    ValidationResult validateOrder(Order& order) {
        ValidationResult result;
        result.isValid = true;
        
        // Get current market data
        Quote quote = getQuote(order.symbol);
        double ltp = quote.lastPrice;
        
        // ═══════════════════════════════════════════════════════
        // STEP 1: TICK SIZE VALIDATION (TER)
        // ═══════════════════════════════════════════════════════
        double tickSize = getTickSize(order.symbol, order.price);
        double remainder = fmod(order.price, tickSize);
        
        if (abs(remainder) > 0.001) {
            log("Tick size violation detected, auto-correcting");
            order.price = roundToTickSize(order.price, tickSize);
            result.suggestedPrice = order.price;
        }
        
        // ═══════════════════════════════════════════════════════
        // STEP 2: LPPR VALIDATION
        // ═══════════════════════════════════════════════════════
        if (!validateLPPR(order, ltp)) {
            result.isValid = false;
            result.errorCode = "LPPR_VIOLATION";
            result.errorMessage = "Price outside LPPR range";
            
            // Calculate suggested price (within LPPR)
            double lpprRange = getLPPRRange(order);
            if (order.side == BUY) {
                result.suggestedPrice = ltp * (1.0 + lpprRange * 0.9); // 90% of LPPR
            } else {
                result.suggestedPrice = ltp * (1.0 - lpprRange * 0.9);
            }
            
            return result;
        }
        
        // ═══════════════════════════════════════════════════════
        // STEP 3: DRP VALIDATION (Circuit Limits)
        // ═══════════════════════════════════════════════════════
        DRPInfo drp = getDRPInfo(order.symbol);
        
        if (order.price < drp.lowerBand) {
            result.isValid = false;
            result.errorCode = "BELOW_LOWER_CIRCUIT";
            result.errorMessage = "Price below lower circuit band";
            result.suggestedPrice = drp.lowerBand;
            return result;
        }
        
        if (order.price > drp.upperBand) {
            result.isValid = false;
            result.errorCode = "ABOVE_UPPER_CIRCUIT";
            result.errorMessage = "Price above upper circuit band";
            result.suggestedPrice = drp.upperBand;
            return result;
        }
        
        // Circuit hit warnings (not rejections)
        if (order.side == BUY && drp.isUpperCircuitHit) {
            log("WARNING: Upper circuit hit, order may not execute");
            alertTrader("Upper circuit: " + order.symbol);
        }
        
        if (order.side == SELL && drp.isLowerCircuitHit) {
            log("WARNING: Lower circuit hit, order may not execute");
            alertTrader("Lower circuit: " + order.symbol);
        }
        
        // ═══════════════════════════════════════════════════════
        // STEP 4: ADDITIONAL VALIDATIONS
        // ═══════════════════════════════════════════════════════
        
        // Deviation from mid-price check
        double midPrice = (quote.bid + quote.ask) / 2.0;
        double deviation = abs(order.price - midPrice) / midPrice;
        
        if (deviation > 0.10) { // 10% deviation warning
            log("WARNING: Order price " + (deviation * 100) + 
                "% away from mid-price");
        }
        
        // Bid-ask cross check
        if (order.side == BUY && order.price < quote.bid) {
            log("WARNING: Buy price below bid (too passive)");
        }
        
        if (order.side == SELL && order.price > quote.ask) {
            log("WARNING: Sell price above ask (too passive)");
        }
        
        return result;
    }
    
    // Get appropriate LPPR range for instrument
    double getLPPRRange(Order order) {
        if (order.segment == "EQ") {
            return lpprConfig_.equityLPPR;
        } else if (isOption(order.symbol)) {
            double moneyness = calculateMoneyness(order.symbol);
            return moneyness < 0.05 ? lpprConfig_.optionsATMLPPR 
                                    : lpprConfig_.optionsOTMLPPR;
        } else if (isFuture(order.symbol)) {
            return lpprConfig_.futuresLPPR;
        }
        return 0.20; // Default 20%
    }
};
```

---

### 3. Real-Time Price Range Monitoring

```cpp
class PriceRangeMonitor {
private:
    map<string, DRPInfo> drpCache_;
    map<string, double> lpprCache_;
    QTimer* updateTimer_;
    
public:
    PriceRangeMonitor() {
        // Update price ranges every 30 seconds
        updateTimer_ = new QTimer(this);
        connect(updateTimer_, &QTimer::timeout, this, &PriceRangeMonitor::updatePriceRanges);
        updateTimer_->start(30000); // 30 sec
    }
    
    void updatePriceRanges() {
        // Get all active symbols
        QStringList symbols = getActiveSymbols();
        
        for (const QString& symbol : symbols) {
            // Update DRP info
            drpCache_[symbol] = getDRPInfo(symbol);
            
            // Update LPPR range
            double ltp = getLastPrice(symbol);
            lpprCache_[symbol] = ltp;
            
            // Check if circuit approaching
            DRPInfo drp = drpCache_[symbol];
            double proximityPercent = calculateCircuitProximity(symbol, ltp, drp);
            
            if (proximityPercent > 0.90) { // Within 90% of circuit
                alertTrader("WARNING: " + symbol + " approaching circuit (" + 
                           (proximityPercent * 100) + "%)");
            }
        }
    }
    
    double calculateCircuitProximity(string symbol, double ltp, DRPInfo drp) {
        double upperProximity = (ltp - drp.lowerBand) / (drp.upperBand - drp.lowerBand);
        return upperProximity;
    }
    
    bool isSymbolNearCircuit(string symbol, double threshold = 0.95) {
        if (drpCache_.find(symbol) == drpCache_.end()) {
            updatePriceRanges(); // Refresh if not in cache
        }
        
        double ltp = getLastPrice(symbol);
        DRPInfo drp = drpCache_[symbol];
        
        double proximity = calculateCircuitProximity(symbol, ltp, drp);
        return proximity > threshold || proximity < (1.0 - threshold);
    }
};
```

---

### 4. Smart Price Adjustment

**Auto-adjust prices to comply with LPPR + DRP + TER:**

```cpp
double calculateCompliantPrice(Order order, double desiredPrice) {
    // Step 1: Get market data
    double ltp = getLastPrice(order.symbol);
    DRPInfo drp = getDRPInfo(order.symbol);
    double lpprRange = getLPPRRange(order);
    
    // Step 2: Calculate allowed LPPR range
    double lpprMin = ltp * (1.0 - lpprRange);
    double lpprMax = ltp * (1.0 + lpprRange);
    
    // Step 3: Calculate circuit limits (DRP)
    double drpMin = drp.lowerBand;
    double drpMax = drp.upperBand;
    
    // Step 4: Combined limits (narrower of LPPR and DRP)
    double minAllowedPrice = max(lpprMin, drpMin);
    double maxAllowedPrice = min(lpprMax, drpMax);
    
    // Step 5: Clamp desired price to allowed range
    double compliantPrice = min(max(desiredPrice, minAllowedPrice), maxAllowedPrice);
    
    // Step 6: Round to tick size (TER)
    double tickSize = getTickSize(order.symbol, compliantPrice);
    compliantPrice = roundToTickSize(compliantPrice, tickSize);
    
    // Step 7: Validate final price
    if (compliantPrice != desiredPrice) {
        log("Price adjusted for compliance:");
        log("  Desired: " + desiredPrice);
        log("  Compliant: " + compliantPrice);
        log("  Reason: LPPR[" + lpprMin + "-" + lpprMax + "], " +
                   "DRP[" + drpMin + "-" + drpMax + "], " +
                   "TER[" + tickSize + "]");
    }
    
    return compliantPrice;
}

// Usage in order placement
void placeCompliantOrder(Order order) {
    // Calculate compliant price
    order.price = calculateCompliantPrice(order, order.price);
    
    // Validate
    OrderValidator validator;
    ValidationResult result = validator.validateOrder(order);
    
    if (result.isValid) {
        placeOrder(order);
    } else {
        log("Order validation failed: " + result.errorMessage);
        
        if (result.suggestedPrice > 0) {
            log("Suggested price: " + result.suggestedPrice);
            // Auto-retry with suggested price?
            if (config.autoRetryWithSuggestedPrice) {
                order.price = result.suggestedPrice;
                placeOrder(order);
            }
        }
    }
}
```

---

### 5. Price Limit Validation (Legacy)

```cpp
bool validatePriceLimit(Order order) {
    Quote quote = getQuote(order.symbol);
    double spread = quote.ask - quote.bid;
    double midPrice = (quote.bid + quote.ask) / 2.0;
    
    // Check if price is within reasonable range
    double maxDeviation = midPrice * 0.05; // 5% from mid
    
    if (order.side == BUY) {
        if (order.price > midPrice + maxDeviation) {
            log("ERROR: Buy price too high, rejecting order");
            return false;
        }
    } else {
        if (order.price < midPrice - maxDeviation) {
            log("ERROR: Sell price too low, rejecting order");
            return false;
        }
    }
    
    return true;
}
```

---

### 2. Slippage Control

```cpp
bool validateSlippage(Order order, double signalPrice) {
    double slippage = abs(order.price - signalPrice);
    double slippagePercent = slippage / signalPrice * 100;
    
    double maxSlippage = 0.50; // 0.5% max slippage
    
    if (slippagePercent > maxSlippage) {
        log("ERROR: Slippage " + slippagePercent + "% exceeds limit");
        return false;
    }
    
    return true;
}
```

---

### 3. Order Timeout Management

```cpp
void manageOrderTimeout(Order order, int timeoutSec) {
    auto startTime = now();
    
    while ((now() - startTime).seconds() < timeoutSec) {
        if (order.isFilled()) {
            return; // Success
        }
        sleep(1000);
    }
    
    // Timeout - cancel stale order
    log("Order timeout after " + timeoutSec + " seconds");
    cancelOrder(order.orderId);
}
```

---

## Code Implementation

### Execution Engine Class

```cpp
class ExecutionEngine {
private:
    struct ExecutionConfig {
        // Pricing strategy
        enum PricingStrategy {
            PASSIVE,      // At bid/ask (maker)
            AGGRESSIVE,   // Cross spread (taker)
            MIDPOINT,     // Middle of spread
            SMART         // Dynamic based on urgency
        } pricingStrategy = SMART;
        
        // Order behavior
        int maxWaitSeconds = 15;
        int maxModifications = 3;
        int modifyIntervalSeconds = 2;
        bool useIOC = false;
        
        // Risk limits
        double maxSlippagePercent = 0.5;
        double maxPriceDeviation = 0.05; // 5% from mid
        
        // Order splitting
        bool enableOrderSplitting = false;
        int maxChunkSize = 100; // lots
    };
    
    ExecutionConfig config_;
    
public:
    bool executeOrder(Order order) {
        // Validate order
        if (!validateOrder(order)) {
            return false;
        }
        
        // Calculate execution price
        double price = calculatePrice(order);
        order.price = price;
        
        // Check if order splitting needed
        if (config_.enableOrderSplitting && order.quantity > config_.maxChunkSize) {
            return executeSplitOrder(order);
        }
        
        // Place order
        placeOrder(order);
        
        // Monitor and modify
        return monitorAndModify(order);
    }
    
private:
    double calculatePrice(Order order) {
        Quote quote = getQuote(order.symbol);
        
        switch (config_.pricingStrategy) {
            case PASSIVE:
                return order.side == BUY ? quote.bid : quote.ask;
                
            case AGGRESSIVE:
                return order.side == BUY ? quote.ask : quote.bid;
                
            case MIDPOINT:
                return (quote.bid + quote.ask) / 2.0;
                
            case SMART:
                return calculateSmartPrice(order, quote);
        }
    }
    
    double calculateSmartPrice(Order order, Quote quote) {
        // Start passive, become more aggressive over time
        double urgency = 0.3; // Start at 30% urgency
        
        double spread = quote.ask - quote.bid;
        if (order.side == BUY) {
            return quote.bid + (urgency * spread);
        } else {
            return quote.ask - (urgency * spread);
        }
    }
    
    bool monitorAndModify(Order order) {
        auto startTime = now();
        int modificationCount = 0;
        
        while ((now() - startTime).seconds() < config_.maxWaitSeconds) {
            // Check fill
            if (order.isFilled()) {
                return true;
            }
            
            // Wait before modification
            sleep(config_.modifyIntervalSeconds * 1000);
            
            // Check if max modifications reached
            if (modificationCount >= config_.maxModifications) {
                break;
            }
            
            // Increase urgency
            double elapsed = (now() - startTime).seconds();
            double urgency = min(1.0, elapsed / config_.maxWaitSeconds);
            
            // Calculate new price
            double newPrice = calculateSpreadBasedPrice(
                order.symbol, order.side, urgency
            );
            
            // Modify order
            modifyOrder(order.orderId, newPrice);
            modificationCount++;
        }
        
        // Timeout - cancel order
        cancelOrder(order.orderId);
        return false;
    }
    
    bool executeSplitOrder(Order order) {
        int numChunks = ceil((double)order.quantity / config_.maxChunkSize);
        int remainingQty = order.quantity;
        int filledQty = 0;
        
        for (int i = 0; i < numChunks; i++) {
            int chunkQty = min(config_.maxChunkSize, remainingQty);
            
            Order chunkOrder = order;
            chunkOrder.quantity = chunkQty;
            chunkOrder.orderId = generateNewOrderId();
            
            bool success = executeOrder(chunkOrder);
            
            if (success) {
                filledQty += chunkOrder.filledQuantity;
                remainingQty -= chunkOrder.filledQuantity;
            } else {
                // Chunk failed - continue or abort?
                if (filledQty == 0) {
                    // No fills yet - abort
                    return false;
                }
                // Some fills - continue with remaining
            }
            
            // Pause between chunks
            if (i < numChunks - 1) {
                sleep(5000); // 5 second pause
            }
        }
        
        return filledQty > 0;
    }
};
```

---

## Best Practices

### DO's ✅

1. **Always use limit orders** - never market orders (compliance)
2. **Validate LPPR before placement** - check against Last Traded Price
3. **Monitor DRP/circuit limits** - subscribe to real-time circuit updates
4. **Round to tick size (TER)** - always round prices before placing orders
5. **Validate prices** - check deviation from mid price
6. **Monitor and modify** - adjust price if not filling
7. **Set order timeouts** - cancel stale orders
8. **Track slippage** - measure execution quality
9. **Use IOC for urgency** - stop losses, time-critical
10. **Split large orders** - minimize market impact
11. **Add price buffer** - ensure fills (0.1-0.5%)
12. **Cache price ranges** - update LPPR/DRP every 30 seconds
13. **Auto-correct tick violations** - round instead of rejecting
14. **Test thoroughly** - paper trade before live

### DON'Ts ❌

1. **Never cross circuit limits (DRP)** - validate against circuit bands
2. **Never exceed LPPR range** - orders beyond ±10%/±20% will be rejected
3. **Never ignore tick size (TER)** - exchange will reject invalid ticks
4. **Never place without price** - always specify limit price
5. **Never ignore order status** - track fills actively
6. **Never retry infinitely** - set max modifications (3-5)
7. **Never ignore timeouts** - cancel after max wait (10-30 sec)
8. **Never overpay** - validate slippage before placement
9. **Never place huge orders** - split into chunks
10. **Never ignore spread** - wide spread = use passive
11. **Never place bid > ask** or **ask < bid** - validation error
12. **Never assume static price ranges** - LPPR changes with every trade
13. **Never ignore circuit proximity warnings** - 90%+ proximity = high risk
14. **Never skip pre-order validation** - validate LPPR + DRP + TER always

---

## Exchange Validation Summary

### Three-Layer Validation Framework

```
┌─────────────────────────────────────────────────────────────┐
│         EXCHANGE ORDER VALIDATION (3 LAYERS)                 │
└─────────────────────────────────────────────────────────────┘

Layer 1: TICK SIZE (TER) ✓
├─ Validates: Price is valid tick multiple
├─ Example: ₹125.50 with ₹0.05 tick = ✅ Valid
├─ Example: ₹125.37 with ₹0.05 tick = ❌ Invalid
├─ Action: Auto-correct to ₹125.35 or ₹125.40
└─ Exchange: Rejects if invalid tick

Layer 2: LPPR (Price Protection Range) ✓
├─ Validates: Price within ±N% of LTP
├─ Example: LTP=₹100, LPPR=±10% → Range: ₹90-₹110
├─ Example: Order at ₹115 → ❌ Rejected (beyond LPPR)
├─ Action: Adjust to ₹110 (max allowed)
└─ Exchange: Hard rejection (order never reaches exchange)

Layer 3: DRP (Circuit Filters) ✓
├─ Validates: Price within daily circuit bands
├─ Example: Prev Close=₹100, Band=±10% → Range: ₹90-₹110
├─ Example: Order at ₹112 → ❌ Rejected (above circuit)
├─ Action: Adjust to ₹110 (upper circuit) or wait
└─ Exchange: Order rejected, may halt trading if circuit hit

Combined Validation:
→ Order Price must satisfy ALL three layers
→ Final allowed range = min(LPPR, DRP) ∩ Valid Tick Size
```

### Quick Reference Table

| Validation | Scope | Typical Range | Rejection Type |
|------------|-------|---------------|----------------|
| **TER (Tick Size)** | Price precision | ₹0.05 - ₹1.00 | Hard (Immediate) |
| **LPPR** | Price deviation from LTP | ±10% to ±200% | Hard (Pre-trade) |
| **DRP (Circuit)** | Daily price movement | ±5%, ±10%, ±20% | Hard (Intraday) |

---

## Summary

Trading without market orders requires:

1. **Smart Price Calculation**
   - Passive: Bid/Ask (best price, low fill rate)
   - Aggressive: Cross spread (high fill rate, pay spread)
   - Midpoint: Balanced (70% fill rate)
   - Dynamic: Adjust with time/urgency

2. **Exchange Compliance (NEW)**
   - **LPPR Validation**: Check price within ±N% of LTP
   - **DRP Monitoring**: Track circuit limits (±5%, ±10%, ±20%)
   - **TER Enforcement**: Round prices to valid tick size
   - **3-Layer Validation**: TER → LPPR → DRP (all must pass)

3. **Order Modification**
   - Start passive (save spread)
   - Modify to aggressive if not filling
   - Validate each modification against LPPR/DRP
   - Max 3-5 modifications
   - 2-3 second intervals

4. **Execution Quality**
   - Fill rate > 95%
   - Slippage < 0.15%
   - Time to fill < 15 seconds
   - LPPR violation rate: 0% (all orders compliant)

5. **Risk Management**
   - Validate prices (within 5% of mid)
   - Control slippage (max 0.5%)
   - Set timeouts (10-30 seconds)
   - Split large orders
   - **Monitor circuit proximity** (alert at 90%)
   - **Cache price ranges** (update every 30 sec)

6. **SEBI Compliance**
   - Limit orders only
   - Order-to-trade ratio < 100:1
   - Kill switch implemented
   - Pre-deployment approval
   - **LPPR validation mandatory**
   - **Real-time DRP monitoring**

### Critical Implementation Steps

```cpp
// Complete order placement flow with LPPR/DRP/TER
bool placeCompliantOrder(Order order) {
    // Step 1: TER - Round to tick size
    double tickSize = getTickSize(order.symbol, order.price);
    order.price = roundToTickSize(order.price, tickSize);
    
    // Step 2: LPPR - Validate against LTP range
    double ltp = getLastPrice(order.symbol);
    if (!validateLPPR(order, ltp)) {
        order.price = calculateCompliantPrice(order, order.price);
    }
    
    // Step 3: DRP - Validate against circuit limits
    DRPInfo drp = getDRPInfo(order.symbol);
    if (!validateDRP(order, drp)) {
        log("Circuit limit violation, rejecting order");
        return false;
    }
    
    // Step 4: Additional validations (spread, slippage, etc.)
    if (!validatePriceLimit(order)) {
        return false;
    }
    
    // Step 5: Place order
    return placeOrder(order);
}
```

With proper implementation including **LPPR, DRP, and TER validation**, limit-only execution can achieve **95%+ fill rates** with **0% exchange rejections**, meeting regulatory requirements while maintaining strategy performance.

---

**End of Document**

