# Market Trading Strategies - In-Depth Analysis

**Project:** Trading Terminal C++  
**Date:** February 17, 2026  
**Purpose:** Comprehensive analysis of prevailing trading strategies to design flexible base strategy framework  
**Target Market:** Indian Stock Market (NSE, BSE - Cash, F&O)

---

## Table of Contents

1. [Introduction](#introduction)
2. [Equity/Cash Market Strategies](#equitycash-market-strategies)
3. [Futures Trading Strategies](#futures-trading-strategies)
4. [Options Trading Strategies](#options-trading-strategies)
5. [Intraday/Scalping Strategies](#intradayscalping-strategies)
6. [Algorithmic Trading Strategies](#algorithmic-trading-strategies)
7. [Option Selling Strategies](#option-selling-strategies)
8. [Volatility-Based Strategies](#volatility-based-strategies)
9. [Market Neutral Strategies](#market-neutral-strategies)
10. [Strategy Classification Framework](#strategy-classification-framework)
11. [Base Strategy Wireframe Design](#base-strategy-wireframe-design)
12. [Implementation Requirements Matrix](#implementation-requirements-matrix)

---

## Introduction

### Purpose of This Document

This document provides an **exhaustive analysis** of trading strategies currently used by retail and institutional traders in Indian markets. The goal is to:

1. **Understand** the diverse needs of different trader types
2. **Identify** common patterns and building blocks
3. **Design** a flexible base strategy framework that can accommodate 90%+ of strategies
4. **Prioritize** features based on popularity and complexity

### Indian Market Context

**Key Segments:**
- **NSE Cash Market (NSECM):** Equity delivery, intraday
- **NSE Futures & Options (NSEFO):** Index + Stock F&O
- **BSE Cash Market (BSECM):** Equity delivery, intraday
- **BSE Futures & Options (BSEFO):** Limited liquidity

**Most Liquid Instruments:**
- NIFTY 50 Index Futures/Options
- BANKNIFTY Index Futures/Options
- FINNIFTY Index Futures/Options
- Stock Futures (Top 50 stocks)
- Stock Options (Limited stocks)

**Trading Hours:**
- Pre-Open: 9:00 AM - 9:15 AM
- Normal: 9:15 AM - 3:30 PM
- Closing: 3:30 PM - 3:40 PM (for F&O)

---

## Equity/Cash Market Strategies

### 1. **Positional Trading (Swing Trading)**

#### Description
Holding positions for **days to weeks** based on technical/fundamental analysis.

#### Typical Timeframe
- Entry: Daily/Weekly charts
- Exit: Days to months
- Holding: 2-30 days average

#### Entry Signals
- Breakout above resistance
- Trendline break with volume
- Golden cross (50-day MA > 200-day MA)
- Fundamental catalyst (results, news)
- Chart patterns (Cup & Handle, Double Bottom)

#### Exit Signals
- Target reached (10-30% gain)
- Stop loss hit (5-10% loss)
- Breakdown below support
- Negative news/results
- Time-based (trailing stop after 15 days)

#### Example Strategy: Breakout Trading
```
Entry Conditions:
- Stock breaks 52-week high
- Volume > 2x average
- RSI between 50-70 (not overbought)
- Market trend bullish (NIFTY above 50-day MA)

Position Sizing:
- 5% of portfolio per stock
- Max 10 stocks at a time

Risk Management:
- Stop Loss: 8% from entry
- Target: 20% gain
- Trailing SL: 10% after 15% profit

Exit:
- Target hit OR stop loss OR 30 days elapsed
```

#### Real-World Example
```
Stock: RELIANCE
Entry: ₹2,450 (breakout on earnings day)
Volume: 15M (avg 6M)
Position: 100 shares (₹2,45,000)
Stop Loss: ₹2,254 (8% = ₹19,600 risk)
Target: ₹2,940 (20% = ₹49,000 gain)
Result: Exited at ₹2,850 after 12 days (+16.3%)
```

---

### 2. **Momentum Trading**

#### Description
Buying **strong stocks getting stronger**, short-term focused (1-5 days).

#### Entry Signals
- Stock up 5%+ with high volume
- Sector rotation (new sector gaining strength)
- News-driven momentum (acquisition, govt policy)
- Relative strength > market (outperforming NIFTY)

#### Exit Signals
- Momentum fades (reduced volume)
- Price fails to make new high for 2 days
- Quick 5-10% profit booking
- Stop loss 3-5% from peak

#### Example Strategy: Gap-Up Momentum
```
Entry:
- Stock gaps up >3% at open
- Pre-market volume >50% of daily avg
- News catalyst present
- Enter after 9:30 AM if sustains above gap level

Exit:
- Intraday target: 5-7%
- Stop loss: Below gap level (usually 2-3%)
- Time exit: 3:00 PM if no momentum
```

---

### 3. **Value Investing (Long-Term)**

#### Description
Buying **undervalued stocks** based on fundamentals, holding for years.

#### Entry Criteria
- P/E ratio < sector average
- P/B ratio < 3
- Debt-to-Equity < 1
- ROE > 15%
- Dividend yield > 2%
- Management quality high
- Temporary bad news (not structural)

#### Exit Criteria
- Target valuation reached (P/E normalization)
- Fundamentals deteriorate
- 3-5 year time frame
- Better opportunity elsewhere

#### Example Strategy: Undervalued Blue Chips
```
Screening Criteria:
- Market Cap > ₹50,000 Cr
- P/E < 15 (when sector avg is 25)
- Consistent profits (last 5 years)
- Debt manageable
- Stock down 30%+ from peak (temporary reasons)

Entry:
- Buy in tranches (3 installments over 3 months)
- Average down if it falls further

Position:
- 10-20% of portfolio per stock
- 5-8 stocks total

Exit:
- P/E normalizes to sector average (50-100% gain)
- Hold minimum 2 years
- Dividend reinvestment
```

---

## Futures Trading Strategies

### 4. **Trend Following (Futures)**

#### Description
Riding **established trends** in index/stock futures using moving averages.

#### Entry Signals
- Price > 20-day EMA > 50-day EMA (uptrend)
- MACD crossover positive
- ADX > 25 (strong trend)
- Enter on pullback to 20-day EMA

#### Exit Signals
- Price crosses below 20-day EMA
- MACD turns negative
- Target: 1.5x risk-reward
- Trailing stop below recent swing low

#### Example Strategy: NIFTY Trend Following
```
Instrument: NIFTY Futures (1 lot = 50 qty)

Entry:
- NIFTY closes above 50-day EMA for 3 consecutive days
- Price pulls back to 20-day EMA
- RSI > 50
- Buy at 20-day EMA support

Lot Size:
- 1-2 lots (₹12-24 lakh exposure at NIFTY 24000)

Risk:
- Stop Loss: Below 50-day EMA (usually 1-1.5%)
- Stop loss amount: ₹3,000-4,500 per lot
- Target: 3-4% (₹9,000-12,000 per lot)

Exit:
- Price closes below 20-day EMA
- OR target reached
- OR weekly expiry approach (roll over)
```

---

### 5. **Reversal Trading (Futures)**

#### Description
Catching **trend reversals** at support/resistance using oscillators.

#### Entry Signals
- Price reaches major support/resistance
- RSI oversold (<30) or overbought (>70)
- Bullish/Bearish divergence
- Candlestick reversal patterns (Hammer, Shooting Star)
- Volume analysis (selling climax at bottom)

#### Exit Signals
- Quick scalp (1-2% move)
- Reversal back to mean
- Stop loss if reversal fails (0.5-1%)

#### Example Strategy: Mean Reversion
```
Instrument: BANKNIFTY Futures

Entry (Long at Support):
- BANKNIFTY at support level (previous week low)
- RSI < 30 (oversold)
- Hammer candlestick forms
- Enter at candle close

Position:
- 1 lot (25 qty, ₹12 lakh exposure at 48000)

Risk:
- Stop Loss: Below hammer low (30-50 points = ₹750-1,250)
- Target: Resistance level (200-300 points = ₹5,000-7,500)

Exit:
- Target hit in 2-4 hours (intraday)
- OR close position by 3:15 PM
```

---

### 6. **Carry Trade (Calendar Spread)**

#### Description
Exploiting the difference between **near-month and far-month** futures contracts.

#### Logic
- Futures trade at premium to spot (contango)
- Premium decays as expiry approaches
- Buy far month, sell near month = Profit from premium difference

#### Entry Signals
- Near month premium > 0.5%
- Far month premium < near month
- 5-10 days before near month expiry

#### Exit Signals
- Near month expiry day
- Premium difference narrows/reverses
- Risk: Sudden market crash increases far month premium

#### Example Strategy: NIFTY Calendar Spread
```
Setup:
- Current month expiry in 5 days
- Next month expiry in 33 days
- Current: NIFTY Spot 24000
- Feb Futures: 24080 (+80 points premium)
- Mar Futures: 24130 (+130 points premium)

Trade:
- Sell Feb Futures: 24080 (1 lot = 50 qty)
- Buy Mar Futures: 24130 (1 lot = 50 qty)
- Net premium difference: 50 points
- Risk: ₹2,500 (if difference increases to 100)
- Profit: ₹1,500-2,000 (if difference reduces to 20-30)

Exit:
- Feb expiry day (settle both)
- If spot moves >2%, exit immediately (volatility risk)
```

---

## Options Trading Strategies

### 7. **Option Buying (Directional)**

#### Description
Buying **Call/Put options** for leveraged directional bets.

#### When to Use
- Expecting big move (>2%) in underlying
- Low IV environment (cheaper premiums)
- Time to expiry: 7-15 days optimal
- News/event-driven (Budget, Fed decision)

#### Entry Criteria
**Call Buying:**
- Strong bullish signal (breakout)
- Buy ATM or slightly OTM strike
- IV percentile < 50
- Greeks: Delta 0.5-0.7, Theta manageable

**Put Buying:**
- Strong bearish signal (breakdown)
- Buy ATM or slightly OTM strike
- IV spike expected

#### Risk Management
- Never risk more than premium paid
- Exit at 50% loss (time decay)
- Book profit at 50-100% gain
- Avoid holding till expiry (theta decay)

#### Example Strategy: Breakout Call Buying
```
Scenario: NIFTY breakout above 24000

Setup:
- NIFTY Spot: 24050 (just broke 24000)
- Days to expiry: 10 days
- ATM Call (24000 CE): Premium ₹180
- Buy 1 lot (50 qty) = ₹9,000 investment

Greeks:
- Delta: 0.55 (₹27.5 profit per 50-point move)
- Theta: -15 (losing ₹750/day to time decay)
- Vega: 5 (₹250 gain per 1% IV increase)

Target Scenarios:
- NIFTY 24200 (2 days): Call = ₹280, Profit = ₹5,000 (56%)
- NIFTY 23950 (break fails): Call = ₹110, Loss = ₹3,500 (39%)

Exit Rules:
- Book profit at 60% (₹5,400)
- Stop loss at 50% (₹4,500)
- Exit if 5 days pass without move (theta kill)
```

---

### 8. **Option Selling (Premium Collection)**

#### Description
**Selling options** to collect premium, betting on time decay + low volatility.

#### When to Use
- Expecting sideways/range-bound market
- High IV environment (expensive premiums)
- After big move (volatility contraction expected)
- Expiry week (rapid theta decay)

#### Popular Strategies

#### 8.1. **Naked Put Selling**
```
Logic: Sell OTM Put, collect premium, pocket if expires worthless

Example:
- NIFTY at 24000
- Sell 23800 PE @ ₹50 (1 lot = 50 qty = ₹2,500 credit)
- Probability of profit: 75-80% (if NIFTY stays >23800)

Risk:
- Unlimited downside if NIFTY crashes
- Margin required: ₹1,20,000 (SPAN + Exposure)
- Max loss: Catastrophic (if NIFTY goes to 20000)

Exit:
- Buy back at 80% profit (₹10 premium = ₹2,000 profit)
- OR let expire worthless
- Stop loss at 2x premium (₹100 = ₹2,500 loss)
```

#### 8.2. **Covered Call (Stock + Call Sell)**
```
Logic: Own stock, sell Call to earn extra income

Example:
- Own 500 shares of RELIANCE @ ₹2,800
- Sell 5 lots of 2900 CE @ ₹40 (₹20,000 income)
- If RELIANCE stays <2900: Keep ₹20,000
- If RELIANCE >2900: Shares called away at 2900

Purpose:
- Earn extra 1-2% monthly income on holdings
- Reduce cost basis of stock
- Works in sideways market
```

#### 8.3. **Credit Spreads**
```
Bull Put Spread (Bullish):
- Sell 23800 PE @ ₹50
- Buy 23700 PE @ ₹30
- Net credit: ₹20 × 50 = ₹1,000
- Max loss: ₹100 × 50 = ₹5,000 (if NIFTY <23700)
- Risk-reward: 5:1
- Margin: ₹5,000 (defined risk)

Bear Call Spread (Bearish):
- Sell 24200 CE @ ₹50
- Buy 24300 CE @ ₹30
- Net credit: ₹1,000
- Max loss: ₹5,000 (if NIFTY >24300)
```

---

### 9. **Iron Condor (Range-Bound)**

#### Description
Profit from **sideways market** by selling both Call and Put spreads.

#### Structure
```
Example: NIFTY trading at 24000

Sell Put Spread:
- Sell 23800 PE @ ₹50
- Buy 23700 PE @ ₹30
- Credit: ₹1,000

Sell Call Spread:
- Sell 24200 CE @ ₹50
- Buy 24300 CE @ ₹30
- Credit: ₹1,000

Total Credit: ₹2,000
Max Loss: ₹5,000 (one side)
Breakeven: 23780 - 24220
Win Zone: NIFTY stays between 23780-24220
Probability: 60-70%
```

#### When to Deploy
- After big move (expecting consolidation)
- Low volatility expected
- 5-10 days to expiry
- Defined range market (support/resistance clear)

#### Risk Management
- Exit at 50% profit (₹1,000)
- Stop loss at 2x loss (₹4,000)
- Adjust if market breaks out (roll strikes)
- Close winners early, let losers recover

---

### 10. **Straddle/Strangle (Volatility Play)**

#### 10.1. **Long Straddle (High Volatility Expected)**

#### Description
Buy **ATM Call + ATM Put**, profit from big move either direction.

#### When to Use
- Before major events (Budget, RBI policy, election results)
- Expecting >3% move but direction unknown
- IV low (cheap premiums)

#### Example: Budget Day Straddle
```
Setup:
- Budget announcement at 11 AM
- NIFTY at 24000
- IV: 15 (low)

Trade:
- Buy 24000 CE @ ₹150
- Buy 24000 PE @ ₹150
- Total cost: ₹15,000 (for 1 lot)

Breakeven:
- Upside: 24000 + 300 = 24300
- Downside: 24000 - 300 = 23700
- Need >1.25% move to profit

Scenarios:
- NIFTY 24500: Call = ₹520, Put = ₹10, Profit = ₹8,000
- NIFTY 23500: Call = ₹10, Put = ₹520, Profit = ₹8,000
- NIFTY 24100: Call = ₹180, Put = ₹80, Loss = ₹4,000 (no big move)

Risk:
- Max loss = ₹15,000 (if no big move)
- Theta decay brutal (losing ₹2,000/day)
- Exit same day (don't hold overnight)
```

#### 10.2. **Short Straddle (Low Volatility Expected)**

#### Description
Sell **ATM Call + ATM Put**, collect premium in range-bound market.

#### When to Use
- After event (volatility crush)
- Market in tight range
- High IV (expensive premiums)
- Expiry week (rapid decay)

#### Example: Post-Event Straddle Sell
```
Setup:
- Day after Budget (volatility crashed)
- NIFTY at 24000
- IV: 25 → 15 (volatility crush)

Trade:
- Sell 24000 CE @ ₹180
- Sell 24000 PE @ ₹180
- Total credit: ₹18,000

Risk:
- Unlimited on both sides (catastrophic)
- Margin: ₹2,40,000 (high)

Profit Zone:
- NIFTY stays 23820-24180 (±0.75%)
- Collect full premium

Exit:
- 50% profit (₹9,000) → buy back
- Stop loss if breaks 24150 or 23850
- Hedge with wings (convert to Iron Condor)
```

---

### 11. **Ratio Spreads (Unbalanced)**

#### Description
Sell **more options than you buy**, collecting extra premium.

#### Call Ratio Spread (Moderately Bullish)
```
Example:
- Buy 1 lot 24000 CE @ ₹150 (₹7,500)
- Sell 2 lots 24200 CE @ ₹80 (₹8,000 credit)
- Net credit: ₹500

Scenarios:
- NIFTY <24000: Max loss = ₹500 (small)
- NIFTY = 24200: Max profit = ₹10,500 (sweet spot)
- NIFTY >24400: Unlimited loss (disaster)

Purpose:
- Free trade (net credit)
- Profit from moderate rise
- Risk: Runaway rally
```

#### Put Ratio Spread (Moderately Bearish)
```
- Buy 1 lot 24000 PE @ ₹150
- Sell 2 lots 23800 PE @ ₹80
- Net credit: ₹500

Sweet spot: 23800 (max profit)
Risk: Crash below 23600 (unlimited loss)
```

---

## Intraday/Scalping Strategies

### 12. **Opening Range Breakout (ORB)**

#### Description
Trade the **breakout of first 15-30 minutes** range.

#### Logic
- Market decides direction in first 30 min
- Breakout above 9:45 high = Bullish day
- Breakdown below 9:45 low = Bearish day

#### Entry Rules
```
Setup:
- Note high/low between 9:15-9:45 AM
- NIFTY High: 24050, Low: 23980
- Range: 70 points
- Volume in first 30 min > 50% of daily avg

Long Entry:
- NIFTY breaks above 24050 (ORB high)
- Volume confirmation
- Enter at 24055-24060 (after breakout)

Short Entry:
- NIFTY breaks below 23980 (ORB low)
- Enter at 23975-23970

Position:
- 1-2 lots futures

Stop Loss:
- Long: Below ORB low (23980) = 70-80 points
- Short: Above ORB high (24050) = 70-80 points

Target:
- 1.5x range = 105 points
- Long target: 24155
- Short target: 23875

Exit by 3:15 PM (no overnight)
```

#### Win Rate
- 60-65% in trending days
- Fails in choppy/range-bound days
- Works best on event days (F&O expiry, macro data)

---

### 13. **VWAP Trading**

#### Description
Use **Volume Weighted Average Price** as dynamic support/resistance.

#### Logic
- VWAP = Fair value of the day
- Price > VWAP = Bullish control
- Price < VWAP = Bearish control
- Institutions use VWAP for benchmarking

#### Entry Rules
```
Long Setup:
- Price pulls back to VWAP in uptrend
- Price bounces from VWAP (support)
- Volume confirmation at bounce
- RSI > 50
- Enter slightly above VWAP

Short Setup:
- Price rallies to VWAP in downtrend
- Price rejects VWAP (resistance)
- Enter slightly below VWAP

Position: 1 lot futures

Stop Loss:
- Long: 20-30 points below VWAP
- Short: 20-30 points above VWAP

Target:
- Previous day high/low
- OR 50-100 points (scalp)

Time:
- Best during 10 AM - 2 PM (stable VWAP)
- Avoid first 15 min (volatile)
```

---

### 14. **Scalping (Tick Trading)**

#### Description
**Ultra-short duration** trades (1-5 minutes), capturing 10-30 points.

#### Requirements
- Level 2 order book (bid-ask visibility)
- Sub-second execution speed
- Low brokerage (₹10-20 per trade)
- High-frequency (20-50 trades/day)

#### Entry Signals
```
Buy Signal:
- Large buying interest at bid (order book)
- Sudden spike in buy volume
- Price at support (pivot point, previous candle low)
- Enter aggressively at bid price

Sell Signal:
- Large selling interest at ask
- Price at resistance
- Enter at ask price

Position:
- NIFTY Futures: 1-2 lots
- BANKNIFTY: 1 lot (more volatile)

Exit:
- Target: 20-30 points (₹1,000-1,500 per lot)
- Stop loss: 10-15 points (₹500-750 per lot)
- Exit immediately if goes against
- Hold max 2-5 minutes

Risk-Reward: 1:2
Win Rate Target: 60-70% (need high win rate)
Daily Target: ₹5,000-10,000
```

#### Scalping Strategies

**Strategy 1: Pullback Scalping**
```
Logic:
- Strong trend (EMA 5 > EMA 13)
- Wait for pullback to EMA 5
- Enter when price bounces from EMA 5
- Exit at recent swing high

Example:
- NIFTY in uptrend at 24000
- Pulls back to 23980 (EMA 5)
- Buy at 23982
- Target: 24020 (recent high)
- SL: 23970 (below EMA 5)
- Profit: 38 points in 3 minutes
```

**Strategy 2: News-Based Scalping**
```
- Morning news triggers spike
- Wait for first 5-min candle to complete
- Enter on 2nd candle if momentum continues
- Exit after 20-30 points
- Don't hold through volatile swings
```

---

### 15. **Pivot Point Trading**

#### Description
Use **mathematical pivot levels** for intraday support/resistance.

#### Calculation
```
Pivot Point (PP) = (High + Low + Close) / 3

Resistance Levels:
R1 = (2 × PP) - Low
R2 = PP + (High - Low)
R3 = High + 2(PP - Low)

Support Levels:
S1 = (2 × PP) - High
S2 = PP - (High - Low)
S3 = Low - 2(High - PP)
```

#### Trading Rules
```
Long Setup:
- Price at S1 or S2 (support)
- Buy with target R1
- Stop loss below S2

Short Setup:
- Price at R1 or R2 (resistance)
- Sell with target S1
- Stop loss above R2

Example (NIFTY):
Previous Day: H=24100, L=23900, C=24050
PP = 24017
R1 = 24133, R2 = 24250, R3 = 24367
S1 = 23900, S2 = 23783, S3 = 23667

Today's Trade:
- NIFTY opens at 24020 (near PP)
- Falls to 23905 (S1)
- Buy at S1 with target R1
- SL at 23880 (25 points risk)
- Target at 24133 (228 points reward)
- Exit at 24110 (205 points profit in 2 hours)
```

---

## Algorithmic Trading Strategies

### 16. **Mean Reversion**

#### Description
Price tends to **revert to average** after extreme moves.

#### Logic
- Standard deviation bands (Bollinger Bands)
- Price at lower band (oversold) → Buy
- Price at upper band (overbought) → Sell
- Mean = 20-period SMA

#### Entry Conditions
```
Long Entry:
- Price touches lower Bollinger Band
- RSI < 30 (oversold confirmation)
- Candle closes above lower band (reversal)
- Enter at close of reversal candle

Short Entry:
- Price touches upper Bollinger Band
- RSI > 70 (overbought)
- Candle closes below upper band

Position: 1 lot futures

Stop Loss:
- Long: Below recent swing low (50-100 points)
- Short: Above recent swing high

Target:
- Middle band (20-period SMA)
- OR 1.5x risk amount

Time Horizon: 2-4 hours to 1 day
```

#### Example Backtest Results
```
Instrument: NIFTY Futures
Period: 6 months (Jan-Jun 2025)
Trades: 45
Win Rate: 67% (30 wins, 15 losses)
Avg Win: ₹8,500
Avg Loss: ₹4,200
Net Profit: ₹1,90,500
Max Drawdown: 12%
```

---

### 17. **Pairs Trading (Statistical Arbitrage)**

#### Description
Trade two **correlated stocks** when correlation breaks.

#### Logic
- Find stocks that move together (correlation >0.8)
- When spread widens → Trade mean reversion
- Long underperformer, Short outperformer
- Capture spread convergence

#### Popular Pairs (Indian Markets)
```
1. HDFC Bank & ICICI Bank
2. TCS & Infosys
3. Reliance & ONGC
4. Asian Paints & Berger Paints
5. Maruti & Tata Motors
```

#### Entry Rules
```
Step 1: Calculate Spread
Spread = Price_Stock1 - (Hedge_Ratio × Price_Stock2)
Hedge Ratio = Correlation coefficient

Step 2: Calculate Z-Score
Z-Score = (Current_Spread - Mean_Spread) / StdDev_Spread

Entry Signals:
- Z-Score > +2: Spread too wide
  Action: Short Stock1, Long Stock2
  
- Z-Score < -2: Spread too narrow
  Action: Long Stock1, Short Stock2

Exit:
- Z-Score returns to 0 (mean)
- OR 10 days elapsed (time stop)
- OR correlation breaks (emergency exit)
```

#### Example Trade
```
Pair: HDFC Bank & ICICI Bank
Date: Jan 15, 2025

Prices:
- HDFC: ₹1,650
- ICICI: ₹1,150
- Historical Hedge Ratio: 1.42
- Expected Spread: 1650 - (1.42 × 1150) = 17
- Actual Spread: 50 (30-day low)
- Z-Score: +2.5 (spread too wide)

Trade:
- Short HDFC Futures: 1 lot (1,250 shares)
- Long ICICI Futures: 2 lots (1,750 shares × 1.42 = 2,485 equivalent)
- Market Neutral: Dollar-neutral position

Result (7 days):
- HDFC: ₹1,650 → ₹1,630 (₹25,000 profit)
- ICICI: ₹1,150 → ₹1,160 (₹17,500 profit)
- Total: ₹42,500 profit
- Risk: Minimal (market-neutral)
```

---

### 18. **Market Making (Bid-Ask Spread Capture)**

#### Description
Simultaneously **place buy and sell orders**, capture spread.

#### Logic
- Place buy order at bid price
- Place sell order at ask price
- Pocket the spread (₹0.05-2 per share)
- High volume, low margin
- Requires algo execution

#### Requirements
- High-frequency trading infrastructure
- Co-located servers (latency <1ms)
- Smart order routing
- Risk management (inventory limits)

#### Example
```
Stock: RELIANCE
Bid: ₹2,800.00
Ask: ₹2,800.50
Spread: ₹0.50

Strategy:
- Place buy order at ₹2,800.05 (just above bid)
- Place sell order at ₹2,800.45 (just below ask)
- If both execute: Profit = ₹0.40 per share
- Volume: 10,000 shares/day
- Daily profit: ₹4,000
- Risk: Inventory holding (if only one side fills)

Exit/Hedging:
- Max inventory: 5,000 shares long/short
- If inventory exceeds: Hedge with futures
- Close all positions by 3:15 PM
```

---

### 19. **Momentum + Volume Breakout (Algo)**

#### Description
Automated detection of **breakout with volume surge**.

#### Algorithm Logic
```python
def detect_breakout():
    # Parameters
    volume_threshold = 2.0  # 2x average volume
    price_threshold = 0.5   # 0.5% above resistance
    
    # Scan all stocks
    for stock in nifty_50:
        current_price = get_ltp(stock)
        resistance = get_52_week_high(stock)
        volume = get_current_volume(stock)
        avg_volume = get_avg_volume(stock, 20_days)
        
        # Breakout conditions
        if (current_price > resistance * (1 + price_threshold/100) and
            volume > avg_volume * volume_threshold and
            time_now() < "14:30"):
            
            # Execute trade
            place_order(
                symbol=stock,
                side="BUY",
                quantity=calculate_position_size(stock),
                order_type="MARKET"
            )
            
            # Set risk management
            set_stop_loss(current_price * 0.98)  # 2% SL
            set_target(current_price * 1.05)     # 5% Target
            
def on_tick_update(stock, tick):
    # Real-time monitoring
    if has_position(stock):
        check_exit_conditions(stock, tick)
```

#### Execution
- Scan frequency: Every 1 second
- Max positions: 5 stocks simultaneously
- Position size: ₹2 lakh per stock
- Exit: Target/SL or 3:15 PM (whichever first)

---

## Option Selling Strategies

### 20. **Weekly Expiry Theta Decay**

#### Description
Sell **weekly options** on Thursday/Friday to capture rapid theta decay.

#### Logic
- Theta decay accelerates in last 2 days
- Probability of expiring worthless: 70-80% for OTM
- Sell on Thursday morning, let expire on Thursday 3:30 PM

#### Strategy: Thursday Morning Strangle Sell
```
Day: Thursday (Expiry day)
Time: 9:30 AM
NIFTY: 24000
IV: 18 (moderate)

Trade:
- Sell 24200 CE @ ₹30 (out of money, delta 0.25)
- Sell 23800 PE @ ₹30 (out of money, delta 0.25)
- Total credit: ₹3,000 per lot
- Holding time: 6 hours

Greeks at 9:30 AM:
- Theta: -50 per option (losing ₹100/hour total)
- For option buyer: Losing ₹2,500/day
- For option seller: Earning ₹2,500/day

By 3:30 PM (expiry):
- If NIFTY stays 23800-24200: Both expire worthless → ₹3,000 profit
- If NIFTY at 24300: Call = ₹100, Put = ₹0 → ₹500 loss
- If NIFTY at 23700: Call = ₹0, Put = ₹100 → ₹500 loss

Risk:
- Black swan event (crash/rally)
- Set stop loss at 2x premium (₹60)
- Max loss per side: ₹1,500
```

#### Advanced: Dynamic Adjustment
```
Scenario: NIFTY moves towards 24200

Adjustment at 2:00 PM:
- NIFTY at 24180 (approaching call strike)
- Call premium: ₹50 (increased from ₹30)
- Action: Buy back call at ₹50 (₹1,000 loss)
- Keep put for full profit (₹1,500)
- Net: ₹500 profit (instead of potential big loss)
```

---

### 21. **Jodi ATM (ATM Straddle Selling)**

#### Description
Sell **At-The-Money Call + Put**, most popular among retail option sellers.

#### Why ATM?
- Highest premium collection
- Highest theta decay
- Delta neutral (market neutral)
- Manageable with adjustments

#### Strategy Details
```
Instrument: NIFTY Weekly (Thursday expiry)
Entry Day: Monday 10 AM
NIFTY Spot: 24000

Setup:
- Sell 24000 CE @ ₹180 (ATM)
- Sell 24000 PE @ ₹180 (ATM)
- Total premium: ₹18,000 per lot
- Margin required: ₹2,20,000

Greeks (per side):
- Delta: 0.50 (neutral at entry)
- Theta: -40 (earning ₹2,000/day per side)
- Vega: -8 (lose if IV increases)
- Gamma: -0.02 (accelerates delta change)

Profit Zones:
- Max profit: ₹18,000 (if expires at 24000)
- Breakeven: 23820 - 24180
- Win probability: 60-65%

Risk:
- Unlimited on both sides
- Max tolerable loss: ₹36,000 (2x premium)
- Mental stop: ±200 points move (₹10,000 loss)
```

#### Risk Management Techniques

**1. Time-Based Exit:**
```
- Hold till Wednesday 2 PM
- If profit >50% (₹9,000): EXIT early
- If no profit by Wednesday: Take small loss
- Don't hold till expiry Thursday (risky)
```

**2. Delta Hedging:**
```
Scenario: NIFTY moves to 24100

Status:
- Call delta: 0.60 (in the money)
- Put delta: 0.40 (out of money)
- Net delta: +0.20 (long exposure)

Hedge:
- Sell 0.20 lot NIFTY Futures (10 qty)
- Neutralize directional risk
- Re-adjust if moves further
```

**3. Strike Adjustment (Rolling):**
```
Scenario: NIFTY rallies to 24200 (call in trouble)

Adjustment:
- Buy back 24000 CE at loss (₹3,000)
- Sell 24200 CE (new ATM) @ ₹180 (₹9,000)
- Net: Still have ₹6,000 credit from call side
- Keep 24000 PE for full profit
```

**4. Hedging with Far OTM:**
```
Conservative Jodi:
- Sell 24000 CE @ ₹180
- Sell 24000 PE @ ₹180
- Buy 24300 CE @ ₹30 (hedge)
- Buy 23700 PE @ ₹30 (hedge)
- Net credit: ₹15,000
- Max loss: Defined (₹15,000 per side)
- Convert to Iron Condor
```

---

### 22. **Nifty-BankNifty Ratio Strategy**

#### Description
Trade the **correlation between NIFTY and BANKNIFTY**.

#### Logic
- Typical ratio: BANKNIFTY / NIFTY = 2.0-2.1
- If ratio > 2.15: BANKNIFTY overvalued → Short BNF, Long NIFTY
- If ratio < 2.0: NIFTY overvalued → Short NIFTY, Long BNF

#### Example Trade
```
Current State:
- NIFTY: 24000
- BANKNIFTY: 51600
- Ratio: 51600/24000 = 2.15 (high)
- Historical average: 2.08
- Z-Score: +2.3 (extreme)

Trade:
- Short BANKNIFTY: 1 lot (25 qty, ₹12.9 lakh exposure)
- Long NIFTY: 1 lot (50 qty, ₹12 lakh exposure)
- Approx market neutral

Expected Reversion:
- Ratio returns to 2.08
- NIFTY 24000, BANKNIFTY should be 49920

Scenario 1: BankNifty falls, Nifty stable
- BankNifty: 51600 → 50400 (₹30,000 profit)
- Nifty: 24000 → 24000 (₹0)
- Net: ₹30,000 profit

Scenario 2: Both fall equally
- BankNifty: 51600 → 50600 (₹25,000 profit)
- Nifty: 24000 → 23800 (₹10,000 loss)
- Net: ₹15,000 profit

Risk:
- Ratio diverges further (rare but possible)
- Stop loss: Ratio > 2.20
```

---

## Volatility-Based Strategies

### 23. **VIX Trading (India VIX)**

#### Description
Trade based on **fear index** (India VIX) movements.

#### VIX Interpretation
- VIX < 12: Complacency, expect sudden spike
- VIX 12-18: Normal range
- VIX 18-25: Elevated, cautious
- VIX > 25: Panic, expect mean reversion

#### Strategy 1: VIX Spike → Sell Volatility
```
Scenario: VIX jumps from 15 to 30 (post-crash)

Logic:
- VIX mean-reverts to 15-18 (always)
- High IV = expensive option premiums
- Sell options, wait for IV crush

Trade:
- Sell ATM straddle when VIX peaks
- Collect huge premium (2x normal)
- Exit when VIX returns to 18

Example:
Entry (VIX=30):
- Sell 24000 CE @ ₹300
- Sell 24000 PE @ ₹300
- Total: ₹30,000 credit

Exit (VIX=18, 5 days later):
- Buy back CE @ ₹180
- Buy back PE @ ₹180
- Cost: ₹18,000
- Profit: ₹12,000 (without underlying move)
```

#### Strategy 2: Low VIX → Buy Volatility
```
Scenario: VIX at 10 (historically low)

Logic:
- VIX bottoms rare, due for spike
- Buy cheap options before event
- Small investment, huge upside if crash

Trade:
- Buy OTM puts for protection/speculation
- Risk: Premium paid (₹5,000)
- Reward: 5-10x if market crashes
```

---

### 24. **Volatility Arbitrage**

#### Description
Exploit **mispricing between implied and realized volatility**.

#### Logic
```
Implied Volatility (IV): What options are pricing in
Realized Volatility (RV): What actually happened

If IV > RV: Options expensive → Sell options
If IV < RV: Options cheap → Buy options
```

#### Example Strategy
```
Analysis:
- NIFTY 30-day IV: 22%
- NIFTY 30-day RV: 15%
- IV is 7% higher than realized

Inference:
- Market overestimating movement
- Options overpriced
- Sell options (premium collection)

Trade:
- Sell Iron Condor (defined risk)
- Collect premium ₹5,000
- If 30-day RV stays <20%, profit
- Probability: 75% (historical)

Result (30 days):
- NIFTY actual movement: 1.2% (low)
- Options expire worthless
- Profit: ₹5,000
```

---

## Market Neutral Strategies

### 25. **Long-Short Equity**

#### Description
Simultaneously **long undervalued + short overvalued** stocks in same sector.

#### Logic
- Remove market risk (beta neutral)
- Profit from stock selection
- Works in bull/bear/sideways markets

#### Example: Banking Sector
```
Analysis (P/E ratios):
- HDFC Bank: P/E 18 (undervalued)
- Kotak Bank: P/E 28 (overvalued)
- Sector average: P/E 22

Trade:
- Long HDFC Futures: ₹10 lakh
- Short Kotak Futures: ₹10 lakh
- Dollar neutral

Scenarios:

Scenario 1: Market up 5%
- HDFC: +8% (outperforms) = +₹80,000
- Kotak: +3% (underperforms) = -₹30,000
- Net: +₹50,000

Scenario 2: Market down 5%
- HDFC: -3% (outperforms) = -₹30,000
- Kotak: -7% (underperforms) = +₹70,000
- Net: +₹40,000

Scenario 3: Market flat
- HDFC: +2% (mean reversion) = +₹20,000
- Kotak: -3% (mean reversion) = +₹30,000
- Net: +₹50,000

Risk:
- Pair diverges (both fall together)
- Stop loss: ₹50,000 combined loss
```

---

### 26. **Dividend Arbitrage**

#### Description
Capture **dividend** while hedging stock position.

#### Logic
- Buy stock before ex-dividend date
- Sell futures (hedge)
- Collect dividend (stock side)
- Close positions after ex-date

#### Example Trade
```
Stock: Reliance
Dividend: ₹8 per share
Ex-Date: February 20, 2025

Trade (Feb 18):
- Buy Reliance: 1,000 shares @ ₹2,800 (₹28 lakh)
- Sell Reliance Futures: 1,000 qty @ ₹2,810 (₹28.1 lakh)
- Net: Fully hedged

Feb 20 (Ex-Date):
- Stock opens at ₹2,792 (adjusted for dividend)
- Futures: ₹2,802
- Dividend credit: ₹8 × 1,000 = ₹8,000

Exit (Feb 20):
- Sell stock at ₹2,792: Loss = ₹8,000
- Buy back futures at ₹2,802: Loss = ₹8,000
- Dividend received: ₹8,000
- Net: ₹0 (before costs)

Profit:
- Tax on dividend: ₹0 (in demat)
- Brokerage: ₹200
- Net profit: ₹7,800

Risk:
- Stock gaps down more than dividend
- Futures mispricing
- Execution slippage
```

---

## Strategy Classification Framework

### By Time Horizon

| Category | Holding Period | Strategies |
|----------|----------------|------------|
| **Ultra Short-Term** | Seconds to Minutes | Scalping, Market Making, HFT |
| **Intraday** | Hours (close by EOD) | ORB, VWAP, Pivot Trading, Momentum |
| **Short-Term** | 1-7 days | Swing Trading, Breakout, Reversal |
| **Medium-Term** | 1-4 weeks | Positional, Trend Following, Options |
| **Long-Term** | Months to Years | Value Investing, Carry Trade, Dividend |

---

### By Market Direction

| Direction | Strategies | When to Use |
|-----------|------------|-------------|
| **Bullish** | Long Stock, Long Call, Bull Spread, Naked Put Sell | Uptrend, positive outlook |
| **Bearish** | Short Stock, Long Put, Bear Spread, Naked Call Sell | Downtrend, negative outlook |
| **Neutral** | Iron Condor, Butterfly, Straddle Sell, Range Trading | Sideways, low volatility |
| **Volatile** | Straddle Buy, Strangle Buy, VIX Trading | Before events, high volatility |
| **Market Neutral** | Pairs Trading, Long-Short, Arbitrage | All conditions |

---

### By Complexity Level

| Level | Description | Strategies | Trader Type |
|-------|-------------|------------|-------------|
| **Level 1: Beginner** | Single leg, simple logic | Long stock, Long futures, VWAP | Retail newbies |
| **Level 2: Intermediate** | 2-leg spreads, basic hedging | Bull/Bear spreads, Covered call, Breakout | 1-2 years exp |
| **Level 3: Advanced** | Multi-leg, Greeks, adjustments | Iron Condor, Straddle, Pairs, Ratio | 3-5 years exp |
| **Level 4: Expert** | Algo, HFT, complex arbitrage | Market making, Volatility arb, Delta hedging | Institutions |

---

### By Capital Requirement

| Capital Range | Strategies | Risk Level |
|---------------|------------|------------|
| **₹50,000 - 2 lakh** | Stock intraday, Option buying, Small lots | High (leverage) |
| **₹2-5 lakh** | Futures (1-2 lots), Credit spreads, Iron Condor | Medium |
| **₹5-20 lakh** | Multiple lots, Pairs trading, Portfolio strategies | Medium-Low |
| **₹20 lakh+** | Naked option selling, Market making, Multi-strategy | Low (with management) |

---

### By Risk Profile

| Risk Level | Max Loss | Strategies | Suitable For |
|------------|----------|------------|--------------|
| **Defined Risk** | Limited, known upfront | Spreads, Iron Condor, Long options | Conservative |
| **Unlimited Risk** | Theoretically unlimited | Naked option selling, Short stock | Aggressive |
| **Market Neutral** | Low, hedged | Pairs, Arbitrage, Long-short | Risk-averse |
| **Leveraged** | High due to margin | Futures, Naked options | Experienced only |

---

## Base Strategy Wireframe Design

### Core Building Blocks

Based on the analysis of 26+ strategies, we can identify **common building blocks** that every strategy needs:

```cpp
// ═══════════════════════════════════════════════════════════
// BASE STRATEGY WIREFRAME - Universal Components
// ═══════════════════════════════════════════════════════════

class UniversalStrategyBase {
public:
    // ────────────────────────────────────────────────────────
    // 1. STRATEGY METADATA
    // ────────────────────────────────────────────────────────
    struct StrategyMetadata {
        QString name;
        QString description;
        QString version;
        QString author;
        QDateTime createdDate;
        
        QString category;    // "Intraday", "Positional", "Options", etc.
        QString direction;   // "Bullish", "Bearish", "Neutral"
        int complexityLevel; // 1-4
        QString timeframe;   // "1m", "5m", "15m", "1h", "1d"
        
        QStringList requiredIndicators;
        QStringList requiredDataSources;
    };
    
    // ────────────────────────────────────────────────────────
    // 2. MARKET CONTEXT (What to trade)
    // ────────────────────────────────────────────────────────
    
    // Strategy can be multi-symbol (for example pair trading, spread, arbitrage)
    struct MarketContext {
        // Single vs Multi-symbol
        bool isMultiSymbol;       // true for pairs trading, spreads, etc.
        
        // Primary symbol (for single-symbol strategies)
        SymbolConfig primarySymbol;
        
        // Multiple symbols (for multi-symbol strategies)
        QVector<SymbolConfig> symbols;
        
        // Strategy type identifier
        enum StrategyType {
            SINGLE_SYMBOL,        // Simple directional
            MULTI_SYMBOL,         // Pairs trading, Long-Short
            MULTI_LEG_OPTIONS,    // Iron Condor, Butterfly
            CALENDAR_SPREAD,      // Same strike, different expiry
            INTER_EXCHANGE_ARB    // NSE vs BSE arbitrage
        };
        StrategyType type;
    };
    
    struct SymbolConfig {
        QString symbol;           // "NIFTY", "BANKNIFTY", "RELIANCE"
        int segment;              // 1=NSECM, 2=NSEFO, 11=BSECM, 12=BSEFO
        QString instrumentType;   // "FUTIDX", "FUTSTK", "OPTIDX", "OPTSTK", "EQ"
        QString exchange;         // "NSE", "BSE"
        QString alias;            // "Stock1", "Stock2" (for script reference)
        
        // Position direction
        QString action;           // "BUY", "SELL"
        
        // For futures
        QDate futuresExpiry;
        
        // For options - ENHANCED STRIKE SELECTION
        QString optionType;       // "CE", "PE"
        StrikeSelection strikeSelection;
        QDate optionExpiry;
        
        // Quantity/Ratio
        int quantity;
        double ratio;             // For ratio spreads, pairs (hedge ratio)
        double weight;            // For portfolio weighting
    };
    
    // ────────────────────────────────────────────────────────
    // ENHANCED STRIKE SELECTION SYSTEM
    // ────────────────────────────────────────────────────────
    struct StrikeSelection {
        enum SelectionMethod {
            STATIC,              // Fixed strike (e.g., 24000)
            ATM_RELATIVE,        // Relative to ATM (ATM, ATM+1, ATM-1, etc.)
            PRICE_BASED,         // Nearest strike to specific price
            DELTA_BASED,         // Strike with specific delta (0.3, 0.5, 0.7)
            PREMIUM_BASED,       // Strike with specific premium range
            PERCENTAGE_OTM,      // X% out of money (5% OTM)
            DYNAMIC_SPREAD       // Dynamic based on volatility/IV
        };
        SelectionMethod method;
        
        // ---- STATIC SELECTION ----
        double staticStrike;     // e.g., 24000, 48000
        
        // ---- ATM RELATIVE SELECTION ----
        int atmOffset;           // e.g., 0=ATM, +1=ATM+1, -1=ATM-1, +2=ATM+2
        int strikeInterval;      // 50 for NIFTY, 100 for BANKNIFTY
        
        enum ATMCalculation {
            SPOT_PRICE,          // Use spot price for ATM
            FUTURES_PRICE,       // Use futures price for ATM
            ROUNDED_SPOT         // Round spot to nearest strike
        };
        ATMCalculation atmBase;
        
        // ---- PRICE-BASED SELECTION ----
        double targetPrice;      // Find strike nearest to this price
        bool roundUp;            // Round up or down to nearest strike
        
        // ---- DELTA-BASED SELECTION ----
        double targetDelta;      // e.g., 0.25, 0.50, 0.75
        double deltaTolerance;   // ±0.05 acceptable range
        
        // ---- PREMIUM-BASED SELECTION ----
        double minPremium;       // Minimum premium to collect
        double maxPremium;       // Maximum premium willing to pay
        
        // ---- PERCENTAGE OTM SELECTION ----
        double percentageOTM;    // e.g., 5.0 = 5% out of money
        
        // ---- DYNAMIC SPREAD SELECTION ----
        double volatilityMultiplier;  // Strike width = ATR * multiplier
        bool useIVSkew;          // Consider IV skew in selection
        
        // Real-time adjustment
        bool adjustOnEntry;      // Recalculate strike at entry time
        bool adjustIntraday;     // Can adjust strike during day
        int maxAdjustments;      // Max times to adjust per day
    };
    
    // ────────────────────────────────────────────────────────
    // EXAMPLE USAGE SCENARIOS
    // ────────────────────────────────────────────────────────
    
    // Example 1: Simple RSI strategy (Single Symbol)
    /*
    MarketContext context;
    context.isMultiSymbol = false;
    context.type = SINGLE_SYMBOL;
    context.primarySymbol = SymbolConfig {
        .symbol = "NIFTY",
        .segment = 2,
        .instrumentType = "FUTIDX",
        .action = "BUY",
        .quantity = 50
    };
    */
    
    // Example 2: Pairs Trading (Multi-Symbol)
    /*
    MarketContext context;
    context.isMultiSymbol = true;
    context.type = MULTI_SYMBOL;
    context.symbols = {
        SymbolConfig {
            .symbol = "HDFCBANK",
            .segment = 2,
            .instrumentType = "FUTSTK",
            .alias = "Stock1",
            .action = "BUY",
            .quantity = 1250,
            .ratio = 1.0
        },
        SymbolConfig {
            .symbol = "ICICIBANK",
            .segment = 2,
            .instrumentType = "FUTSTK",
            .alias = "Stock2",
            .action = "SELL",
            .quantity = 1750,
            .ratio = 1.42  // Hedge ratio
        }
    };
    */
    
    // Example 3: Iron Condor (Multi-Leg Options)
    /*
    MarketContext context;
    context.isMultiSymbol = false;
    context.type = MULTI_LEG_OPTIONS;
    context.symbols = {
        // Sell Put Spread
        SymbolConfig {
            .symbol = "NIFTY",
            .segment = 2,
            .instrumentType = "OPTIDX",
            .optionType = "PE",
            .action = "SELL",
            .strikeSelection = {
                .method = ATM_RELATIVE,
                .atmOffset = -2,        // ATM - 2 (200 points below)
                .strikeInterval = 50,
                .atmBase = SPOT_PRICE
            },
            .quantity = 50
        },
        SymbolConfig {
            .symbol = "NIFTY",
            .segment = 2,
            .instrumentType = "OPTIDX",
            .optionType = "PE",
            .action = "BUY",
            .strikeSelection = {
                .method = ATM_RELATIVE,
                .atmOffset = -4,        // ATM - 4 (400 points below)
                .strikeInterval = 50
            },
            .quantity = 50
        },
        // Sell Call Spread
        SymbolConfig {
            .symbol = "NIFTY",
            .segment = 2,
            .instrumentType = "OPTIDX",
            .optionType = "CE",
            .action = "SELL",
            .strikeSelection = {
                .method = ATM_RELATIVE,
                .atmOffset = +2,        // ATM + 2 (200 points above)
                .strikeInterval = 50
            },
            .quantity = 50
        },
        SymbolConfig {
            .symbol = "NIFTY",
            .segment = 2,
            .instrumentType = "OPTIDX",
            .optionType = "CE",
            .action = "BUY",
            .strikeSelection = {
                .method = ATM_RELATIVE,
                .atmOffset = +4,        // ATM + 4 (400 points above)
                .strikeInterval = 50
            },
            .quantity = 50
        }
    };
    */
    
    // Example 4: Delta-Based Option Selling
    /*
    SymbolConfig optionSell {
        .symbol = "BANKNIFTY",
        .segment = 2,
        .instrumentType = "OPTIDX",
        .optionType = "PE",
        .action = "SELL",
        .strikeSelection = {
            .method = DELTA_BASED,
            .targetDelta = 0.25,    // Sell 0.25 delta put (OTM)
            .deltaTolerance = 0.03
        },
        .quantity = 25
    };
    */
    
    // Example 5: Premium Collection Strategy
    /*
    SymbolConfig premiumStrategy {
        .symbol = "NIFTY",
        .segment = 2,
        .instrumentType = "OPTIDX",
        .optionType = "PE",
        .action = "SELL",
        .strikeSelection = {
            .method = PREMIUM_BASED,
            .minPremium = 80.0,     // Collect at least ₹80 premium
            .maxPremium = 150.0
        },
        .quantity = 50
    };
    */
    
    // Example 6: Percentage OTM Selection
    /*
    SymbolConfig otmSelection {
        .symbol = "NIFTY",
        .segment = 2,
        .instrumentType = "OPTIDX",
        .optionType = "CE",
        .action = "SELL",
        .strikeSelection = {
            .method = PERCENTAGE_OTM,
            .percentageOTM = 5.0,   // 5% out of money from current price
            .atmBase = SPOT_PRICE
        },
        .quantity = 50
    };
    */
    
    // Example 7: Dynamic Spread Based on Volatility
    /*
    SymbolConfig dynamicSpread {
        .symbol = "NIFTY",
        .segment = 2,
        .instrumentType = "OPTIDX",
        .optionType = "CE",
        .action = "SELL",
        .strikeSelection = {
            .method = DYNAMIC_SPREAD,
            .volatilityMultiplier = 1.5,  // Strike width = 1.5 * ATR
            .useIVSkew = true,
            .adjustOnEntry = true
        },
        .quantity = 50
    };
    */
    
    // ────────────────────────────────────────────────────────
    // 3. PRE-ENTRY SCANNING (When to prepare)
    // ────────────────────────────────────────────────────────
    struct ScanningRules {
        // Time-based
        QTime tradingStartTime;   // 9:15 AM
        QTime tradingEndTime;     // 3:15 PM
        QVector<DayOfWeek> tradingDays;
        
        // Market condition filters
        bool requireTrendingMarket;
        bool requireHighVolatility;
        double minVolumeRatio;    // Current / Average
        double minATR;            // Minimum average true range
        
        // Technical filters
        ConditionGroup technicalFilters;
        
        // Fundamental filters (for stocks)
        ConditionGroup fundamentalFilters;
        
        // Event-based
        bool avoidEarningsDay;
        bool avoidExpiryDay;
        bool avoidHolidays;
    };
    
    // ────────────────────────────────────────────────────────
    // 4. ENTRY LOGIC (When to enter)
    // ────────────────────────────────────────────────────────
    struct EntryRules {
        // Primary conditions
        ConditionGroup longEntryConditions;
        ConditionGroup shortEntryConditions;
        
        // Entry timing
        enum EntryTiming {
            IMMEDIATE,           // Market order now
            LIMIT,               // Limit order at price
            STOP_LIMIT,          // Break above/below level
            END_OF_CANDLE,       // Wait for candle close
            PULLBACK,            // Wait for retracement
            CONFIRMATION         // Wait for X bars confirmation
        };
        EntryTiming timingType;
        
        // Entry price determination
        enum EntryPrice {
            MARKET_PRICE,
            BID_PRICE,
            ASK_PRICE,
            MID_PRICE,
            VWAP,
            CUSTOM_OFFSET       // Entry + X points
        };
        EntryPrice priceType;
        double priceOffset;
        
        // Position sizing
        PositionSizingRule sizingRule;
    };
    
    struct PositionSizingRule {
        enum SizingMethod {
            FIXED_QUANTITY,      // Always 50 shares
            FIXED_CAPITAL,       // Always ₹1 lakh worth
            RISK_BASED,          // 1% of capital at risk
            VOLATILITY_BASED,    // ATR multiplier
            KELLY_CRITERION,     // Mathematical optimal
            PYRAMIDING           // Add to winners
        };
        SizingMethod method;
        
        double fixedQuantity;
        double fixedCapital;
        double riskPercent;      // % of portfolio
        double atrMultiplier;
        double kellyFraction;
        
        // Limits
        double maxPositionSize;  // Max ₹10 lakh per strategy
        int maxLots;
        int maxContracts;
    };
    
    // ────────────────────────────────────────────────────────
    // 5. RUNTIME POSITION MANAGEMENT (After entry)
    // ────────────────────────────────────────────────────────
    struct PositionManagement {
        
        // 5.1 Stop Loss Management
        StopLossRule stopLoss;
        
        // 5.2 Target Management
        TargetRule target;
        
        // 5.3 Trailing Stop
        TrailingStopRule trailingStop;
        
        // 5.4 Position Scaling
        ScalingRule scaling;
        
        // 5.5 Time-based management
        TimeBasedRule timeRule;
        
        // 5.6 Hedge management
        HedgeRule hedging;
    };
    
    struct StopLossRule {
        enum StopType {
            NONE,
            FIXED_POINTS,        // 50 points
            FIXED_PERCENT,       // 2% of entry
            ATR_BASED,           // 2 * ATR
            SUPPORT_RESISTANCE,  // Technical level
            TIME_BASED,          // Close after X minutes
            VOLATILITY_BASED     // Based on IV
        };
        StopType type;
        
        double fixedPoints;
        double fixedPercent;
        double atrMultiplier;
        int timeMinutes;
        
        // Dynamic adjustment
        bool moveToBreakeven;
        double moveToBreakevenAfterProfit; // After 1% profit
        
        bool lockProfit;
        double lockProfitAfterPercent;     // Lock 50% after 2% profit
    };
    
    struct TargetRule {
        enum TargetType {
            NONE,
            FIXED_POINTS,
            FIXED_PERCENT,
            RISK_REWARD_RATIO,   // 1:2, 1:3
            ATR_BASED,
            FIBONACCI,           // 1.27, 1.618 extensions
            PREVIOUS_HIGH_LOW,
            MULTIPLE_TARGETS     // Partial exits
        };
        TargetType type;
        
        double fixedPoints;
        double fixedPercent;
        double riskRewardRatio;
        double atrMultiplier;
        
        // Multiple targets (scale out)
        struct PartialTarget {
            double targetLevel;   // 1%, 2%, 3%
            double exitPercent;   // Exit 33%, 33%, 34%
        };
        QVector<PartialTarget> partialTargets;
    };
    
    struct TrailingStopRule {
        bool enabled;
        
        enum TrailType {
            FIXED_POINTS,        // Trail by 20 points
            FIXED_PERCENT,       // Trail by 0.5%
            ATR_BASED,           // Trail by 1 * ATR
            PARABOLIC_SAR,       // Technical indicator
            MOVING_AVERAGE,      // EMA as trailing stop
            HIGHEST_HIGH,        // Trail below highest high since entry
            CHANDELIER_STOP      // Chandelier exit
        };
        TrailType type;
        
        double triggerProfit;    // Activate after 1% profit
        double trailAmount;      // Trail by 0.5%
        int trailFrequency;      // Update every X ticks
        
        bool trailToBreakeven;
        double atrMultiplier;
    };
    
    struct ScalingRule {
        // Pyramiding (add to winners)
        bool allowPyramiding;
        double pyramidTriggerProfit;  // Add after 1% profit
        int maxPyramidLevels;         // Max 3 additions
        double pyramidSizeRatio;      // 0.5 = half of original
        
        // Averaging (add to losers - risky!)
        bool allowAveraging;
        double averageTriggerLoss;    // Add after 2% loss
        int maxAverageLevels;         // Max 2 additions
        double averageSizeRatio;      // 1.0 = same as original
        
        // Partial exits (reduce exposure)
        bool allowPartialExit;
        double partialExitTrigger;
        double partialExitPercent;
    };
    
    struct TimeBasedRule {
        // Maximum hold time
        bool hasMaxHoldTime;
        int maxHoldMinutes;      // 120 minutes (2 hours)
        int maxHoldDays;         // 5 days
        
        // Market close exit
        bool exitBeforeClose;
        QTime exitTime;          // 3:15 PM
        
        // Expiry management (for options)
        bool exitBeforeExpiry;
        int daysBeforeExpiry;    // 2 days before
        
        // Consolidation exit
        bool exitOnStagnation;
        int stagnationMinutes;   // Exit if no 0.5% move in 30 min
    };
    
    struct HedgeRule {
        bool useHedge;
        
        enum HedgeType {
            NONE,
            FUTURES_HEDGE,       // Hedge with opposite futures
            OPTIONS_HEDGE,       // Buy protective put/call
            DELTA_NEUTRAL,       // Maintain delta neutral
            DYNAMIC_HEDGE        // Adjust based on PnL
        };
        HedgeType type;
        
        double hedgeTriggerLoss;     // Hedge after 3% loss
        double hedgeRatio;           // 0.5 = 50% hedge
        bool removeHedgeOnProfit;
    };
    
    // ────────────────────────────────────────────────────────
    // 6. EXIT LOGIC (When to close)
    // ────────────────────────────────────────────────────────
    struct ExitRules {
        // Exit triggers (multiple can be active)
        bool exitOnStopLoss;
        bool exitOnTarget;
        bool exitOnTrailingStop;
        bool exitOnSignalReversal;
        bool exitOnTimeLimit;
        bool exitOnMarketClose;
        bool exitOnRiskLimit;
        
        // Signal reversal
        ConditionGroup reversalConditions;
        
        // Risk limit breach
        double maxDailyLoss;
        double maxWeeklyLoss;
        int maxConsecutiveLosses;
        
        // Portfolio limits
        double maxDrawdownPercent;
        double maxPortfolioRisk;
    };
    
    // ────────────────────────────────────────────────────────
    // 7. RISK MANAGEMENT (Overall controls)
    // ────────────────────────────────────────────────────────
    struct RiskManagement {
        // Account limits
        double totalCapital;
        double maxCapitalPerStrategy;    // 10% per strategy
        double maxCapitalPerTrade;       // 2% per trade
        
        // Position limits
        int maxPositionsPerStrategy;     // 3 positions max
        int maxPositionsTotal;           // 10 across all strategies
        
        // Loss limits
        double maxLossPerTrade;          // ₹5,000
        double maxDailyLoss;             // ₹20,000
        double maxWeeklyLoss;            // ₹50,000
        double maxMonthlyLoss;           // ₹1,50,000
        
        // Win limits (psychology)
        double dailyProfitTarget;        // Lock after ₹30,000
        bool stopAfterTargetReached;
        
        // Trade frequency
        int maxTradesPerDay;             // 10 trades max
        int minMinutesBetweenTrades;     // 5 min cooldown
        
        // Leverage limits
        double maxLeverage;              // 5x
        double marginUtilizationLimit;   // 80% of available
        
        // Correlation limits
        double maxCorrelation;           // Don't trade highly correlated
        
        // Volatility limits
        double maxVIXForEntry;           // Don't enter if VIX >30
        double minVIXForVolatility;      // Need VIX >15 for vol strategies
    };
    
    // ────────────────────────────────────────────────────────
    // 8. LOGGING & MONITORING (Track everything)
    // ────────────────────────────────────────────────────────
    struct LoggingConfig {
        enum LogLevel {
            DEBUG,    // Every tick
            INFO,     // Entry/exit + condition checks
            WARNING,  // Risk warnings
            ERROR     // Failures
        };
        LogLevel level;
        
        bool logEveryTick;
        bool logConditionEvaluation;
        bool logOrderPlacement;
        bool logPnLUpdates;
        bool logRiskMetrics;
        
        // Performance tracking
        bool trackExecutionLatency;
        bool trackSlippage;
        bool trackFillRates;
        
        // Alerts
        bool sendEmailAlerts;
        bool sendSMSAlerts;
        bool playAudioAlerts;
        QString webhookURL;
    };
    
    // ────────────────────────────────────────────────────────
    // 9. BACKTESTING & OPTIMIZATION (Historical testing)
    // ────────────────────────────────────────────────────────
    struct BacktestConfig {
        QDate startDate;
        QDate endDate;
        double initialCapital;
        
        // Realism settings
        double commissionPerTrade;
        double slippagePercent;
        double impactCost;
        
        // Metrics to calculate
        bool calculateSharpeRatio;
        bool calculateMaxDrawdown;
        bool calculateWinRate;
        bool calculateProfitFactor;
        bool calculateExpectancy;
        
        // Optimization
        QVector<ParameterRange> parametersToOptimize;
        QString optimizationMetric;   // "Sharpe", "ProfitFactor", "CAGR"
    };
    
    struct ParameterRange {
        QString parameterName;
        double minValue;
        double maxValue;
        double stepSize;
    };
    
    // ────────────────────────────────────────────────────────
    // 10. EXECUTION DETAILS (How to place orders)
    // ────────────────────────────────────────────────────────
    struct ExecutionConfig {
        enum OrderType {
            MARKET,
            LIMIT,
            STOP_LOSS,
            STOP_LOSS_MARKET,
            BRACKET_ORDER,
            COVER_ORDER,
            OCO_ORDER        // One-cancels-other
        };
        OrderType defaultOrderType;
        
        enum ProductType {
            INTRADAY,        // MIS
            DELIVERY,        // CNC
            NORMAL           // NRML
        };
        ProductType productType;
        
        // Execution quality
        int maxRetries;
        int retryDelayMs;
        double maxSlippagePercent;
        bool useIcebergOrders;
        
        // Smart order routing
        bool splitLargeOrders;
        int maxOrderSize;
        
        // Timing
        bool avoidOpeningRange;  // Don't trade 9:15-9:25
        bool avoidClosingRange;  // Don't trade 3:20-3:30
    };
};
```

---

## Implementation Requirements Matrix

### Phase 1: MVP (Form-Based Strategy Builder)

| Component | Required Features | Priority | Effort |
|-----------|-------------------|----------|--------|
| **Entry Conditions** | Basic indicators (5-10), Simple comparisons (>, <, ==), AND/OR logic | P0 | 1 week |
| **Exit Conditions** | Fixed stop loss %, Fixed target %, Time exit | P0 | 3 days |
| **Position Sizing** | Fixed quantity, Fixed capital | P0 | 2 days |
| **Risk Management** | Max daily loss, Max daily trades | P0 | 2 days |
| **Market Context** | Symbol, Segment, Timeframe | P0 | 1 day |

**Total MVP: 2 weeks**

---

### Phase 2: Intermediate (Visual + More Features)

| Component | Required Features | Priority | Effort |
|-----------|-------------------|----------|--------|
| **Entry Conditions** | 20+ indicators, Nested conditions, Multiple symbols | P1 | 1 week |
| **Exit Conditions** | Trailing stop, Multiple targets, Signal reversal | P1 | 1 week |
| **Position Sizing** | Risk-based, ATR-based, Pyramiding | P1 | 1 week |
| **Visual Editor** | Drag-drop blocks, Flow visualization | P1 | 3 weeks |
| **Backtesting** | Historical data replay, Performance metrics | P1 | 2 weeks |

**Total Phase 2: 8 weeks**

---

### Phase 3: Advanced (Script-Based + Complex Strategies)

| Component | Required Features | Priority | Effort |
|-----------|-------------------|----------|--------|
| **Script Engine** | ChaiScript integration, Hot-reload, Debugger | P2 | 3 weeks |
| **Real-time Variables** | Tick-by-tick updates, Dynamic calculations | P2 | 2 weeks |
| **Multi-Leg Strategies** | Options spreads, Pairs trading, Hedging | P2 | 3 weeks |
| **Greeks Management** | Delta, Gamma, Vega, Theta tracking | P2 | 2 weeks |
| **Advanced Risk** | Portfolio risk, Correlation, VaR | P2 | 2 weeks |

**Total Phase 3: 12 weeks**

---

## Conclusion

This in-depth analysis reveals that **90% of trading strategies** share common building blocks:

### Core Requirements (Must Have)
1. ✅ Entry conditions (technical/fundamental)
2. ✅ Exit conditions (stop loss + target)
3. ✅ Position sizing rules
4. ✅ Market context (symbol, timeframe)
5. ✅ Risk management (loss limits)

### Advanced Requirements (Should Have)
6. ✅ Trailing stops
7. ✅ Multiple targets (scaling out)
8. ✅ Time-based rules
9. ✅ Signal reversal
10. ✅ Backtesting

### Expert Requirements (Nice to Have)
11. ✅ Real-time Greeks
12. ✅ Multi-leg execution
13. ✅ Dynamic hedging
14. ✅ Portfolio optimization
15. ✅ Machine learning

### Recommended Implementation

**Start with:** Universal base class with all building blocks  
**Allow:** Strategies to override/customize specific blocks  
**Result:** Flexibility to handle simple RSI to complex Iron Condor

**Example Usage:**
```cpp
// Simple RSI strategy
SimpleRSIStrategy : UniversalStrategyBase {
    // Only override entry conditions
    EntryRules getEntryRules() {
        return {RSI < 30 AND Positions == 0};
    }
    
    // Use default exit (stop loss + target)
    // Use default position sizing
}

// Complex Jodi ATM with adjustments
JodiATMStrategy : UniversalStrategyBase {
    // Override everything
    EntryRules, ExitRules, PositionManagement, etc.
    
    // Add custom adjustment logic
    void onRuntimeAdjustment() {
        if (delta > 0.5) hedgeWithFutures();
    }
}
```

---

**Document Version:** 1.0  
**Last Updated:** February 17, 2026  
**Next Steps:** Design UI mockups for strategy builder based on this wireframe
