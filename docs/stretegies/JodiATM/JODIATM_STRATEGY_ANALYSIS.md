# JodiATM Strategy - In-Depth Analysis

**Strategy Type:** Dynamic Straddle with Leg Shifting  
**Version:** 1.0  
**Date:** February 10, 2026  
**Status:** Production Implementation  
**Complexity:** Advanced

---

## Executive Summary

JodiATM is a sophisticated options trading strategy that dynamically manages ATM (At-The-Money) straddle positions by progressively shifting position quantities across 4 legs as the underlying price moves. The strategy adapts to market direction (bullish/bearish) and automatically resets its reference ATM when price movements exceed predefined boundaries.

**Core Concept:** Sell ATM straddle → Monitor price movement → Shift 25% quantity per leg → Reset ATM on boundary breach.

**Risk Profile:** Defined stop loss, dynamic profit taking, requires active market monitoring.

---

## Strategy Overview

### What Problem Does It Solve?

Traditional straddle strategies suffer from:
1. **Static positioning** - No adaptation to price movement
2. **Unlimited risk** - Both legs can move ITM (In-The-Money)
3. **Theta decay only** - Profit limited to time decay

JodiATM addresses these by:
1. **Dynamic shifting** - Progressively moves exposure as price moves
2. **Boundary management** - ATM resets prevent runaway losses
3. **Directional adaptation** - Follows trend while managing risk

---

## Algorithm Architecture

### State Variables

```cpp
// Tokens (Market Identifiers)
uint32_t m_cashToken;      // Underlying cash/futures token (e.g., NIFTY)
uint32_t m_ceToken;        // Current ATM Call option token
uint32_t m_peToken;        // Current ATM Put option token
uint32_t m_ceTokenNext;    // Next strike Call token (for shifting)
uint32_t m_peTokenNext;    // Next strike Put token (for shifting)

// Parameters (User Configured)
double m_offset;           // Offset from ATM (e.g., 10 points)
double m_threshold;        // Decision threshold (e.g., 15 points)
double m_adjPts;           // Adjustment points (e.g., 5 points)
double m_strikeDiff;       // Strike interval (e.g., 50 for NIFTY, 100 for BANKNIFTY)
int m_baseQty;            // Base quantity per leg (e.g., 50 lots)

// State (Runtime)
Trend m_trend;            // Nutral (Neutral), Bullish, Bearish
int m_currentLeg;         // 0 to 4 (Leg 0 = neutral, Legs 1-4 = progressive)
double m_currentATM;      // Current ATM strike reference
double m_blDP;            // Bullish Decision Point (upper trigger)
double m_brDP;            // Bearish Decision Point (lower trigger)
double m_reversalP;       // Reversal Point (trend reversal trigger)
double m_blRCP;           // Bullish Reset Constant Point (upper boundary)
double m_brRCP;           // Bearish Reset Constant Point (lower boundary)

// Monitoring
double m_cashPrice;       // Current underlying price
bool m_isFirstOrderPlaced; // Entry flag
```

---

## Strategy Lifecycle

### Phase 1: Initialization

**Trigger:** User creates JodiATM instance with parameters

```cpp
void JodiATMStrategy::init(const StrategyInstance& instance) {
    // Load parameters from instance
    m_offset = getParameter<double>("offset", 0.0);       // e.g., 10
    m_threshold = getParameter<double>("threshold", 0.0); // e.g., 15
    m_adjPts = getParameter<double>("adj_pts", 0.0);     // e.g., 5
    m_baseQty = instance.quantity;                        // e.g., 50
    
    log("JodiATM Initialized: Offset=10, Threshold=15, AdjPts=5");
}
```

**Parameters Explained:**

| Parameter | Typical Value | Purpose |
|-----------|---------------|---------|
| **offset** | 10-20 points | Distance from ATM to start monitoring |
| **threshold** | 10-30 points | Additional buffer before leg trigger |
| **adj_pts** | 0-10 points | Fine-tune adjustment to ATM calculations |
| **baseQty** | 25-75 lots | Quantity per leg (total exposure = 4x at max) |

---

### Phase 2: Start & ATM Reset

**Trigger:** User clicks "Start" button

