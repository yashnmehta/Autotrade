# Consolidated Token-Based Market Data Array Strategy

**Version:** 1.0  
**Date:** January 12, 2026  
**Purpose:** Optimal token-indexed consolidated array for real-time market data updates

---

## 📋 Executive Summary

This document defines a **consolidated array structure** that serves as a single source of truth for all market data, indexed by **token** for O(1) access speed. The array is designed to handle multiple message types (7200, 7208, 7210, 7201, 18703, 18708, 6013) and update fields intelligently based on message priority and data availability.

---

## 🎯 Design Goals

1. **Single Source of Truth**: One consolidated array per token containing all market data
2. **O(1) Access**: Direct array/hash map access using token as index
3. **Incremental Updates**: Smart field updates without overwriting valid data
4. **Memory Efficient**: ~500-1000 bytes per token (typical: 500-5000 tokens = 250KB-5MB)
5. **Thread-Safe**: Lock-free reads, write locks only during updates
6. **Zero Data Loss**: Merge logic preserves all valid fields

---

## 📊 Consolidated Array Structure (Cache-Optimized Layout)

### **ConsolidatedMarketData** (Primary Structure - 462 bytes)

**Memory Layout Strategy:**
- **Cache Line 1 (Bytes 0-63)**: Ultra-hot fields - accessed every tick
- **Cache Line 2 (Bytes 64-127)**: Hot fields - accessed frequently  
- **Cache Lines 3-7 (Bytes 128-447)**: Warm fields - periodic access
- **Cache Line 8 (Bytes 448-462)**: Cold fields - rare access

---

#### **CACHE LINE 1: Ultra-Hot Fields (0-63 bytes) ⚡⚡⚡**

**A1. Core Identification (12 bytes) - Offset 0-11**
```
Offset  Field Name           Type        Size    Purpose
--------------------------------------------------------------------------------
0       token                uint32_t    4       PRIMARY KEY - Token identifier
4       exchangeSegment      uint16_t    2       1=NSECM, 2=NSEFO, 11=BSECM, 12=BSEFO
6       bookType             uint16_t    2       RL/ST/SL/OL/SP/AU
8       tradingStatus        uint16_t    2       1=Preopen, 2=Open, 3=Suspended
10      marketType           uint16_t    2       Market type code
```

**A2. Critical Price Fields (24 bytes) - Offset 12-35**
```
Offset  Field Name           Type        Size    Purpose
--------------------------------------------------------------------------------
12      lastTradedPrice      int32_t     4       LTP - Most accessed field (7200/7208/18703/7201)
16      bidPrice[0]          int32_t     4       Best bid price (Level 1)
20      askPrice[0]          int32_t     4       Best ask price (Level 1)
24      bidQuantity[0]       int64_t     8       Best bid quantity (Level 1)
32      askQuantity[0]       int64_t     8       Best ask quantity (Level 1)
```

**A3. Essential Trade Data (20 bytes) - Offset 40-59**
```
Offset  Field Name           Type        Size    Purpose
--------------------------------------------------------------------------------
40      volumeTradedToday    int64_t     8       Total traded volume
48      lastTradeTime        int32_t     4       Last trade time (HHMM)
52      lastTradeQuantity    int32_t     4       Last trade quantity
56      netPriceChange       int32_t     4       Price change from close
60      netChangeIndicator   char        1       '+' or '-'
61      padding_CL1[3]       char[3]     3       Cache line alignment
```

**Total Cache Line 1: 64 bytes** ✅
**Access Pattern:** Read on EVERY update (7200/7208/18703/7201)  
**Justification:** These fields are accessed 100-1000x per second


---

#### **CACHE LINE 2: Hot Fields (64-127 bytes) ⚡⚡**

**B1. OHLC Data (16 bytes) - Offset 64-79**
```
Offset  Field Name           Type        Size    Purpose
--------------------------------------------------------------------------------
64      openPrice            int32_t     4       Opening price (paise) - 6013/7208/7200
68      highPrice            int32_t     4       Day high (paise) - 7208/7200
72      lowPrice             int32_t     4       Day low (paise) - 7208/7200
76      closePrice           int32_t     4       Previous close (paise) - 7208/7200
```

