# Broadcast System Architecture Analysis
## Current Implementation & Requirements

---

## 📊 Current Broadcast Implementation Status

### ✅ Implemented Exchanges

| Exchange | Segment | Library | Status | Callback Namespace |
|----------|---------|---------|--------|-------------------|
| NSE | F&O | `cpp_broacast_nsefo` | ✅ Active | `nsefo::` |
| NSE | CM | `cpp_broadcast_nsecm` | ✅ Active | `nsecm::` |
| BSE | F&O | `cpp_broadcast_bsefo` | ⚠️ Partial | None (needs integration) |
| BSE | CM | - | ❌ Missing | - |

---

## 🏗️ Architecture Overview

### 1. **FeedHandler (Central Hub)**
**Location**: `src/services/FeedHandler.cpp`

**Role**: Token-based publish/subscribe system
- Singleton pattern
- Thread-safe with mutex
- Creates `TokenPublisher` per instrument token
- Distributes ticks to all subscribers

**Key Methods**:
```cpp
void subscribe(int token, QObject* receiver, Slot slot)
void unsubscribe(int token, QObject* receiver)
void onTickReceived(const XTS::Tick& tick)  // Entry point from broadcast
```

### 2. **Broadcast Receivers**
**Location**: `lib/cpp_broacast_nsefo/`, `lib/cpp_broadcast_nsecm/`

**Architecture**:
- **MulticastReceiver**: UDP socket management
- **UdpReceiver**: Packet decompression (LZO) and parsing
- **Parsers**: Message-specific parsers (7200, 7201, 7202, etc.)
- **Callbacks**: Registry pattern for data distribution

**Data Flow**:
```
UDP Multicast → MulticastReceiver → UdpReceiver → Parser → Callback → MainWindow → FeedHandler → Subscribers
```

### 3. **MainWindow Integration**
**Location**: `src/app/MainWindow/MainWindow.cpp`

**Current Setup** (from your changes):
```cpp
// NSE FO Receiver
std::unique_ptr<nsefo::MulticastReceiver> m_udpReceiver;
std::thread m_udpThread;

// NSE CM Receiver  
std::unique_ptr<nsecm::MulticastReceiver> m_udpReceiverCM;
std::thread m_udpThreadCM;
```

**Callbacks Registered**:
- `nsefo::MarketDataCallbackRegistry::instance().registerTouchlineCallback(...)`
- `nsefo::MarketDataCallbackRegistry::instance().registerMarketDepthCallback(...)`
- `nsefo::MarketDataCallbackRegistry::instance().registerTickerCallback(...)`
- Same for `nsecm::`

---

## 📋 Windows Requiring Broadcast Data

### 1. **Market Watch Window** ✅
**Current Implementation**: 
```cpp
FeedHandler::instance().subscribe(token, this, &MarketWatchWindow::onTickUpdate);
```

**Data Required**:
- LTP, Volume, OHLC
- Bid/Ask prices
- Total Buy/Sell quantity
- All exchanges: NSE FO, NSE CM, BSE FO, BSE CM

### 2. **Snap Quote Window** ✅
**Expected Implementation**: Similar to Market Watch
```cpp
FeedHandler::instance().subscribe(token, this, &SnapQuoteWindow::onTickUpdate);
```

**Data Required**:
- Full market depth (5 levels)
- LTP, Volume, OHLC
- OI (for derivatives)
- All exchanges

### 3. **Net Position Window** (Optional)
**Two Approaches**:

**Option A: Direct Broadcast** (Real-time)
```cpp
FeedHandler::instance().subscribe(token, this, &PositionWindow::onTickUpdate);
```

**Option B: Periodic Update** (Your suggestion - 500ms from PriceCache)
```cpp
QTimer *m_updateTimer = new QTimer(this);
connect(m_updateTimer, &QTimer::timeout, this, &PositionWindow::updateFromCache);
m_updateTimer->start(500); // 500ms
```

**Recommendation**: Use **Option B** (Periodic from PriceCache)
- Less CPU overhead
- Sufficient for P&L updates
- Positions don't need tick-by-tick updates

---

## 🔧 Required Implementation Tasks

### Task 1: Integrate BSE FO Broadcast ⚠️

**Current State**:
- Library exists: `lib/cpp_broadcast_bsefo/`
- Has `bse_receiver.cpp` (you updated with empirical offsets)
- **Missing**: Callback integration like NSE

**Steps**:
1. Create `bse_callback.h` (similar to `nsefo_callback.h`)
2. Add namespace `bsefo::`
3. Implement callback registry
4. Add to MainWindow:
   ```cpp
   std::unique_ptr<bsefo::MulticastReceiver> m_udpReceiverBSEFO;
   std::thread m_udpThreadBSEFO;
   ```

### Task 2: Create BSE CM Broadcast ❌

**Required**:
- New library: `lib/cpp_broadcast_bsecm/`
- Copy structure from `cpp_broadcast_nsecm/`
- Adapt protocol for BSE CM
- Implement parsers
- Add callback system

**Estimated Effort**: 4-6 hours (if protocol docs available)

### Task 3: Update MainWindow::startBroadcastReceiver()

**Add BSE FO & BSE CM** similar to NSE:
```cpp
// BSE FO Receiver
m_udpReceiverBSEFO = std::make_unique<bsefo::MulticastReceiver>(bseFoIp, bseFoPort);
if (m_udpReceiverBSEFO->isValid()) {
    bsefo::MarketDataCallbackRegistry::instance().registerTouchlineCallback([this](const bsefo::TouchlineData& data) {
        XTS::Tick tick;
        tick.exchangeSegment = 4; // BSEFO
        tick.exchangeInstrumentID = data.token;
        // ... populate tick
        QMetaObject::invokeMethod(this, "onUdpTickReceived", Qt::QueuedConnection, Q_ARG(XTS::Tick, tick));
    });
}

// BSE CM Receiver (similar structure)
```