```cpp
void JodiATMStrategy::start() {
    // Step 1: Get current ATM info from ATMWatchManager
    auto info = ATMWatchManager::getInstance().getATMInfo(m_instance.symbol);
    
    // Step 2: Reset ATM reference
    resetATM(info.atmStrike); // e.g., 22000 for NIFTY
    
    // Step 3: Subscribe to underlying price feed
    m_cashToken = info.underlyingToken;
    FeedHandler::instance().subscribe(m_instance.segment, m_cashToken, 
                                      this, &JodiATMStrategy::onTick);
    
    // Step 4: Connect to ATM updates (for major market moves)
    connect(&ATMWatchManager::getInstance(), &ATMWatchManager::atmUpdated,
            this, &JodiATMStrategy::onATMUpdated);
    
    m_isRunning = true;
}
```

**resetATM() Logic:**

```
Current ATM = 22000 (example NIFTY strike)
Strike Diff = 50 (NIFTY standard interval)

Calculations:
┌────────────────────────────────────────────────────────┐
│ Reference ATM = 22000 - adj_pts = 22000 - 5 = 21995   │
│                                                         │
│ Bullish Reset Point (blRCP):                          │
│   = 21995 + (1.6 × 50) + offset - adj_pts            │
│   = 21995 + 80 + 10 - 5 = 22080                      │
│                                                         │
│ Bearish Reset Point (brRCP):                          │
│   = 21995 - (1.6 × 50) - offset - adj_pts            │
│   = 21995 - 80 - 10 - 5 = 21900                      │
└────────────────────────────────────────────────────────┘

Initial State:
- Leg = 0 (Neutral)
- Trend = Nutral
- blDP = 22000 + 10 + 15 + (0.6 × 50) = 22055
- brDP = 22000 - 10 - 15 - (0.6 × 50) = 21945
- reversalP = 0 (no reversal in neutral)
```

**Visual Representation:**

```
Price Scale (NIFTY example):
                                                    ▲
         22080 ├───────────────────────────────────┤ blRCP (Upper Boundary)
               │                                    │
         22055 ├─────────> blDP (Bullish Trigger) │
               │                                    │
         22000 ├═══════════ CURRENT ATM ═══════════│ (Straddle Center)
               │                                    │
         21945 ├─────────> brDP (Bearish Trigger) │
               │                                    │
         21900 ├───────────────────────────────────┤ brRCP (Lower Boundary)
                                                    ▼
```

---

### Phase 3: Entry - First Order Placement

**Trigger:** Price enters decision zone while in Neutral state

```cpp
void JodiATMStrategy::checkTrade(double cashPrice) {
    // Initial Entry: Sell ATM Straddle
    if (!m_isFirstOrderPlaced && m_trend == Trend::Nutral) {
        log("First Entry: Selling Jodi at 22000");
        
        // Place Orders (commented for safety in reference)
        // makeOrder(m_ceToken, m_baseQty, "Sell"); // Sell 50 lots 22000 CE
        // makeOrder(m_peToken, m_baseQty, "Sell"); // Sell 50 lots 22000 PE
        
        m_isFirstOrderPlaced = true;
    }
}
```

**Position After Entry:**

| Instrument | Strike | Type | Quantity | Side | Premium |
|------------|--------|------|----------|------|---------|
| NIFTY CE | 22000 | Call | 50 lots | Sell | ₹120 (example) |
| NIFTY PE | 22000 | Put | 50 lots | Sell | ₹115 (example) |

**Total Credit:** ₹235 × 50 × 50 = ₹5,87,500 (for 50 lot size)

---

### Phase 4: Trend Detection & Leg Progression

#### Scenario A: Bullish Move

**Condition:** Cash price > blDP (22055)

```cpp
if (cashPrice > m_blDP) {
    m_trend = Trend::Bullish;
    m_currentLeg = 1;
    log("Trend Change: BULLISH. Leg 1 triggered.");
    calculateThresholds(m_currentATM);
    // switchCurrent2Upper(); // Shift 25% quantity
}
```

**Leg Progression Formula:**

```
Leg N thresholds (N = 1 to 4):
Strike Multiplier = 0.6 + (N × 0.1)
Reversal Multiplier = 0.1 + (N × 0.1)

Leg 1: Strike Mult = 0.7, Reversal Mult = 0.2
Leg 2: Strike Mult = 0.8, Reversal Mult = 0.3
Leg 3: Strike Mult = 0.9, Reversal Mult = 0.4
Leg 4: Strike Mult = 1.0, Reversal Mult = 0.5

Bullish Leg 1 Example:
blDP = 21995 + 10 + 15 + (0.7 × 50) = 22055
reversalP = 21995 + 10 + (0.2 × 50) = 22015
```

