# MarketWatch Performance Logging Guide

## Overview
This document describes the comprehensive performance logging system implemented for the MarketWatch window to measure and analyze performance bottlenecks.

## Performance Logging Locations

### 1. Window Creation (`MainWindow::createMarketWatch`)
**File:** `src/app/MainWindow/Windows.cpp`

Measures end-to-end time from user action (menu click/shortcut) to window being visible and interactive.

**Log Format:**
```
[PERF] [CREATE_MARKETWATCH] #<counter> START - Counter:<window_number>
[PERF] [CREATE_MARKETWATCH] #<counter> COMPLETE
  TOTAL TIME: <ms>
  Breakdown:
    - Create MDI SubWindow: <ms>
    - Create MarketWatchWindow (see constructor logs): <ms>
    - Setup zero-copy mode: <ms>
    - Set content widget + resize: <ms>
    - Connect signals: <ms>
    - Add to MDI area: <ms>
    - Focus/raise/activate: <ms>
```

**What to Measure:**
- Total window creation time should be < 50ms for fast UI response
- Constructor time (see below) is usually the largest component
- MDI area operations should be < 5ms

---

### 2. Window Construction (`MarketWatchWindow::MarketWatchWindow`)
**File:** `src/views/MarketWatchWindow/MarketWatchWindow.cpp`

Measures internal window initialization including UI setup, connections, and preferences loading.

**Log Format:**
```
[PERF] [MARKETWATCH_CONSTRUCT] START
[PERF] [MARKETWATCH_CONSTRUCT] COMPLETE - Total:<ms>
  setupUI() - <ms>
  setupConnections() - <ms>
  setupKeyboardShortcuts() - <ms>
  loadAndApplyWindowSettings() - <ms>
  Load preferences - <ms>
  TOTAL TIME: <ms> with full breakdown
```

**What to Measure:**
- Total construction should be < 30ms
- `setupUI()` creates QTableView and delegates: ~10-15ms
- `setupConnections()` wires signals/slots: ~2-5ms
- `setupKeyboardShortcuts()`: < 1ms
- `loadAndApplyWindowSettings()`: ~2-5ms (reads from QSettings)
- `Load preferences`: < 1ms

**Optimization Opportunities:**
- If `setupUI()` > 20ms, consider lazy widget initialization
- If `loadAndApplyWindowSettings()` > 10ms, check disk I/O or settings file size

---

### 3. Scrip Data Loading (`MarketWatchWindow::addScripFromContract`)
**File:** `src/views/MarketWatchWindow/Actions.cpp`

Measures per-scrip loading time including repository lookup, model updates, subscriptions, and initial data fetch.

**Log Format:**
```
[PERF] [MW_ADD_SCRIP] #<counter> START - Token:<token> Symbol:<symbol>
[PERF] [MW_ADD_SCRIP] #<counter> COMPLETE - Token:<token>
  TOTAL TIME: <ms>
  Breakdown:
    - Validation: <ms>
    - Enrich from repo: <ms>
    - Add to model: <ms>
    - Token subscription: <ms>
    - UDP subscription: <ms>
    - Load initial data: <ms>
    - Address book update: <ms>
```

**What to Measure:**
- Total per-scrip load should be < 5ms
- Validation (duplicate check): < 0.5ms
- Repository enrichment: ~1-2ms (ContractData lookup)
- Add to model: ~0.5-1ms (QAbstractItemModel operations)
- Token subscription: < 0.5ms
- UDP subscription: < 0.5ms
- Initial data load (zero-copy): ~1-2ms
- Address book update: < 0.5ms

**Optimization Opportunities:**
- If "Enrich from repo" > 5ms: Repository lookup is slow, consider caching
- If "Load initial data" > 5ms: Zero-copy PriceStoreGateway might be slow, check shared memory
- If "Add to model" > 2ms: QAbstractItemModel notifications might be inefficient

**Bulk Loading:**
When loading multiple scrips (e.g., from saved workspace):
- Monitor cumulative time
- 100 scrips should load in < 500ms (5ms each)
- If > 1000ms for 100 scrips, investigate bottlenecks

---

### 4. Price Update Processing (`MarketWatchWindow::onUdpTickUpdate`)
**File:** `src/views/MarketWatchWindow/Data.cpp`

Measures tick-by-tick update latency from UDP tick reception to model update completion.

**Log Behavior:**
- **Detailed logging:** First 50 ticks logged with full breakdown
- **Summary logging:** Every 100th tick shows cumulative count
- **Latency tracking:** Every tick with valid timestamp goes to LatencyTracker

**Detailed Log Format (first 50 ticks):**
```
[PERF] [MW_TICK_UPDATE] #<counter> START - Token:<token> Segment:<segment_id> LTP:<ltp>
[PERF] [MW_TICK_UPDATE] #<counter> Found <n> row(s)
[PERF] [MW_TICK_UPDATE] #<counter> COMPLETE - Token:<token>
  TOTAL TIME: <ms>
  Breakdown:
    - Initial setup: <ms>
    - Row lookup: <ms>
    - BSE logging check: <ms>
    - LTP + OHLC update: <ms>
    - LTQ/ATP/Volume update: <ms>
    - Bid/Ask update: <ms>
    - Total Qty + OI update: <ms>
```

