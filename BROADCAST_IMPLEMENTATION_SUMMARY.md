# Broadcast System - Implementation Summary
## Current Status & Action Items

---

## ✅ What's Working

### 1. **NSE FO + NSE CM Dual Broadcast** ✅
**Status**: Fully implemented and integrated

**Components**:
- `nsefo::MulticastReceiver` - Running in separate thread
- `nsecm::MulticastReceiver` - Running in separate thread
- Callback registries for both exchanges
- MainWindow integration complete

**Data Flow**:
```
UDP → MulticastReceiver → Parser → Callback → MainWindow → FeedHandler → Market Watch
```

### 2. **FeedHandler (Central Hub)** ✅
**Status**: Fully functional

**Features**:
- Token-based publish/subscribe
- Thread-safe
- Supports multiple subscribers per token
- Used by Market Watch Window

### 3. **Market Watch Window** ✅
**Status**: Subscribed to broadcast

**Implementation**:
```cpp
FeedHandler::instance().subscribe(token, this, &MarketWatchWindow::onTickUpdate);
```

**Works for**: NSE FO, NSE CM (currently active)

---

## ⚠️ Partially Implemented

### 1. **BSE FO Broadcast** ⚠️
**Status**: Library exists, needs integration

**What's Done**:
- ✅ `lib/cpp_broadcast_bsefo/` library
- ✅ `bse_receiver.cpp` with empirically verified offsets
- ✅ Protocol parsing

**What's Missing**:
- ❌ Callback system (no `bse_callback.h`)
- ❌ MainWindow integration
- ❌ Thread management
- ❌ Registry pattern

**Action Required**:
1. Create `bse_callback.h` with namespace `bsefo::`
2. Implement callback registry
3. Add to MainWindow::startBroadcastReceiver()
4. Start separate thread

---

## ❌ Not Implemented

### 1. **BSE CM Broadcast** ❌
**Status**: Library doesn't exist

**Required**:
- Create `lib/cpp_broadcast_bsecm/`
- Implement protocol parsers
- Create callback system
- Integrate with MainWindow

**Estimated Effort**: 4-6 hours

### 2. **Snap Quote Broadcast Integration** ❌
**Status**: Window exists, no broadcast subscription

**Current State**:
- SnapQuoteWindow shows static data
- No real-time updates
- No FeedHandler subscription

**Action Required**:
```cpp
// In SnapQuoteWindow.cpp
void SnapQuoteWindow::setInstrument(const InstrumentData& instrument) {
    // Subscribe to broadcast
    FeedHandler::instance().subscribe(
        instrument.token,
        this,
        &SnapQuoteWindow::onTickUpdate
    );
}

void SnapQuoteWindow::onTickUpdate(const XTS::Tick& tick) {
    // Update UI with real-time data
    updateLTP(tick.lastTradedPrice);
    updateVolume(tick.volume);
    updateDepth(tick);
}
```

### 3. **Net Position Periodic Updates** ❌
**Status**: No price updates

**Recommended Approach**: Periodic updates from PriceCache (500ms)

**Implementation**:
```cpp
// In PositionWindow.cpp
QTimer *m_updateTimer = new QTimer(this);
connect(m_updateTimer, &QTimer::timeout, this, &PositionWindow::updatePnL);
m_updateTimer->start(500); // 500ms

void PositionWindow::updatePnL() {
    for (auto& position : m_positions) {
        XTS::Tick tick = PriceCache::instance().getPrice(position.token);
        double mtm = (tick.lastTradedPrice - position.avgPrice) * position.quantity;
        updatePositionRow(position.token, mtm);
    }
}
```

---

## 📊 Broadcast Coverage Matrix

| Exchange | Segment | Broadcast Lib | Callback System | MainWindow Integration | Market Watch | Snap Quote | Net Position |
|----------|---------|---------------|-----------------|------------------------|--------------|------------|--------------|
| NSE | F&O | ✅ | ✅ | ✅ | ✅ | ❌ | ❌ |
| NSE | CM | ✅ | ✅ | ✅ | ✅ | ❌ | ❌ |
| BSE | F&O | ✅ | ❌ | ❌ | ❌ | ❌ | ❌ |
| BSE | CM | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ |

---

## 🎯 Priority Action Items

### **High Priority** (Core Functionality)

1. **Integrate BSE FO Broadcast** ⚠️
   - Create callback system
   - Add to MainWindow
   - Test with BSE instruments
   - **Estimated Time**: 2-3 hours

2. **Add Snap Quote Broadcast Subscription** ⏳
   - Subscribe to FeedHandler
   - Implement onTickUpdate()
   - Update UI in real-time
   - **Estimated Time**: 1 hour

### **Medium Priority** (Enhanced Features)

