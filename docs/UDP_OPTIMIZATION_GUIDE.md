# UDP System Optimization Guide

**Version**: 1.0  
**Date**: February 6, 2026  
**Status**: Production Ready  

---

## Overview

This guide demonstrates how to use the optimized UDP system infrastructure implemented in Phases 0-4 of the UDP refactoring project. The new infrastructure provides:

- **Thread-safe snapshot API** for race-free price reads
- **Type-specific callbacks** for granular event handling
- **Enhanced callback signatures** with explicit exchange segment and message type
- **Greeks per-feed policy** with configurable calculation mode
- **UI optimization infrastructure** with type-specific signals

---

## 1. Reading Price Data (Thread-Safe)

### ❌ OLD WAY (Race Condition Risk)
```cpp
// UNSAFE: Direct pointer access without locking
auto* state = nsefo::g_nseFoPriceStore.getUnifiedState(token);
if (state) {
    double ltp = state->ltp;  // Might be torn read!
    double bid = state->bids[0].price;  // Might change mid-read!
}
```

### ✅ NEW WAY (Thread-Safe Snapshot)
```cpp
// SAFE: Returns atomic copy under lock
auto snapshot = nsefo::g_nseFoPriceStore.getUnifiedSnapshot(token);
if (snapshot.token != 0) {
    double ltp = snapshot.ltp;  // Guaranteed consistent
    double bid = snapshot.bids[0].price;  // All fields atomic
}
```

### Using PriceStoreGateway (Recommended)
```cpp
#include "data/PriceStoreGateway.h"

// Multi-exchange support with single API
auto snapshot = MarketData::PriceStoreGateway::instance()
    .getUnifiedSnapshot(exchangeSegment, token);

if (snapshot.token != 0) {
    updateUI(snapshot.ltp, snapshot.volume, snapshot.oi);
}
```

---

## 2. Optimizing UI Components

### Strategy 1: Filter by UpdateType (Quick Win)

**Use Case**: OptionChain doesn't need depth-only updates

```cpp
void OptionChainWindow::onTickUpdate(const UDP::MarketTick &tick) {
    // Skip depth-only updates (message 7208) - reduces load by 40%
    if (tick.updateType == UDP::UpdateType::DEPTH_UPDATE) {
        return;
    }
    
    // Process price changes (touchline, ticker, market watch)
    updateCell(tick);
}
```

### Strategy 2: Subscribe to Specific Signals

**Use Case**: DepthWindow only needs depth and trade updates

```cpp
void DepthWindow::setupSubscriptions() {
    auto& feedHandler = FeedHandler::instance();
    
    // Subscribe to depth updates only (not touchline)
    connect(&feedHandler, &FeedHandler::depthUpdateReceived,
            this, &DepthWindow::onDepthUpdate);
    
    // Subscribe to trade updates for LTP changes
    connect(&feedHandler, &FeedHandler::tradeUpdateReceived,
            this, &DepthWindow::onTradeUpdate);
}
```

### Strategy 3: Use validFlags for Partial Updates

**Use Case**: Process only changed fields

```cpp
void MarketWatchWindow::onTickUpdate(const UDP::MarketTick &tick) {
    // Check which fields are valid before accessing
    if (tick.validFlags & UDP::VALID_LTP) {
        updateLTP(tick.ltp);
    }
    
    if (tick.validFlags & UDP::VALID_VOLUME) {
        updateVolume(tick.volume);
    }
    
    if (tick.validFlags & UDP::VALID_DEPTH) {
        updateDepth(tick.bids, tick.asks);
    }
}
```

---

## 3. Configuring Greeks Calculation

### Config File (`configs/config.ini`)

```ini
[GREEKS_CALCULATION]
# Enable/disable Greeks calculation
enabled = true

# Risk-free rate (RBI repo rate as of Jan 2026)
risk_free_rate = 0.065

# Base price mode: "cash" (spot) or "future" (next expiry future)
base_price_mode = cash

# Auto-calculate Greeks on price updates
auto_calculate = true

# Calculate on every feed update (bypass throttling)
# false = Hybrid mode (immediate for liquid, timer for illiquid) - RECOMMENDED
# true  = Per-feed mode (calculate on EVERY update) - high CPU
calculate_on_every_feed = false

# Throttle (ms) - only used when calculate_on_every_feed = false
throttle_ms = 100

# IV solver settings
iv_initial_guess = 0.20
iv_tolerance = 0.000001
iv_max_iterations = 100
```