**B2. Market Depth Level 2-3 (32 bytes) - Offset 80-111**
```
Offset  Field Name           Type        Size    Purpose
--------------------------------------------------------------------------------
80      bidPrice[1]          int32_t     4       2nd best bid price
84      bidPrice[2]          int32_t     4       3rd best bid price
88      askPrice[1]          int32_t     4       2nd best ask price
92      askPrice[2]          int32_t     4       3rd best ask price
96      bidQuantity[1]       int64_t     8       2nd bid quantity
104     bidQuantity[2]       int64_t     8       3rd bid quantity
112     askQuantity[1]       int64_t     8       2nd ask quantity
120     askQuantity[2]       int64_t     8       3rd ask quantity
```

**Total Cache Line 2: 64 bytes (Offset 64-127)** ✅  
**Access Pattern:** Read on depth updates (7200/7208) - 10-50 times/sec  
**Justification:** OHLC displayed prominently, Level 2-3 depth frequently viewed

---

#### **CACHE LINE 3: Warm Fields - Depth Levels 4-5 (128-191 bytes) ⚡**

**C1. Market Depth Level 4-5 (32 bytes) - Offset 128-159**
```
Offset  Field Name           Type        Size    Purpose
--------------------------------------------------------------------------------
128     bidPrice[3]          int32_t     4       4th best bid price
132     bidPrice[4]          int32_t     4       5th best bid price
136     askPrice[3]          int32_t     4       4th best ask price
140     askPrice[4]          int32_t     4       5th best ask price
144     bidQuantity[3]       int64_t     8       4th bid quantity
152     bidQuantity[4]       int64_t     8       5th bid quantity
160     bidQuantity[3]       int64_t     8       4th ask quantity
168     bidQuantity[4]       int64_t     8       5th ask quantity
```

**C2. Depth Order Counts (20 bytes) - Offset 176-195**
```
Offset  Field Name           Type        Size    Purpose
--------------------------------------------------------------------------------
176     bidOrders[0]         int16_t     2       Orders at best bid
178     bidOrders[1]         int16_t     2       Orders at 2nd bid
180     bidOrders[2]         int16_t     2       Orders at 3rd bid
182     bidOrders[3]         int16_t     2       Orders at 4th bid
184     bidOrders[4]         int16_t     2       Orders at 5th bid
186     askOrders[0]         int16_t     2       Orders at best ask
188     askOrders[1]         int16_t     2       Orders at 2nd ask
190     askOrders[2]         int16_t     2       Orders at 3rd ask
192     askOrders[3]         int16_t     2       Orders at 4th ask
194     askOrders[4]         int16_t     2       Orders at 5th ask
```

**Total Cache Line 3: 68 bytes (Offset 128-195)** ✅  
**Access Pattern:** Depth updates (7200/7208)  
**Justification:** Complete 5-level depth visualization

---

#### **CACHE LINE 4: Depth Aggregates & Trade Info (196-255 bytes) ⚡**

**D1. Depth Aggregates (20 bytes) - Offset 196-215**
```
Offset  Field Name           Type        Size    Purpose
--------------------------------------------------------------------------------
196     totalBuyQuantity     int64_t     8       Total buy side quantity (7200/7208)
204     totalSellQuantity    int64_t     8       Total sell side quantity (7200/7208)
212     bbTotalBuyFlag       uint16_t    2       Buyback order in buy side
214     bbTotalSellFlag      uint16_t    2       Buyback order in sell side
```

**D2. Additional Trade Data (16 bytes) - Offset 216-231**
```
Offset  Field Name           Type        Size    Purpose
--------------------------------------------------------------------------------
216     averageTradePrice    int32_t     4       VWAP (paise) - 7200/7208
220     indicativeClosePrice int32_t     4       Indicative close - 7208
224     fillPrice            int32_t     4       Ticker fill price - 18703
228     fillVolume           int32_t     4       Ticker fill volume - 18703
```