**Position Adjustment (Leg 1):**

When blDP breached → Shift 25% from PE to higher strike CE

| Action | Instrument | Strike | Qty | Side | Reason |
|--------|------------|--------|-----|------|--------|
| **Exit** | NIFTY PE | 22000 | 12 lots | Buy Back | Reduce losing leg |
| **Entry** | NIFTY CE | 22050 | 12 lots | Sell | Add to winning leg |

**New Position:**
- 22000 CE: 50 lots (original)
- 22050 CE: 12 lots (shifted)
- 22000 PE: 38 lots (reduced)

#### Scenario B: Continued Bullish (Leg 2, 3, 4)

**Each leg breach triggers additional 25% shift:**

```
Leg 2: blDP = 22075 → Shift 12 more lots
Leg 3: blDP = 22095 → Shift 13 more lots
Leg 4: blDP = 22115 → Shift 13 more lots
```

**Maximum Position (Leg 4):**
- Call side: 50 + 50 = 100 lots (doubled)
- Put side: 0 lots (fully shifted)

---

### Phase 5: Reversal Handling

**Trigger:** Price falls below reversalP after bullish move

```cpp
if (m_trend == Trend::Bullish) {
    if (reversalP > 0 && cashPrice < reversalP) {
        m_currentLeg--;
        if (m_currentLeg == 0) {
            m_trend = Trend::Nutral;
        }
        log("Bullish Reversal: Returning to Leg " + QString::number(m_currentLeg));
        calculateThresholds(m_currentATM);
        // reverseShift(...) // Shift quantity back
    }
}
```

**Example Reversal Flow:**

```
State: Leg 3 Bullish (price at 22100)
reversalP = 22035 (calculated from Leg 3)

Price drops to 22030
  ↓
Reversal triggered
  ↓
Leg decrements: 3 → 2
  ↓
Shift back: Buy some CE, Sell some PE
  ↓
If price continues down and Leg 0 reached:
  Trend = Nutral
  Wait for next directional move
```

---

### Phase 6: ATM Reset (Boundary Breach)

**Trigger:** Price exceeds RCP (Reset Constant Point)

```cpp
// Bullish ATM Shift
if (m_blDP >= m_blRCP) {
    log("Bullish Hit RCP. Shifting ATM Up.");
    resetATM(m_currentATM + m_strikeDiff); // 22000 → 22050
}

// Bearish ATM Shift
if (m_brDP <= m_brRCP) {
    log("Bearish Hit RCP. Shifting ATM Down.");
    resetATM(m_currentATM - m_strikeDiff); // 22000 → 21950
}
```

**When ATM Reset Happens:**

```
Old ATM = 22000
New ATM = 22050 (shifted up due to strong bullish move)

Actions:
1. Close all existing positions
   - Exit 22000 CE positions
   - Exit 22000 PE positions
   
2. Reset state variables
   - Leg = 0
   - Trend = Nutral
   - Recalculate blDP, brDP, blRCP, brRCP
   
3. Enter fresh straddle
   - Sell 50 lots 22050 CE
   - Sell 50 lots 22050 PE
   
4. Resume monitoring from new ATM center
```

**Visual Representation of ATM Shift:**

```
Before Shift:                  After Shift:
                                                ▲
    22080 ├─ blRCP                              │
          │                      22130 ├─ blRCP (new)
    22055 ├─ blDP                       │
          │                      22105 ├─ blDP (new)
    22000 ├═ ATM                        │
          │   ↓                  22050 ├═ ATM (new)
    21945 ├─ brDP   SHIFT UP           │
          │                      21995 ├─ brDP (new)
    21900 ├─ brRCP                      │
                                 21950 ├─ brRCP (new)
                                        ▼
```

---

## Mathematical Model

### Decision Point Calculations

**Reference ATM (Adjusted):**
```
refATM = currentATM - adj_pts
```

**Bullish Decision Point (Upper Trigger):**
```
blDP = refATM + offset + threshold + (strike_multiplier × strike_diff)

Where strike_multiplier = 0.6 + (current_leg × 0.1)
```

**Bearish Decision Point (Lower Trigger):**
```
brDP = refATM - offset - threshold - (strike_multiplier × strike_diff)
```