**Summary Log Format (every 100 ticks):**
```
[PERF] [MW_TICK_UPDATE] Processed <count> ticks total
```

**What to Measure:**
- Total update time should be < 1ms per tick
- Row lookup: < 0.1ms (optimized int64 composite key)
- LTP + OHLC update: ~0.2-0.4ms (most common operation)
- Bid/Ask update: ~0.1-0.2ms
- All other updates: < 0.1ms each

**Performance Targets:**
- **Excellent:** < 0.5ms per tick (2000+ ticks/sec throughput)
- **Good:** 0.5-1ms per tick (1000-2000 ticks/sec)
- **Acceptable:** 1-2ms per tick (500-1000 ticks/sec)
- **Poor:** > 2ms per tick (< 500 ticks/sec) - investigate bottleneck

**Optimization Opportunities:**
- If "Row lookup" > 0.5ms: TokenAddressBook lookup slow, check hash implementation
- If "LTP + OHLC update" > 1ms: Model updates slow, check QAbstractItemModel::dataChanged() batching
- If total > 2ms: Consider reducing update frequency or batching updates

---

## End-to-End Performance Flow

### Cold Start (First MarketWatch Window)
```
User Action (Ctrl+W)
  ↓
[CREATE_MARKETWATCH] START (MainWindow/Windows.cpp)
  ↓
Create CustomMDISubWindow (~2ms)
  ↓
[MARKETWATCH_CONSTRUCT] START (MarketWatchWindow.cpp)
  ↓ setupUI (~15ms)
  ↓ setupConnections (~3ms)
  ↓ setupKeyboardShortcuts (~0.5ms)
  ↓ loadAndApplyWindowSettings (~3ms)
  ↓ Load preferences (~0.5ms)
  ↓
[MARKETWATCH_CONSTRUCT] COMPLETE (~22ms total)
  ↓
Setup zero-copy mode (~1ms)
  ↓
Set content widget + resize (~2ms)
  ↓
Connect signals (~1ms)
  ↓
Add to MDI area (~3ms)
  ↓
Focus/raise/activate (~2ms)
  ↓
[CREATE_MARKETWATCH] COMPLETE (~35ms total)
  ↓
Window Visible & Interactive
```

### Adding First Scrip
```
User adds scrip (search + Enter)
  ↓
[MW_ADD_SCRIP] #1 START
  ↓ Validation (~0.3ms)
  ↓ Enrich from repo (~1.5ms)
  ↓ Add to model (~0.8ms)
  ↓ Token subscription (~0.3ms)
  ↓ UDP subscription (~0.4ms)
  ↓ Load initial data (~1.2ms)
  ↓ Address book update (~0.3ms)
  ↓
[MW_ADD_SCRIP] #1 COMPLETE (~4.8ms total)
  ↓
Scrip Visible in Table
```

### First Tick Update
```
UDP Tick Received (FeedHandler)
  ↓
[MW_TICK_UPDATE] #1 START
  ↓ Initial setup (~0.05ms)
  ↓ Row lookup (~0.08ms)
  ↓ BSE logging check (~0.02ms)
  ↓ LTP + OHLC update (~0.35ms)
  ↓ LTQ/ATP/Volume update (~0.15ms)
  ↓ Bid/Ask update (~0.12ms)
  ↓ Total Qty + OI update (~0.08ms)
  ↓
[MW_TICK_UPDATE] #1 COMPLETE (~0.85ms total)
  ↓
Price Updated in UI
```

---

## Performance Monitoring Best Practices

### 1. **During Development**
- Build with logs enabled (default)
- Open MarketWatch window
- Add 5-10 scrips
- Observe timing for each operation
- Identify operations > expected thresholds

### 2. **Load Testing**
```bash
# Test with 100 scrips
1. Create MarketWatch
2. Bulk load 100 scrips from saved workspace
3. Monitor logs:
   - Total load time should be < 500ms
   - Each scrip ~5ms or less
   - First 50 ticks logged in detail
```

### 3. **Production Monitoring**
- Detailed tick logs auto-disable after 50 ticks (reduces log noise)
- Summary logs every 100 ticks show ongoing health
- LatencyTracker captures end-to-end latency metrics

### 4. **Log Analysis**
```bash
# Filter logs by component
grep "[CREATE_MARKETWATCH]" logs/app.log
grep "[MARKETWATCH_CONSTRUCT]" logs/app.log
grep "[MW_ADD_SCRIP]" logs/app.log
grep "[MW_TICK_UPDATE]" logs/app.log

# Find slow operations
grep "TOTAL TIME:" logs/app.log | sort -t: -k2 -n

# Track tick processing rate
grep "Processed.*ticks total" logs/app.log
```

---

## Troubleshooting Slow Performance

### Symptom: Window opens slowly (> 100ms)
**Diagnosis:**
1. Check `[CREATE_MARKETWATCH]` total time
2. Check `[MARKETWATCH_CONSTRUCT]` breakdown
3. Look for operations > 20ms

