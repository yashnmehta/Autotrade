# UDP Market Data Architecture

## Overview

This document describes the architecture for receiving, processing, distributing, and caching real-time market data from NSE and BSE exchange UDP multicast feeds.

---

## System Architecture

```
┌─────────────────────────────────────────────────────────────────────────────────────┐
│                              MARKET DATA ARCHITECTURE                                │
├─────────────────────────────────────────────────────────────────────────────────────┤
│                                                                                      │
│  ╔══════════════════════════════════════════════════════════════════════════════╗   │
│  ║                           LAYER 1: UDP COLLECTION                            ║   │
│  ╠══════════════════════════════════════════════════════════════════════════════╣   │
│  ║                                                                               ║   │
│  ║  ┌──────────────┐ ┌──────────────┐ ┌──────────────┐ ┌──────────────┐         ║   │
│  ║  │   NSE FO     │ │   NSE CM     │ │   BSE FO     │ │   BSE CM     │         ║   │
│  ║  │  Multicast   │ │  Multicast   │ │  Multicast   │ │  Multicast   │         ║   │
│  ║  │   Receiver   │ │   Receiver   │ │   Receiver   │ │   Receiver   │         ║   │
│  ║  │ 239.x.x.x    │ │ 239.x.x.x    │ │ 239.x.x.x    │ │ 239.x.x.x    │         ║   │
│  ║  │ :26001       │ │ :26002       │ │ :26003       │ │ :26004       │         ║   │
│  ║  └───────┬──────┘ └───────┬──────┘ └───────┬──────┘ └───────┬──────┘         ║   │
│  ║          │                │                │                │                ║   │
│  ║          │ Callback       │ Callback       │ Callback       │ Callback       ║   │
│  ║          └────────────────┴────────────────┴────────────────┘                ║   │
│  ║                                     │                                         ║   │
│  ╚═════════════════════════════════════╪═════════════════════════════════════════╝   │
│                                        │                                             │
│  ╔═════════════════════════════════════╪═════════════════════════════════════════╗   │
│  ║                    LAYER 2: NORMALIZATION & EMISSION                          ║   │
│  ╠═════════════════════════════════════╪═════════════════════════════════════════╣   │
│  ║                                     ▼                                          ║   │
│  ║           ┌───────────────────────────────────────────────┐                   ║   │
│  ║           │           UdpBroadcastService                 │                   ║   │
│  ║           │              (Singleton)                      │                   ║   │
│  ║           │                                               │                   ║   │
│  ║           │  • Normalizes exchange-specific structs       │                   ║   │
│  ║           │  • Converts to unified XTS::Tick             │                   ║   │
│  ║           │  • Populates 5-level depth arrays            │                   ║   │
│  ║           │  • Emits: tickReceived(XTS::Tick)            │                   ║   │
│  ║           └───────────────────────┬───────────────────────┘                   ║   │
│  ║                                   │                                            ║   │
│  ║                     UNIFIED SIGNAL: XTS::Tick                                 ║   │
│  ║                                   │                                            ║   │
│  ╚═══════════════════════════════════╪════════════════════════════════════════════╝   │
│                                      │                                               │
│           ┌──────────────────────────┼──────────────────────────┐                   │
│           │                          │                          │                   │
│           ▼                          ▼                          ▼                   │
│  ╔════════════════════╗  ╔═══════════════════════╗  ╔═══════════════════════╗       │
│  ║  LAYER 3: STORAGE  ║  ║ LAYER 3: DISTRIBUTION ║  ║  LAYER 3: DIRECT      ║       │
│  ╠════════════════════╣  ╠═══════════════════════╣  ╠═══════════════════════╣       │
│  ║                    ║  ║                        ║  ║                       ║       │
│  ║    PriceCache      ║  ║      MainWindow        ║  ║  Direct Subscribers   ║       │
│  ║   (Singleton)      ║  ║  (Routing to FeedHdlr) ║  ║  (SnapQuote, etc.)    ║       │
│  ║                    ║  ║                        ║  ║                       ║       │
│  ║ • Composite key    ║  ║ • Routes to FeedHandler║  ║ • Filter by token     ║       │
│  ║ • Latest tick/time ║  ║ • Debug logging        ║  ║ • Full depth access   ║       │
│  ║ • Stale cleanup    ║  ║                        ║  ║                       ║       │
│  ╚════════════════════╝  ╚═══════════════════════╝  ╚═══════════════════════╝       │
│                                      │                                               │
│  ╔═══════════════════════════════════╪════════════════════════════════════════════╗   │
│  ║                    LAYER 4: TOKEN-BASED PUB/SUB                               ║   │
│  ╠═══════════════════════════════════╪════════════════════════════════════════════╣   │
│  ║                                   ▼                                            ║   │
│  ║               ┌─────────────────────────────────────┐                         ║   │
│  ║               │           FeedHandler               │                         ║   │
│  ║               │            (Singleton)              │                         ║   │
│  ║               │                                     │                         ║   │
│  ║               │  • Composite key: segment|token     │                         ║   │
│  ║               │  • TokenPublisher per instrument    │                         ║   │
│  ║               │  • Qt signal/slot connections       │                         ║   │
│  ║               └─────────────────┬───────────────────┘                         ║   │
│  ║                                 │                                              ║   │
│  ║           ┌─────────────────────┼─────────────────────┐                       ║   │
│  ║           ▼                     ▼                     ▼                       ║   │
│  ║   TokenPublisher_1       TokenPublisher_2      TokenPublisher_N               ║   │
│  ║     (key: 2|49508)         (key: 1|12345)        (key: 12|9999)               ║   │
│  ║           │                     │                     │                       ║   │
│  ╚═══════════╪═════════════════════╪═════════════════════╪═══════════════════════╝   │
│              │                     │                     │                           │
│  ╔═══════════╪═════════════════════╪═════════════════════╪═══════════════════════╗   │
│  ║                        LAYER 5: CONSUMERS                                      ║   │
│  ╠═══════════╧═════════════════════╧═════════════════════╧═══════════════════════╣   │
│  ║                                                                                ║   │
│  ║  ┌────────────┐ ┌────────────┐ ┌────────────┐ ┌────────────┐ ┌────────────┐   ║   │
│  ║  │ MarketWatch│ │ SnapQuote  │ │ OptionChain│ │ Buy/Sell   │ │ OrderBook  │   ║   │
│  ║  │   Window   │ │   Window   │ │   Window   │ │  Window    │ │   Window   │   ║   │
│  ║  │            │ │            │ │            │ │            │ │            │   ║   │
│  ║  │ Uses:      │ │ Uses:      │ │ Uses:      │ │ Uses:      │ │ Uses:      │   ║   │
│  ║  │ FeedHandler│ │ Direct +   │ │ FeedHandler│ │ PriceCache │ │ FeedHandler│   ║   │
│  ║  │+ PriceCache│ │ PriceCache │ │            │ │            │ │            │   ║   │
│  ║  └────────────┘ └────────────┘ └────────────┘ └────────────┘ └────────────┘   ║   │
│  ║                                                                                ║   │
│  ╚════════════════════════════════════════════════════════════════════════════════╝   │
│                                                                                      │
└─────────────────────────────────────────────────────────────────────────────────────┘
```