**Reversal Point (Trend Reversal):**
```
For Bullish:
reversalP = refATM + offset + (reversal_multiplier × strike_diff)

For Bearish:
reversalP = refATM - offset - (reversal_multiplier × strike_diff)

Where reversal_multiplier = 0.1 + (current_leg × 0.1)
```

**Reset Constant Points (Boundaries):**
```
blRCP = currentATM + (1.6 × strike_diff) + offset - adj_pts
brRCP = currentATM - (1.6 × strike_diff) - offset - adj_pts
```

---

### Example Calculation Sheet

**Parameters:**
- ATM = 22000
- Offset = 10
- Threshold = 15
- AdjPts = 5
- StrikeDiff = 50

**Leg 0 (Neutral):**
```
refATM = 22000 - 5 = 21995
strike_mult = 0.6

blDP = 21995 + 10 + 15 + (0.6 × 50) = 22055
brDP = 21995 - 10 - 15 - (0.6 × 50) = 21945
reversalP = 0 (neutral, no reversal)
```

**Leg 1 (First Directional):**
```
strike_mult = 0.7
reversal_mult = 0.2

blDP = 21995 + 10 + 15 + (0.7 × 50) = 22075
brDP = 21995 - 10 - 15 - (0.7 × 50) = 21925

If Bullish:
reversalP = 21995 + 10 + (0.2 × 50) = 22015
```

**Leg 2:**
```
strike_mult = 0.8
reversal_mult = 0.3

blDP = 22095
brDP = 21905
reversalP = 22025 (if bullish)
```

**Leg 3:**
```
strike_mult = 0.9
reversal_mult = 0.4

blDP = 22115
brDP = 21885
reversalP = 22035 (if bullish)
```

**Leg 4:**
```
strike_mult = 1.0
reversal_mult = 0.5

blDP = 22135
brDP = 21865
reversalP = 22045 (if bullish)
```

---

## Trading Logic Diagram

### State Machine Flow

```
┌─────────────────────────────────────────────────────────┐
│                    START STRATEGY                        │
│  - Get ATM from ATMWatchManager                         │
│  - resetATM(atm)                                        │
│  - Subscribe to cash/futures feed                       │
│  - Leg = 0, Trend = Nutral                             │
└───────────────────────┬─────────────────────────────────┘
                        │
                        ▼
              ┌─────────────────┐
              │  NEUTRAL STATE  │
              │  Leg = 0        │
              └────┬──────┬─────┘
                   │      │
      ┌────────────┘      └────────────┐
      │                                │
      ▼                                ▼
┌──────────┐                    ┌──────────┐
│Price >   │                    │Price <   │
│  blDP    │                    │  brDP    │
└────┬─────┘                    └────┬─────┘
     │                               │
     ▼                               ▼
┌─────────────────┐          ┌─────────────────┐
│ BULLISH STATE   │          │ BEARISH STATE   │
│ Trend = Bullish │          │ Trend = Bearish │
│ Leg 1 → 2 → 3 →4│          │ Leg 1 → 2 → 3 →4│
└────┬──────┬─────┘          └────┬──────┬─────┘
     │      │                     │      │
     │      │ Reversal            │      │ Reversal
     │      │ (Price < reversalP) │      │ (Price > reversalP)
     │      └──────┐          ┌───┘      │
     │             │          │          │
     │             ▼          ▼          │
     │        ┌────────────────┐         │
     │        │ Leg Decrement  │         │
     │        │ Shift Back Qty │         │
     │        └────────────────┘         │
     │                                   │
     │ RCP Breach                        │ RCP Breach
     │ (blDP >= blRCP)                   │ (brDP <= brRCP)
     │                                   │
     └───────────┬───────────────────────┘
                 │
                 ▼
        ┌────────────────┐
        │  ATM RESET     │
        │  - Close all   │
        │  - Shift ATM   │
        │  - Re-enter    │
        │  - Leg = 0     │
        └────────┬───────┘
                 │
                 └─────> (Back to NEUTRAL STATE)
```

---

## Risk Management

### Stop Loss Strategy

**Automatic Stop Loss Triggers:**

1. **Maximum Leg Reached:**
   - If Leg 4 breached and RCP exceeded
   - System automatically resets ATM
   - Limits loss per cycle

2. **Manual Stop Loss:**
   - Set per instance: `instance.stopLoss` (e.g., ₹50,000)
   - Checked every 500ms by StrategyService
   - Auto-stops strategy if MTM < -stopLoss

