# Exchange Message Parser Coverage Analysis

## Overview

This document analyzes all UDP broadcast message codes defined in exchange protocols and their implementation status in the trading terminal.

---

## NSE FO Message Codes

**Protocol Docs**: `docs/nse_trimm_protocol_fo.md`, `lib/cpp_broacast_nsefo/include/nse_market_data.h`

| Code | Name | Status | Parser File | Priority | Data Fields |
|------|------|--------|-------------|----------|-------------|
| **7200** | BCAST_MBO_MBP_UPDATE | ✅ Implemented | `parse_message_7200.cpp` | High | 5-level depth, OHLC, Volume, LTP, OI |
| **7201** | BCAST_MW_ROUND_ROBIN | ✅ Implemented | `parse_message_7201.cpp` | Medium | Market Watch data, 3 levels |
| **7202** | BCAST_TICKER_AND_MKT_INDEX | ✅ Implemented | `parse_message_7202.cpp` | High | Trade ticks, Fill price/qty, OI |
| **7207** | BCAST_INDICES | ❌ Not implemented | - | Low | Index values (NIFTY, BANKNIFTY) |
| **7208** | BCAST_ONLY_MBP | ✅ Implemented | `parse_message_7208.cpp` | High | 5-level depth only |
| **7211** | BCAST_SPD_MBP_DELTA | ✅ Implemented | `parse_message_7211.cpp` | Low | Spread depth delta |
| **7220** | BCAST_LIMIT_PRICE_PROTECTION | ✅ Implemented | `parse_message_7220.cpp` | Low | Circuit limits |
| **17201** | BCAST_ENHNCD_MW_ROUND_ROBIN | ✅ Implemented | `parse_message_17201.cpp` | Medium | Enhanced MW (64-bit OI) |
| **17202** | BCAST_ENHNCD_TICKER | ✅ Implemented | `parse_message_17202.cpp` | Medium | Enhanced Ticker (64-bit OI) |

### Missing NSE FO Parsers

| Code | Name | Priority | Data Value | Implementation Effort |
|------|------|----------|------------|----------------------|
| **7207** | BCAST_INDICES | Low | Index OHLC values | 2 hours |

---

## NSE CM Message Codes

**Protocol Docs**: `lib/cpp_broadcast_nsecm/include/`

| Code | Name | Status | Parser File | Priority | Data Fields |
|------|------|--------|-------------|----------|-------------|
| **7200** | BCAST_MBO_MBP_UPDATE | ✅ Implemented | `parse_message_7200.cpp` | High | 5-level depth (64-bit qty), OHLC |
| **7201** | BCAST_MW_ROUND_ROBIN | ✅ Implemented | `parse_message_7201.cpp` | Medium | Market Watch data |
| **7203** | BCAST_INDUSTRY_INDICES | ✅ Implemented | `parse_message_7203.cpp` | Low | Sectoral indices |
| **7207** | BCAST_INDICES | ✅ Implemented | `parse_message_7207.cpp` | Medium | Index values |
| **7208** | BCAST_ONLY_MBP | ✅ Implemented | `parse_message_7208.cpp` | High | 5-level depth |
| **18703** | BCAST_ENHANCED | ✅ Implemented | `parse_message_18703.cpp` | Medium | Enhanced data |

### Missing NSE CM Parsers

**None - Full coverage for essential messages**

---

## BSE FO/CM Message Codes

**Protocol Docs**: `bse_manual.txt`, `lib/cpp_broadcast_bsefo/include/bse_protocol.h`

| Code | Name | Status | Parser File | Priority | Data Fields |
|------|------|--------|-------------|----------|-------------|
| **2001** | TIME_BROADCAST | ❌ Not implemented | - | Very Low | Exchange time sync |
| **2002** | PRODUCT_STATE_CHANGE | ❌ Not implemented | - | Medium | Session changes |
| **2003** | AUCTION_SESSION_CHANGE | ❌ Not implemented | - | Low | Auction session |
| **2004** | NEWS_HEADLINE | ❌ Not implemented | - | Very Low | News URL |
| **2011** | INDEX_CHANGE | ✅ Implemented | `bse_receiver.cpp` | Medium | Index OHLC |
| **2012** | INDEX_CHANGE_2 | ✅ Implemented | `bse_receiver.cpp` | Medium | Index OHLC (alt) |
| **2014** | CLOSE_PRICE | ❌ Not implemented | - | Medium | Official close price |
| **2015** | OPEN_INTEREST | ✅ Implemented | `bse_receiver.cpp` | **High** | OI for derivatives |
| **2016** | VAR_PERCENTAGE | ❌ Not implemented | - | Low | VaR margin % |
| **2017** | AUCTION_MARKET_PICTURE | ❌ Not implemented | - | Low | Auction order book |
| **2020** | MARKET_PICTURE | ✅ Implemented | `bse_receiver.cpp` | **Critical** | 5-level depth, OHLC, Volume, LTP |
| **2021** | MARKET_PICTURE_COMPLEX | ✅ Implemented | `bse_receiver.cpp` | **Critical** | Same as 2020 (64-bit token) |
| **2022** | RBI_REFERENCE_RATE | ❌ Not implemented | - | Very Low | USD ref rate |
| **2027** | ODD_LOT_MARKET_PICTURE | ❌ Not implemented | - | Very Low | Odd lot orders |
| **2028** | IMPLIED_VOLATILITY | ❌ Not implemented | - | Medium | Options IV |
| **2030** | AUCTION_KEEP_ALIVE | ❌ Not implemented | - | Very Low | Keep-alive |
| **2033** | DEBT_MARKET_PICTURE | ❌ Not implemented | - | Very Low | Debt instrument |
| **2034** | LIMIT_PRICE_PROTECTION | ❌ Not implemented | - | Low | Circuit limits |
| **2035** | CALL_AUCTION_CANCELLED_QTY | ❌ Not implemented | - | Very Low | Cancelled qty |

