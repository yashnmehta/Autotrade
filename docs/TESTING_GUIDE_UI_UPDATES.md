# Testing Guide - MarketWatch UI Updates

## Overview

This guide helps you test the MarketWatch UI update system after reverting from native callbacks to standard Qt Model-View architecture.

---

## Prerequisites

### Build Status
✅ Application compiled successfully with Qt signals enabled

### What Changed
- **DISABLED**: Native C++ callbacks (bypassed Qt)
- **ENABLED**: Qt's standard `emit dataChanged()` signals
- **REMOVED**: Direct `viewport()->repaint()` calls

---

## Test Procedure

### Test 1: Basic UI Update

**Goal**: Verify prices update automatically

**Steps**:
1. Launch application:
   ```bash
   cd /home/ubuntu/Desktop/trading_terminal_cpp/build
   ./TradingTerminal
   ```

2. Add scrips to market watch (e.g., NIFTY options)

3. Start UDP broadcast receiver:
   - Menu: `Data → Start NSE Broadcast Receiver`

4. **OBSERVE**: Do prices update automatically without clicking?

**Expected Result**:
- ✅ Prices change in real-time
- ✅ Changes visible within 1-2 seconds
- ✅ No need to click on table

**If FAILS**:
- Check console for "dataChanged" debug messages
- Verify UDP receiver is actually running
- Check if tokens match between scrip and UDP data

---

### Test 2: Click Interaction

**Goal**: Verify table interactions still work

**Steps**:
1. Click on a row
2. Click on different columns
3. Select multiple rows

**Expected Result**:
- ✅ Rows highlight on click
- ✅ Selection changes work
- ✅ No crashes or freezes

---

### Test 3: Sorting During Updates

**Goal**: Verify sorting works while prices update

**Steps**:
1. Make sure prices are updating (UDP receiver running)
2. Click column header to sort by LTP
3. Click again to reverse sort
4. Sort by different columns

**Expected Result**:
- ✅ Sorting works immediately
- ✅ Prices continue updating after sort
- ✅ Sorted order maintained during updates
- ✅ No visual glitches

---

### Test 4: Rapid Updates

**Goal**: Verify system handles high-frequency updates

**Steps**:
1. Add 10+ scrips to market watch
2. Start UDP receiver
3. Observe during active market hours

**Expected Result**:
- ✅ All scrips update smoothly
- ✅ No lag or freezing
- ✅ UI remains responsive
- ✅ CPU usage reasonable (<30%)

---

### Test 5: Console Verification

**Goal**: Verify Qt signals are being emitted

**Steps**:
1. Launch from terminal to see debug output
2. Watch for these log messages:

```
[MARKETWATCH-UPDATEPRICE] Token: XXXXX | Rows found: 1
[MARKETWATCH-COMPLETE] TOTAL LATENCY: XX µs
```

3. If you added debug logging, also look for:
```
Emitting dataChanged for row X cols Y-Z
View received dataChanged: row X
```

**Expected Result**:
- ✅ Model updates happening (<100µs latency)
- ✅ dataChanged signals being emitted
- ✅ View receiving signals

---

## Performance Benchmarks

### Latency Measurements

**Add this code to measure Qt signal latency**:

In `MarketWatchModel.cpp`, add to `data()` method:
```cpp
QVariant MarketWatchModel::data(const QModelIndex &index, int role) const
{
    static auto lastTime = std::chrono::high_resolution_clock::now();
    auto now = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(now - lastTime).count();
    
    if (elapsed < 1000000) {  // Less than 1 second
        qDebug() << "[MODEL-DATA] Called" << elapsed << "µs after last call";
    }
    lastTime = now;
    
    // ... rest of data() implementation
}
```

**Expected Latencies**:
- `dataChanged` emit: <10µs
- `data()` call from view: <1000µs (1ms)
- Total end-to-end: <50ms (UDP → visible)

---

## Troubleshooting

### Problem: UI Still Not Updating

**Check 1**: Is dataChanged being called?

Add temporary logging in `MarketWatchModel::notifyRowUpdated()`:
```cpp
void MarketWatchModel::notifyRowUpdated(int row, int firstColumn, int lastColumn)
{
    qDebug() << "🔔 notifyRowUpdated: row" << row << "m_viewCallback =" << (m_viewCallback ? "YES" : "NO");
    
    if (m_viewCallback) {
        // Should NOT happen - we disabled this
        qDebug() << "❌ ERROR: Callback path taken (should be disabled!)";
        m_viewCallback->onRowUpdated(row, firstColumn, lastColumn);
    } else {
        qDebug() << "✅ Emitting dataChanged for row" << row;
        emit dataChanged(index(row, firstColumn), index(row, lastColumn));
    }
}
```

**Expected**: "✅ Emitting dataChanged" message should appear

---

**Check 2**: Is view receiving signals?