### Task 4: Snap Quote Window Integration

**Location**: `src/views/SnapQuoteWindow.cpp`

**Add Subscription**:
```cpp
void SnapQuoteWindow::setInstrument(const InstrumentData& instrument) {
    m_instrument = instrument;
    
    // Subscribe to broadcast updates
    FeedHandler::instance().subscribe(
        instrument.token, 
        this, 
        &SnapQuoteWindow::onTickUpdate
    );
}

void SnapQuoteWindow::onTickUpdate(const XTS::Tick& tick) {
    // Update UI with tick data
    updateLTP(tick.lastTradedPrice);
    updateVolume(tick.volume);
    updateDepth(tick);
}
```

### Task 5: Net Position Window (Periodic Update)

**Location**: `src/views/PositionWindow.cpp`

**Implementation**:
```cpp
// In constructor
m_updateTimer = new QTimer(this);
connect(m_updateTimer, &QTimer::timeout, this, &PositionWindow::updatePnL);
m_updateTimer->start(500); // 500ms updates

void PositionWindow::updatePnL() {
    for (auto& position : m_positions) {
        // Get latest price from cache
        XTS::Tick tick = PriceCache::instance().getPrice(position.token);
        
        // Calculate P&L
        double mtm = (tick.lastTradedPrice - position.avgPrice) * position.quantity;
        
        // Update UI
        updatePositionRow(position.token, mtm);
    }
}
```

---

## 📊 Exchange Segment Mapping

```cpp
enum ExchangeSegment {
    NSECM = 1,   // NSE Cash
    NSEFO = 2,   // NSE F&O
    NSECD = 3,   // NSE Currency Derivatives
    BSEFO = 4,   // BSE F&O
    BSECM = 5,   // BSE Cash
    BSECD = 6,   // BSE Currency Derivatives
    MCXFO = 7    // MCX F&O
};
```

---

## 🎯 Priority Implementation Order

1. **High Priority** (Core Functionality):
   - ✅ NSE FO Broadcast (Done)
   - ✅ NSE CM Broadcast (Done)
   - ⚠️ BSE FO Broadcast Integration (Needs callback system)
   - ✅ Market Watch subscription (Done)

2. **Medium Priority** (Enhanced Features):
   - ⏳ Snap Quote broadcast subscription
   - ⏳ Net Position periodic updates
   - ❌ BSE CM Broadcast (New library needed)

3. **Low Priority** (Optional):
   - Currency derivatives (NSE CD, BSE CD)
   - MCX commodities

---

## 🔍 Current Broadcast Flow

```
┌─────────────────────────────────────────────────────────────┐
│                    UDP Multicast Feeds                       │
│  NSE FO (233.1.2.5:34330)  NSE CM (233.1.2.5:8222)         │
│  BSE FO (239.1.2.5:26001)  BSE CM (239.1.2.5:26002)         │
└──────────────────┬──────────────────────────────────────────┘
                   │
                   ▼
┌─────────────────────────────────────────────────────────────┐
│              MulticastReceiver (per exchange)                │
│  - Socket management                                         │
│  - Packet buffering                                          │
└──────────────────┬──────────────────────────────────────────┘
                   │
                   ▼
┌─────────────────────────────────────────────────────────────┐
│                   UdpReceiver                                │
│  - LZO decompression                                         │
│  - Transaction code detection                                │
└──────────────────┬──────────────────────────────────────────┘
                   │
                   ▼
┌─────────────────────────────────────────────────────────────┐
│              Message Parsers (7200, 7201, etc.)              │
│  - Parse binary protocol                                     │
│  - Create TouchlineData / MarketDepthData / TickerData       │
└──────────────────┬──────────────────────────────────────────┘
                   │
                   ▼
┌─────────────────────────────────────────────────────────────┐
│           MarketDataCallbackRegistry                         │
│  - Invoke registered callbacks                               │
└──────────────────┬──────────────────────────────────────────┘
                   │
                   ▼
┌─────────────────────────────────────────────────────────────┐
│              MainWindow::onUdpTickReceived()                 │
│  - Convert to XTS::Tick                                      │
│  - Add latency timestamps                                    │
└──────────────────┬──────────────────────────────────────────┘
                   │
                   ▼
┌─────────────────────────────────────────────────────────────┐
│                   FeedHandler                                │
│  - Token-based routing                                       │
│  - Publish to all subscribers                                │
└──────────────────┬──────────────────────────────────────────┘
                   │
                   ▼
┌─────────────────────────────────────────────────────────────┐
│         Subscriber Windows (Market Watch, Snap Quote)        │
│  - Receive tick updates via Qt signals                       │
│  - Update UI                                                 │
└─────────────────────────────────────────────────────────────┘
```

---

## 📝 Next Steps

1. **Test Current NSE FO + NSE CM Implementation**
   - Verify both receivers are running
   - Check logs for tick counts
   - Test Market Watch updates

2. **Integrate BSE FO**
   - Create callback system
   - Add to MainWindow
   - Test with BSE instruments

3. **Implement Snap Quote Subscription**
   - Add FeedHandler subscription
   - Handle tick updates
   - Update depth display

4. **Add Net Position Periodic Updates**
   - Implement 500ms timer
   - Fetch from PriceCache
   - Calculate and display P&L

5. **Consider BSE CM** (if required)
   - Assess protocol availability
   - Create new library
   - Integrate similar to NSE CM

---

*Analysis complete. Ready for implementation!* 🚀
