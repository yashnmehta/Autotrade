# Model Update Strategy Benchmark - README

## Purpose

This benchmark **proves with real measurements** whether Qt's `emit dataChanged()` signal actually adds latency compared to custom update methods.

Instead of assuming, we **test 3 approaches**:
1. ✅ **Qt Signals** - Standard `emit dataChanged()`
2. 🔧 **Direct Viewport** - Manual `viewport()->update()`
3. ⚡ **Custom Callback** - Bypass Qt signals entirely

---

## Why This Matters

### The Claim
Native callbacks were implemented because of claims that:
- Qt signals are "slow" (15ms)
- Custom callbacks are "ultra-fast" (50ns)
- Direct viewport updates avoid signal overhead

### The Reality
We **need measurements** to prove:
- What's the actual latency of `emit dataChanged()`?
- Does it matter for 60 FPS rendering (16.67ms frame budget)?
- Is the complexity of custom callbacks worth it?

---

## What Gets Measured

### Model Update Latency
Time spent in model notification code:
- `emit dataChanged()` overhead
- Callback invocation time
- Data structure access time

### View Paint Performance
Time spent rendering updates:
- `paintEvent()` execution time
- Number of repaints triggered
- Actual FPS achieved

### End-to-End Metrics
- Updates per second achieved
- Paint events per second
- Average latency from update → visible

---

## Running the Benchmark

### Build
```bash
cd /home/ubuntu/Desktop/trading_terminal_cpp/build
cmake --build . --target benchmark_model_update_methods
```

### Run
```bash
cd /home/ubuntu/Desktop/trading_terminal_cpp/build/tests
./benchmark_model_update_methods
```

### GUI Controls

**Strategy Selection**:
- **Qt Signals**: Standard emit dataChanged()
- **Direct Viewport**: Manual viewport()->update()
- **Custom Callback**: Bypass signals with C++ callback

**Frequency**:
- 10, 50, 100, 200, 500, 1000 updates/second
- Simulates different market data rates

**Update Type**:
- **Single Cell**: One cell at a time (like individual field updates)
- **Full Row**: Entire row (like complete tick updates)

**Duration**: 10 seconds per test

---

## Test Scenarios

### Scenario 1: Low Frequency (10 updates/sec)
**Purpose**: Baseline - even inefficient code should work
**Expected**: All strategies perform similarly

### Scenario 2: Medium Frequency (100 updates/sec)
**Purpose**: Typical market data rate
**Expected**: Differences become measurable

### Scenario 3: High Frequency (500+ updates/sec)
**Purpose**: Stress test
**Expected**: Inefficiencies exposed

### Scenario 4: Full Row Updates
**Purpose**: Simulates real tick updates (multiple fields change)
**Expected**: Signal coalescing benefits Qt signals

---

## Interpreting Results

### Good Performance Indicators
- ✅ Model latency <100µs
- ✅ Paint time <5ms (60 FPS = 16.67ms budget)
- ✅ Actual FPS close to screen refresh (60 Hz)
- ✅ Update rate matches requested frequency

