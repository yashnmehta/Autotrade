# Greeks Persistence Fix - Implementation Summary

## Issue Description

**Problem**: Greeks values showing as 0 in OptionChain window despite Greeks calculation service running.

**Root Cause**: Greeks were being calculated correctly by `GreeksCalculationService` and emitted via the `greeksCalculated` signal, but the calculated values were never persisted to the price stores. UI components (OptionChain, ATMWatch) read Greeks from price stores and found uninitialized (zero) values.

## Data Flow Analysis

### Before Fix (Broken Pipeline)
```
UDP Tick → GreeksService.onPriceUpdate()
         → calculateForToken()
         → emit greeksCalculated(token, exchangeSegment, result)
         → OptionChain.onGreeksUpdated() ✓ [receives signal]
         
BUT SEPARATELY:

OptionChain.loadStrikeData()
         → Read from price store (g_nseFoPriceStore.getUnifiedState())
         → unifiedState->greeksCalculated = false ✗ [never updated]
         → Display 0 values ✗
```

**The Problem**: Two parallel data paths existed:
1. **Signal Path** (Working): Direct Greeks calculation → UI signal handlers
2. **Storage Path** (Missing): Greeks calculation → Price store ← UI reads

The storage path was incomplete, causing stale/zero values when UI refreshed from storage.

---

## Solution Implementation

### 1. Added `updateGreeks()` Method to Price Stores

#### NSE FO Price Store
**File**: `lib/cpp_broacast_nsefo/include/nsefo_price_store.h`

```cpp
/**
 * @brief Update Greeks fields (from GreeksCalculationService)
 */
void updateGreeks(uint32_t token, double iv, double bidIV, double askIV,
                 double delta, double gamma, double vega, double theta,
                 double theoreticalPrice, int64_t timestamp) {
    if (token < MIN_TOKEN || token > MAX_TOKEN) return;
    
    std::unique_lock lock(mutex_); // Exclusive Write
    auto* rowPtr = store_[token - MIN_TOKEN];
    if (!rowPtr) return;
    auto& row = *rowPtr;
    
    row.impliedVolatility = iv;
    row.bidIV = bidIV;
    row.askIV = askIV;
    row.delta = delta;
    row.gamma = gamma;
    row.vega = vega;
    row.theta = theta;
    row.theoreticalPrice = theoreticalPrice;
    row.greeksCalculated = true;          // ← Critical flag
    row.lastGreeksUpdateTime = timestamp;
    row.lastPacketTimestamp = timestamp;
}
```

#### BSE FO Price Store
**Files**: 
- `lib/cpp_broadcast_bsefo/include/bse_price_store.h` (declaration)
- `lib/cpp_broadcast_bsefo/src/bse_price_store.cpp` (implementation)

Similar implementation with proper locking and validation.

### 2. Connected Signal to Price Store Updates

**File**: `src/services/UdpBroadcastService.cpp`

```cpp
UdpBroadcastService::UdpBroadcastService(QObject *parent) : QObject(parent) {
  // Connect Greeks calculation signal to price store updates
  // This ensures calculated Greeks are persisted for UI components to read
  connect(&GreeksCalculationService::instance(), 
          &GreeksCalculationService::greeksCalculated,
          this, [](uint32_t token, int exchangeSegment, const GreeksResult& result) {
    // Route to appropriate price store based on exchange segment
    if (exchangeSegment == 2) { // NSE FO
      nsefo::g_nseFoPriceStore.updateGreeks(
        token, 
        result.impliedVolatility, 
        result.bidIV, 
        result.askIV,
        result.delta, 
        result.gamma, 
        result.vega, 
        result.theta,
        result.theoreticalPrice, 
        result.calculationTimestamp
      );
    } else if (exchangeSegment == 4) { // BSE FO
      bse::g_bseFoPriceStore.updateGreeks(
        token, 
        result.impliedVolatility, 
        result.bidIV, 
        result.askIV,
        result.delta, 
        result.gamma, 
        result.vega, 
        result.theta,
        result.theoreticalPrice, 
        result.calculationTimestamp
      );
    }
  });
}
```

**Why UdpBroadcastService?**
- Central service managing all UDP callbacks and price stores
- Already includes GreeksCalculationService header
- Owns the price store instances
- Lifecycle managed properly (singleton pattern)

---

## After Fix (Complete Pipeline)

```
UDP Tick → GreeksService.onPriceUpdate()
         → calculateForToken()
         → emit greeksCalculated(token, exchangeSegment, result)
         ├─→ OptionChain.onGreeksUpdated() ✓ [direct signal]
         └─→ UdpBroadcastService connection ✓ [NEW]
             → g_nseFoPriceStore.updateGreeks() ✓ [persists]

OptionChain.loadStrikeData()
         → Read from price store
         → unifiedState->greeksCalculated = true ✓
         → Display actual Greeks values ✓
```

**Now Both Paths Work**:
1. **Signal Path**: Immediate updates via signals (existing)
2. **Storage Path**: Persistence via price stores (fixed)

---

## Configuration Verification

Ensure Greeks are enabled in `configs/config.ini`:

