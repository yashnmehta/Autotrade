# MarketWatch UI Update Architecture - Analysis & Roadmap

## Executive Summary

**Problem**: UI not updating automatically with UDP broadcast prices despite <100µs data latency
**Root Cause**: Native callback system bypassing Qt's Model-View architecture
**Solution**: Revert to standard Qt signals (dataChanged) and measure actual performance

---

## Current Architecture (BROKEN)

### Data Flow
```
UDP Broadcast → Parser → FeedHandler → MarketWatch::updatePrice() 
→ Model::updatePrice() → Model::notifyRowUpdated() 
→ [NATIVE CALLBACK] → View::onRowUpdated() → viewport()->repaint()
```

### Problems Identified

1. **Native Callback Bypass**: `m_viewCallback->onRowUpdated()` bypasses `emit dataChanged()`
2. **Direct repaint() Breaks Qt**: `viewport()->repaint()` bypasses Qt's paint event system
3. **No Proxy Model Integration**: Native callbacks don't account for QSortFilterProxyModel
4. **State Corruption**: Direct viewport manipulation breaks Qt's internal view state

### Why It Failed

```cpp
// In MarketWatchModel::notifyRowUpdated()
if (m_viewCallback) {
    m_viewCallback->onRowUpdated(row, firstColumn, lastColumn);  // ❌ Bypass Qt
} else {
    emit dataChanged(index(row, firstColumn), index(row, lastColumn));  // ✅ Qt way
}
```

**Issue**: Native callbacks called `viewport()->repaint()` directly, which:
- Doesn't trigger Qt's data change events
- Breaks proxy model updates
- Corrupts view state (click events stop working)
- Ignores Qt's update coalescing

---

## Proper Qt Model-View Architecture

### How Qt SHOULD Work

```
Model Data Changes → emit dataChanged() → QAbstractItemView receives signal
→ View invalidates affected cells → Schedules repaint through event loop
→ Qt coalesces updates → paintEvent() called once for multiple changes
```

### Key Concepts

1. **Models Don't Touch Views**: Models emit signals, views listen
2. **Proxy Models**: Sit between model and view, must receive dataChanged signals
3. **Event Coalescing**: Qt automatically batches rapid updates
4. **Paint Events**: Views repaint through Qt's event system, not direct calls

---

## Performance Reality Check

### Claimed Benefits of Native Callbacks
- "10-50ns vs 500ns-15ms for emit dataChanged"
- "Ultra-low latency mode"

### Actual Measurements Needed
We need to benchmark:
1. Time for `emit dataChanged()` in typical scenario
2. Time for view to repaint after dataChanged
3. End-to-end latency: data arrives → UI visible

### Expected Results
- Qt signals are fast (likely <1ms)
- View repaints are deferred anyway (event loop)
- Native callbacks don't actually improve visible latency

---

## Recommended Solution

### Phase 1: Stabilize (IMMEDIATE)

**Status**: ✅ COMPLETED

Changes made:
```cpp
// DISABLED native callback registration
// m_model->setViewCallback(this);

// Callback implementations now empty
void MarketWatchWindow::onRowUpdated(int row, int firstColumn, int lastColumn)
{
    // DISABLED: Using Qt's proper dataChanged signal instead
}
```

**Result**: Model now uses standard Qt path:
```cpp
emit dataChanged(index(row, firstColumn), index(row, lastColumn));
```

### Phase 2: Test & Measure (NEXT STEP)

**Tasks**:
1. Run application with Qt signals only
2. Verify UI updates work reliably
3. Measure actual update latency:
   ```cpp
   // Add to MarketWatchModel::data()
   auto t1 = std::chrono::high_resolution_clock::now();
   // ... data access ...
   auto t2 = std::chrono::high_resolution_clock::now();
   qDebug() << "data() latency:" << (t2-t1).count() << "ns";
   ```
4. Benchmark paintEvent latency
5. Get end-to-end measurements

### Phase 3: Optimize If Needed

**Only if measurements show >50ms latency:**

#### Option A: Optimized Qt Signals (Recommended)
```cpp
// Use roles to limit repaints
emit dataChanged(index(row, col), index(row, col), {Qt::DisplayRole});

// Batch updates
beginResetModel();
// ... multiple updates ...
endResetModel();
```

#### Option B: QAbstractItemView::viewport()->update()
```cpp
// In custom delegate or after dataChanged
QRect rect = visualRect(index);
viewport()->update(rect);  // Schedule update (not immediate repaint)
```

#### Option C: Custom View Optimization
```cpp
// Override dataChanged to optimize repaints
void MarketWatchWindow::dataChanged(const QModelIndex &topLeft, 
                                     const QModelIndex &bottomRight,
                                     const QVector<int> &roles) override
{
    QTableView::dataChanged(topLeft, bottomRight, roles);
    
    // Optional: Reduce update region
    if (roles.isEmpty() || roles.contains(Qt::DisplayRole)) {
        QRect updateRect;
        for (int row = topLeft.row(); row <= bottomRight.row(); ++row) {
            for (int col = topLeft.column(); col <= bottomRight.column(); ++col) {
                updateRect = updateRect.united(visualRect(model()->index(row, col)));
            }
        }
        viewport()->update(updateRect);
    }
}
```

### Phase 4: Remove Dead Code

**If Qt signals work well, remove:**
- `IMarketWatchViewCallback` interface
- `setViewCallback()` method
- All callback implementations (onRowUpdated, etc.)
- Related comments claiming "ultra-low latency"