---

## Component Details

### 1. UdpBroadcastService (Layer 1-2)

**File**: `include/services/UdpBroadcastService.h`

**Responsibilities**:
- Initialize and manage 4 UDP multicast receivers (NSE FO, NSE CM, BSE FO, BSE CM)
- Register callbacks with exchange-specific parsers
- Convert exchange-specific data to unified `XTS::Tick` structure
- Emit single unified `tickReceived(XTS::Tick)` signal

**Key Methods**:
```cpp
// Start all receivers
void start(const Config& config);

// Stop all receivers
void stop();

// Individual receiver control
bool startReceiver(ExchangeReceiver receiver, const std::string& ip, int port);
void stopReceiver(ExchangeReceiver receiver);
```

**Signal**:
```cpp
signals:
    void tickReceived(const XTS::Tick& tick);  // UNIFIED SIGNAL
```

---

### 2. XTS::Tick (Data Structure)

**File**: `include/api/XTSTypes.h`

The unified tick structure carries all market data:

```cpp
struct DepthLevel {
    double price = 0.0;
    int64_t quantity = 0;
    int orders = 0;
};

struct Tick {
    int exchangeSegment;           // 1=NSECM, 2=NSEFO, 11=BSECM, 12=BSEFO
    int64_t exchangeInstrumentID;  // Token
    
    // Price data
    double lastTradedPrice;
    int lastTradedQuantity;
    double open, high, low, close;
    double averagePrice;
    
    // Volume & OI
    int64_t volume;
    int64_t openInterest;
    
    // Best bid/ask (level 0)
    double bidPrice, askPrice;
    int bidQuantity, askQuantity;
    
    // 5-Level Market Depth
    DepthLevel bidDepth[5];
    DepthLevel askDepth[5];
    
    // Totals
    int totalBuyQuantity;
    int totalSellQuantity;
    
    // Timestamps for latency tracking
    int64_t lastUpdateTime;
    int64_t timestampUdpRecv;
    // ... more timing fields
};
```

---

### 3. FeedHandler (Layer 4)

**File**: `include/services/FeedHandler.h`

**Responsibilities**:
- Token-based publish/subscribe system
- Route ticks to only interested subscribers
- Minimize overhead with direct Qt signal/slot

**Key Pattern**:
```cpp
// Composite key = (segment << 32) | token
int64_t key = FeedHandler::makeKey(2, 49508);  // NSEFO token 49508

// Subscribe with exchange segment (recommended)
FeedHandler::instance().subscribe(2, 49508, this, &MyWindow::onTick);

// Legacy subscribe (all segments)
FeedHandler::instance().subscribe(49508, this, &MyWindow::onTick);
```