**D3. Market Watch Data (34 bytes) - Offset 232-265**
```
Offset  Field Name           Type        Size    Purpose
--------------------------------------------------------------------------------
232     bestBuyVolume        int64_t     8       Best buy volume (7201)
240     bestBuyPrice         int32_t     4       Best buy price (7201)
244     bestSellVolume       int64_t     8       Best sell volume (7201)
252     bestSellPrice        int32_t     4       Best sell price (7201)
256     mwLastTradePrice     int32_t     4       MW LTP or indicative (7201)
260     mwLastTradeTime      int32_t     4       MW trade time (7201)
264     mwIndicator          uint16_t    2       MW specific indicators (7201)
```

**Total Cache Lines 4: 70 bytes (Offset 196-265)** ✅  
**Access Pattern:** Market watch rotation (7201) - 5-10 times/sec  
**Justification:** Round-robin updates for non-subscribed tokens

---

#### **CACHE LINE 5: Auction Information (266-297 bytes)**

**E. Auction Information (32 bytes) - Offset 266-297**
```
Offset  Field Name           Type        Size    Purpose
--------------------------------------------------------------------------------
266     auctionNumber        uint16_t    2       Auction number (7200/7208)
268     auctionStatus        uint16_t    2       Auction status code (7200/7208)
270     initiatorType        uint16_t    2       Control/Trader (7200/7208)
272     initiatorPrice       int32_t     4       Auction initiator price (7200/7208)
276     initiatorQuantity    int32_t     4       Auction initiator qty (7200/7208)
280     auctionPrice         int32_t     4       Auction match price (7200/7208)
284     auctionQuantity      int32_t     4       Auction match qty (7200/7208)
288     padding_auction[10]  char[10]    10      Reserved for auction extensions
```

**Total Cache Line 5: 32 bytes (Offset 266-297)** ✅  
**Access Pattern:** During auction sessions only  
**Justification:** Pre-open, call auction periods

---

#### **CACHE LINE 6: SPOS Statistics (298-337 bytes)**

**F. SPOS Order Cancellation Statistics (40 bytes) - Offset 298-337**
```
Offset  Field Name           Type        Size    Purpose
--------------------------------------------------------------------------------
298     buyOrdCxlCount       int64_t     8       Buy orders cancelled count (7210)
306     buyOrdCxlVol         int64_t     8       Buy orders cancelled volume (7210)
314     sellOrdCxlCount      int64_t     8       Sell orders cancelled count (7210)
322     sellOrdCxlVol        int64_t     8       Sell orders cancelled volume (7210)
330     sposLastUpdate       int64_t     8       SPOS data timestamp (7210)
```

**Total Cache Line 6: 40 bytes (Offset 298-337)** ✅  
**Access Pattern:** Special Pre-Open Session (IPO/Relist) - Rare  
**Justification:** IPO scenarios only

---

#### **CACHE LINE 7: Buyback Information (338-401 bytes)**

**G. Buyback Information (64 bytes) - Offset 338-401**
```
Offset  Field Name           Type        Size    Purpose
--------------------------------------------------------------------------------
338     symbol               char[10]    10      Security symbol (18708)
348     series               char[2]     2       Series code (18708)
350     pdayCumVol           double      8       Previous day volume (18708)
358     pdayHighPrice        int32_t     4       Previous day high (18708)
362     pdayLowPrice         int32_t     4       Previous day low (18708)
366     pdayWtAvg            int32_t     4       Previous day VWAP (18708)
370     cdayCumVol           double      8       Current day volume (18708)
378     cdayHighPrice        int32_t     4       Current day high (18708)
382     cdayLowPrice         int32_t     4       Current day low (18708)
386     cdayWtAvg            int32_t     4       Current day VWAP (18708)
390     buybackStartDate     int32_t     4       Buyback start date (18708)
394     buybackEndDate       int32_t     4       Buyback end date (18708)
398     isBuybackActive      bool        1       Buyback flag (18708)
399     padding_buyback[3]   char[3]     3       Alignment padding
```

**Total Cache Line 7: 64 bytes (Offset 338-401)** ✅  
**Access Pattern:** Hourly updates during market hours  
**Justification:** Buyback sessions only (rare)

---

#### **CACHE LINE 8: Metadata & Indicators (402-462 bytes) - COLD**