Add to `MarketWatchWindow` constructor:
```cpp
// After setSourceModel(m_model);
connect(proxyModel(), &QAbstractItemModel::dataChanged, 
    [](const QModelIndex &tl, const QModelIndex &br) {
        qDebug() << "📥 View received dataChanged:" 
                 << "rows" << tl.row() << "-" << br.row()
                 << "cols" << tl.column() << "-" << br.column();
    });
```

**Expected**: "📥 View received dataChanged" message should appear

---

**Check 3**: Is proxy model connected?

```cpp
// In MarketWatchWindow constructor
qDebug() << "Source model:" << m_model;
qDebug() << "Proxy model:" << proxyModel();
qDebug() << "View's model:" << model();
```

**Expected**: All three should be valid pointers

---

### Problem: Updates Too Slow

**Measure actual latency**:

In `MarketWatchWindow::updatePrice()`:
```cpp
void MarketWatchWindow::updatePrice(int token, double value, /* ... */)
{
    auto t_start = std::chrono::high_resolution_clock::now();
    
    // ... existing code ...
    
    auto t_end = std::chrono::high_resolution_clock::now();
    auto latency = std::chrono::duration_cast<std::chrono::microseconds>(t_end - t_start).count();
    
    qDebug() << "[UPDATEPRICE-TOTAL] Including view update:" << latency << "µs";
}
```

**If latency >50ms**, check:
1. Event loop blocked? (long-running operations on main thread)
2. Too many scrips? (test with fewer)
3. Proxy model filtering complex? (disable temporarily)

---

### Problem: Crashes or Freezes

**Check for**:
1. Null pointer access (use debugger)
2. Infinite signal loops (dataChanged → updatePrice → dataChanged)
3. Thread safety issues (UDP thread → main thread)

**Debug with**:
```bash
# Run with debugger
gdb ./TradingTerminal
run

# When it crashes:
bt  # backtrace
frame 0  # examine top frame
print variableName  # examine variables
```

---

## Comparison Tests

### Before (Native Callbacks) vs After (Qt Signals)

**Test**: Add NIFTY option, start UDP receiver, measure latency

| Metric | Native Callbacks (Broken) | Qt Signals (Fixed) |
|--------|---------------------------|---------------------|
| Compiles | ✅ Yes | ✅ Yes |
| UI updates auto | ❌ NO | ⏳ Test |
| Click works | ❌ NO | ⏳ Test |
| Latency claimed | "10-50ns" | ⏳ Measure |
| Actual latency | N/A (broken) | ⏳ Measure |
| Maintainable | ❌ Complex | ✅ Standard Qt |

---

## Acceptance Criteria

### Must Pass All These Tests

- [ ] UI updates automatically without clicks
- [ ] All interactions work (click, sort, filter)
- [ ] No crashes during 10-minute run
- [ ] Handles 10+ scrips simultaneously
- [ ] Latency <100ms visible to user

### Performance Targets

- [ ] dataChanged emit: <10µs
- [ ] View data() call: <1ms
- [ ] End-to-end (UDP→UI): <50ms
- [ ] CPU usage: <30% with 20 scrips

---

## Test Report Template

```
## Test Report - MarketWatch UI Updates

**Date**: ___________
**Tester**: ___________
**Build**: ___________

### Test Results

| Test | Status | Notes |
|------|--------|-------|
| Basic UI Update | ⏳ | |
| Click Interaction | ⏳ | |
| Sorting | ⏳ | |
| Rapid Updates | ⏳ | |
| Console Logs | ⏳ | |

### Performance Measurements

- dataChanged latency: ____ µs
- data() call latency: ____ µs
- End-to-end latency: ____ ms
- CPU usage (20 scrips): ____ %

### Issues Found

1. 
2. 
3. 

### Recommendation

[ ] PASS - Deploy to production
[ ] FAIL - More work needed
[ ] CONDITIONAL - Pass with noted limitations
```

---

## Next Steps After Testing

### If Tests Pass ✅

1. **Measure performance properly**
   - Get exact numbers for all latencies
   - Compare with XTS WebSocket baseline
   - Document in benchmarks file

2. **Remove dead code**
   - Delete `IMarketWatchViewCallback` interface
   - Remove `setViewCallback()` method
   - Clean up callback implementations

3. **Update documentation**
   - Remove "ultra-low latency" claims
   - Document actual measured performance
   - Update architecture diagrams

### If Tests Fail ❌

1. **Debug systematically**
   - Use logging to trace signal flow
   - Verify proxy model connections
   - Check for event loop blocking

2. **Consider alternative approaches**
   - Custom delegates for optimized painting
   - Batch updates with beginResetModel()
   - Optimize only bottlenecks identified by profiling

3. **Ask for help**
   - Post logs and measurements
   - Share profiling results
   - Describe observed vs expected behavior

---

## Contact & Support

**Questions?** 
- Check `docs/MARKETWATCH_UI_UPDATE_ARCHITECTURE.md` for design details
- Review Qt documentation on Model/View programming
- Search for "QAbstractItemModel dataChanged performance"

**Reporting Issues**:
Include:
1. Console output (full log)
2. Steps to reproduce
3. Expected vs actual behavior
4. Performance measurements if relevant
