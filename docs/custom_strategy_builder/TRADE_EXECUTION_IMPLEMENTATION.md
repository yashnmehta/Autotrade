# Trade Execution Implementation - Spread & Multi-Leg Strategies

**Project:** Trading Terminal C++  
**Date:** February 17, 2026  
**Version:** 1.0  
**Purpose:** In-depth guide for executing spread/pair trades with different liquidity levels

---

## Table of Contents

1. [Introduction](#introduction)
2. [The Liquidity Challenge](#the-liquidity-challenge)
3. [Execution Methodologies](#execution-methodologies)
4. [Liquidity Assessment](#liquidity-assessment)
5. [Price Improvement Strategies](#price-improvement-strategies)
6. [Legging Risk Management](#legging-risk-management)
7. [Multi-Leg Execution Modes](#multi-leg-execution-modes)
8. [Order Book Analysis](#order-book-analysis)
9. [Spread Trading Specifics](#spread-trading-specifics)
10. [Pair Trading Specifics](#pair-trading-specifics)
11. [Options Spread Execution](#options-spread-execution)
12. [Calendar Spread Execution](#calendar-spread-execution)
13. [Recovery from Partial Fill](#recovery-from-partial-fill)
14. [Code Implementation](#code-implementation)
15. [Performance Metrics](#performance-metrics)
16. [Best Practices](#best-practices)

---

## Introduction

### The Core Problem

**Spread/Pair trading** involves placing **multiple orders simultaneously or sequentially**. The challenge:

```
Strategy: Buy NIFTY FUT + Sell BANKNIFTY FUT (Pair Trade)

Leg 1: NIFTY       Liquidity: ★★★★★ (Very High) - fills instantly
Leg 2: BANKNIFTY   Liquidity: ★★★★☆ (High)      - fills in 2-3 sec

Risk: Leg 1 fills, Leg 2 doesn't → NAKED POSITION (unhedged)
```

### Why This Matters

| Scenario | Risk | Example |
|----------|------|---------|
| **Single Leg Fill** | Directional risk | Bought NIFTY, didn't sell BANKNIFTY → long NIFTY (naked) |
| **Price Slippage** | Wider spread cost | Leg 1 at good price, Leg 2 slips by ₹50 → spread cost increases |
| **Time Decay** | Options lose value | Leg 1 fills immediately, Leg 2 takes 30 sec → theta decay |
| **Market Movement** | Spread widens | Placed both orders, market moves, spread no longer profitable |

### Strategy Requirements

To execute spreads safely:
1. **Liquidity Assessment** - measure available quantity at each price level
2. **Simultaneous/Sequential** - decide execution order
3. **Price Adjustment** - add buffer (`+ x bps`) to ensure fills
4. **Legging Risk Control** - never leave naked positions
5. **Recovery Procedures** - what if one leg fails?

---

## The Liquidity Challenge

### Liquidity Imbalance Example

**Scenario:** Iron Condor (4 legs)

```
+-------------------------------------------------------------+
| LEG | SYMBOL           | LIQUIDITY DEPTH | TYPICAL SPREAD  |
+-------------------------------------------------------------+
| 1   | NIFTY 23000 PE   | ★★★★★ (200 lots)| ₹0.50          |
| 2   | NIFTY 23500 PE   | ★★★☆☆ (50 lots) | ₹2.00          |
| 3   | NIFTY 24500 CE   | ★★★☆☆ (50 lots) | ₹2.00          |
| 4   | NIFTY 25000 CE   | ★★★★★ (200 lots)| ₹0.50          |
+-------------------------------------------------------------+

Problem:
- ATM strikes (23500 PE, 24500 CE) have lower liquidity
- OTM strikes (23000 PE, 25000 CE) have higher liquidity
- If we place all 4 at bid/ask, ATM strikes may not fill
```

### Real Market Data (Example)

**NIFTY 24000 CE (ATM) - Order Book:**
```
BID SIDE:                    ASK SIDE:
Price    Qty                 Price    Qty
125.00   10  ◄─ Top Bid     125.50   25  ◄─ Top Ask
124.50   25                 126.00   50
124.00   50                 126.50   75
123.50   100                127.00   100
...                         ...

Liquidity Analysis:
- Spread: ₹0.50 (125.50 - 125.00)
- Bid depth (top 3): 85 lots
- Ask depth (top 3): 150 lots
- Our order: 50 lots

Execution Decision:
→ If we place at 125.00 (bid), only 10 lots immediately available
→ If we place at 125.50 (ask), 25 lots immediately available
→ Need to decide: Pay spread OR wait in queue
```

---

## Execution Methodologies

### Methodology 1: Simultaneous Execution (Market Maker Style)

**Philosophy:** Place all legs at same time, accept some unfilled

```cpp
void executeSimultaneous(vector<Order> legs) {
    vector<string> orderIds;
    
    // Place all orders within 100ms window
    for (auto& leg : legs) {
        string orderId = placeOrder(leg);
        orderIds.push_back(orderId);
    }
    
    // Wait 2 seconds
    sleep(2000);
    
    // Check fills
    bool allFilled = true;
    for (auto& orderId : orderIds) {
        if (!isOrderFilled(orderId)) {
            allFilled = false;
            break;
        }
    }
    
    if (allFilled) {
        log("All legs filled successfully");
    } else {
        // Handle partial fill (see section 13)
        handlePartialFill(orderIds);
    }
}
```

**Pros:**
- ✅ Fair execution (no leg has price advantage)
- ✅ Fast (all orders sent together)
- ✅ Minimal market impact

**Cons:**
- ❌ May not all fill (liquidity imbalance)
- ❌ Requires sophisticated partial fill recovery
- ❌ Higher risk of leaving naked legs

**Best For:**
- Liquid instruments (NIFTY, BANKNIFTY ATM)
- Small order sizes (<50 lots)
- Tight spreads (< ₹1)

---

### Methodology 2: Sequential Execution (Priority-Based)

**Philosophy:** Execute legs in order of priority (most risky first)

```cpp
enum LegPriority {
    CRITICAL,    // Short options, stop loss exit
    HIGH,        // Long options (theta risk)
    MEDIUM,      // Futures (directional risk)
    LOW          // Hedge leg (can wait)
};

void executeSequential(vector<Order> legs) {
    // Sort by priority
    sortByPriority(legs);
    
    for (auto& leg : legs) {
        bool filled = executeSingleLeg(leg, maxWaitSec: 10);
        
        if (!filled) {
            log("Leg failed: " + leg.symbol);
            
            // Reverse all filled legs
            reversePreviousLegs(legs);
            return;
        }
    }
    
    log("All legs filled sequentially");
}
```

**Priority Rules:**
1. **Short Options** - highest priority (unlimited risk)
2. **Long Options** - high priority (theta decay)
3. **Futures Hedge** - medium priority
4. **Protective Hedge** - low priority (can wait)

**Pros:**
- ✅ Control over execution order
- ✅ React to each leg's fill before next
- ✅ Clear recovery path (reverse in order)

**Cons:**
- ❌ Slower execution (sequential)
- ❌ Early legs may move price
- ❌ Legging risk (time between legs)

**Best For:**
- Illiquid instruments
- Large order sizes (>100 lots)
- Wide spreads (>₹2)
- Risk-sensitive strategies (short options)

---

### Methodology 3: Hybrid (Leader-Follower)

**Philosophy:** Execute most liquid leg first, then others

```cpp
void executeHybrid(vector<Order> legs) {
    // Identify most liquid leg (leader)
    Order leader = findMostLiquid(legs);
    vector<Order> followers = getOtherLegs(legs, leader);
    
    // Execute leader
    bool leaderFilled = executeSingleLeg(leader, maxWaitSec: 5);
    
    if (!leaderFilled) {
        log("Leader leg failed, aborting");
        return;
    }
    
    // Execute followers simultaneously
    vector<string> followerIds;
    for (auto& follower : followers) {
        followerIds.push_back(placeOrder(follower));
    }
    
    // Wait for followers (with timeout)
    bool allFollowersFilled = waitForFills(followerIds, maxWaitSec: 10);
    
    if (!allFollowersFilled) {
        // Reverse leader + any filled followers
        reverseLeader(leader);
        reverseFilledFollowers(followerIds);
    }
}
```

**Pros:**
- ✅ Leader fills quickly (most liquid)
- ✅ Reduces overall execution time
- ✅ Clear reference price for followers

**Cons:**
- ⚠️ Naked position during follower execution
- ⚠️ If followers fail, must reverse leader (transaction cost)

**Best For:**
- Pairs trading (one liquid, one less liquid)
- Calendar spreads (near-month more liquid)
- Basket trading (core holding + satellites)

---

### Methodology 4: Legging with Safety Buffer

**Philosophy:** Place orders at worse prices to ensure fills

```cpp
void executeWithBuffer(vector<Order> legs, double bufferBps) {
    // bufferBps: 5 = 0.05% = 5 basis points worse
    
    for (auto& leg : legs) {
        Quote quote = getQuote(leg.symbol);
        
        // Calculate buffer price
        double buffer = quote.midPrice() * (bufferBps / 10000.0);
        
        if (leg.side == BUY) {
            // Pay more (worse for us, better for seller)
            leg.price = quote.ask + buffer;
        } else {
            // Receive less (worse for us, better for buyer)
            leg.price = quote.bid - buffer;
        }
        
        // Round to tick size
        leg.price = roundToTickSize(leg.price, getTickSize(leg.symbol));
        
        // Place order
        placeOrder(leg);
    }
    
    // Monitor fills
    monitorAndModify(legs, maxWaitSec: 15);
}
```

**Buffer Examples:**

| Instrument | Typical Spread | Recommended Buffer | Rationale |
|------------|----------------|---------------------|-----------|
| NIFTY ATM | ₹0.50 | 5 bps (₹0.10)       | Already liquid |
| NIFTY OTM | ₹2.00 | 10 bps (₹0.40)      | Less liquid |
| BANKNIFTY ATM | ₹1.00 | 10 bps (₹0.20)  | Moderate liquidity |
| Stock Options | ₹5.00 | 20 bps (₹1.00)      | Illiquid |

**Pros:**
- ✅ High fill probability (paying for certainty)
- ✅ All legs fill together (low legging risk)
- ✅ Predictable execution cost

**Cons:**
- ❌ Pay extra spread cost (buffer)
- ❌ May overpay if market is favorable
- ❌ Eats into strategy profit

**Best For:**
- Time-sensitive strategies (news, events)
- Risk-averse execution (no naked positions)
- Backtested with slippage assumptions

---

## Liquidity Assessment

### Real-Time Liquidity Metrics

```cpp
struct LiquidityMetrics {
    string symbol;
    
    // Order book depth
    int bidDepth;           // Lots available at top 3 bid levels
    int askDepth;           // Lots available at top 3 ask levels
    
    // Spread metrics
    double spread;          // Ask - Bid
    double spreadPercent;   // (Spread / Mid) * 100
    
    // Volume metrics
    int volumeLast1Min;
    int volumeLast5Min;
    int avgDailyVolume;
    
    // Liquidity score (0-100)
    int liquidityScore;
    
    // Recommended execution
    string recommendedMethod; // SIMULTANEOUS/SEQUENTIAL/HYBRID
};

LiquidityMetrics assessLiquidity(string symbol) {
    LiquidityMetrics metrics;
    metrics.symbol = symbol;
    
    // Get order book
    OrderBook ob = getOrderBook(symbol, depth: 5);
    
    // Calculate bid/ask depth (top 3 levels)
    metrics.bidDepth = sumQuantity(ob.bids, levels: 3);
    metrics.askDepth = sumQuantity(ob.asks, levels: 3);
    
    // Spread
    metrics.spread = ob.asks[0].price - ob.bids[0].price;
    double midPrice = (ob.asks[0].price + ob.bids[0].price) / 2.0;
    metrics.spreadPercent = (metrics.spread / midPrice) * 100;
    
    // Volume
    metrics.volumeLast1Min = getVolume(symbol, minutes: 1);
    metrics.volumeLast5Min = getVolume(symbol, minutes: 5);
    metrics.avgDailyVolume = getAvgVolume(symbol, days: 20);
    
    // Calculate liquidity score (0-100)
    int score = 0;
    
    // Spread component (40 points max)
    if (metrics.spreadPercent < 0.1) score += 40;
    else if (metrics.spreadPercent < 0.5) score += 30;
    else if (metrics.spreadPercent < 1.0) score += 20;
    else if (metrics.spreadPercent < 2.0) score += 10;
    
    // Depth component (30 points max)
    int totalDepth = metrics.bidDepth + metrics.askDepth;
    if (totalDepth > 200) score += 30;
    else if (totalDepth > 100) score += 20;
    else if (totalDepth > 50) score += 10;
    else if (totalDepth > 20) score += 5;
    
    // Volume component (30 points max)
    if (metrics.volumeLast1Min > 50) score += 30;
    else if (metrics.volumeLast1Min > 20) score += 20;
    else if (metrics.volumeLast1Min > 10) score += 10;
    else if (metrics.volumeLast1Min > 5) score += 5;
    
    metrics.liquidityScore = score;
    
    // Recommend execution method
    if (score >= 70) {
        metrics.recommendedMethod = "SIMULTANEOUS";
    } else if (score >= 40) {
        metrics.recommendedMethod = "HYBRID";
    } else {
        metrics.recommendedMethod = "SEQUENTIAL";
    }
    
    return metrics;
}
```

### Liquidity Classification

```
┌─────────────────────────────────────────────────────────────┐
│              LIQUIDITY CLASSIFICATION                        │
├──────────┬──────────┬──────────┬──────────┬─────────────────┤
│ SCORE    │ RATING   │ SPREAD   │ DEPTH    │ EXECUTION       │
├──────────┼──────────┼──────────┼──────────┼─────────────────┤
│ 80-100   │ ★★★★★    │ <0.1%    │ >200 lots│ SIMULTANEOUS    │
│ 60-79    │ ★★★★☆    │ 0.1-0.5% │ 100-200  │ SIMULTANEOUS    │
│ 40-59    │ ★★★☆☆    │ 0.5-1.0% │ 50-100   │ HYBRID          │
│ 20-39    │ ★★☆☆☆    │ 1.0-2.0% │ 20-50    │ SEQUENTIAL      │
│ 0-19     │ ★☆☆☆☆    │ >2.0%    │ <20 lots │ AVOID / MANUAL  │
└──────────┴──────────┴──────────┴──────────┴─────────────────┘
```

---

## Price Improvement Strategies

### Strategy 1: Aggressive Taker (Pay Spread)

**Use When:** Must fill, time-critical, small quantity

```cpp
double aggressivePrice(string symbol, Side side) {
    Quote quote = getQuote(symbol);
    
    if (side == BUY) {
        return quote.ask; // Pay ask (taker)
    } else {
        return quote.bid; // Hit bid (taker)
    }
}
```

**Cost:** Full spread (₹0.50 to ₹2.00 per contract)

---

### Strategy 2: Passive Maker (Join Queue)

**Use When:** Can wait, large quantity, liquid instrument

```cpp
double passivePrice(string symbol, Side side) {
    Quote quote = getQuote(symbol);
    
    if (side == BUY) {
        return quote.bid; // Join bid queue (maker)
    } else {
        return quote.ask; // Join ask queue (maker)
    }
}
```

**Cost:** Zero spread (but may not fill)

---

### Strategy 3: Improved Maker (Queue Jump)

**Use When:** Want better fill rate than passive, avoid paying full spread

```cpp
double improvedMakerPrice(string symbol, Side side, int improveTicks = 1) {
    Quote quote = getQuote(symbol);
    double tickSize = getTickSize(symbol);
    
    if (side == BUY) {
        // Place 1 tick above current bid
        return quote.bid + (improveTicks * tickSize);
    } else {
        // Place 1 tick below current ask
        return quote.ask - (improveTicks * tickSize);
    }
}
```

**Cost:** 1 tick (₹0.05 to ₹0.50) - better than full spread

---

### Strategy 4: Mid-Point with Buffer

**Use When:** Spread trading, need certainty, willing to pay small premium

```cpp
double midPointWithBuffer(string symbol, Side side, double bufferBps) {
    Quote quote = getQuote(symbol);
    double midPrice = (quote.bid + quote.ask) / 2.0;
    double buffer = midPrice * (bufferBps / 10000.0);
    
    if (side == BUY) {
        return midPrice + buffer; // Pay slightly above mid
    } else {
        return midPrice - buffer; // Receive slightly below mid
    }
}
```

**Cost:** Mid + buffer (₹0.10 to ₹0.50), **HIGH FILL RATE**

---

### Strategy 5: Dynamic (Time-Weighted)

**Use When:** Flexible execution window, optimize fill vs cost

```cpp
double dynamicPrice(string symbol, Side side, int secondsElapsed, int maxWaitSec) {
    Quote quote = getQuote(symbol);
    
    // Start passive (0%), end aggressive (100%)
    double aggression = (double)secondsElapsed / maxWaitSec;
    
    if (side == BUY) {
        // Interpolate from bid to ask
        return quote.bid + (aggression * (quote.ask - quote.bid));
    } else {
        return quote.ask - (aggression * (quote.ask - quote.bid));
    }
}
```

**Cost:** Time-dependent (₹0 to full spread)

---

## Legging Risk Management

### What is Legging Risk?

**Definition:** Risk of having only some legs of a multi-leg strategy filled, leaving a **directional exposure**.

**Example:**
```
Iron Condor Strategy:
Leg 1: Sell NIFTY 24000 CE  ✅ FILLED
Leg 2: Buy NIFTY 24500 CE   ❌ NOT FILLED
Leg 3: Sell NIFTY 23500 PE  ✅ FILLED
Leg 4: Buy NIFTY 23000 PE   ✅ FILLED

Result: SHORT 24000 CE without protection (NAKED SHORT CALL)
Risk: Unlimited upside loss if NIFTY rallies
```

### Legging Risk Controls

#### 1. Maximum Leg Delay

```cpp
struct LeggingControl {
    int maxSecondsBetweenLegs = 10;  // Max 10 sec between legs
    
    bool checkLegDelay(vector<QDateTime> fillTimes) {
        QDateTime firstFill = fillTimes[0];
        QDateTime lastFill = fillTimes.back();
        
        int delaySec = firstFill.secsTo(lastFill);
        
        if (delaySec > maxSecondsBetweenLegs) {
            log("ERROR: Leg delay " + delaySec + "s exceeds limit");
            return false;
        }
        
        return true;
    }
};
```

#### 2. Mandatory Hedge First

```cpp
enum LegType {
    UNHEDGED_SHORT,   // Naked short (highest risk)
    UNHEDGED_LONG,    // Naked long (high risk)
    DIRECTIONAL,      // Futures (medium risk)
    HEDGE             // Protective leg (low risk)
};

void executeWithHedgeFirst(vector<Order> legs) {
    // Sort: HEDGE first, UNHEDGED_SHORT last
    sortByLegType(legs);
    
    for (auto& leg : legs) {
        bool filled = executeSingleLeg(leg, maxWaitSec: 10);
        
        if (!filled) {
            // Reverse all filled legs
            reversePreviousLegs(legs);
            return;
        }
    }
}
```

#### 3. All-or-None Enforcement

```cpp
void executeAllOrNone(vector<Order> legs) {
    // Place all orders
    vector<string> orderIds = placeAllOrders(legs);
    
    // Wait for fills (max 15 sec)
    sleep(15000);
    
    // Check if ALL filled
    bool allFilled = checkAllFilled(orderIds);
    
    if (allFilled) {
        log("SUCCESS: All legs filled");
        return;
    }
    
    // Not all filled - CANCEL ALL
    for (auto& orderId : orderIds) {
        if (!isOrderFilled(orderId)) {
            cancelOrder(orderId);
        } else {
            // Reverse filled orders
            reverseOrder(orderId);
        }
    }
    
    log("All-or-none failed, all legs canceled/reversed");
}
```

#### 4. Spread Validation

```cpp
bool validateSpreadPrice(vector<Order> legs, double expectedNetPrice, 
                         double maxDeviation) {
    // Calculate actual net price
    double netPrice = calculateNetPrice(legs);
    
    // Check if within tolerance
    double deviation = abs(netPrice - expectedNetPrice);
    
    if (deviation > maxDeviation) {
        log("ERROR: Spread price deviation " + deviation + 
            " exceeds " + maxDeviation);
        return false;
    }
    
    return true;
}

double calculateNetPrice(vector<Order> legs) {
    double netPrice = 0.0;
    
    for (auto& leg : legs) {
        if (leg.side == BUY) {
            netPrice -= leg.executionPrice * leg.quantity;  // Cash out
        } else {
            netPrice += leg.executionPrice * leg.quantity;  // Cash in
        }
    }
    
    return netPrice;
}
```

---

## Multi-Leg Execution Modes

### Mode 1: SIMULTANEOUS (Market Maker Style)

**Flow:**
```
Place Leg 1 ──┬── Place Leg 2 ──┬── Place Leg 3 ──┬── Place Leg 4
              │                 │                 │
             [All sent within 100ms]
                       │
                  Wait 2 sec
                       │
              Check if all filled
                  │        │
               YES ✅     NO ❌
                          │
                  Handle partial fill
```

**Code:**
```cpp
void executeSimultaneous(vector<Order> legs) {
    auto startTime = now();
    vector<string> orderIds;
    
    // Place all within tight window
    for (auto& leg : legs) {
        orderIds.push_back(placeOrder(leg));
        
        // Microsecond delay (optional, avoid spam detection)
        usleep(100); // 0.1ms
    }
    
    auto placementTime = (now() - startTime).milliseconds();
    log("All " + legs.size() + " legs placed in " + placementTime + "ms");
    
    // Wait for fills
    sleep(2000);
    
    // Validate
    if (checkAllFilled(orderIds)) {
        log("SUCCESS");
    } else {
        handlePartialFill(orderIds, legs);
    }
}
```

---

### Mode 2: SEQUENTIAL (Risk-Ordered)

**Flow:**
```
Place Leg 1 (Critical)
    ↓
Wait for fill (max 10 sec)
    ↓
Filled? → YES → Place Leg 2
            NO → ABORT
    ↓
Place Leg 2 (High priority)
    ↓
(Continue for all legs)
```

**Code:**
```cpp
void executeSequential(vector<Order> legs, vector<int> priorities) {
    // Sort by priority (highest first)
    sortByPriority(legs, priorities);
    
    vector<string> filledOrderIds;
    
    for (size_t i = 0; i < legs.size(); i++) {
        log("Executing leg " + (i+1) + "/" + legs.size() + 
            ": " + legs[i].symbol);
        
        bool filled = executeSingleLeg(legs[i], maxWaitSec: 10);
        
        if (filled) {
            filledOrderIds.push_back(legs[i].orderId);
        } else {
            log("Leg " + (i+1) + " failed, reversing previous legs");
            
            // Reverse all filled legs
            for (auto& orderId : filledOrderIds) {
                reverseOrder(orderId);
            }
            
            return; // Abort
        }
    }
    
    log("SUCCESS: All " + legs.size() + " legs filled sequentially");
}
```

---

### Mode 3: HYBRID (Leader-Follower)

**Flow:**
```
Identify most liquid leg (LEADER)
    ↓
Execute LEADER first
    ↓
Filled? → YES → Execute all FOLLOWERS simultaneously
            NO → ABORT
    ↓
Wait for FOLLOWERS (max 10 sec)
    ↓
All filled? → YES → SUCCESS
              NO → Reverse LEADER + filled FOLLOWERS
```

**Code:**
```cpp
void executeHybrid(vector<Order> legs) {
    // Find most liquid leg
    int leaderIndex = findMostLiquidLeg(legs);
    Order leader = legs[leaderIndex];
    
    // Remove leader from vector
    vector<Order> followers = getOtherLegs(legs, leaderIndex);
    
    // Execute leader
    log("Executing leader: " + leader.symbol);
    bool leaderFilled = executeSingleLeg(leader, maxWaitSec: 5);
    
    if (!leaderFilled) {
        log("Leader failed, aborting strategy");
        return;
    }
    
    log("Leader filled at " + leader.executionPrice + 
        ", executing " + followers.size() + " followers");
    
    // Execute followers simultaneously
    vector<string> followerIds;
    for (auto& follower : followers) {
        followerIds.push_back(placeOrder(follower));
    }
    
    // Wait for followers
    sleep(10000);
    
    // Check fills
    bool allFollowersFilled = checkAllFilled(followerIds);
    
    if (allFollowersFilled) {
        log("SUCCESS: Leader + all followers filled");
    } else {
        log("Some followers failed, reversing all");
        
        // Reverse leader
        reverseOrder(leader.orderId);
        
        // Reverse filled followers
        for (auto& followerId : followerIds) {
            if (isOrderFilled(followerId)) {
                reverseOrder(followerId);
            } else {
                cancelOrder(followerId);
            }
        }
    }
}
```

---

## Order Book Analysis

### Deep Order Book Assessment

```cpp
struct OrderBookAnalysis {
    string symbol;
    
    // Total available quantity at reasonable prices
    int buyableLots;   // Lots we can buy without moving market >1%
    int sellableLots;  // Lots we can sell without moving market >1%
    
    // Average execution price for given quantity
    double avgBuyPrice(int lots);
    double avgSellPrice(int lots);
    
    // Market impact estimate
    double estimatedSlippage(Side side, int lots);
};

OrderBookAnalysis analyzeOrderBook(string symbol, int targetLots) {
    OrderBookAnalysis analysis;
    analysis.symbol = symbol;
    
    // Get order book (20 levels deep)
    OrderBook ob = getOrderBook(symbol, depth: 20);
    
    double midPrice = (ob.bids[0].price + ob.asks[0].price) / 2.0;
    double maxPriceDeviation = midPrice * 0.01; // 1% max deviation
    
    // Calculate buyable lots
    int buyableLots = 0;
    for (auto& askLevel : ob.asks) {
        if (askLevel.price - midPrice > maxPriceDeviation) {
            break; // Too far from mid price
        }
        buyableLots += askLevel.quantity;
    }
    analysis.buyableLots = buyableLots;
    
    // Calculate sellable lots
    int sellableLots = 0;
    for (auto& bidLevel : ob.bids) {
        if (midPrice - bidLevel.price > maxPriceDeviation) {
            break;
        }
        sellableLots += bidLevel.quantity;
    }
    analysis.sellableLots = sellableLots;
    
    // Calculate average execution price for target quantity
    analysis.avgBuyPrice = [&](int lots) -> double {
        int remaining = lots;
        double totalCost = 0.0;
        
        for (auto& askLevel : ob.asks) {
            int takeLots = min(remaining, askLevel.quantity);
            totalCost += takeLots * askLevel.price;
            remaining -= takeLots;
            
            if (remaining == 0) break;
        }
        
        return totalCost / lots;
    };
    
    analysis.avgSellPrice = [&](int lots) -> double {
        int remaining = lots;
        double totalRevenue = 0.0;
        
        for (auto& bidLevel : ob.bids) {
            int sellLots = min(remaining, bidLevel.quantity);
            totalRevenue += sellLots * bidLevel.price;
            remaining -= sellLots;
            
            if (remaining == 0) break;
        }
        
        return totalRevenue / lots;
    };
    
    // Estimate slippage
    analysis.estimatedSlippage = [&](Side side, int lots) -> double {
        if (side == BUY) {
            double avgPrice = analysis.avgBuyPrice(lots);
            return (avgPrice - ob.asks[0].price) / ob.asks[0].price * 100; // %
        } else {
            double avgPrice = analysis.avgSellPrice(lots);
            return (ob.bids[0].price - avgPrice) / ob.bids[0].price * 100; // %
        }
    };
    
    return analysis;
}
```

### Using Order Book Analysis

```cpp
void executWithOrderBookAnalysis(vector<Order> legs) {
    // Analyze each leg
    for (auto& leg : legs) {
        OrderBookAnalysis analysis = analyzeOrderBook(leg.symbol, leg.quantity);
        
        // Check if enough liquidity
        int availableLots = leg.side == BUY ? analysis.buyableLots 
                                            : analysis.sellableLots;
        
        if (availableLots < leg.quantity) {
            log("WARNING: Not enough liquidity for " + leg.symbol);
            log("  Needed: " + leg.quantity + ", Available: " + availableLots);
            
            // Option 1: Reduce quantity
            leg.quantity = availableLots * 0.8; // Use only 80% of available
            
            // Option 2: Abort
            // return;
        }
        
        // Estimate slippage
        double slippage = analysis.estimatedSlippage(leg.side, leg.quantity);
        log("Estimated slippage for " + leg.symbol + ": " + slippage + "%");
        
        // Calculate smart price
        double avgPrice = leg.side == BUY ? analysis.avgBuyPrice(leg.quantity)
                                          : analysis.avgSellPrice(leg.quantity);
        
        // Add buffer (5 bps)
        double buffer = avgPrice * 0.0005;
        leg.price = leg.side == BUY ? avgPrice + buffer : avgPrice - buffer;
        
        // Round to tick size
        leg.price = roundToTickSize(leg.price, getTickSize(leg.symbol));
    }
    
    // Now execute with smart prices
    executeSimultaneous(legs);
}
```

---

## Spread Trading Specifics

### Types of Spreads

| Spread Type | Legs | Liquidity Challenge | Execution Method |
|-------------|------|---------------------|------------------|
| **Bull Call Spread** | 2 (Buy ATM call + Sell OTM call) | OTM less liquid | Sequential (ATM first) |
| **Bear Put Spread** | 2 (Buy ATM put + Sell OTM put) | OTM less liquid | Sequential (ATM first) |
| **Iron Condor** | 4 (Sell ATM call/put + Buy OTM call/put) | ATM more liquid | Hybrid (ATM first) |
| **Calendar Spread** | 2 (Sell near month + Buy far month) | Near month more liquid | Sequential (near first) |
| **Ratio Spread** | 2+ (1:2 or 2:3 ratio) | Asymmetric quantity | Sequential |

### Bull Call Spread Example

**Strategy:** Buy 24000 CE + Sell 24500 CE

```cpp
void executeBullCallSpread(int atmStrike, int otmStrike, int lots) {
    // Leg 1: Buy ATM (more liquid, more expensive)
    Order buyATM = {
        .type = LIMIT,
        .symbol = "NIFTY" + atmStrike + "CE",
        .side = BUY,
        .quantity = lots,
        .price = 0.0 // Calculate below
    };
    
    // Leg 2: Sell OTM (less liquid, cheaper)
    Order sellOTM = {
        .type = LIMIT,
        .symbol = "NIFTY" + otmStrike + "CE",
        .side = SELL,
        .quantity = lots,
        .price = 0.0
    };
    
    // Analyze liquidity
    LiquidityMetrics atmLiq = assessLiquidity(buyATM.symbol);
    LiquidityMetrics otmLiq = assessLiquidity(sellOTM.symbol);
    
    log("ATM liquidity: " + atmLiq.liquidityScore);
    log("OTM liquidity: " + otmLiq.liquidityScore);
    
    // Calculate prices
    Quote atmQuote = getQuote(buyATM.symbol);
    Quote otmQuote = getQuote(sellOTM.symbol);
    
    // ATM: Use mid-point + small buffer (liquid)
    buyATM.price = (atmQuote.bid + atmQuote.ask) / 2.0 + 0.10;
    
    // OTM: Use aggressive price (less liquid, must fill)
    sellOTM.price = otmQuote.bid; // Hit bid to ensure fill
    
    // Round to tick size
    buyATM.price = roundToTickSize(buyATM.price, getTickSize(buyATM.symbol));
    sellOTM.price = roundToTickSize(sellOTM.price, getTickSize(sellOTM.symbol));
    
    // Calculate spread cost
    double netDebit = (buyATM.price - sellOTM.price) * lots;
    log("Spread cost: ₹" + netDebit);
    
    // Validate spread cost
    double expectedCost = (atmQuote.midPrice() - otmQuote.midPrice()) * lots;
    double costDeviation = abs(netDebit - expectedCost);
    
    if (costDeviation > expectedCost * 0.10) { // 10% deviation
        log("ERROR: Spread cost deviation too high");
        return;
    }
    
    // Execute sequentially (ATM first, it's more critical)
    vector<Order> legs = {buyATM, sellOTM};
    executeSequential(legs, priorities: {HIGH, MEDIUM});
}
```

---

## Pair Trading Specifics

### Example: HDFC vs ICICI Pair

**Strategy:** Long HDFC + Short ICICI (equal ₹ value)

```cpp
void executePairTrade(string symbol1, string symbol2, double notionalValue) {
    // Get prices
    double price1 = getLastPrice(symbol1);
    double price2 = getLastPrice(symbol2);
    
    // Calculate quantities (equal notional)
    int qty1 = (int)(notionalValue / price1);
    int qty2 = (int)(notionalValue / price2);
    
    // Create orders
    Order leg1 = createOrder(symbol1, BUY, qty1);
    Order leg2 = createOrder(symbol2, SELL, qty2);
    
    // Assess liquidity
    LiquidityMetrics liq1 = assessLiquidity(symbol1);
    LiquidityMetrics liq2 = assessLiquidity(symbol2);
    
    // Decide execution method
    if (liq1.liquidityScore >= 60 && liq2.liquidityScore >= 60) {
        // Both liquid - simultaneous
        log("Both legs liquid, executing simultaneously");
        executeSimultaneous({leg1, leg2});
        
    } else if (liq1.liquidityScore < liq2.liquidityScore) {
        // Symbol1 less liquid - execute it first
        log("Symbol1 less liquid, executing it first");
        executeSequential({leg1, leg2}, priorities: {HIGH, MEDIUM});
        
    } else {
        // Symbol2 less liquid - execute it first
        log("Symbol2 less liquid, executing it first");
        executeSequential({leg2, leg1}, priorities: {HIGH, MEDIUM});
    }
}
```

### Spread Ratio Monitoring

```cpp
void monitorSpreadRatio(Position pos1, Position pos2) {
    while (bothPositionsOpen(pos1, pos2)) {
        double price1 = getLastPrice(pos1.symbol);
        double price2 = getLastPrice(pos2.symbol);
        
        // Calculate spread ratio
        double currentRatio = price1 / price2;
        
        // Compare to entry ratio
        double entryRatio = pos1.entryPrice / pos2.entryPrice;
        double ratioChange = (currentRatio - entryRatio) / entryRatio * 100;
        
        log("Spread ratio change: " + ratioChange + "%");
        
        // Alert if ratio diverges significantly
        if (abs(ratioChange) > 5.0) { // 5% divergence
            alert("Spread ratio diverged by " + ratioChange + "%");
        }
        
        sleep(5000); // Check every 5 sec
    }
}
```

---

## Options Spread Execution

### Iron Condor (4 Legs)

```cpp
void executeIronCondor(int atmStrike, int wingWidth, int lots) {
    // 4 legs:
    // 1. Buy OTM put (ATM - wingWidth)
    // 2. Sell ATM put
    // 3. Sell ATM call
    // 4. Buy OTM call (ATM + wingWidth)
    
    Order buyOTMPut = createOrder("NIFTY" + (atmStrike - wingWidth) + "PE", 
                                  BUY, lots);
    Order sellATMPut = createOrder("NIFTY" + atmStrike + "PE", SELL, lots);
    Order sellATMCall = createOrder("NIFTY" + atmStrike + "CE", SELL, lots);
    Order buyOTMCall = createOrder("NIFTY" + (atmStrike + wingWidth) + "CE", 
                                   BUY, lots);
    
    // Assess liquidity for all legs
    vector<Order> legs = {buyOTMPut, sellATMPut, sellATMCall, buyOTMCall};
    vector<LiquidityMetrics> liquidityScores;
    
    for (auto& leg : legs) {
        LiquidityMetrics liq = assessLiquidity(leg.symbol);
        liquidityScores.push_back(liq);
        log(leg.symbol + " liquidity: " + liq.liquidityScore);
    }
    
    // Typically: ATM strikes more liquid than OTM
    // Strategy: Execute shorts (ATM) first, then longs (OTM)
    // Reason: Must establish short positions with protection
    
    // Calculate prices with buffer
    for (size_t i = 0; i < legs.size(); i++) {
        Quote quote = getQuote(legs[i].symbol);
        
        // For shorts (sell): Use bid - buffer (ensure fill)
        // For longs (buy): Use ask + buffer (ensure fill)
        if (legs[i].side == SELL) {
            legs[i].price = quote.bid - (quote.bid * 0.005); // 0.5% buffer
        } else {
            legs[i].price = quote.ask + (quote.ask * 0.005);
        }
        
        legs[i].price = roundToTickSize(legs[i].price, 
                                        getTickSize(legs[i].symbol));
    }
    
    // Execute: Shorts simultaneously, then longs simultaneously
    vector<Order> shorts = {sellATMPut, sellATMCall};
    vector<Order> longs = {buyOTMPut, buyOTMCall};
    
    // Execute shorts first
    log("Executing short legs...");
    bool shortsFilledSuccess = executeSimultaneous(shorts);
    
    if (!shortsFilledSuccess) {
        log("Short legs failed, aborting");
        return;
    }
    
    // Execute longs (protection)
    log("Short legs filled, executing protective long legs...");
    bool longsFilledSuccess = executeSimultaneous(longs);
    
    if (!longsFilledSuccess) {
        log("Long legs failed, REVERSING SHORT LEGS");
        reverseFilled Shorts(shorts);
        return;
    }
    
    log("SUCCESS: All 4 legs of Iron Condor filled");
    
    // Validate net credit
    double netCredit = (sellATMPut.executionPrice + sellATMCall.executionPrice -
                        buyOTMPut.executionPrice - buyOTMCall.executionPrice) * lots;
    
    log("Net credit received: ₹" + netCredit);
}
```

---

## Calendar Spread Execution

### Example: Sell Current Month + Buy Next Month

```cpp
void executeCalendarSpread(int strike, int lots) {
    // Get expiry dates
    QDate currentExpiry = getCurrentMonthExpiry();
    QDate nextExpiry = getNextMonthExpiry();
    
    // Create orders
    Order sellCurrent = createOrder(
        "NIFTY" + currentExpiry.toString("ddMMMyy") + strike + "CE",
        SELL, lots
    );
    
    Order buyNext = createOrder(
        "NIFTY" + nextExpiry.toString("ddMMMyy") + strike + "CE",
        BUY, lots
    );
    
    // Assess liquidity
    LiquidityMetrics currentLiq = assessLiquidity(sellCurrent.symbol);
    LiquidityMetrics nextLiq = assessLiquidity(buyNext.symbol);
    
    log("Current month liquidity: " + currentLiq.liquidityScore);
    log("Next month liquidity: " + nextLiq.liquidityScore);
    
    // Current month typically more liquid
    // Execute current month first (it's the reference price)
    
    // Calculate prices
    Quote currentQuote = getQuote(sellCurrent.symbol);
    Quote nextQuote = getQuote(buyNext.symbol);
    
    // Sell current: Aggressive (hit bid)
    sellCurrent.price = currentQuote.bid;
    
    // Buy next: Relative to current month execution
    // Target spread: next - current = X (time value)
    double targetSpread = calculateTheoretical TimeValue(
        strike, currentExpiry, nextExpiry
    );
    
    // Execute sell current first
    log("Executing sell current month...");
    bool currentFilled = executeSingleLeg(sellCurrent, maxWaitSec: 5);
    
    if (!currentFilled) {
        log("Current month sell failed, aborting");
        return;
    }
    
    // Calculate buy next price based on actual current month execution
    double targetBuyPrice = sellCurrent.executionPrice + targetSpread;
    buyNext.price = max(targetBuyPrice, nextQuote.bid); // Don't pay more than target
    
    // Execute buy next
    log("Executing buy next month at " + buyNext.price + "...");
    bool nextFilled = executeSingleLeg(buyNext, maxWaitSec: 10);
    
    if (!nextFilled) {
        log("Next month buy failed, REVERSING current month");
        reverseOrder(sellCurrent.orderId);
        return;
    }
    
    // Validate spread
    double actualSpread = buyNext.executionPrice - sellCurrent.executionPrice;
    log("Calendar spread: " + actualSpread);
    
    // Check if within 10% of target
    if (abs(actualSpread - targetSpread) > targetSpread * 0.10) {
        log("WARNING: Spread deviation " + 
            abs(actualSpread - targetSpread) + " is high");
    }
}
```

---

## Recovery from Partial Fill

### Scenario 1: Some Legs Filled, Some Pending

```cpp
void handlePartialFill(vector<string> orderIds, vector<Order> originalOrders) {
    // Check which orders filled
    vector<string> filledIds;
    vector<string> pendingIds;
    
    for (size_t i = 0; i < orderIds.size(); i++) {
        if (isOrderFilled(orderIds[i])) {
            filledIds.push_back(orderIds[i]);
        } else {
            pendingIds.push_back(orderIds[i]);
        }
    }
    
    log("Filled: " + filledIds.size() + "/" + orderIds.size());
    
    // Decision tree
    if (pendingIds.size() == 0) {
        // All filled - success
        return;
    }
    
    if (filledIds.size() == 0) {
        // None filled - cancel all
        log("No fills, canceling all");
        for (auto& orderId : pendingIds) {
            cancelOrder(orderId);
        }
        return;
    }
    
    // Partial fill - CRITICAL DECISION
    log("Partial fill detected, evaluating risk...");
    
    // Option 1: Retry pending orders with aggressive pricing
    log("Option 1: Retry pending orders with aggressive prices");
    for (size_t i = 0; i < pendingIds.size(); i++) {
        Order pendingOrder = findOrder(originalOrders, pendingIds[i]);
        
        // Cancel current order
        cancelOrder(pendingIds[i]);
        
        // Re-place with aggressive price
        Quote quote = getQuote(pendingOrder.symbol);
        pendingOrder.price = pendingOrder.side == BUY ? quote.ask : quote.bid;
        pendingOrder.orderId = generateNewOrderId();
        
        placeOrder(pendingOrder);
        
        // Wait for fill (urgent)
        sleep(3000);
        
        if (!isOrderFilled(pendingOrder.orderId)) {
            // Still not filling - abort and reverse
            log("Retry failed, reversing all filled orders");
            for (auto& filledId : filledIds) {
                reverseOrder(filledId);
            }
            cancelOrder(pendingOrder.orderId);
            return;
        }
    }
    
    log("SUCCESS: All legs eventually filled after partial fill recovery");
}
```

### Scenario 2: Single Leg Failed, Others Filled

```cpp
void recoverFromSingleLegFailure(vector<Order> legs, int failedLegIndex) {
    // Get filled orders (excluding failed leg)
    vector<Order> filledLegs;
    for (size_t i = 0; i < legs.size(); i++) {
        if (i != failedLegIndex && legs[i].isFilled()) {
            filledLegs.push_back(legs[i]);
        }
    }
    
    // Assess risk of filled legs without failed leg
    Risk risk = assessRisk(filledLegs, legs[failedLegIndex]);
    
    log("Risk assessment: " + riskToString(risk));
    
    if (risk == CRITICAL) {
        // Naked short or unlimited risk - MUST reverse
        log("CRITICAL risk, reversing all filled legs immediately");
        for (auto& leg : filledLegs) {
            reverseOrder(leg.orderId);
        }
        
    } else if (risk == HIGH) {
        // High risk but manageable - retry failed leg aggressively
        log("HIGH risk, retrying failed leg with max aggression");
        
        Order failedLeg = legs[failedLegIndex];
        Quote quote = getQuote(failedLeg.symbol);
        
        // Pay premium to ensure fill
        if (failedLeg.side == BUY) {
            failedLeg.price = quote.ask * 1.01; // Pay 1% above ask
        } else {
            failedLeg.price = quote.bid * 0.99; // Receive 1% below bid
        }
        
        failedLeg.orderId = generateNewOrderId();
        placeOrder(failedLeg);
        
        // Wait (urgent)
        sleep(5000);
        
        if (!failedLeg.isFilled()) {
            // Still failed - reverse all
            log("Urgent retry failed, reversing");
            for (auto& leg : filledLegs) {
                reverseOrder(leg.orderId);
            }
        }
        
    } else {
        // Low/medium risk - can hold or reverse based on strategy
        log("MEDIUM risk, strategy can decide to hold or reverse");
        
        // Option 1: Hold filled legs as standalone positions
        // Option 2: Reverse filled legs
        
        if (config.allowPartialFill) {
            log("Holding filled legs as partial position");
        } else {
            log("Partial fill not allowed, reversing");
            for (auto& leg : filledLegs) {
                reverseOrder(leg.orderId);
            }
        }
    }
}
```

---

## Code Implementation

### Multi-Leg Execution Engine

```cpp
class MultiLegExecutionEngine {
private:
    struct ExecutionConfig {
        ExecutionMode mode = HYBRID; // SIMULTANEOUS/SEQUENTIAL/HYBRID
        int maxWaitSeconds = 15;
        double bufferBps = 10.0; // 10 basis points
        bool allowPartialFill = false;
        bool reverseOnFailure = true;
    };
    
    ExecutionConfig config_;
    
public:
    bool executeMultiLeg(vector<Order> legs) {
        // Pre-execution validation
        if (!validateLegs(legs)) {
            log("Leg validation failed");
            return false;
        }
        
        // Assess liquidity
        vector<LiquidityMetrics> liquidityMetrics = assessAllLegs(legs);
        
        // Decide execution method (can override config)
        ExecutionMode mode = determineOptimalMode(liquidityMetrics);
        
        log("Executing " + legs.size() + " legs using " + 
            modeToString(mode) + " method");
        
        // Execute based on mode
        bool success = false;
        
        switch (mode) {
            case SIMULTANEOUS:
                success = executeSimultaneous(legs);
                break;
                
            case SEQUENTIAL:
                success = executeSequential(legs);
                break;
                
            case HYBRID:
                success = executeHybrid(legs);
                break;
        }
        
        if (success) {
            log("Multi-leg execution SUCCESS");
            validateSpread(legs);
        } else {
            log("Multi-leg execution FAILED");
        }
        
        return success;
    }
    
private:
    ExecutionMode determineOptimalMode(vector<LiquidityMetrics> metrics) {
        // Calculate average liquidity score
        double avgScore = 0.0;
        int minScore = 100;
        
        for (auto& m : metrics) {
            avgScore += m.liquidityScore;
            minScore = min(minScore, m.liquidityScore);
        }
        avgScore /= metrics.size();
        
        // Decision logic
        if (avgScore >= 70 && minScore >= 60) {
            // All legs liquid - simultaneous
            return SIMULTANEOUS;
            
        } else if (minScore < 40) {
            // At least one leg illiquid - sequential
            return SEQUENTIAL;
            
        } else {
            // Mixed liquidity - hybrid
            return HYBRID;
        }
    }
    
    bool executeSimultaneous(vector<Order> legs) {
        // Calculate prices with buffer
        for (auto& leg : legs) {
            leg.price = calculatePrice WithBuffer(leg, config_.bufferBps);
        }
        
        // Place all orders
        vector<string> orderIds;
        for (auto& leg : legs) {
            orderIds.push_back(placeOrder(leg));
        }
        
        // Wait for fills
        auto startTime = now();
        
        while ((now() - startTime).seconds() < config_.maxWaitSeconds) {
            // Check if all filled
            bool allFilled = true;
            for (auto& orderId : orderIds) {
                if (!isOrderFilled(orderId)) {
                    allFilled = false;
                    break;
                }
            }
            
            if (allFilled) {
                return true;
            }
            
            sleep(500); // Check every 500ms
        }
        
        // Timeout - handle partial fill
        return handlePartialFill(orderIds, legs);
    }
    
    bool executeSequential(vector<Order> legs) {
        // Sort by priority
        sortByLegPriority(legs);
        
        // Execute one by one
        for (auto& leg : legs) {
            bool filled = executeSingleLeg(leg);
            
            if (!filled) {
                log("Leg " + leg.symbol + " failed");
                
                if (config_.reverseOnFailure) {
                    reversePreviousLegs(legs);
                }
                
                return false;
            }
        }
        
        return true;
    }
    
    bool executeHybrid(vector<Order> legs) {
        // Find most liquid leg (leader)
        int leaderIndex = findMostLiquidLegIndex(legs);
        Order leader = legs[leaderIndex];
        
        // Remove leader from vector
        legs.erase(legs.begin() + leaderIndex);
        
        // Execute leader
        bool leaderFilled = executeSingleLeg(leader);
        
        if (!leaderFilled) {
            return false;
        }
        
        // Execute remaining legs simultaneously
        return executeSimultaneous(legs);
    }
    
    bool executeSingleLeg(Order leg) {
        // Calculate price
        leg.price = calculatePriceWithBuffer(leg, config_.bufferBps);
        
        // Place order
        string orderId = placeOrder(leg);
        
        // Wait with progressive price modification
        int elapsed = 0;
        int modifyInterval = 3; // Modify every 3 seconds
        
        while (elapsed < config_.maxWaitSeconds) {
            if (isOrderFilled(orderId)) {
                return true;
            }
            
            sleep(1000);
            elapsed++;
            
            // Modify price to be more aggressive
            if (elapsed % modifyInterval == 0 && elapsed < config_.maxWaitSeconds - 1) {
                double urgency = (double)elapsed / config_.maxWaitSeconds;
                double newPrice = calculateDynamicPrice(leg, urgency);
                
                modifyOrder(orderId, newPrice);
                log("Modified " + leg.symbol + " price to " + newPrice + 
                    " (urgency " + (urgency * 100) + "%)");
            }
        }
        
        // Timeout - cancel
        cancelOrder(orderId);
        return false;
    }
};
```

---

## Performance Metrics

### Key Metrics to Track

```cpp
struct MultiLegExecutionMetrics {
    // Fill metrics
    double allLegsFillRate;         // % of times all legs filled
    double partialFillRate;         // % of times some legs filled
    double noFillRate;              // % of times no legs filled
    
    // Timing metrics
    double avgExecutionTime;        // Average time to fill all legs
    double avgLegDelay;             // Average time between first and last fill
    double maxLegDelay;             // Maximum time between legs
    
    // Cost metrics
    double avgSlippagePerLeg;       // Average slippage per leg
    double avgSpreadCost;           // Average spread cost paid
    double avgTotalExecutionCost;   // Spread + slippage + fees
    
    // Risk metrics
    int nakedPositionCount;         // Times left with naked position
    double maxNakedExposure;        // Largest naked exposure (₹)
    double avgRecoveryTime;         // Time to recover from partial fill
    
    // Quality metrics
    double spreadDeviationPercent;  // Actual vs expected spread deviation
    double priceImpactPercent;      // Market impact of our orders
};
```

### Benchmark Targets

| Metric | Excellent | Good | Acceptable | Poor |
|--------|-----------|------|------------|------|
| **All Legs Fill Rate** | >98% | 95-98% | 90-95% | <90% |
| **Avg Execution Time** | <5 sec | 5-10 sec | 10-20 sec | >20 sec |
| **Avg Leg Delay** | <2 sec | 2-5 sec | 5-10 sec | >10 sec |
| **Avg Slippage** | <0.1% | 0.1-0.3% | 0.3-0.5% | >0.5% |
| **Naked Position Count** | 0 | 0 | 1-2 per month | >2 per month |
| **Spread Deviation** | <5% | 5-10% | 10-20% | >20% |

---

## Best Practices

### DO's ✅

1. **Assess liquidity before execution** - understand order book depth
2. **Use buffer pricing** - pay 5-10 bps extra for certainty
3. **Monitor leg delays** - max 10 sec between legs
4. **Reverse on critical failure** - naked shorts = immediate reverse
5. **Log every execution** - analyze patterns
6. **Test with small sizes** - verify logic before scaling
7. **Use IOC for urgent legs** - stop loss, time-critical
8. **Validate spread cost** - check actual vs expected
9. **Implement circuit breakers** - halt on repeated failures
10. **Monitor in real-time** - dashboard for multi-leg status

### DON'Ts ❌

1. **Never leave naked shorts** - unlimited risk
2. **Never assume high liquidity** - always check order book
3. **Never use same price for all legs** - adjust per liquidity
4. **Never ignore partial fills** - handle explicitly
5. **Never execute without timeouts** - prevent hanging orders
6. **Never skip recovery procedures** - must reverse on failure
7. **Never execute without validation** - check spread cost first
8. **Never use market orders** - SEBI non-compliant
9. **Never ignore leg priority** - short options = highest priority
10. **Never exceed liquidity depth** - split if order too large

---

## Summary

Executing multi-leg strategies (spreads, pairs, options) with different liquidity requires:

### 1. Liquidity Assessment
- **Order book analysis** (depth, spread, volume)
- **Liquidity scoring** (0-100, determines execution method)
- **Real-time monitoring** (liquidity changes constantly)

### 2. Execution Method Selection
- **Simultaneous:** High liquidity (score >70), fast, fair
- **Sequential:** Low liquidity (score <40), controlled, slower
- **Hybrid:** Mixed liquidity (40-70), leader-follower

### 3. Price Calculation
- **Passive:** Bid/ask (best price, low fill rate)
- **Aggressive:** Cross spread (high fill rate, pay spread)
- **Buffer:** Mid + 5-10 bps (certainty, small cost)
- **Dynamic:** Time-based (start passive, end aggressive)

### 4. Legging Risk Management
- **Max leg delay:** 10 seconds between fills
- **Hedge first:** Protect before exposing
- **All-or-none:** Cancel all if any fails
- **Immediate reverse:** On critical failures

### 5. Recovery Procedures
- **Partial fill:** Retry with aggressive pricing
- **Single leg failure:** Reverse or retry based on risk
- **Circuit breaker:** Halt after repeated failures

With proper implementation:
- **Fill rate:** >95%
- **Naked positions:** <0.1% of executions
- **Spread deviation:** <10%
- **Execution time:** <15 seconds

This ensures **safe, efficient multi-leg execution** even with liquidity imbalances.

---

**End of Document**