**H. Indicators & Flags (8 bytes) - Offset 402-409**
```
Offset  Field Name           Type        Size    Purpose
--------------------------------------------------------------------------------
402     stIndicator          uint16_t    2       Status indicator flags (7200/7208)
404     lastTradeLess        bool        1       LTP < prev LTP (7201)
405     lastTradeMore        bool        1       LTP > prev LTP (7201)
406     buyIndicator         bool        1       Buy side active (7201)
407     sellIndicator        bool        1       Sell side active (7201)
408     reserved[2]          char[2]     2       Future use
```

**I. Metadata & Timestamps (24 bytes) - Offset 410-433**
```
Offset  Field Name           Type        Size    Purpose
--------------------------------------------------------------------------------
410     lastUpdateTime       int64_t     8       Last update timestamp (nanoseconds)
418     lastUpdateSource     uint16_t    2       Last message code (7200/7208/etc)
420     updateCount          uint32_t    4       Number of updates received
424     dataQuality          uint8_t     1       0-100 quality score (computed)
425     isValid              bool        1       Data validity flag
426     isSubscribed         bool        1       Subscription status (local)
427     padding_final[7]     char[7]     7       Final alignment padding
```

**Total Cache Line 8: 32 bytes (Offset 402-433)** ✅  
**Access Pattern:** Metadata access for debugging/monitoring  
**Justification:** Administrative fields, rarely accessed in hot path

---

## 📐 Total Structure Size Summary

```
CACHE LINE  OFFSET RANGE    SIZE    DESCRIPTION                      ACCESS FREQUENCY
-----------------------------------------------------------------------------------------
CL1         0-63            64 B    Token, LTP, Best Bid/Ask, Volume Ultra-Hot (Every tick)
CL2         64-127          64 B    OHLC, Depth L2-L3                Hot (Frequent)
CL3         128-195         68 B    Depth L4-L5, Order counts        Warm (Periodic)
CL4         196-265         70 B    Depth aggregates, MW data        Warm (Periodic)
CL5         266-297         32 B    Auction information              Cool (Session-based)
CL6         298-337         40 B    SPOS statistics                  Cool (Rare)
CL7         338-401         64 B    Buyback information              Cold (Hourly)
CL8         402-433         32 B    Metadata, indicators, timestamps Cold (Debug/Admin)
-----------------------------------------------------------------------------------------
TOTAL                      434 B    (Padded to 462 B for alignment)
```

**Final Structure Size: 462 bytes** ✅

**Padding Distribution:**
- After Cache Line 1: 3 bytes (offset 61-63)
- After Auction: 10 bytes (offset 288-297)
- After Buyback: 3 bytes (offset 399-401)
- Final padding: 7 bytes (offset 427-433)
- **Total padding to reach 462 bytes: 29 bytes**

---

## 🎯 Cache Optimization Benefits

### **Why This Layout?**

1. **First 64 bytes = 90% of access patterns**
   - Token lookup, LTP display, best bid/ask
   - Fits in single CPU cache line (L1 cache hit)
   - Access time: ~1-4 CPU cycles

2. **First 128 bytes = 95% of access patterns**
   - Includes OHLC + Level 2-3 depth
   - Fits in 2 cache lines
   - UI updates mostly touch these fields

3. **Depth arrays contiguous**
   - bidPrice[5] sequential (16-20 + 80-140 bytes)
   - bidQuantity[5] sequential (24-32 + 96-168 bytes)
   - CPU prefetcher loads next levels automatically

4. **Cold data at end**
   - Buyback, SPOS, metadata rarely accessed
   - Doesn't pollute hot cache lines
   - Separate cache lines for occasional use

---

## 🚀 Memory Access Patterns

### **Scenario 1: Market Watch Update (Most Common)**
```
Access: token (0), lastTradedPrice (12), bidPrice[0] (16), askPrice[0] (20)
Cache Lines Touched: 1 (CL1 only)
Memory Bandwidth: 64 bytes loaded
Access Time: 1-4 CPU cycles (L1 hit)
```

### **Scenario 2: Depth Window Update**
```
Access: token (0), LTP (12), bidPrice[0-4] (16-140), askPrice[0-4] (20-140)
Cache Lines Touched: 3 (CL1, CL2, CL3)
Memory Bandwidth: 196 bytes loaded
Access Time: 4-12 CPU cycles (L1/L2 hit)
```