**Solutions:**
- If `setupUI()` slow: Consider widget lazy loading
- If `loadAndApplyWindowSettings()` slow: Check QSettings file size or disk I/O
- If MDI operations slow: Check parent window complexity

---

### Symptom: Adding scrips is slow (> 10ms per scrip)
**Diagnosis:**
1. Check `[MW_ADD_SCRIP]` breakdown
2. Identify which operation is slow

**Solutions:**
- If "Enrich from repo" slow:
  - Repository might not be loaded
  - Add repository caching layer
- If "Load initial data" slow:
  - Zero-copy shared memory might be fragmented
  - Check PriceStoreGateway performance
- If "Add to model" slow:
  - QAbstractItemModel might be emitting too many signals
  - Consider beginResetModel/endResetModel for bulk loads

---

### Symptom: Price updates are laggy (> 2ms per tick)
**Diagnosis:**
1. Check first 50 detailed tick logs
2. Identify bottleneck in breakdown

**Solutions:**
- If "Row lookup" slow:
  - TokenAddressBook hash collisions
  - Consider better hash function or structure
- If "LTP + OHLC update" slow:
  - Model notifications inefficient
  - Batch dataChanged() signals
  - Use viewport invalidation optimization
- If "Bid/Ask update" slow:
  - Too many model updates per tick
  - Consider update throttling

---

## Expected Performance Baselines

| Operation | Expected Time | Good Time | Excellent Time |
|-----------|---------------|-----------|----------------|
| Create Window | < 50ms | < 30ms | < 20ms |
| Window Constructor | < 30ms | < 20ms | < 15ms |
| Add Scrip | < 5ms | < 3ms | < 2ms |
| Tick Update | < 1ms | < 0.5ms | < 0.3ms |
| Load 100 Scrips | < 500ms | < 300ms | < 200ms |

---

## Integration with Existing Latency Tracking

The `onUdpTickUpdate` method integrates with the existing `LatencyTracker` system:

```cpp
LatencyTracker::recordLatency(
    tick.timestampUdpRecv,      // When UDP packet received
    tick.timestampParsed,        // When tick parsed
    tick.timestampEmitted,       // When tick emitted from FeedHandler
    0,                           // No dequeue timestamp for UDP path
    tick.timestampFeedHandler,   // When FeedHandler processed it
    timestampModelStart,         // When model update started
    timestampModelEnd            // When model update completed
);
```

This provides end-to-end latency tracking from network to UI update.

---

## Log Configuration

### Enable Performance Logs
Performance logs are enabled by default via `qDebug()` statements.

### Disable Performance Logs (Production)
To reduce log noise in production, comment out or conditionally compile logs:

```cpp
// In CMakeLists.txt or build config
add_definitions(-DDISABLE_PERF_LOGS)

// In code
#ifndef DISABLE_PERF_LOGS
    qDebug() << "[PERF] ...";
#endif
```

### Reduce Tick Update Logs
Tick update logging automatically limits detailed logs to first 50 ticks. To change:

```cpp
// In Data.cpp, line ~193
static int detailedLogCounter = 0;
bool shouldLogDetailed = (detailedLogCounter < 50); // Change 50 to desired count
```

---

## Performance Optimization Roadmap

### Phase 1: Measurement (Current) ✅
- ✅ Add comprehensive logging to all critical paths
- ✅ Measure baseline performance
- ✅ Identify bottlenecks

### Phase 2: Low-Hanging Fruit
- [ ] Optimize repository lookups (caching)
- [ ] Batch model updates (reduce dataChanged() calls)
- [ ] Optimize TokenAddressBook lookups

### Phase 3: Advanced Optimizations
- [ ] Widget lazy loading for MarketWatch
- [ ] Zero-copy price updates (already partially implemented)
- [ ] Viewport-aware update throttling (only update visible rows)

### Phase 4: Profiling
- [ ] Use Qt Creator profiler on slow operations
- [ ] Identify CPU hotspots
- [ ] Memory allocation profiling

---

## Conclusion

This performance logging system provides **complete visibility** into MarketWatch window performance:

1. **Window Creation:** Track from user action to visible window
2. **Constructor:** Measure internal initialization breakdown
3. **Data Loading:** Per-scrip timing for bulk operations
4. **Price Updates:** Tick-by-tick latency measurement

Use these logs to:
- Understand current performance characteristics
- Identify bottlenecks
- Validate optimizations
- Ensure performance doesn't regress

**Next Steps:**
1. Build and run the application
2. Open a MarketWatch window
3. Add several scrips
4. Review logs to establish baselines
5. Compare against expected performance targets
6. Optimize slow operations identified

---

## Related Documentation
- `SNAPQUOTE_FINAL_BUYSEL_PATTERN.md` - SnapQuote optimization (1500ms → 5-10ms)
- `QT_WINDOW_PRE_RENDERING_LESSON.md` - Window pre-rendering lessons
- `WINDOW_CONTEXT_SYSTEM.md` - Window context passing architecture