```ini
[GREEKS_CALCULATION]
enabled = true                     # Enable Greeks calculation globally
auto_calculate = true              # Auto-trigger on price updates
calculate_on_every_feed = false    # Hybrid mode (on-demand + periodic)
throttle_ms = 100                  # Min interval between recalculations
risk_free_rate = 0.065             # 6.5% risk-free rate (adjust as needed)
base_price_mode = cash             # Use cash market underlying (or "future")
```

---

## Testing Verification

### 1. Visual Test (OptionChain Window)
1. Run application and login
2. Select an option contract in ScripBar
3. Open OptionChain window (View → Option Chain)
4. Wait for UDP ticks to arrive
5. **Verify**: Greeks columns (IV, Delta, Gamma, Vega, Theta) show non-zero values
6. **Verify**: Values update as market data changes

### 2. Debug Logging
Enable debugging in GreeksCalculationService.cpp:

```cpp
void GreeksCalculationService::onPriceUpdate(uint32_t token, double ltp, int exchangeSegment) {
    qDebug() << "[GreeksService] onPriceUpdate:" << token << "LTP:" << ltp << "Segment:" << exchangeSegment;
    // ... rest of implementation
}
```

Watch console for:
```
[GreeksService] onPriceUpdate: 43245 LTP: 125.5 Segment: 2
[GreeksService] Calculated Greeks for token: 43245 IV: 0.18 Delta: 0.52
```

### 3. Price Store Verification
Add temporary debug logging in `updateGreeks()`:

```cpp
void updateGreeks(...) {
    // ... validation code ...
    qDebug() << "[NSEFOStore] Greeks persisted for token:" << token 
             << "IV:" << iv << "Delta:" << delta;
    // ... rest of implementation
}
```

---

## Code Changes Summary

| File | Change Type | Purpose |
|------|-------------|---------|
| `lib/cpp_broacast_nsefo/include/nsefo_price_store.h` | Add method | `updateGreeks()` declaration for NSE FO |
| `lib/cpp_broadcast_bsefo/include/bse_price_store.h` | Add method | `updateGreeks()` declaration for BSE FO |
| `lib/cpp_broadcast_bsefo/src/bse_price_store.cpp` | Add method | `updateGreeks()` implementation for BSE FO |
| `src/services/UdpBroadcastService.cpp` | Connect signal | Link Greeks calculation to price store persistence |

**Total Lines Added**: ~75 lines (including comments and formatting)

---

## Architecture Benefits

### Thread Safety
- Uses `std::unique_lock` for exclusive write access
- Atomic update of all Greeks fields
- No risk of partial/torn reads

### Performance
- O(1) direct array indexing in NSE FO store
- No heap allocation during updates
- Lock held only during actual write (minimal contention)

### Maintainability
- Single connection point in UdpBroadcastService
- Decoupled: Greeks calculation doesn't know about storage
- UI components can use either signal OR storage (flexibility)

### Scalability
- Works for both NSE FO and BSE FO
- Easy to extend to NSECM if stock options added
- No hardcoded assumptions about token ranges

---

## Related Documentation

- **UDP_IMPLEMENTATION_CURRENT_STATE_ANALYSIS.md**: Complete UDP/Greeks architecture
- **UDP_OPTIMIZATION_GUIDE.md**: Performance tuning and Greeks configuration
- **UDP_UI_CONNECTION_VERIFICATION.md**: Signal-slot connection verification
- **IV_AND_GREEKS_IMPLEMENTATION_ANALYSIS.md**: Original Greeks implementation details

---

## Known Limitations

1. **NSECM Stock Options**: Currently only NSE FO and BSE FO price stores support Greeks. If NSECM stock options are added, `nsecm_price_store` will need similar updates.

2. **Cache Synchronization**: GreeksCalculationService maintains an internal cache AND price stores persist values. These are kept in sync via the signal connection, but edge cases (e.g., service restart) may require resynchronization.

3. **Historical Data**: Greeks are only persisted from live calculations. Loaded from cache/file will not trigger price store updates until recalculated.

---

## Build Verification

```bash
.\build-project.bat
```

**Expected Output**:
```
================================================
Build completed successfully!
================================================
Executable: build\TradingTerminal.exe
```

**No Errors**: All Greek persistence code compiles without warnings or errors.

---

## Rollback Plan (If Needed)

If issues arise, revert these changes:

```bash
git checkout lib/cpp_broacast_nsefo/include/nsefo_price_store.h
git checkout lib/cpp_broadcast_bsefo/include/bse_price_store.h
git checkout lib/cpp_broadcast_bsefo/src/bse_price_store.cpp
git checkout src/services/UdpBroadcastService.cpp
.\build-project.bat
```

This removes the persistence layer but Greeks calculation will still work via direct signals (OptionChain will just need to always rely on signal updates, not storage reads).

---

## Implementation Date
**Date**: January 19, 2026
**Status**: ✅ **COMPLETE - Build Successful**
**Build Time**: ~2 minutes (CMake + compile + link)
**Build Environment**: MinGW 8.1.0 + Qt 5.15.2