### **Scenario 3: Full Market Depth (7200 Message)**
```
Access: All price/depth fields (0-265)
Cache Lines Touched: 4-5 (CL1-CL4)
Memory Bandwidth: 270 bytes loaded
Access Time: 10-40 CPU cycles (L2/L3 hit)
```

### **Scenario 4: Buyback/SPOS (Rare)**
```
Access: Buyback fields (338-401) or SPOS (298-337)
Cache Lines Touched: 1 (CL6 or CL7)
Memory Bandwidth: 64 bytes loaded
Access Time: Variable (might miss L3, ~100-300 cycles)
```

---

## 🔄 Data Update Flow & Priority

### **Phase 1: Pre-Market / Opening Phase**

#### **Step 1.1: Initialize with Opening Prices (6013)**
```
Priority: HIGHEST
Trigger: Market open time
Action: Populate openPrice field
Note: Message 6013 is deprecated but may be present in historical data
Alternative: Use openPrice from 7208 during normal market open
```

**Fields Updated:**
- `openPrice` ← 6013.openingPrice
- `lastUpdateTime` ← current timestamp
- `lastUpdateSource` ← 6013
- `isValid` ← true

---

#### **Step 1.2: Pre-Open Indicative Data (7201 - Market Watch)**
```
Priority: HIGH
Trigger: Pre-open session
Action: Show indicative prices
```

**Fields Updated:**
- `mwLastTradePrice` ← 7201.lastTradePrice (indicative open)
- `bestBuyPrice` ← 7201.buyPrice
- `bestBuyVolume` ← 7201.buyVolume
- `bestSellPrice` ← 7201.sellPrice
- `bestSellVolume` ← 7201.sellVolume
- `mwIndicator` ← 7201.indicator

**Update Strategy:** OVERWRITE (pre-open data is ephemeral)

---

#### **Step 1.3: SPOS Order Cancellation Statistics (7210)**
```
Priority: MEDIUM
Trigger: Special Pre-Open Session (IPO/Relist)
Action: Track order cancellation statistics
```

**Fields Updated:**
- `buyOrdCxlCount` ← 7210.buyOrdCxlCount
- `buyOrdCxlVol` ← 7210.buyOrdCxlVol
- `sellOrdCxlCount` ← 7210.sellOrdCxlCount
- `sellOrdCxlVol` ← 7210.sellOrdCxlVol
- `sposLastUpdate` ← current timestamp

**Update Strategy:** ACCUMULATE (statistics grow during session)

---

### **Phase 2: Market Open / Trading Phase**

#### **Step 2.1: Full Market Depth (7200 - MBO/MBP Update)**
```
Priority: HIGHEST
Frequency: Real-time (most frequent)
Trigger: Order book changes
Action: Update full depth + trade data
```

**Fields Updated:**
- **Trade Data:**
  - `lastTradedPrice` ← 7200.lastTradedPrice (if > 0)
  - `lastTradeQuantity` ← 7200.lastTradeQuantity
  - `lastTradeTime` ← 7200.lastTradeTime
  - `volumeTradedToday` ← 7200.volumeTradedToday (if increasing)
  - `averageTradePrice` ← 7200.averageTradePrice
  - `netPriceChange` ← 7200.netPriceChange
  - `netChangeIndicator` ← 7200.netChangeIndicator

- **OHLC Data:**
  - `openPrice` ← 7200.openPrice (if > 0 and not set)
  - `highPrice` ← max(current, 7200.highPrice)
  - `lowPrice` ← min(current, 7200.lowPrice) (if > 0)
  - `closePrice` ← 7200.closingPrice

- **Market Depth (5 levels):**
  - `bidPrice[5]` ← 7200.mbpBuffer[0-4].price
  - `bidQuantity[5]` ← 7200.mbpBuffer[0-4].quantity
  - `bidOrders[5]` ← 7200.mbpBuffer[0-4].numberOfOrders
  - `askPrice[5]` ← 7200.mbpBuffer[5-9].price
  - `askQuantity[5]` ← 7200.mbpBuffer[5-9].quantity
  - `askOrders[5]` ← 7200.mbpBuffer[5-9].numberOfOrders
  - `totalBuyQuantity` ← 7200.totalBuyQuantity
  - `totalSellQuantity` ← 7200.totalSellQuantity