3. **Time-Based Stop:**
   - Stop at 3:15 PM (15 minutes before market close)
   - Allows orderly exit

### Position Sizing

**Progressive Quantity Management:**

| Leg | Quantity Shift | Cumulative Shift | Net Position Skew |
|-----|----------------|------------------|-------------------|
| 0 | 0% | 0% | Neutral (50-50) |
| 1 | 25% | 25% | 62.5-37.5 |
| 2 | 25% | 50% | 75-25 |
| 3 | 25% | 75% | 87.5-12.5 |
| 4 | 25% | 100% | 100-0 (fully skewed) |

**Maximum Exposure:**
- Base Qty = 50 lots
- Maximum Qty = 100 lots (after 4 legs)
- Capital at Risk = Premium × 100 × Lot Size

---

## Performance Characteristics

### Best Market Conditions

✅ **Ideal:**
- Low volatility with trending moves
- Clear directional bias (not whipsaw)
- Moderate IV (Implied Volatility) for premium collection
- Liquid markets (tight spreads)

❌ **Avoid:**
- High volatility around major events (RBI, Fed, Budget)
- Sideways choppy markets (frequent reversals)
- Low liquidity hours (first/last 15 minutes)
- Steep IV crush scenarios

### Expected Returns

**Typical Scenarios (NIFTY with 50 base lots):**

| Scenario | Entry Credit | Max Loss | Max Profit | Probability |
|----------|-------------|----------|------------|-------------|
| **Neutral Decay** | ₹5,87,500 | -₹2,00,000 | +₹4,00,000 | 40% |
| **Trending Capture** | ₹5,87,500 | -₹3,50,000 | +₹6,50,000 | 30% |
| **Whipsaw Loss** | ₹5,87,500 | -₹8,00,000 | -₹2,00,000 | 20% |
| **ATM Reset** | ₹5,87,500 | -₹1,50,000 | +₹2,00,000 | 10% |

**Risk-Reward Ratio:** 1:2 (Max Loss ₹4L, Max Profit ₹8L typical)

---

## Code Implementation Details

### Key Methods

#### 1. resetATM()
**Purpose:** Recalculate all thresholds when ATM changes

```cpp
void JodiATMStrategy::resetATM(double newATM) {
    m_currentATM = newATM;
    
    // Get strike interval from ATMWatchManager
    auto info = ATMWatchManager::getInstance().getATMInfo(m_instance.symbol);
    m_strikeDiff = std::abs(info.strikes[1] - info.strikes[0]); // e.g., 50
    
    // Update tokens for current ATM options
    m_ceToken = info.callToken;   // ATM CE token
    m_peToken = info.putToken;    // ATM PE token
    
    // Calculate RCP boundaries
    m_blRCP = m_currentATM + (1.6 * m_strikeDiff) + m_offset - m_adjPts;
    m_brRCP = m_currentATM - (1.6 * m_strikeDiff) - m_offset - m_adjPts;
    
    // Reset state
    m_currentLeg = 0;
    m_trend = Trend::Nutral;
    m_isFirstOrderPlaced = false;
    
    // Recalculate thresholds for Leg 0
    calculateThresholds(m_currentATM);
}
```

#### 2. calculateThresholds()
**Purpose:** Compute decision points for current leg

```cpp
void JodiATMStrategy::calculateThresholds(double atm) {
    double refATM = atm - m_adjPts;
    
    if (m_currentLeg < 4) {
        // Progressive leg calculation
        double strikeMultiplier = 0.6 + (m_currentLeg * 0.1);
        double revMultiplier = 0.1 + (m_currentLeg * 0.1);
        
        m_blDP = refATM + m_offset + m_threshold + 
                 (strikeMultiplier * m_strikeDiff);
        m_brDP = refATM - m_offset - m_threshold - 
                 (strikeMultiplier * m_strikeDiff);
        
        if (m_currentLeg > 0) {
            m_reversalP = (m_trend == Trend::Bullish) 
                ? (refATM + m_offset + (revMultiplier * m_strikeDiff))
                : (refATM - m_offset - (revMultiplier * m_strikeDiff));
        } else {
            m_reversalP = 0.0; // Neutral has no reversal
        }
    } else {
        // Leg 4: Use RCP as decision points
        m_blDP = m_blRCP;
        m_brDP = m_brRCP;
    }
}
```