### Choosing the Right Mode

| Mode | Use Case | CPU Impact | Accuracy |
|------|----------|------------|----------|
| **Hybrid** (`false`) | Normal trading (100-500 options) | Low | High for liquid options |
| **Per-Feed** (`true`) | Market making (real-time Greeks) | High | Highest for all options |

---

## 4. Message Type Reference

### UpdateType Enum

```cpp
enum class UpdateType : uint8_t {
    TRADE_TICK = 0,      // LTP, volume, OI change (7202, 17202)
    DEPTH_UPDATE = 1,    // Order book only (7208)
    TOUCHLINE = 2,       // BBO + basic stats (7200)
    MARKET_WATCH = 3,    // Comprehensive data (7201, 17201)
    OI_CHANGE = 4,       // OI-only update
    FULL_SNAPSHOT = 5,   // Complete state refresh
    CIRCUIT_LIMIT = 6,   // Circuit limit update (7220)
    UNKNOWN = 255
};
```

### Message Code Mapping

| Code | Name | UpdateType | Valid Fields | Greeks Trigger |
|------|------|-----------|--------------|----------------|
| 7200 | Touchline | TOUCHLINE | LTP, BID, ASK, OHLC, Volume, Close | ✅ Yes |
| 7202 | Ticker | TRADE_TICK | LTP, Volume, OI | ✅ Yes |
| 7208 | Market Depth | DEPTH_UPDATE | Bids, Asks (5 levels) | ❌ No |
| 7201 | Market Watch | MARKET_WATCH | All fields | ✅ Yes |
| 7220 | Circuit Limit | CIRCUIT_LIMIT | LTP | ❌ No |
| 17201 | Enhanced Market Watch | MARKET_WATCH | All + 3-level depth | ✅ Yes |
| 17202 | Enhanced Ticker | TRADE_TICK | LTP, Volume, OI | ✅ Yes |

---

## 5. Creating Custom Callbacks

### Example: Custom Price Alert System

```cpp
class PriceAlertSystem : public QObject {
    Q_OBJECT

public:
    void setupAlerts() {
        auto& feedHandler = FeedHandler::instance();
        
        // Subscribe only to trade updates (price changes)
        connect(&feedHandler, &FeedHandler::tradeUpdateReceived,
                this, &PriceAlertSystem::onPriceUpdate);
    }

private slots:
    void onPriceUpdate(const UDP::MarketTick& tick) {
        // Skip if not our token
        if (tick.token != m_watchToken) return;
        
        // Check alert conditions
        if (tick.ltp >= m_targetPrice) {
            emit alertTriggered(tick.token, tick.ltp, "Target Hit");
        }
    }

signals:
    void alertTriggered(uint32_t token, double price, QString reason);

private:
    uint32_t m_watchToken = 0;
    double m_targetPrice = 0.0;
};
```

---

## 6. Performance Best Practices

### ✅ DO

1. **Use snapshot API** for all price reads
   ```cpp
   auto snap = store.getUnifiedSnapshot(token);
   ```

2. **Filter early** by updateType to reduce processing
   ```cpp
   if (tick.updateType == DEPTH_UPDATE) return;
   ```

3. **Check validFlags** before accessing fields
   ```cpp
   if (tick.validFlags & VALID_LTP) { /* ... */ }
   ```

4. **Use type-specific signals** for targeted subscriptions
   ```cpp
   connect(&feed, &FeedHandler::tradeUpdateReceived, /* ... */);
   ```

### ❌ DON'T

1. **Don't use direct pointer access**
   ```cpp
   auto* state = store.getUnifiedState(token);  // UNSAFE!
   ```

2. **Don't process all events if you only need some**
   ```cpp
   // BAD: Processes depth updates unnecessarily
   void onTickUpdate(const UDP::MarketTick& tick) {
       updateCell(tick);  // Called for ALL update types
   }
   ```

3. **Don't access fields without checking validFlags**
   ```cpp
   updateVolume(tick.volume);  // Might be stale!
   ```

4. **Don't use `calculateOnEveryFeed=true` unless necessary**
   - High CPU usage with 1000+ options
   - Hybrid mode is sufficient for most use cases

---

## 7. Migration Checklist

### Migrating from Old API