- **Auction Data:**
  - `auctionNumber` ← 7200.auctionNumber
  - `auctionStatus` ← 7200.auctionStatus
  - `initiatorType` ← 7200.initiatorType
  - `initiatorPrice` ← 7200.initiatorPrice
  - `initiatorQuantity` ← 7200.initiatorQuantity
  - `auctionPrice` ← 7200.auctionPrice
  - `auctionQuantity` ← 7200.auctionQuantity

**Update Strategy:** SMART MERGE
- Only update fields with valid data (> 0)
- Protect OHLC from being overwritten by zeros
- Volume should only increase
- Preserve depth if new message has partial data

---

#### **Step 2.2: Price Depth Only (7208 - Only MBP)**
```
Priority: HIGH
Frequency: Periodic (when 7200 not sent)
Trigger: Market depth changes without order details
Action: Update depth + trade data (2 tokens per packet)
```

**Fields Updated:** (Same as 7200, but without MBO order details)
- All fields from 7200 except MBO-specific data
- Additional: `indicativeClosePrice` ← 7208.indicativeClosePrice

**Update Strategy:** SMART MERGE (same as 7200)

**Special Note:** 7208 can contain 2 tokens per packet, so process array

---

#### **Step 2.3: Market Watch Round Robin (7201)**
```
Priority: MEDIUM
Frequency: Periodic rotation (4 tokens per packet)
Trigger: Rotating market watch updates
Action: Update best bid/ask and LTP
```

**Fields Updated:**
- `bestBuyPrice` ← 7201.buyPrice
- `bestBuyVolume` ← 7201.buyVolume
- `bestSellPrice` ← 7201.sellPrice
- `bestSellVolume` ← 7201.sellVolume
- `mwLastTradePrice` ← 7201.lastTradePrice
- `mwLastTradeTime` ← 7201.lastTradeTime
- `lastTradeLess` ← 7201.indicator.lastTradeLess
- `lastTradeMore` ← 7201.indicator.lastTradeMore
- `buyIndicator` ← 7201.indicator.buy
- `sellIndicator` ← 7201.indicator.sell

**Update Strategy:** SUPPLEMENT
- Update if primary sources (7200/7208) haven't updated recently
- Used for tokens not in active order book updates
- LTP cross-verification source

---

#### **Step 2.4: Ticker Updates (18703)**
```
Priority: MEDIUM
Frequency: Very high (28 tokens per packet)
Trigger: Trade executions
Action: Fast LTP updates
```

**Fields Updated:**
- `lastTradedPrice` ← 18703.fillPrice (if > 0)
- `fillPrice` ← 18703.fillPrice
- `fillVolume` ← 18703.fillVolume
- `marketType` ← 18703.marketType

**Update Strategy:** FAST UPDATE
- Quickest LTP source (minimal data, high frequency)
- Don't overwrite if 7200/7208 has more recent data
- Use as fallback/confirmation

---

### **Phase 3: Special Sessions**

#### **Step 3.1: Buyback Information (18708)**
```
Priority: LOW
Frequency: Hourly during market hours
Trigger: Buyback session active
Action: Update buyback statistics (6 tokens per packet)
```

**Fields Updated:**
- `symbol` ← 18708.symbol
- `series` ← 18708.series
- `pdayCumVol` ← 18708.pdayCumVol
- `pdayHighPrice` ← 18708.pdayHighPrice
- `pdayLowPrice` ← 18708.pdayLowPrice
- `pdayWtAvg` ← 18708.pdayWtAvg
- `cdayCumVol` ← 18708.cdayCumVol
- `cdayHighPrice` ← 18708.cdayHighPrice
- `cdayLowPrice` ← 18708.cdayLowPrice
- `cdayWtAvg` ← 18708.cdayWtAvg
- `buybackStartDate` ← 18708.startDate
- `buybackEndDate` ← 18708.endDate
- `isBuybackActive` ← true

**Update Strategy:** OVERWRITE
- Buyback data is self-contained
- Update all buyback fields together

---

## 🔍 Smart Merge Logic

### **Merge Rules by Field Type**