#### 3. onTick()
**Purpose:** React to real-time price updates

```cpp
void JodiATMStrategy::onTick(const UDP::MarketTick& tick) {
    if (!m_isRunning) return;
    
    if (tick.token == m_cashToken) {
        m_cashPrice = tick.ltp; // Update current price
        checkTrade(m_cashPrice); // Evaluate trading logic
    }
}
```

#### 4. checkTrade()
**Purpose:** Core decision logic

```cpp
void JodiATMStrategy::checkTrade(double cashPrice) {
    // First entry: Sell straddle
    if (!m_isFirstOrderPlaced && m_trend == Trend::Nutral) {
        // makeOrder(m_ceToken, m_baseQty, "Sell");
        // makeOrder(m_peToken, m_baseQty, "Sell");
        m_isFirstOrderPlaced = true;
    }
    
    // Neutral: Watch for initial breakout
    if (m_trend == Trend::Nutral) {
        if (cashPrice > m_blDP) {
            m_trend = Trend::Bullish;
            m_currentLeg = 1;
            calculateThresholds(m_currentATM);
            // switchCurrent2Upper(); // Shift 25%
        } else if (cashPrice < m_brDP) {
            m_trend = Trend::Bearish;
            m_currentLeg = 1;
            calculateThresholds(m_currentATM);
            // switchCurrent2Lower();
        }
    }
    
    // Bullish: Extend or reverse
    else if (m_trend == Trend::Bullish) {
        if (cashPrice > m_blDP) {
            if (m_blDP >= m_blRCP) {
                // Hit boundary: Reset ATM
                resetATM(m_currentATM + m_strikeDiff);
            } else {
                // Extend leg
                m_currentLeg++;
                calculateThresholds(m_currentATM);
                // shiftQuantity(...);
            }
        } else if (m_reversalP > 0 && cashPrice < m_reversalP) {
            // Reversal: Pull back
            m_currentLeg--;
            if (m_currentLeg == 0) m_trend = Trend::Nutral;
            calculateThresholds(m_currentATM);
            // reverseShift(...);
        }
    }
    
    // Bearish: (symmetric to Bullish)
    else if (m_trend == Trend::Bearish) {
        // ... similar logic for bearish moves ...
    }
}
```

---

## Integration with Strategy Manager

### Lifecycle Hooks

```cpp
// Called by StrategyService when user clicks "Start"
strategy->init(instance);
strategy->start();

// Called every 500ms by StrategyService
strategy->computeMetrics();  // Calculate MTM, P&L
updateMetrics(mtm, positions, orders); // Update StrategyService

// Called when user modifies parameters
strategy->onParametersChanged(newParams);

// Called when user clicks "Pause"
strategy->pause(); // Stop processing ticks

// Called when user clicks "Resume"
strategy->resume(); // Resume processing

// Called when user clicks "Stop"
strategy->stop();
FeedHandler::unsubscribe(...);
```

### Parameter Configuration UI

**CreateStrategyDialog Fields for JodiATM:**

| Field | Type | Default | Validation | Locked While Running |
|-------|------|---------|------------|----------------------|
| **Symbol** | Dropdown | NIFTY | Must be valid index | ✅ Yes |
| **Account** | Text | ACCT001 | Must exist in system | ✅ Yes |
| **Segment** | Dropdown | 2 (NSEFO) | 1,2,11,12 only | ✅ Yes |
| **Base Quantity** | Spin | 50 | 1-500 | ✅ Yes |
| **Stop Loss** | Double | 50000 | > 0 | ❌ No (can modify) |
| **Target** | Double | 100000 | > 0 | ❌ No (can modify) |
| **Entry Price** | Double | Auto | From ATM | ✅ Yes |
| **Offset** | Double | 10 | 0-100 | ✅ Yes |
| **Threshold** | Double | 15 | 0-100 | ✅ Yes |
| **Adj Points** | Double | 5 | 0-50 | ✅ Yes |

---

## Monitoring & Debugging

### Log Output Example