### Red Flags
- ❌ Model latency >1ms (something wrong)
- ❌ Paint time >16ms (can't maintain 60 FPS)
- ❌ Actual FPS << update rate (view not keeping up)
- ❌ Update rate << requested (system overloaded)

### Example Output
```
========================================
TEST RESULTS: Qt Signals (emit dataChanged)
========================================
Duration:         10000 ms
Total updates:    1000
Update rate:      100.0 updates/sec
Model latency:    15.43 µs (avg)
Paint count:      598
Paint time:       2847.32 µs (avg)
Actual FPS:       59.8
========================================
```

**Analysis**:
- Model latency: 15µs = **0.015ms** (NOT 15ms!)
- Paint time: 2.8ms (well under 16.67ms budget)
- FPS: 59.8 ≈ 60 Hz (perfect)
- **Conclusion**: Qt signals are FAST ENOUGH

---

## Expected Results

### Hypothesis 1: Qt Signals Are Fast
**Prediction**: Model latency <100µs, no visible lag

**If TRUE**: 
- Use standard Qt signals (reliable, maintainable)
- Remove custom callback complexity
- Focus optimization elsewhere

**If FALSE**: 
- Custom callbacks justified
- Need to understand WHY Qt is slow
- Consider Qt6 upgrade (faster signals)

### Hypothesis 2: Paint Time Dominates
**Prediction**: paintEvent() takes 1-5ms, signal overhead negligible

**If TRUE**:
- Signal latency doesn't matter (paint is bottleneck)
- Optimize rendering, not signals
- Use delegates for custom drawing

**If FALSE**:
- Signal processing is actually slow
- Need to investigate Qt's event loop
- Consider batch updates

### Hypothesis 3: Update Frequency Matters
**Prediction**: <200 updates/sec = no difference between strategies

**If TRUE**:
- Normal market data (100 ticks/sec) handled fine
- Only optimize for high-frequency scenarios
- Simple code for 99% of cases

**If FALSE**:
- Even low frequencies show issues
- Deeper problem than signal overhead
- Check threading, event loop blocking

---

## Comparison Table Template

| Strategy | Model Latency | Paint Time | FPS | Rating |
|----------|--------------|------------|-----|--------|
| Qt Signals | ? µs | ? ms | ? | ? |
| Direct Viewport | ? µs | ? ms | ? | ? |
| Custom Callback | ? µs | ? ms | ? | ? |

**Fill in after running tests!**

---

## Technical Details

### Test Configuration
```cpp
constexpr int NUM_ROWS = 50;           // Number of scrips in table
constexpr int NUM_COLS = 10;           // Columns (symbol, LTP, bid, etc.)
constexpr int TEST_DURATION_SEC = 10;  // How long each test runs
```

### What Gets Updated
- Random cell selection (simulates non-uniform updates)
- Value changes ±1% (simulates real price movements)
- No actual market data (pure Qt performance test)

### Measurement Methods
```cpp
// Model latency (nanosecond precision)
auto t_start = std::chrono::high_resolution_clock::now();
// ... update code ...
auto t_end = std::chrono::high_resolution_clock::now();
auto latency_ns = (t_end - t_start).count();

// Paint latency (microsecond precision)
void paintEvent(QPaintEvent* event) override {
    auto t_start = std::chrono::high_resolution_clock::now();
    QTableView::paintEvent(event);
    auto t_end = std::chrono::high_resolution_clock::now();
    // Record time
}
```

---

## Common Mistakes to Avoid

### ❌ Don't Compare to Different Machines
- Run all tests on SAME machine
- Close other applications
- Multiple runs for consistency

### ❌ Don't Test During High System Load
- Close browsers, IDEs, etc.
- Disable background updates
- Monitor CPU usage

### ❌ Don't Trust Single Run
- Run each test 3-5 times
- Average the results
- Check for outliers

### ❌ Don't Ignore Paint Time
- Model latency is only part of story
- View rendering often dominates
- End-to-end matters most

---

## Decision Matrix

### If Qt Signals <100µs AND Paint <5ms:
**Decision**: ✅ **Use Qt Signals**
- Remove custom callbacks
- Standard Qt Model/View
- Focus on other optimizations

### If Qt Signals >1ms OR Paint >16ms:
**Decision**: 🔍 **Investigate Deeper**
- Profile with Qt Creator
- Check for unnecessary copies
- Look for O(n²) operations
- Consider Qt6 migration

### If Custom Callbacks Significantly Faster:
**Decision**: ⚖️ **Weigh Tradeoffs**
- Document measured speedup
- Consider maintenance cost
- Evaluate code complexity
- Keep both as option?

---

## Next Steps After Benchmarking

### 1. Document Results
Create `BENCHMARK_RESULTS_MODEL_UPDATES.md` with:
- Table of all measurements
- Graphs if possible
- Conclusions and recommendations

### 2. Update Architecture Decision
Based on data, either:
- **Keep Qt signals** (if fast enough)
- **Implement optimized callbacks** (if justified)
- **Hybrid approach** (signals + manual updates)

### 3. Apply to Production
- Use winning strategy in MarketWatchModel
- Update documentation
- Remove dead code paths

---

## Questions This Answers

✅ **Is `emit dataChanged()` actually slow?**
→ Measure model latency directly

✅ **Does custom callback bypass help?**
→ Compare with callback strategy

✅ **What's the real bottleneck?**
→ Compare model vs paint latency

✅ **Can we hit 60 FPS with Qt signals?**
→ Check actual FPS achieved

✅ **Is the complexity worth it?**
→ Compare performance vs maintainability

---

## Related Documentation

- `docs/MARKETWATCH_UI_UPDATE_ARCHITECTURE.md` - Why we're testing this
- `docs/TESTING_GUIDE_UI_UPDATES.md` - How to test production code
- `tests/benchmark_qt_vs_native.cpp` - Related Qt performance tests

---

## Support

**Getting weird results?**
- Check console output for errors
- Verify Qt version (Qt5 recommended)
- Try different update frequencies
- Test with fewer rows/columns

**Want to modify test?**
- Edit `NUM_ROWS`, `NUM_COLS` constants
- Change `TEST_DURATION_SEC` for longer tests
- Add custom strategies in `UpdateStrategy` enum

**Need help interpreting?**
- Post results with system specs
- Include screenshots of GUI
- Share console output

---

## Expected Timeline

1. **Run baseline tests** (30 minutes)
   - All 3 strategies
   - Multiple frequencies
   - Both update types

2. **Analyze results** (30 minutes)
   - Compare numbers
   - Identify winner
   - Document findings

3. **Make decision** (15 minutes)
   - Choose strategy
   - Plan implementation
   - Update architecture

**Total time**: ~90 minutes to **solve the debate with data**

---

**Remember**: 
- 📊 **Measure, don't guess**
- 🎯 **Profile first, optimize later**
- 📝 **Document your findings**
- ✅ **Let data drive decisions**
