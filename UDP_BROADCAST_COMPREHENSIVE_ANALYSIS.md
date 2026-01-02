# UDP Broadcast Comprehensive Analysis & Implementation Roadmap

**Trading Terminal C++ - Complete Protocol Coverage**

**Document Version:** 1.0  
**Last Updated:** January 2, 2026  
**Author:** Architecture Analysis Team

---

## 📋 Table of Contents

1. [Executive Summary](#executive-summary)
2. [Critical Issue: Data Source Confusion](#critical-issue-data-source-confusion)
3. [NSE Protocol Complete Coverage](#nse-protocol-complete-coverage)
4. [BSE Protocol Complete Coverage](#bse-protocol-complete-coverage)
5. [Current Implementation Status](#current-implementation-status)
6. [Gap Analysis](#gap-analysis)
7. [Implementation Roadmap](#implementation-roadmap)
8. [Architecture Recommendations](#architecture-recommendations)

---

## Executive Summary

### Critical Finding: Mixed Data Sources in XTS::Tick

**PROBLEM:** `XTSTypes.h` contains a single `XTS::Tick` structure used for:
1. **XTS WebSocket API** (REST/WebSocket from Symphony API)
2. **NSE FO UDP Broadcast** (Direct market feed)
3. **NSE CM UDP Broadcast** (Direct market feed)
4. **BSE FO UDP Broadcast** (Direct market feed)
5. **BSE CM UDP Broadcast** (Direct market feed)

**CONFUSION:**
- WebSocket provides fields like `totalBuyQuantity`, `totalSellQuantity` which UDP doesn't have
- UDP provides 5-level depth which WebSocket doesn't have
- Field semantics differ (e.g., `openInterest` in UDP vs WebSocket)
- Different latency tracking needs (UDP is microsecond-level, WebSocket is millisecond-level)

### Current Protocol Coverage

| Exchange | Protocol Doc | Messages in Spec | Parsed | Used | Coverage |
|----------|--------------|------------------|--------|------|----------|
| NSE FO | NSE TRIMM Protocol v9.46 | 50+ broadcast | 9 | 3 | **18%** |
| NSE CM | NSE TRIMM Protocol v9.46 | 50+ broadcast | 7 | 3 | **14%** |
| BSE FO/CM | BSE Direct NFCAST v5.0 | 19 messages | 2 | 2 | **10%** |

---

## Critical Issue: Data Source Confusion

### Problem: Single Tick Structure for All Data Sources

```cpp
// Current: include/api/XTSTypes.h
namespace XTS {
    struct Tick {
        // WebSocket fields (Symphony XTS API)
        int totalBuyQuantity;        // ❌ NOT in UDP
        int totalSellQuantity;       // ❌ NOT in UDP
        double close;                // ❌ Means different things (prevClose in UDP)
        
        // UDP-only fields
        DepthLevel bidDepth[5];      // ❌ NOT in WebSocket
        DepthLevel askDepth[5];      // ❌ NOT in WebSocket
        
        // Latency tracking (UDP-only)
        int64_t timestampUdpRecv;    // ❌ NOT in WebSocket
        int64_t timestampParsed;     // ❌ NOT in WebSocket
        
        // Ambiguous fields
        double averagePrice;         // WebSocket: VWAP, UDP: ATP (different calculation)
        int64_t openInterest;        // WebSocket: Current OI, UDP: OI + changes
    };
}
```

### Impact of This Design

| Issue | Impact | Severity |
|-------|--------|----------|
| Field confusion | Developers don't know which fields are valid for which source | 🔴 High |
| Memory waste | WebSocket ticks allocate 5-level depth arrays (unused) | 🟡 Medium |
| Semantic errors | `close` means `prevClose` in UDP but `lastClose` in WebSocket | 🔴 High |
| Testing difficulty | Unit tests can't distinguish data source expectations | 🔴 High |
| Maintenance burden | Adding UDP-specific fields breaks WebSocket assumptions | 🟡 Medium |

---

## Proposed Solution: Separate Data Structures

### Option A: Source-Specific Tick Types (Recommended)

```cpp
// include/api/XTSTypes.h - Keep for WebSocket only
namespace XTS {
    struct Tick {
        // Only WebSocket API fields
        int exchangeSegment;
        int64_t exchangeInstrumentID;
        double lastTradedPrice;
        int totalBuyQuantity;      // ✅ WebSocket VWAP context
        int totalSellQuantity;
        double close;              // ✅ Last close (not prevClose)
        // ... other WebSocket fields
    };
}

// NEW: include/udp/UDPTypes.h
namespace UDP {
    struct MarketTick {
        // Core identification
        uint8_t exchangeSegment;   // 1=NSECM, 2=NSEFO, 11=BSECM, 12=BSEFO
        uint32_t token;
        
        // Price data (UDP semantics)
        double ltp;
        double open, high, low;
        double prevClose;          // ✅ Explicit: previous close
        double atp;                // ✅ Explicit: Average Traded Price
        
        // Volume
        uint64_t volume;
        uint64_t ltq;              // Last Trade Quantity
        
        // Open Interest (derivatives only)
        int64_t openInterest;
        int64_t oiChange;
        
        // 5-Level Depth
        struct DepthLevel {
            double price;
            uint64_t quantity;
            uint32_t orders;
        };
        DepthLevel bids[5];
        DepthLevel asks[5];
        
        // Aggregated depth
        uint64_t totalBidQty;
        uint64_t totalAskQty;
        
        // Latency tracking (microseconds)
        uint64_t refNo;            // Packet sequence
        uint64_t timestampUdpRecv;
        uint64_t timestampParsed;
        uint64_t timestampEmitted;
    };
}
```

**Benefits:**
- ✅ Clear separation of concerns
- ✅ No field ambiguity
- ✅ Type-safe APIs
- ✅ 40% smaller WebSocket ticks (no unused depth arrays)
- ✅ Explicit semantics (prevClose vs close)

### Conversion Layer

```cpp
// src/services/UdpBroadcastService.cpp
void UdpBroadcastService::emitTick(const UDP::MarketTick& udpTick) {
    emit marketTickReceived(udpTick);  // NEW: Dedicated UDP signal
}

// Backward compatibility adapter (optional)
XTS::Tick convertToLegacy(const UDP::MarketTick& udpTick) {
    XTS::Tick xtsTick;
    xtsTick.exchangeSegment = udpTick.exchangeSegment;
    xtsTick.exchangeInstrumentID = udpTick.token;
    xtsTick.lastTradedPrice = udpTick.ltp;
    xtsTick.close = udpTick.prevClose;  // ✅ Semantic mapping
    xtsTick.averagePrice = udpTick.atp;
    // ... map other fields with correct semantics
    return xtsTick;
}
```

---

## NSE Protocol Complete Coverage

### NSE Broadcast Message Types (F&O Segment)

From **NSE TRIMM Protocol v9.46**, Chapter 9 - Broadcast Messages:

| Code | Name | Size | Purpose | Parser | Integration | Priority |
|------|------|------|---------|--------|-------------|----------|
| **7200** | BCAST_MBO_MBP_UPDATE | 410 bytes | Touchline + 5-level depth | ✅ | ✅ | **CRITICAL** |
| **7201** | BCAST_MW_ROUND_ROBIN | 472 bytes | Market Watch (3 market types) | ✅ | ⚠️ Partial | HIGH |
| **7202** | BCAST_TICKER_AND_MKT_INDEX | 484 bytes | Ticker (OI updates) | ✅ | ✅ | HIGH |
| **7203** | BCAST_INDUSTRY_INDEX_UPDATE | 442 bytes | 20 industry indices | ❌ | ❌ | MEDIUM |
| **7207** | BCAST_INDICES | 468 bytes | 6 major indices (NIFTY, etc.) | ❌ | ❌ | MEDIUM |
| **7208** | BCAST_ONLY_MBP | 470 bytes | MBP-only (2 records) | ✅ | ✅ | HIGH |
| **7211** | BCAST_SPD_MBP_DELTA | 204 bytes | Spread depth | ✅ | ❌ | LOW |
| **7220** | BCAST_LIMIT_PRICE_PROTECTION | 344 bytes | Circuit limits | ✅ | ❌ | MEDIUM |
| **7305** | BCAST_SECURITY_MSTR_CHG | 298 bytes | Security master update | ❌ | ❌ | LOW |
| **7340** | BCAST_SEC_MSTR_CHNG_PERIODIC | 298 bytes | Periodic master update | ❌ | ❌ | LOW |
| **17201** | BCAST_ENHNCD_MW_ROUND_ROBIN | 492 bytes | Enhanced MW (64-bit OI) | ✅ | ❌ | MEDIUM |
| **17202** | BCAST_ENHNCD_TICKER | 498 bytes | Enhanced Ticker (64-bit OI) | ✅ | ❌ | MEDIUM |
| **7130** | MKT_MVMT_CM_OI_IN | 504 bytes | CM Asset OI | ❌ | ❌ | LOW |
| **17130** | ENHNCD_MKT_MVMT_CM_OI_IN | 508 bytes | Enhanced CM Asset OI | ❌ | ❌ | LOW |
| **1833** | RPRT_MARKET_STATS_OUT_RPT | 488 bytes | Market statistics report | ❌ | ❌ | LOW |
| **11833** | ENHNCD_RPRT_MARKET_STATS | 372 bytes | Enhanced market stats | ❌ | ❌ | LOW |
| **7732** | GI_INDICES_ASSETS | 138 bytes | Global indices | ❌ | ❌ | LOW |

**Total: 17 message types, 9 parsed (53%), 3 fully integrated (18%)**

### NSE CM Broadcast Messages

Same message codes as NSE FO, key differences:
- **Volume fields:** 64-bit instead of 32-bit
- **No Open Interest** fields (cash market)
- **Message 18703:** CM-specific ticker with market index value

| Code | Name | CM-Specific Feature | Parser | Integration |
|------|------|---------------------|--------|-------------|
| 7200 | MBO/MBP Update | 64-bit volume | ✅ | ✅ |
| 7201 | Market Watch | 64-bit quantities | ✅ | ⚠️ Partial |
| 7203 | Industry Index | 20 industry indices | ❌ | ❌ |
| 7207 | Indices | 6 major indices | ❌ | ❌ |
| 7208 | Only MBP | 64-bit quantities | ✅ | ✅ |
| **18703** | **CM_TICKER** | **Market index value** | ❌ | ❌ |

---

## BSE Protocol Complete Coverage

### BSE Direct NFCAST Messages (v5.0)

From **BSE Direct NFCAST Manual v5.0**:

| Code | Name | Size | Purpose | Parser | Integration | Priority |
|------|------|------|---------|--------|-------------|----------|
| **2001** | TIME_BROADCAST | 44 bytes | Time sync (Hour/Min/Sec/Msec) | ❌ | ❌ | LOW |
| **2002** | PRODUCT_STATE_CHANGE | Variable | Session/market state | ❌ | ❌ | HIGH |
| **2003** | AUCTION_SESSION_CHANGE | Variable | Shortage auction state | ❌ | ❌ | MEDIUM |
| **2004** | NEWS_HEADLINE | Variable | News URL | ❌ | ❌ | LOW |
| **2011** | INDEX_CHANGE | Variable | Index values (SENSEX, etc.) | ✅ | ✅ | HIGH |
| **2012** | INDEX_CHANGE | 120 bytes | Index with OHLC | ✅ | ✅ | HIGH |
| **2014** | CLOSE_PRICE | Variable | Close price broadcast | ❌ | ❌ | MEDIUM |
| **2015** | OPEN_INTEREST | Variable | OI for derivatives | ✅ | ✅ | **CRITICAL** |
| **2016** | VAR_PERCENTAGE | Variable | Margin percentage | ❌ | ❌ | LOW |
| **2017** | AUCTION_MARKET_PICTURE | Variable | Auction order book (sell-side) | ❌ | ❌ | LOW |
| **2020** | MARKET_PICTURE | 36 + N×264 | LTP, OHLC, 5-level depth | ✅ | ⚠️ L1 only | **CRITICAL** |
| **2021** | MARKET_PICTURE_COMPLEX | 36 + N×264 | Same as 2020 (64-bit token) | ✅ | ⚠️ L1 only | **CRITICAL** |
| **2022** | RBI_REFERENCE_RATE | Variable | USD reference rate | ❌ | ❌ | LOW |
| **2027** | ODD_LOT_MARKET_PICTURE | Variable | Odd-lot order book | ❌ | ❌ | LOW |
| **2028** | IMPLIED_VOLATILITY | Variable | IV for options | ❌ | ❌ | MEDIUM |
| **2030** | AUCTION_KEEP_ALIVE | Variable | Network keep-alive | ❌ | ❌ | LOW |
| **2033** | DEBT_MARKET_PICTURE | Variable | Debt instrument data | ❌ | ❌ | LOW |
| **2034** | LIMIT_PRICE_PROTECTION | Variable | Circuit limits (DPR) | ❌ | ❌ | MEDIUM |
| **2035** | CALL_AUCTION_CANCELLED_QTY | Variable | Cancelled quantity | ❌ | ❌ | LOW |

**Total: 19 message types, 4 parsed (21%), 4 integrated (21%)**

### BSE 5-Level Depth Structure (Critical Gap)

**Currently Parsing:** Only Level 1 (Best Bid/Ask)

**Protocol Spec (Message 2020/2021):**
```
Each record = 264 bytes
Offset 104-263: 5-Level Order Book (160 bytes)

Per depth level (16 bytes per side):
  Bid Side:
    - Best Bid Rate:        Long (4 bytes) - Paise
    - Total Bid Quantity:   Long Long (8 bytes)
    - No. of Bid Orders:    Unsigned Long (4 bytes)
    - Reserved:             Long (4 bytes)
  
  Ask Side:
    - Best Offer Rate:      Long (4 bytes) - Paise
    - Total Offer Qty:      Long Long (8 bytes)
    - No. of Offer Orders:  Unsigned Long (4 bytes)
    - Reserved:             Long (4 bytes)

Layout: Interleaved Bid1, Ask1, Bid2, Ask2, Bid3, Ask3, Bid4, Ask4, Bid5, Ask5
Total: 5 levels × 2 sides × 16 bytes = 160 bytes
```

**Gap:** We're only parsing offset 104-119 (Bid1) and 120-135 (Ask1). **Levels 2-5 are ignored!**

---

## Current Implementation Status

### File Structure Analysis

```
lib/
├── cpp_broacast_nsefo/          # NSE F&O
│   ├── include/
│   │   ├── nse_common.h         # ✅ Base structures
│   │   ├── nse_market_data.h    # ✅ 7200, 7201, 7202, 7208, 7211, 7220
│   │   ├── nse_index_messages.h # ✅ 7203, 7207, 7732 structures (NOT PARSED)
│   │   ├── nse_market_statistics.h # ✅ 1833, 11833, 7130, 17130 (NOT PARSED)
│   │   └── nse_database_updates.h  # ✅ 7305, 7340 structures (NOT PARSED)
│   └── src/parser/
│       ├── parse_message_7200.cpp  # ✅ Full 5-level depth
│       ├── parse_message_7201.cpp  # ✅ Market Watch
│       ├── parse_message_7202.cpp  # ✅ Ticker
│       ├── parse_message_7208.cpp  # ✅ MBP Only
│       ├── parse_message_7211.cpp  # ✅ Spread
│       ├── parse_message_7220.cpp  # ✅ Price protection
│       ├── parse_message_17201.cpp # ✅ Enhanced MW
│       └── parse_message_17202.cpp # ✅ Enhanced Ticker
│
├── cpp_broadcast_nsecm/         # NSE Cash
│   └── src/parser/
│       ├── parse_message_7200.cpp  # ✅ Full 5-level depth
│       ├── parse_message_7201.cpp  # ✅ Market Watch
│       ├── parse_message_7203.cpp  # ✅ Industry Index
│       ├── parse_message_7207.cpp  # ✅ Indices
│       ├── parse_message_7208.cpp  # ✅ MBP Only
│       └── parse_message_18703.cpp # ✅ CM Ticker
│
└── cpp_broadcast_bsefo/         # BSE F&O and CM
    ├── include/
    │   ├── bse_protocol.h       # ✅ 2011, 2012, 2015, 2020, 2021
    │   └── bse_receiver.h       # ✅ Decoding logic
    └── src/
        └── bse_receiver.cpp     # ⚠️ Only L1 depth parsed!
```

### Integration Points

```
UdpBroadcastService (src/services/UdpBroadcastService.cpp)
    │
    ├─ registerTouchlineCallback()     # ✅ NSE FO/CM: 7200/7208 → bidDepth[0], askDepth[0]
    ├─ registerMarketDepthCallback()   # ✅ NSE FO/CM: 7200/7208 → bidDepth[0-4], askDepth[0-4]
    ├─ registerTickerCallback()        # ✅ NSE FO/CM: 7202 → openInterest, OI changes
    └─ BSE RecordCallback              # ⚠️ BSE: 2020/2021 → bidDepth[0], askDepth[0] ONLY
```

---

## Gap Analysis

### Critical Gaps (Blocking Production Use)

| Gap | Impact | Affected Use Cases | Severity |
|-----|--------|-------------------|----------|
| **BSE 5-level depth not parsed** | SnapQuote shows only L1 | SnapQuote, Option Chain | 🔴 **CRITICAL** |
| **XTS::Tick semantic confusion** | Wrong data interpretation | All subscribers | 🔴 **CRITICAL** |
| **No session state handling** | App doesn't know market open/close | Trading logic | 🔴 **CRITICAL** |
| **No index data** | Can't display NIFTY/SENSEX | Market overview | 🟡 HIGH |

### High-Priority Gaps

| Gap | Impact | Priority |
|-----|--------|----------|
| Market Watch (7201) not used | 3 market types (Normal/SL/Auction) ignored | HIGH |
| Industry indices (7203) not parsed | Sector analysis impossible | HIGH |
| Close price (2014) not parsed | EOD processing incomplete | HIGH |
| Circuit limits (7220, 2034) not used | No DPR warnings | HIGH |

### Medium-Priority Gaps

| Gap | Impact | Priority |
|-----|--------|----------|
| Enhanced messages (17xxx) not used | Missing 64-bit OI for high OI contracts | MEDIUM |
| Spread depth (7211) not used | No spread trading support | MEDIUM |
| Implied Volatility (2028) not parsed | Options analysis limited | MEDIUM |
| Security master updates (7305/7340) | Symbol changes not tracked | MEDIUM |

---

## Implementation Roadmap

### Phase 1: Fix Critical Issues (Week 1-2)

#### 1.1 Separate Data Structures
**Files to Create:**
- `include/udp/UDPTypes.h` - New UDP::MarketTick structure
- `include/udp/UDPEnums.h` - Exchange segments, message types

**Files to Modify:**
- `src/services/UdpBroadcastService.h` - New signal `marketTickReceived(UDP::MarketTick)`
- `src/services/UdpBroadcastService.cpp` - Emit UDP-specific ticks
- `src/services/FeedHandler.h` - Accept `UDP::MarketTick`
- `src/views/MarketWatchWindow/Data.cpp` - Update to `UDP::MarketTick`

**Backward Compatibility:**
- Keep `XTS::Tick` for WebSocket API only
- Add `convertToLegacy()` adapter for existing code

#### 1.2 Fix BSE 5-Level Depth Parsing
**File:** `lib/cpp_broadcast_bsefo/src/bse_receiver.cpp`

**Current Code (Lines ~200-250):**
```cpp
// ❌ Only parsing Level 1
int bidOffset = 104;
int askOffset = 120;
bid.price = le32toh_func(*(uint32_t*)(record + bidOffset));
bid.quantity = le64toh_func(*(uint64_t*)(record + bidOffset + 4));
ask.price = le32toh_func(*(uint32_t*)(record + askOffset));
ask.quantity = le64toh_func(*(uint64_t*)(record + askOffset + 4));
```

**New Code:**
```cpp
// ✅ Parse all 5 levels (interleaved)
for (int level = 0; level < 5; level++) {
    int bidOffset = 104 + (level * 32);  // Each pair = 32 bytes
    int askOffset = bidOffset + 16;       // Ask follows Bid
    
    DecodedDepthLevel bid, ask;
    
    // Bid side (16 bytes)
    bid.price = le32toh_func(*(int32_t*)(record + bidOffset));
    bid.quantity = le64toh_func(*(uint64_t*)(record + bidOffset + 4));
    bid.numOrders = le32toh_func(*(uint32_t*)(record + bidOffset + 12));
    
    // Ask side (16 bytes)
    ask.price = le32toh_func(*(int32_t*)(record + askOffset));
    ask.quantity = le64toh_func(*(uint64_t*)(record + askOffset + 4));
    ask.numOrders = le32toh_func(*(uint32_t*)(record + askOffset + 12));
    
    decRec.bids.push_back(bid);
    decRec.asks.push_back(ask);
}
```

**Verification:**
- Add debug logging to print all 5 levels
- Compare with BSE MarketPicture manual parsing
- Test with live BSE FO data

#### 1.3 Implement BSE Session State (2002)
**File:** `lib/cpp_broadcast_bsefo/include/bse_protocol.h`

```cpp
struct ProductStateChange {
    uint16_t msgType;              // 2002
    uint32_t sessionNumber;
    uint16_t marketSegmentId;
    uint8_t marketType;            // 0=Pre-open, 1=Continuous, 2=Auction
    uint8_t startEndFlag;          // 0=Start, 1=End
    uint64_t timestamp;
};
```

**Integration:**
- Create `SessionManager` singleton
- Emit `sessionChanged(segment, marketType, sessionNumber)` signal
- Update UI to show "Market Closed" / "Pre-Open" / "Trading"

---

### Phase 2: High-Priority Messages (Week 3-4)

#### 2.1 NSE Indices (7207)
**Parser:** `parse_message_7207.cpp`
```cpp
// Parse 6 indices: NIFTY 50, NIFTY Bank, etc.
MS_BCAST_INDICES* msg = (MS_BCAST_INDICES*)buffer;
for (int i = 0; i < msg->numberOfRecords; i++) {
    IndexData idx;
    idx.name = std::string(msg->indices[i].indexName, 21);
    idx.value = ntohl(msg->indices[i].indexValue) / 100.0;
    idx.high = ntohl(msg->indices[i].highIndexValue) / 100.0;
    idx.low = ntohl(msg->indices[i].lowIndexValue) / 100.0;
    // ... dispatch
}
```

#### 2.2 NSE Industry Indices (7203)
**Parser:** `parse_message_7203.cpp`
```cpp
// Parse 20 industry indices
MS_BCAST_INDUSTRY_INDICES* msg = (MS_BCAST_INDUSTRY_INDICES*)buffer;
for (int i = 0; i < msg->noOfRecs; i++) {
    IndustryIndexData idx;
    idx.name = std::string(msg->industryIndices[i].industryName, 15);
    idx.value = ntohl(msg->industryIndices[i].indexValue) / 100.0;
    // ... dispatch
}
```

#### 2.3 BSE Close Price (2014)
**Parser:** `bse_receiver.cpp::decodeClosePrice()`
```cpp
void BSEReceiver::decodeClosePrice(const uint8_t* data, size_t len) {
    // Parse close price for all instruments
    // Called at day-end and day-start
}
```

#### 2.4 Use Market Watch Data (7201)
**Current:** Parsed but not integrated

**Action:** 
- Add 3 market type columns to MarketWatch (Normal, Stop-Loss, Auction)
- Display buy/sell volumes and prices for each market type

---

### Phase 3: Enhanced Messages (Week 5-6)

#### 3.1 Enhanced Ticker (17202) & Market Watch (17201)
**Purpose:** 64-bit OI for contracts with high open interest

**Parser:** Already implemented, need integration:
```cpp
void UdpBroadcastService::registerEnhancedTickerCallback() {
    nsefo::MarketDataCallbackRegistry::instance()
        .registerEnhancedTickerCallback([this](const nsefo::EnhancedTickerData& data) {
            // Use int64_t OI instead of uint32_t
            UDP::MarketTick tick;
            tick.openInterest = data.openInterest;  // int64_t
            tick.oiDayHigh = data.dayHiOI;
            tick.oiDayLow = data.dayLoOI;
            emit marketTickReceived(tick);
        });
}
```

#### 3.2 Circuit Limits (7220, 2034)
**Purpose:** Display price bands and circuit breaker status

**Parser:** 7220 already implemented, need integration

**UI:** Add circuit limit indicators to MarketWatch:
- Show upper/lower execution bands
- Highlight scrips near circuit limits
- Flash alerts when circuit breaker triggered

---

### Phase 4: Advanced Features (Week 7-8)

#### 4.1 Implied Volatility (BSE 2028)
**Parser:** `bse_receiver.cpp::decodeImpliedVolatility()`

**UI:** Add IV column to MarketWatch for options

#### 4.2 Security Master Updates (7305, 7340)
**Purpose:** Update symbol names, tick sizes, lot sizes dynamically

**Parser:** `parse_message_7305.cpp`

**Integration:** 
- Update `RepositoryManager` when master change received
- Refresh MarketWatch display names
- Update order entry validations (tick size, lot size)

#### 4.3 Spread Trading Support (7211)
**Purpose:** Support calendar spreads, butterfly spreads

**Parser:** Already implemented

**UI:** Create SpreadWindow for spread depth and P&L

---

## Architecture Recommendations

### 1. Data Structure Hierarchy

```
┌─────────────────────────────────────────────────┐
│            Application Layer                     │
├─────────────────────────────────────────────────┤
│  - MarketWatchWindow                             │
│  - SnapQuoteWindow                               │
│  - OptionChainWindow                             │
│  - BuyWindow, SellWindow                         │
└──────────────┬──────────────────────────────────┘
               │ Subscribe via FeedHandler
               ▼
┌─────────────────────────────────────────────────┐
│          FeedHandler (Pub/Sub)                   │
│  Token-based routing: (segment, token) → subs   │
└──────────────┬──────────────────────────────────┘
               │ emit marketTickReceived(UDP::MarketTick)
               ▼
┌─────────────────────────────────────────────────┐
│       UdpBroadcastService                        │
│  - 4 receivers (NSE FO/CM, BSE FO/CM)            │
│  - Converts native structs → UDP::MarketTick     │
└──────────────┬──────────────────────────────────┘
               │
      ┌────────┴─────────┐
      ▼                  ▼
┌─────────────┐    ┌─────────────┐
│ NSE Parsers │    │ BSE Parsers │
│  nsefo::    │    │  bse::      │
│  nsecm::    │    │             │
└─────────────┘    └─────────────┘
```

### 2. Separate XTS Types from UDP Types

**Current Problem:**
```cpp
// ❌ Mixing concerns
namespace XTS {
    struct Tick {
        // WebSocket fields
        int totalBuyQuantity;
        
        // UDP fields
        DepthLevel bidDepth[5];
        
        // Latency tracking (UDP-only)
        int64_t timestampUdpRecv;
    };
}
```

**Recommended:**
```cpp
// ✅ Clean separation
namespace XTS {
    struct Tick {
        // Only WebSocket/REST API fields
        // ~80 bytes
    };
}

namespace UDP {
    struct MarketTick {
        // Only UDP broadcast fields
        // ~200 bytes (with 5-level depth)
    };
}

// Conversion when needed
UDP::MarketTick toUDP(const XTS::Tick& xts);
XTS::Tick toLegacy(const UDP::MarketTick& udp);
```

### 3. Message Priority Handling

**Current:** All messages processed equally

**Recommended:**
```cpp
enum class MessagePriority {
    CRITICAL,  // 7200, 7208, 2020, 2021 (price updates)
    HIGH,      // 7202, 2015 (OI updates)
    NORMAL,    // 7201, 7207 (indices)
    LOW        // 7305, 7340 (master updates)
};

// Process CRITICAL messages first in queue
void UdpBroadcastService::dispatchMessage(uint16_t msgType, const uint8_t* data) {
    auto priority = getPriority(msgType);
    if (priority == CRITICAL) {
        // Direct dispatch
        parseAndEmit(msgType, data);
    } else {
        // Queue for background thread
        m_backgroundQueue.push({msgType, data, priority});
    }
}
```

### 4. Latency Optimization

**Measurement Points:**
```
[T1] UDP recv()          ← 0µs baseline
[T2] Parse complete      ← Target: <50µs
[T3] Emit signal         ← Target: <10µs
[T4] FeedHandler route   ← Target: <20µs
[T5] UI update           ← Target: <100µs
-----------------------------------------
Total: <180µs (acceptable for 0.8s netted feed)
```

**Optimization:**
- Use `UDP::MarketTick` (smaller, cache-friendly)
- Lock-free hash map in FeedHandler
- Direct signal-slot connection (not queued)

---

## Testing Strategy

### Unit Tests

```cpp
// tests/udp/test_bse_depth_parsing.cpp
TEST(BSEDepthParser, ParsesAll5Levels) {
    // Load sample 2020 packet
    uint8_t packet[300] = { /* ... */ };
    
    bse::DecodedRecord rec = parseRecord(packet);
    
    EXPECT_EQ(rec.bids.size(), 5);
    EXPECT_EQ(rec.asks.size(), 5);
    
    // Verify interleaved offsets
    EXPECT_EQ(rec.bids[0].price, /* Offset 104 */);
    EXPECT_EQ(rec.asks[0].price, /* Offset 120 */);
    EXPECT_EQ(rec.bids[1].price, /* Offset 136 */);
    EXPECT_EQ(rec.asks[1].price, /* Offset 152 */);
}

// tests/udp/test_udp_types.cpp
TEST(UDPTypes, SemanticClarity) {
    UDP::MarketTick udp;
    udp.prevClose = 100.0;
    udp.atp = 101.5;
    
    XTS::Tick xts = toLegacy(udp);
    
    EXPECT_EQ(xts.close, 100.0);        // ✅ prevClose mapped to close
    EXPECT_EQ(xts.averagePrice, 101.5); // ✅ ATP mapped correctly
}
```

### Integration Tests

```cpp
// tests/integration/test_live_feed.cpp
TEST(LiveFeed, BSE5LevelDepthReceived) {
    // Connect to BSE FO test feed
    UdpBroadcastService service;
    service.start(config);
    
    QSignalSpy spy(&service, &UdpBroadcastService::marketTickReceived);
    
    // Wait for 5 seconds
    QTest::qWait(5000);
    
    ASSERT_GT(spy.count(), 0);
    
    UDP::MarketTick tick = spy.at(0).at(0).value<UDP::MarketTick>();
    
    // Verify 5 levels parsed
    for (int i = 0; i < 5; i++) {
        EXPECT_GT(tick.bids[i].quantity, 0);
        EXPECT_GT(tick.asks[i].quantity, 0);
    }
}
```

---

## Appendix A: Complete Message Type Reference

### NSE F&O Transaction Codes

| Code | Hex | Name | Size | Priority |
|------|-----|------|------|----------|
| 7200 | 0x1C20 | BCAST_MBO_MBP_UPDATE | 410 | CRITICAL |
| 7201 | 0x1C21 | BCAST_MW_ROUND_ROBIN | 472 | HIGH |
| 7202 | 0x1C22 | BCAST_TICKER_AND_MKT_INDEX | 484 | HIGH |
| 7203 | 0x1C23 | BCAST_INDUSTRY_INDEX_UPDATE | 442 | NORMAL |
| 7207 | 0x1C27 | BCAST_INDICES | 468 | NORMAL |
| 7208 | 0x1C28 | BCAST_ONLY_MBP | 470 | CRITICAL |
| 7211 | 0x1C2B | BCAST_SPD_MBP_DELTA | 204 | NORMAL |
| 7220 | 0x1C34 | BCAST_LIMIT_PRICE_PROTECTION | 344 | HIGH |
| 7305 | 0x1C89 | BCAST_SECURITY_MSTR_CHG | 298 | LOW |
| 7340 | 0x1CAC | BCAST_SEC_MSTR_CHNG_PERIODIC | 298 | LOW |
| 7130 | 0x1BDA | MKT_MVMT_CM_OI_IN | 504 | NORMAL |
| 17130 | 0x42EA | ENHNCD_MKT_MVMT_CM_OI_IN | 508 | NORMAL |
| 17201 | 0x4331 | BCAST_ENHNCD_MW_ROUND_ROBIN | 492 | HIGH |
| 17202 | 0x4332 | BCAST_ENHNCD_TICKER | 498 | HIGH |
| 1833 | 0x0729 | RPRT_MARKET_STATS_OUT_RPT | 488 | LOW |
| 11833 | 0x2E39 | ENHNCD_RPRT_MARKET_STATS | 372 | LOW |
| 7732 | 0x1E34 | GI_INDICES_ASSETS | 138 | NORMAL |

### BSE Direct NFCAST Message Types

| Code | Hex | Name | Purpose | Priority |
|------|-----|------|---------|----------|
| 2001 | 0x07D1 | TIME_BROADCAST | Time sync | LOW |
| 2002 | 0x07D2 | PRODUCT_STATE_CHANGE | Session state | CRITICAL |
| 2003 | 0x07D3 | AUCTION_SESSION_CHANGE | Auction state | NORMAL |
| 2004 | 0x07D4 | NEWS_HEADLINE | News | LOW |
| 2011 | 0x07DB | INDEX_CHANGE | Index values | HIGH |
| 2012 | 0x07DC | INDEX_CHANGE | Index OHLC | HIGH |
| 2014 | 0x07DE | CLOSE_PRICE | Close prices | HIGH |
| 2015 | 0x07DF | OPEN_INTEREST | OI for derivatives | CRITICAL |
| 2016 | 0x07E0 | VAR_PERCENTAGE | Margin % | NORMAL |
| 2017 | 0x07E1 | AUCTION_MARKET_PICTURE | Auction book | NORMAL |
| 2020 | 0x07E4 | MARKET_PICTURE | LTP, OHLC, 5-depth | CRITICAL |
| 2021 | 0x07E5 | MARKET_PICTURE_COMPLEX | Same (64-bit token) | CRITICAL |
| 2022 | 0x07E6 | RBI_REFERENCE_RATE | USD rate | LOW |
| 2027 | 0x07EB | ODD_LOT_MARKET_PICTURE | Odd-lot book | LOW |
| 2028 | 0x07EC | IMPLIED_VOLATILITY | Option IV | NORMAL |
| 2030 | 0x07EE | AUCTION_KEEP_ALIVE | Keep-alive | LOW |
| 2033 | 0x07F1 | DEBT_MARKET_PICTURE | Debt data | LOW |
| 2034 | 0x07F2 | LIMIT_PRICE_PROTECTION | Circuit limits | HIGH |
| 2035 | 0x07F3 | CALL_AUCTION_CANCELLED_QTY | Cancelled qty | NORMAL |

---

## Appendix B: Implementation Checklist

### Week 1-2: Critical Fixes
- [ ] Create `include/udp/UDPTypes.h`
- [ ] Create `include/udp/UDPEnums.h`
- [ ] Add `UDP::MarketTick` structure
- [ ] Update `UdpBroadcastService` to emit `UDP::MarketTick`
- [ ] Fix BSE 5-level depth parsing
- [ ] Add BSE session state parsing (2002)
- [ ] Update FeedHandler to accept `UDP::MarketTick`
- [ ] Update MarketWatchWindow to use `UDP::MarketTick`
- [ ] Add backward compatibility adapter
- [ ] Write unit tests for depth parsing
- [ ] Write unit tests for type conversion

### Week 3-4: High-Priority Messages
- [ ] Implement NSE indices parser (7207)
- [ ] Implement NSE industry indices parser (7203)
- [ ] Implement BSE close price parser (2014)
- [ ] Integrate Market Watch data (7201) into UI
- [ ] Add index display widget
- [ ] Add circuit limit indicators
- [ ] Write integration tests

### Week 5-6: Enhanced Messages
- [ ] Integrate enhanced ticker (17202)
- [ ] Integrate enhanced MW (17201)
- [ ] Add 64-bit OI handling
- [ ] Integrate circuit limits (7220, 2034) into UI
- [ ] Add price band warnings

### Week 7-8: Advanced Features
- [ ] Implement IV parser (2028)
- [ ] Implement security master parsers (7305, 7340)
- [ ] Add spread trading support (7211)
- [ ] Create SpreadWindow
- [ ] Add master update handling
- [ ] Performance testing and optimization

---

**End of Document**