3. **Implement Net Position Periodic Updates** ⏳
   - Add 500ms QTimer
   - Fetch from PriceCache
   - Calculate P&L
   - **Estimated Time**: 1 hour

4. **Create BSE CM Broadcast Library** ❌
   - New library structure
   - Protocol implementation
   - Callback system
   - **Estimated Time**: 4-6 hours

### **Low Priority** (Optional)

5. **Add Currency Derivatives** (NSE CD, BSE CD)
6. **Add MCX Commodities**

---

## 🔧 Quick Implementation Guide

### Task 1: BSE FO Integration

**Step 1: Create Callback Header**
```bash
# Create lib/cpp_broadcast_bsefo/include/bse_callback.h
```

**Step 2: Add Namespace and Registry**
```cpp
namespace bsefo {
    struct TouchlineData { /* ... */ };
    struct MarketDepthData { /* ... */ };
    
    class MarketDataCallbackRegistry {
    public:
        static MarketDataCallbackRegistry& instance();
        void registerTouchlineCallback(std::function<void(const TouchlineData&)> cb);
        void registerMarketDepthCallback(std::function<void(const MarketDepthData&)> cb);
    };
}
```

**Step 3: Update MainWindow**
```cpp
// Add member variables
std::unique_ptr<bsefo::MulticastReceiver> m_udpReceiverBSEFO;
std::thread m_udpThreadBSEFO;

// In startBroadcastReceiver()
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

m_udpThreadBSEFO = std::thread([this]() {
    if (m_udpReceiverBSEFO) m_udpReceiverBSEFO->start();
});
```

### Task 2: Snap Quote Integration

**File**: `src/views/SnapQuoteWindow/SnapQuoteWindow.cpp`

**Add**:
```cpp
#include "services/FeedHandler.h"

// In setInstrument()
FeedHandler::instance().subscribe(instrument.token, this, &SnapQuoteWindow::onTickUpdate);

// Add slot
void SnapQuoteWindow::onTickUpdate(const XTS::Tick& tick) {
    m_ltpLabel->setText(QString::number(tick.lastTradedPrice, 'f', 2));
    m_volumeLabel->setText(QString::number(tick.volume));
    m_bidLabel->setText(QString::number(tick.bidPrice, 'f', 2));
    m_askLabel->setText(QString::number(tick.askPrice, 'f', 2));
    // Update depth levels...
}

// In destructor
FeedHandler::instance().unsubscribeAll(this);
```

### Task 3: Net Position Updates

**File**: `src/views/PositionWindow.cpp`

**Add**:
```cpp
// In constructor
m_updateTimer = new QTimer(this);
connect(m_updateTimer, &QTimer::timeout, this, &PositionWindow::updatePnL);
m_updateTimer->start(500);

// Add method
void PositionWindow::updatePnL() {
    for (int row = 0; row < m_model->rowCount(); ++row) {
        int token = m_model->data(m_model->index(row, COL_TOKEN)).toInt();
        double avgPrice = m_model->data(m_model->index(row, COL_AVG_PRICE)).toDouble();
        int quantity = m_model->data(m_model->index(row, COL_QUANTITY)).toInt();
        
        XTS::Tick tick = PriceCache::instance().getPrice(token);
        double mtm = (tick.lastTradedPrice - avgPrice) * quantity;
        
        m_model->setData(m_model->index(row, COL_MTM), mtm);
    }
}
```

---

## 📝 Testing Checklist

### NSE FO + NSE CM (Current)
- [ ] Both receivers start successfully
- [ ] Logs show tick counts for both exchanges
- [ ] Market Watch updates in real-time
- [ ] No crashes or memory leaks
- [ ] Latency is acceptable (<10ms)

### BSE FO (After Integration)
- [ ] BSE FO receiver starts
- [ ] BSE instruments update in Market Watch
- [ ] Callback system works correctly
- [ ] Thread management is stable

### Snap Quote (After Integration)
- [ ] Real-time LTP updates
- [ ] Volume updates
- [ ] Depth updates (5 levels)
- [ ] Works for all exchanges

### Net Position (After Integration)
- [ ] P&L updates every 500ms
- [ ] Calculations are correct
- [ ] No performance impact
- [ ] UI remains responsive

---

## 🚀 Next Steps

1. **Test Current NSE Implementation**
   - Run the app
   - Check logs for both NSE FO and NSE CM
   - Verify Market Watch updates

2. **Implement BSE FO Integration**
   - Create callback system
   - Add to MainWindow
   - Test with BSE instruments

3. **Add Snap Quote Subscription**
   - Implement FeedHandler subscription
   - Test real-time updates

4. **Add Net Position Timer**
   - Implement 500ms periodic updates
   - Test P&L calculations

5. **Consider BSE CM** (if needed)
   - Assess requirements
   - Create library if necessary

---

*Ready for implementation! Let me know which task to start with.* 🚀