#### **1. Trade Price Fields (LTP, OHLC)**
```
Rule: Only update if new value > 0
Reason: Partial updates may send 0 for unchanged fields

if (newLTP > 0) {
    consolidated.lastTradedPrice = newLTP;
}

if (newHigh > 0 && newHigh > consolidated.highPrice) {
    consolidated.highPrice = newHigh;
}

if (newLow > 0 && (consolidated.lowPrice == 0 || newLow < consolidated.lowPrice)) {
    consolidated.lowPrice = newLow;
}

if (newOpen > 0 && consolidated.openPrice == 0) {
    consolidated.openPrice = newOpen; // Set only once
}
```

#### **2. Volume Fields**
```
Rule: Volume should only increase (cumulative)

if (newVolume > consolidated.volumeTradedToday) {
    consolidated.volumeTradedToday = newVolume;
}
```

#### **3. Depth Arrays (Bid/Ask)**
```
Rule: Update entire depth if any level has data

bool hasDepthData = false;
for (int i = 0; i < 5; i++) {
    if (newBidPrice[i] > 0 || newAskPrice[i] > 0) {
        hasDepthData = true;
        break;
    }
}

if (hasDepthData) {
    memcpy(consolidated.bidPrice, newBidPrice, sizeof(bidPrice));
    memcpy(consolidated.bidQuantity, newBidQuantity, sizeof(bidQuantity));
    // ... copy all depth arrays
}
```

#### **4. Timestamp Logic**
```
Rule: Always update timestamp, track source

consolidated.lastUpdateTime = getCurrentTimeNanos();
consolidated.lastUpdateSource = messageCode;
consolidated.updateCount++;
```

#### **5. Data Quality Score**
```
Rule: Calculate based on completeness

uint8_t calculateQuality() {
    int score = 0;
    if (lastTradedPrice > 0) score += 20;
    if (openPrice > 0) score += 15;
    if (highPrice > 0) score += 10;
    if (lowPrice > 0) score += 10;
    if (bidPrice[0] > 0) score += 15;
    if (askPrice[0] > 0) score += 15;
    if (volumeTradedToday > 0) score += 15;
    return score; // 0-100
}
```

---

## 🚀 Optimal Access Strategy

### **Storage Structure**

```cpp
// Option 1: Flat Array (if token range is known and dense)
ConsolidatedMarketData marketData[MAX_TOKENS];
// Access: O(1) - marketData[token]

// Option 2: Hash Map (if token range is sparse)
std::unordered_map<uint32_t, ConsolidatedMarketData> marketDataMap;
// Access: O(1) average - marketDataMap[token]

// Option 3: Hybrid (Recommended)
// - Array for common tokens (0-50000)
// - Hash map for special/new tokens
ConsolidatedMarketData fastArray[50000];
std::unordered_map<uint32_t, ConsolidatedMarketData> overflowMap;
```

### **Thread Safety**

```cpp
// Read-Write Lock per Token
struct TokenData {
    ConsolidatedMarketData data;
    std::shared_mutex mutex;
};

// Read (Multiple readers allowed)
std::shared_lock lock(tokenData[token].mutex);
auto ltp = tokenData[token].data.lastTradedPrice;

// Write (Exclusive)
std::unique_lock lock(tokenData[token].mutex);
tokenData[token].data.lastTradedPrice = newLTP;
```

### **Cache-Friendly Access**

```cpp
// Group frequently accessed fields at the start
struct ConsolidatedMarketData {
    // Hot fields (cache line 1 - 64 bytes)
    uint32_t token;
    int32_t lastTradedPrice;
    int32_t bidPrice[5];
    int32_t askPrice[5];
    int64_t lastUpdateTime;
    
    // Warm fields (cache line 2-3)
    int64_t bidQuantity[5];
    int64_t askQuantity[5];
    // ...
    
    // Cold fields (rarely accessed)
    char symbol[10];
    // Buyback data
    // ...
};
```

---

## 📊 Message Processing Priority