### Missing BSE Parsers - Prioritized

| Code | Name | Priority | Data Value | Implementation Effort |
|------|------|----------|------------|----------------------|
| ~~**2015**~~ | ~~OPEN_INTEREST~~ | ~~**High**~~ | ~~OI for BSE derivatives~~ | ✅ DONE |
| **2014** | CLOSE_PRICE | Medium | Previous close for % change calc | 2 hours |
| **2028** | IMPLIED_VOLATILITY | Medium | Greeks for options | 3 hours |
| **2002** | PRODUCT_STATE_CHANGE | Medium | Know when market opens/closes | 2 hours |
| **2034** | LIMIT_PRICE_PROTECTION | Low | Circuit limits (DPR) | 1 hour |

---

## Current Implementation Summary

### Fully Implemented

| Exchange | Critical Messages | Data Fields |
|----------|------------------|-------------|
| **NSE FO** | 7200, 7202, 7208 | 5-level depth, LTP, OHLC, Volume, OI |
| **NSE CM** | 7200, 7208 | 5-level depth (64-bit), LTP, OHLC, Volume |
| **BSE FO/CM** | 2020, 2021 | 5-level depth, LTP, OHLC, Volume |

### Gaps

| Exchange | Missing | Impact | Priority |
|----------|---------|--------|----------|
| **BSE** | ~~2015 (OI)~~ | ~~No OI display for BSE derivatives~~ | ✅ Done |
| **BSE** | 2014 (Close) | % change may be inaccurate | Medium |
| **NSE FO** | 7207 (Indices) | No index values | Low |
| **BSE** | 2028 (IV) | No Greeks for BSE options | Medium |

---

## Data Field Coverage

### XTS::Tick Population Status

| Field | NSE FO | NSE CM | BSE FO | BSE CM |
|-------|--------|--------|--------|--------|
| `lastTradedPrice` | ✅ | ✅ | ✅ | ✅ |
| `lastTradedQuantity` | ✅ | ✅ | ✅ | ✅ |
| `open` | ✅ | ✅ | ✅ | ✅ |
| `high` | ✅ | ✅ | ✅ | ✅ |
| `low` | ✅ | ✅ | ✅ | ✅ |
| `close` | ✅ | ✅ | ⚠️ Prev close | ⚠️ Prev close |
| `volume` | ✅ | ✅ | ✅ | ✅ |
| `openInterest` | ✅ (7202) | ✅ | ✅ (2015) | ✅ (2015) |
| `averagePrice` | ⚠️ Partial | ⚠️ Partial | ✅ | ✅ |
| `bidDepth[5]` | ✅ | ✅ | ✅ | ✅ |
| `askDepth[5]` | ✅ | ✅ | ✅ | ✅ |
| `totalBuyQuantity` | ✅ | ✅ | ⚠️ | ⚠️ |
| `totalSellQuantity` | ✅ | ✅ | ⚠️ | ⚠️ |

---

## Recommended Implementation Roadmap

### Phase 1: Critical (Done)
1. ✅ **BSE 2015 (Open Interest)** - Implemented!
2. **BSE 2014 (Close Price)** - Required for accurate % change

### Phase 2: Important
3. **BSE 2028 (Implied Volatility)** - Options analytics
4. **NSE FO 7207 (Indices)** - Index display

### Phase 3: Nice-to-have
5. **BSE 2002 (Product State)** - Session management
6. **BSE 2034 (Circuit Limits)** - DPR display

---

## Implementation Pattern

When adding a new message parser, follow this pattern:

### 1. Define Structure (header file)
```cpp
// In bse_protocol.h or similar
struct MS_OPEN_INTEREST_2015 {
    uint32_t messageType;   // 2015
    uint64_t token;
    int64_t openInterest;
    // ... fields from protocol doc
};
```

### 2. Create Parser Function
```cpp
// In parser file
void parse_message_2015(const uint8_t* data, size_t len) {
    if (len < sizeof(MS_OPEN_INTEREST_2015)) return;
    
    auto* msg = reinterpret_cast<const MS_OPEN_INTEREST_2015*>(data);
    // Parse and call callback
}
```

### 3. Register in Dispatcher
```cpp
// In bse_receiver.cpp
switch (messageType) {
    case 2015:
        parse_message_2015(data, len);
        break;
}
```

### 4. Update Callback to Fill XTS::Tick
```cpp
// In UdpBroadcastService.cpp
tick.openInterest = bseData.openInterest;
```
