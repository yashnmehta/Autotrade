# Quick Start - Model Update Benchmark

## 1. Build
```bash
cd /home/ubuntu/Desktop/trading_terminal_cpp/build
cmake --build . --target benchmark_model_update_methods
```

## 2. Run
```bash
cd /home/ubuntu/Desktop/trading_terminal_cpp/build/tests
./benchmark_model_update_methods
```

## 3. Test Each Strategy

### Test A: Qt Signals (Standard)
1. Select "Qt Signals (emit dataChanged)"
2. Set frequency: 100 updates/sec
3. Update type: Full Row
4. Click "Start Test"
5. **Wait 10 seconds for results**

### Test B: Direct Viewport (Manual)
1. Select "Direct Viewport (manual update)"
2. Same settings
3. Click "Start Test"
4. **Compare results**

### Test C: Custom Callback (Bypass)
1. Select "Custom Callback (bypass signals)"
2. Same settings
3. Click "Start Test"
4. **Compare results**

## 4. Compare Results

Look at:
- **Model latency** - Should be <100µs
- **Paint time** - Should be <5ms
- **Actual FPS** - Should be ~60

## 5. Decision

**If Qt Signals fast enough** → Use it! (Standard, reliable, maintainable)

**If Custom Callback much faster** → Consider tradeoffs (Complexity vs speed)

---

## Example Results to Expect

### Good (Qt Signals Fast Enough):
```
Model latency:    15.43 µs     ← FAST!
Paint time:       2847.32 µs   ← Under 16ms budget
Actual FPS:       59.8          ← Perfect 60Hz
```
**Decision**: ✅ Use Qt signals

### Bad (Qt Signals Too Slow):
```
Model latency:    15432.00 µs  ← 15ms! Way too slow
Paint time:       18234.12 µs  ← Can't hit 60 FPS
Actual FPS:       30.2          ← Dropping frames
```
**Decision**: ❌ Need custom solution

---

## What to Document

Create `BENCHMARK_RESULTS_MODEL_UPDATES.md`:

```markdown
## Test Results - [Your Name] - [Date]

### System Specs
- CPU: [Your CPU]
- RAM: [Your RAM]
- Qt Version: [5.x.x]

### Results Table

| Strategy | Model (µs) | Paint (µs) | FPS | Rating |
|----------|-----------|-----------|-----|--------|
| Qt Signals | X | Y | Z | ⭐⭐⭐ |
| Direct Viewport | X | Y | Z | ⭐⭐ |
| Custom Callback | X | Y | Z | ⭐⭐⭐⭐ |

### Conclusion
[Based on data, we should use: _____ ]

### Reasoning
[Why this choice makes sense]
```

---

## Time Required
- **Build**: 1 minute
- **Run 3 tests**: 30 seconds
- **Analyze**: 5 minutes
- **Total**: ~10 minutes to **settle the debate!**

---

## Questions Answered
✅ Is emit dataChanged() slow? → **See model latency**  
✅ Should we use custom callbacks? → **Compare all 3**  
✅ Can we hit 60 FPS? → **See actual FPS**  

**Let the data decide!** 📊