---

## Testing Plan

### Functional Tests
1. ✅ Application compiles
2. ⏳ Add scrip to market watch
3. ⏳ Start UDP broadcast receiver
4. ⏳ Verify prices update automatically
5. ⏳ Test click interactions work
6. ⏳ Test sorting while prices update
7. ⏳ Test filtering while prices update

### Performance Tests
1. Measure dataChanged signal latency
2. Measure paintEvent frequency
3. Measure end-to-end: UDP → visible update
4. Compare with XTS WebSocket latency baseline
5. Visual test: Can you see delays with naked eye?

### Stress Tests
1. 100 scrips updating simultaneously
2. Rapid sorting/filtering during updates
3. Multiple market watch windows
4. Background CPU load

---

## Success Criteria

### Must Have
- ✅ Application doesn't crash
- ⏳ UI updates automatically (no manual clicks needed)
- ⏳ All interactions work (click, sort, filter)
- ⏳ No visible lag (<100ms perceived)

### Should Have
- End-to-end latency <50ms (UDP arrive → UI visible)
- Handles 100+ scrips without slowdown
- Clean, maintainable code using standard Qt

### Nice to Have
- End-to-end latency <10ms
- Benchmark documentation
- Performance profiling tools

---

## Decision Points

### Should We Keep Native Callbacks?

**NO** - Remove them because:
1. They bypass Qt's architecture (maintenance nightmare)
2. They don't actually work (we just proved that)
3. Performance claims are unverified
4. Standard Qt is fast enough for trading apps

### When to Optimize?

**AFTER measuring** - Only optimize if:
1. Qt signals show >50ms latency in tests
2. Users can visibly see delays
3. We've profiled and identified bottleneck

Don't optimize prematurely!

---

## Implementation Timeline

### Week 1: Stabilize & Measure
- ✅ Day 1: Revert to Qt signals
- ⏳ Day 2: Test basic functionality
- ⏳ Day 3: Measure performance
- ⏳ Day 4: Analyze results
- ⏳ Day 5: Document findings

### Week 2: Optimize (if needed)
- ⏳ Day 1: Implement chosen optimization
- ⏳ Day 2: Test optimization
- ⏳ Day 3: Compare before/after
- ⏳ Day 4: Stress testing
- ⏳ Day 5: Code review & documentation

### Week 3: Cleanup
- ⏳ Day 1: Remove dead code
- ⏳ Day 2: Update documentation
- ⏳ Day 3: Performance benchmarks
- ⏳ Day 4: User acceptance testing
- ⏳ Day 5: Release

---

## Code Changes Summary

### Reverted (Completed)

**File**: `src/views/MarketWatchWindow.cpp`

1. **Disabled callback registration** (line 59):
```cpp
// OLD: m_model->setViewCallback(this);  ❌
// NEW: // m_model->setViewCallback(this);  ✅
```

2. **Emptied callback implementations**:
```cpp
// All four callbacks now have empty bodies and comments explaining why
void MarketWatchWindow::onRowUpdated(...) { /* disabled */ }
void MarketWatchWindow::onRowsInserted(...) { /* disabled */ }
void MarketWatchWindow::onRowsRemoved(...) { /* disabled */ }
void MarketWatchWindow::onModelReset() { /* disabled */ }
```

3. **Model falls back to Qt signals automatically**:
```cpp
// In MarketWatchModel::notifyRowUpdated()
if (m_viewCallback) {  // Now false!
    m_viewCallback->onRowUpdated(...);
} else {
    emit dataChanged(...);  // ✅ This path is now taken
}
```

### To Be Removed Later

**After confirming Qt signals work well:**
- `include/views/IMarketWatchViewCallback.h` (entire file)
- `MarketWatchModel::m_viewCallback` member variable
- `MarketWatchModel::setViewCallback()` method
- All callback override declarations in `MarketWatchWindow.h`
- All callback implementations in `MarketWatchWindow.cpp`

---

## Lessons Learned

1. **Don't bypass framework architecture** - Qt's Model-View is well-designed
2. **Measure before optimizing** - "Premature optimization is the root of all evil"
3. **Trust the framework** - Qt handles millions of apps, it's fast enough
4. **Test incrementally** - Random fixes without understanding make things worse
5. **Document assumptions** - "Ultra-low latency" claims need benchmarks

---

## Next Steps

1. ⏳ **Test the application** - Verify UI updates work now
2. ⏳ **Measure performance** - Get real numbers, not guesses
3. ⏳ **Make data-driven decisions** - Optimize only if measurements show need
4. ⏳ **Clean up code** - Remove dead native callback system
5. ⏳ **Document properly** - Update architecture docs with real benchmarks

---

## Contact & Questions

If UI still doesn't update after these changes:
1. Check if dataChanged is actually being emitted
2. Verify proxy model is connected properly
3. Check if view is receiving signals
4. Look for event loop blocking

**Debugging commands**:
```cpp
// In MarketWatchModel::notifyRowUpdated()
qDebug() << "Emitting dataChanged for row" << row << "cols" << firstColumn << "-" << lastColumn;

// In MarketWatchWindow, connect to dataChanged:
connect(model(), &QAbstractItemModel::dataChanged, [](const QModelIndex &tl, const QModelIndex &br) {
    qDebug() << "View received dataChanged:" << tl.row() << br.row();
});
```

---

**Document Version**: 1.0  
**Date**: December 2024  
**Status**: Phase 1 Complete, Ready for Testing