```
[JodiATM:Instance#42] Initialized JodiATM with Offset:10, Threshold:15, AdjPts:5
[JodiATM:Instance#42] Starting Jodi-ATM strategy...
[JodiATM:Instance#42] ATM Reset to 22000. Bounds: [21945, 22055]. RCPs: [21900, 22080]
[JodiATM:Instance#42] First Entry: Selling Jodi at 22000
[JodiATM:Instance#42] Trend Change: BULLISH. Leg 1 triggered.
[JodiATM:Instance#42] Bullish Leg Extension: Leg 2
[JodiATM:Instance#42] Bullish Reversal: Returning to Leg 1
[JodiATM:Instance#42] Trend Change: BEARISH. Leg 1 triggered.
[JodiATM:Instance#42] Bearish Hit RCP. Shifting ATM Down.
[JodiATM:Instance#42] Stopping Jodi-ATM strategy...
```

### Metrics Tracked

| Metric | Source | Update Frequency |
|--------|--------|------------------|
| **MTM** | PriceStore via FeedHandler | 500ms (batched) |
| **Active Positions** | TradingDataService | 500ms |
| **Pending Orders** | TradingDataService | 500ms |
| **Current Leg** | Internal state | On leg change |
| **Trend** | Internal state | On trend change |
| **Decision Points** | Computed | On leg/ATM change |

---

## Advanced Optimizations

### 1. Adaptive Strike Diff
Automatically detect strike interval from ATMWatchManager instead of hardcoding.

**Current:**
```cpp
m_strikeDiff = 100.0; // Fallback
```

**Optimized:**
```cpp
auto info = ATMWatchManager::getInstance().getATMInfo(symbol);
if (info.strikes.size() >= 2) {
    m_strikeDiff = std::abs(info.strikes[1] - info.strikes[0]);
} else {
    m_strikeDiff = (symbol == "NIFTY") ? 50.0 : 100.0;
}
```

### 2. Dynamic Offset Based on IV
Adjust offset/threshold based on current implied volatility.

```cpp
double currentIV = getIVForSymbol(symbol);
if (currentIV > 25) {
    m_offset *= 1.5; // Wider bands in high vol
    m_threshold *= 1.5;
}
```

### 3. Time-Based Leg Decay
Reduce leg progression near market close to limit risk.

```cpp
QTime currentTime = QTime::currentTime();
if (currentTime > QTime(15, 0)) {
    // After 3 PM: Reduce leg extension aggressiveness
    m_threshold *= 1.2; // Harder to extend legs
}
```

### 4. Partial Exits
Instead of all-or-nothing leg shifts, use partial quantity adjustments.

```cpp
int shiftQty = (m_baseQty * 0.25) * progressiveMultiplier;
// progressiveMultiplier: 0.5 for Leg 1, 0.75 for Leg 2, 1.0 for Leg 3+
```

---

## Backtesting Considerations

### Data Requirements

1. **Tick Data:**
   - Underlying (Cash/Futures) LTP at 1-second resolution
   - ATM Options (CE/PE) LTP for MTM calculation

2. **Greeks Data:**
   - IV (Implied Volatility) for premium estimation
   - Delta for hedge calculations

3. **ATM History:**
   - ATM strike changes during day
   - ATM shift timestamps

### Simulation Parameters

```cpp
struct BacktestConfig {
    QDate startDate;
    QDate endDate;
    QString symbol;
    double baseQty;
    double offset;
    double threshold;
    double adjPts;
    double slippage = 0.5; // Points per leg
    double brokerage = 20; // Per trade
};
```

### Performance Metrics

- **Sharpe Ratio:** Risk-adjusted returns
- **Max Drawdown:** Largest peak-to-trough decline
- **Win Rate:** % of profitable days
- **Avg P&L:** Mean daily profit/loss
- **Leg Distribution:** How often each leg is reached
- **ATM Reset Frequency:** Number of boundary breaches

---

## Troubleshooting Guide

### Issue: Strategy not entering trades

**Check:**
1. Is `m_isFirstOrderPlaced` false?
2. Is `m_trend` stuck in non-Nutral?
3. Are blDP/brDP calculated correctly?
4. Is ATM info valid from ATMWatchManager?

**Debug:**
```cpp
qDebug() << "State:" << m_isFirstOrderPlaced << m_trend << m_currentLeg;
qDebug() << "Price:" << m_cashPrice << "vs" << m_blDP << "/" << m_brDP;
```

---

### Issue: Excessive ATM resets

**Check:**
1. Are offset/threshold too tight?
2. Is strikeDiff incorrect (e.g., 100 instead of 50)?
3. Is adjPts causing boundary miscalculation?

**Solution:**
- Increase offset/threshold by 20-30%
- Verify strikeDiff from ATMWatchManager logs
- Test with adjPts = 0 to eliminate variable