- [ ] Replace `getUnifiedState()` with `getUnifiedSnapshot()`
- [ ] Update callbacks to accept `(token, exchangeSegment, messageType)`
- [ ] Add updateType filtering where appropriate
- [ ] Check validFlags before accessing fields
- [ ] Remove hardcoded exchange segment IDs (2, 1, 12, 11)
- [ ] Configure Greeks policy in config.ini
- [ ] Subscribe to type-specific signals if needed
- [ ] Test with live UDP feeds
- [ ] Benchmark performance improvements

---

## 8. Troubleshooting

### Issue: Greeks not calculating

**Check**:
1. `enabled = true` in config.ini
2. `auto_calculate = true` in config.ini
3. Contract has valid underlying price
4. Option not expired
5. Market price > 0 (or LTP checks removed)

**Debug**:
```cpp
auto& greeksService = GreeksCalculationService::instance();
qDebug() << "Enabled:" << greeksService.isEnabled();
qDebug() << "Auto:" << greeksService.config().autoCalculate;
```

### Issue: UpdateType always UNKNOWN

**Check**:
1. Using Phase 2 callbacks (signature includes messageType)
2. Converters set `tick.updateType` correctly
3. Parser dispatches with correct messageType parameter

**Debug**:
```cpp
qDebug() << "UpdateType:" << (int)tick.updateType;
qDebug() << "MessageType:" << tick.messageType;
qDebug() << "ValidFlags:" << tick.validFlags;
```

### Issue: Race conditions still occurring

**Verify**:
1. All `getUnifiedState()` calls replaced with `getUnifiedSnapshot()`
2. No direct pointer access to price store internal structures
3. Using snapshot API through PriceStoreGateway

---

## 9. Performance Metrics (Expected)

| Optimization | Expected Gain | Measurement |
|--------------|---------------|-------------|
| OptionChain depth filtering | 40-60% fewer updates | Count tick callbacks |
| MarketWatch touchline-only | 30-40% fewer updates | Count signal emissions |
| Snapshot API overhead | <5% slower reads | Microbenchmark |
| Greeks per-feed mode | 2-3x CPU vs hybrid | Profile under load |

---

## 10. Example: Complete Component Optimization

```cpp
// Complete example: Optimized option chain component

class OptimizedOptionChain : public QWidget {
    Q_OBJECT

public:
    void loadChain(int exchangeSegment, const QString& symbol) {
        m_exchangeSegment = exchangeSegment;
        
        // Unsubscribe old tokens
        FeedHandler::instance().unsubscribeAll(this);
        
        // Load contracts from repository
        auto contracts = m_repoManager->getNSEFORepository()->getOptionChain(
            symbol, m_selectedExpiry);
        
        // Subscribe to trade updates only (skip depth)
        for (const auto& contract : contracts) {
            FeedHandler::instance().subscribe(
                exchangeSegment, contract.token, 
                this, &OptimizedOptionChain::onTickUpdate);
        }
    }

private slots:
    void onTickUpdate(const UDP::MarketTick& tick) {
        // Early exit: Skip depth-only updates (40% reduction)
        if (tick.updateType == UDP::UpdateType::DEPTH_UPDATE) {
            return;
        }
        
        // Thread-safe price read
        auto snapshot = MarketData::PriceStoreGateway::instance()
            .getUnifiedSnapshot(m_exchangeSegment, tick.token);
        
        if (snapshot.token == 0) return;
        
        // Check valid fields before using
        if (tick.validFlags & UDP::VALID_LTP) {
            updateLTP(tick.token, snapshot.ltp);
        }
        
        if (tick.validFlags & UDP::VALID_VOLUME) {
            updateVolume(tick.token, snapshot.volume);
        }
        
        // Greeks are already calculated by service
        auto greeksResult = GreeksCalculationService::instance()
            .getCachedGreeks(tick.token, m_exchangeSegment);
        
        if (greeksResult.has_value()) {
            updateGreeks(tick.token, greeksResult.value());
        }
    }

private:
    int m_exchangeSegment = 0;
};
```

---

## Support

For implementation questions or issues:
- See `docs/UDP_REFACTORING_ROADMAP.md` for complete implementation details
- Check Phase 0-4 completion status and deliverables
- Review code examples in `src/services/UdpBroadcastService.cpp`

**Implementation Date**: February 6, 2026  
**Status**: Production Ready  
**Version**: 1.0