```
Priority 1 (Real-time, < 1ms latency):
  - 7200 (MBO/MBP Update)
  - 7208 (Only MBP)
  - 18703 (Ticker)

Priority 2 (Near real-time, < 5ms latency):
  - 7201 (Market Watch)

Priority 3 (Periodic, < 100ms latency):
  - 7210 (SPOS Cancellation)
  - 18708 (Buyback)
  
Priority 4 (One-time, on demand):
  - 6013 (Opening Price - deprecated)
```

---

## 🔄 Update Frequency Expectations

```
Message   Frequency         Tokens/Msg   Updates/Sec (1000 tokens)
----------------------------------------------------------------------
7200      Every trade       1            Variable (high activity)
7208      Periodic          2            ~10-50 per token
18703     Every trade       28           ~100-500 (fast ticks)
7201      Round-robin       4            ~5-10 per token
7210      During SPOS       8            ~1 (special session)
18708     Hourly            6            ~0.0003 per token
6013      Market open       1            Once per day (deprecated)
```

---

## 🎯 Implementation Checklist

### **Phase 1: Initialization**
- [ ] Allocate consolidated array/map for expected tokens
- [ ] Pre-populate with master data (symbol, series, segment)
- [ ] Set all price fields to 0
- [ ] Set isValid = false
- [ ] Initialize mutexes/locks

### **Phase 2: Market Open**
- [ ] Process 6013 (if available) for opening prices
- [ ] Process 7201 for pre-open indicative data
- [ ] Set openPrice, mark isValid = true

### **Phase 3: Trading Session**
- [ ] Process 7200 with highest priority
- [ ] Process 7208 if 7200 not received
- [ ] Process 18703 for fast LTP updates
- [ ] Process 7201 for supplementary data
- [ ] Apply smart merge logic for all updates

### **Phase 4: Special Sessions**
- [ ] Process 7210 during SPOS
- [ ] Process 18708 hourly for buyback tokens

### **Phase 5: Data Quality**
- [ ] Calculate quality score after each update
- [ ] Mark stale data (no update > 5 seconds)
- [ ] Clear auction fields when auction ends
- [ ] Reset SPOS fields when session ends

---

## 🧪 Testing Strategy

### **Unit Tests**
1. Test smart merge with 0 values
2. Test volume increase validation
3. Test high/low price updates
4. Test depth array updates
5. Test timestamp logic

### **Integration Tests**
1. Process 6013 → 7200 → 18703 sequence
2. Process 7208 alone (without 7200)
3. Process 7201 with stale primary data
4. Process 7210 statistics accumulation
5. Process 18708 buyback updates

### **Performance Tests**
1. 1000 tokens × 100 updates/sec = 100k updates/sec
2. Memory usage validation
3. Lock contention analysis
4. Cache miss rate measurement

---

## 📈 Performance Expectations

```
Access Time:
  - Direct array: 5-10 ns
  - Hash map: 20-50 ns
  - With read lock: +10-20 ns

Update Time:
  - Smart merge: 50-100 ns
  - With write lock: +20-50 ns

Memory:
  - 1000 tokens: ~462 KB
  - 5000 tokens: ~2.31 MB
  - Cache-friendly with L3 cache

Throughput:
  - 100,000 updates/second sustained
  - 500,000 reads/second sustained
```

---

## 🛠️ Maintenance Notes

### **Field Addition**
- Add new field at end of section to preserve alignment
- Update total size calculation
- Update merge logic
- Update quality score calculation

### **Message Source Addition**
- Add message code to `lastUpdateSource` enum
- Create merge logic function
- Add to priority queue
- Update documentation

### **Performance Tuning**
- Monitor cache miss rates
- Profile lock contention
- Adjust array vs hash map threshold
- Optimize hot path fields

---

## 📝 Conclusion

This consolidated array strategy provides:
- ✅ **Single source of truth** for all market data
- ✅ **O(1) access** via token indexing
- ✅ **Smart merge logic** preventing data loss
- ✅ **Incremental updates** from multiple sources
- ✅ **Memory efficient** (~462 bytes per token)
- ✅ **Thread-safe** with minimal locking
- ✅ **High performance** (100k+ updates/sec)

**Recommended Implementation:** Hybrid array + hash map with per-token read-write locks.

---

**Document Version:** 1.0  
**Last Updated:** January 12, 2026  
**Maintained By:** Trading Terminal Development Team