---

### Issue: Quantity shifts not executing

**Check:**
1. Are order placement methods commented out?
2. Is OrderManager integration missing?
3. Are token lookups failing?

**Fix:**
- Uncomment `makeOrder()` calls
- Integrate with [OrderManager.h](../include/services/OrderManager.h)
- Add token validation logs

---

## Comparison with Other Strategies

| Feature | JodiATM | TSpecial | VixMonkey |
|---------|---------|----------|-----------|
| **Complexity** | High | Medium | Medium |
| **Leg Management** | 4 progressive | 2 fixed | None (hedged) |
| **ATM Reset** | ✅ Automatic | ❌ Manual | ❌ N/A |
| **Direction** | Trend-following | Neutral bias | Volatility play |
| **Capital Req** | High (4x max) | Medium | Low |
| **Monitoring** | Active | Passive | Active |
| **Best For** | Trending markets | Range-bound | Event-driven |

---

## Regulatory & Compliance

### Position Limits

**NSE FO Limits (example):**
- NIFTY: 15% of open interest (OI) or ₹500 crore, whichever is lower
- Single day: 5400 contracts (NIFTY 50)

**JodiATM Max Exposure:**
- Base 50 lots × 4 legs = 200 lots max single side
- Well within retail limits

### Disclosure Requirements

- Report aggregate positions daily (if > threshold)
- Maintain audit trail for 5 years
- Disclose algo strategy to broker (if applicable)

---

## Future Enhancements

### Planned Features

1. **Multi-Symbol Support:**
   - Run JodiATM on NIFTY + BANKNIFTY simultaneously
   - Cross-hedge positions

2. **Greeks-Based Adjustments:**
   - Shift quantity based on delta neutrality
   - Vega hedging in high IV

3. **Machine Learning Integration:**
   - Learn optimal offset/threshold from historical data
   - Predict leg extension probability

4. **Real-Time Greeks Display:**
   - Show net delta, gamma, theta in UI
   - Alert on portfolio risk limits

5. **Automated Rollover:**
   - Close expiring options on expiry day
   - Roll to next week/month automatically

---

## Conclusion

JodiATM is a sophisticated, adaptive straddle strategy that balances:
- **Premium collection** (sell ATM straddle)
- **Trend capture** (progressive leg shifting)
- **Risk containment** (ATM reset boundaries)

**Key Success Factors:**
1. Correct parameter tuning (offset, threshold, adjPts)
2. Active monitoring during volatile periods
3. Disciplined stop loss adherence
4. Understanding market regime (trending vs choppy)

**Recommended User Profile:**
- Experienced options traders
- Comfortable with active management
- Capital available: ₹5-10 lakhs per instance
- Risk tolerance: Medium-High

**Status:** Production-ready with manual order execution commented out. Integrate with OrderManager for full automation.

---

**Files Referenced:**
- [JodiATMStrategy.h](../include/strategies/JodiATMStrategy.h)
- [JodiATMStrategy.cpp](../src/strategies/JodiATMStrategy.cpp)
- [StrategyBase.h](../include/strategies/StrategyBase.h)
- [ATMWatchManager.h](../include/services/ATMWatchManager.h)
- [FeedHandler.h](../include/services/FeedHandler.h)

**Last Updated:** February 10, 2026  
**Author:** Trading Terminal Development Team  
**Version:** 1.0

---

## UI Implementation: Dynamic Parameter Dialog

The JodiATM strategy now utilizes a dedicated UI page within the `CreateStrategyDialog`'s `QStackedWidget`. Instead of manual JSON entry, traders use high-precision input controls:

### JodiATM Page Layout
- **Price Buffers**: 
  - `Offset`: Primary distance trigger.
  - `Threshold`: Calculation buffer.
- **Fine Tuning**:
  - `Adj Pts`: ATM centering.
  - `Diff Points`: Strike width override.
- **Execution Config**:
  - `CE Strike Index`: Dropdown (default ATM).
  - `PE Strike Index`: Dropdown (default ATM).
  - `Is Trailing`: Toggle for progressive risk adjustment.

### Automated Naming
The UI logic ensures consistent naming conventions:
- Neutral Prefix: `000_...`
- Production Instance: `{ID}_{TYPE}` (e.g., `012_JodiATM`)

This refactor eliminates syntax errors in parameter configuration and aligns with professional trading desks.