---

### 4. PriceCache (Layer 3)

**File**: `include/services/PriceCache.h`

**Responsibilities**:
- Store latest tick for each instrument
- Eliminate "0.00 flash" when opening new windows
- Thread-safe with shared_mutex

**Key Pattern**:
```cpp
// Update with composite key (recommended)
PriceCache::instance().updatePrice(2, 49508, tick);

// Get cached price
auto cached = PriceCache::instance().getPrice(2, 49508);
if (cached) {
    updateUI(*cached);  // No 0.00 flash!
}
```

---

## Connection Patterns

### Pattern A: High-Frequency (MarketWatch Rows)
```cpp
// MarketWatchWindow.cpp
void MarketWatchWindow::subscribeToToken(int segment, int token) {
    // Seed from cache first
    auto cached = PriceCache::instance().getPrice(segment, token);
    if (cached) m_model->updateTick(*cached);
    
    // Then subscribe for live updates
    FeedHandler::instance().subscribe(segment, token, this, &onTickUpdate);
}
```

### Pattern B: Single-Token Windows (SnapQuote)
```cpp
// Created in MainWindow::createSnapQuoteWindow()
connect(&UdpBroadcastService::instance(), &tickReceived, 
        snapWindow, &SnapQuoteWindow::onTickUpdate);

// SnapQuoteWindow::onTickUpdate()
void SnapQuoteWindow::onTickUpdate(const XTS::Tick& tick) {
    if (tick.exchangeInstrumentID != m_token) return;
    if (tick.exchangeSegment != m_segment) return;
    
    // Update all 5 depth levels
    for (int i = 0; i < 5; i++) {
        updateBidDepth(i+1, tick.bidDepth[i]);
        updateAskDepth(i+1, tick.askDepth[i]);
    }
}
```

### Pattern C: Initial Seed + Live (OrderBook)
```cpp
void OrderBookWindow::setInstrument(int segment, int token) {
    // Seed from cache
    auto cached = PriceCache::instance().getPrice(segment, token);
    if (cached) populateDepth(*cached);
    
    // Subscribe for live
    FeedHandler::instance().subscribe(segment, token, this, &onDepthUpdate);
}
```

---

## Message Code Coverage

### NSE FO (Implemented)

| Code | Name | Status | Data |
|------|------|--------|------|
| 7200 | BCAST_MBO_MBP_UPDATE | ✅ Parsed | 5-level depth, OHLC, Volume |
| 7201 | BCAST_MW_ROUND_ROBIN | ✅ Parsed | OI, Market levels |
| 7202 | BCAST_TICKER_AND_MKT_INDEX | ✅ Parsed | Fill data, OI |
| 7207 | BCAST_INDICES | ❌ Not parsed | Index values |
| 7208 | BCAST_ONLY_MBP | ✅ Parsed | 5-level depth |
| 7211 | BCAST_SPD_MBP_DELTA | ❌ Not parsed | Spread depth |

### NSE CM (Implemented)

| Code | Name | Status | Data |
|------|------|--------|------|
| 7200 | BCAST_MBO_MBP_UPDATE | ✅ Parsed | 5-level depth (64-bit qty) |
| 7201 | BCAST_MW_ROUND_ROBIN | ✅ Parsed | OI, Market levels |
| 7208 | BCAST_ONLY_MBP | ✅ Parsed | 5-level depth |

### BSE FO/CM (Implemented)

| Code | Name | Status | Data |
|------|------|--------|------|
| 2020 | Market Picture | ✅ Parsed | 5-level depth, OHLC, Volume |
| 2021 | Market Picture (Complex) | ✅ Parsed | Same as 2020 (64-bit token) |
| 2011/2012 | Index Change | ✅ Parsed | Index values |
| 2015 | Open Interest | ❌ Not parsed | OI for derivatives |
| 2014 | Close Price | ❌ Not parsed | Close price |

---

## Performance Characteristics

| Metric | Value |
|--------|-------|
| UDP receive → Tick emit | ~50µs |
| FeedHandler routing | ~70ns (1 subscriber) |
| PriceCache update | ~50ns |
| PriceCache read | ~30ns |
| Max throughput | 500k ticks/sec |

---

## Thread Safety

| Component | Lock Type | Notes |
|-----------|-----------|-------|
| UdpBroadcastService | None (Qt event) | Signals are thread-safe |
| FeedHandler | std::mutex | Protects subscription map |
| PriceCache | std::shared_mutex | Multiple readers, single writer |

---

## Future Enhancements

1. **Message Code Expansion**: Parse remaining message codes (7207, 7211, 2015, 2014)
2. **Compression**: BSE uses native compression for Market Picture
3. **Index Feed**: Dedicated index data handler
4. **Historical Cache**: Store N seconds of tick history
5. **Latency Dashboard**: Real-time latency visualization
