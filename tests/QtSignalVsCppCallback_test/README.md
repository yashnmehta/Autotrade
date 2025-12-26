# Signal vs Callback Communication Test

## Purpose

This benchmark **measures the actual performance difference** between:
1. **Qt Signals/Slots** - Qt's standard cross-object communication
2. **C++ Callbacks** - Direct function pointer calls
3. **Observer Pattern** - std::function-based notifications

For the specific pattern: **Parent → Child A → Child B** (sibling communication)

---

## Test Scenario

Simulates real trading terminal communication:

```
UDP Parser (Child A)
    ↓ receives tick
    ↓ notifies via signal/callback
    ↓
FeedHandler (Parent)
    ↓ broadcasts to
    ↓
Multiple Subscribers (Children B):
    - MarketWatch
    - OrderBook  
    - Chart Widget
    - Position Manager
    - Risk Calculator
```

---

## What Gets Measured

- **Per-message latency** (nanoseconds)
- **Total delivery time** (microseconds)
- **Message delivery reliability** (all subscribers received?)
- **Median, Min, Max latency** (distribution analysis)

### Test Configuration
- **Subscribers**: 5 components listening
- **Messages**: 10,000 market ticks
- **Runs**: 5 iterations for consistency
- **Data**: Realistic market tick structure (token, LTP, bid, ask, volume, timestamp)

---

## Building & Running

### Build
```bash
cd /home/ubuntu/Desktop/trading_terminal_cpp/build
cmake ..
cmake --build . --target test_signal_vs_callback_communication
```

### Run
```bash
cd build/tests/QtSignalVsCppCallback_test
./test_signal_vs_callback_communication
```

---

## Expected Results

### Fast Network/Modern CPU
```
Qt Signals:     2-5 µs per message
C++ Callbacks:  0.5-2 µs per message  
Observer:       1-3 µs per message
```

**Typical speedup**: Callbacks 2-3x faster than Qt Signals

### Interpretation Guide

**If Qt Signals < 10 µs**:
- ✅ Fast enough for trading terminal
- Network latency (1000-5000 µs) dominates
- UI refresh (16667 µs for 60 FPS) is bottleneck
- **Recommendation**: Use Qt Signals (simpler, safer)

**If Qt Signals > 50 µs**:
- ⚠️ Potential bottleneck for high-frequency updates
- Consider C++ callbacks for hot paths
- Profile production code to confirm

**If Callbacks < 2 µs AND Qt > 20 µs**:
- ⚡ Significant speedup (>10x)
- Consider hybrid approach:
  - Callbacks for critical paths (UDP → FeedHandler)
  - Qt Signals for UI updates (FeedHandler → Views)

---

## Tradeoffs Summary

### Qt Signals/Slots

**Pros**:
- ✅ Thread-safe (can emit from any thread)
- ✅ Automatic connection management (no manual cleanup)
- ✅ Type-safe at compile time
- ✅ Standard Qt pattern (maintainable)
- ✅ Can queue connections (cross-thread)
- ✅ Disconnect on object deletion (no dangling pointers)

**Cons**:
- ❌ 2-5x slower than raw callbacks
- ❌ Requires QObject inheritance
- ❌ Meta-object system overhead

**Best For**:
- Cross-thread communication
- GUI applications
- When coupling needs to be loose
- Production code (reliability > speed)

---

### C++ Callbacks (Function Pointers)

**Pros**:
- ✅ Fastest (direct function call)
- ✅ No overhead (compiled to single call instruction)
- ✅ No Qt dependency
- ✅ Explicit and simple

**Cons**:
- ❌ NOT thread-safe (same thread only!)
- ❌ Manual lifetime management (dangling pointer risk)
- ❌ No automatic cleanup
- ❌ More complex unsubscription logic
- ❌ Less type-safe (easier to break)

**Best For**:
- Same-thread communication only
- Ultra-low latency requirements (<2 µs)
- High-frequency updates (>50,000/sec)
- Non-Qt codebases

---

### Observer Pattern (std::function)

**Pros**:
- ✅ Fast (close to raw callbacks)
- ✅ Flexible (can bind member functions, lambdas)
- ✅ Can capture context
- ✅ Standard C++ (no Qt needed)

**Cons**:
- ❌ NOT thread-safe
- ❌ Manual lifetime management
- ❌ std::function heap allocation overhead
- ❌ Slightly slower than raw pointers

**Best For**:
- When you need lambda capture
- Modern C++ codebases
- Flexible callback signatures
- Non-Qt but need flexibility

---

## Real-World Recommendations

### For Your Trading Terminal

#### Use Qt Signals For:
1. **UDP Parser → FeedHandler**
   - Benefits: Thread-safe (UDP on separate thread)
   - Cost: 2-5 µs (negligible vs network)

2. **FeedHandler → UI Components**
   - Benefits: Automatic thread handling
   - Cost: Acceptable for UI refresh rates

3. **Cross-window communication**
   - Benefits: Loose coupling
   - Cost: Irrelevant for user actions

#### Use C++ Callbacks For:
1. **High-frequency inner loops**
   - Only if profiling shows Qt signals as bottleneck
   - Example: Processing >50,000 ticks/sec

2. **Same-thread tight coupling**
   - When you control both publisher and subscriber lifetime
   - Example: Model → View in same thread

#### Use Observer Pattern For:
1. **Plugin systems**
   - Dynamic subscriber registration
   - No Qt dependency required

2. **Complex event handling**
   - Need to capture context
   - Flexible callback signatures

---

## Comparison with Previous Benchmark

### This Test (Communication)
- Measures: Signal/callback invocation overhead
- Result: Qt Signals 2-5 µs, Callbacks 0.5-2 µs

### Previous Test (Model Updates)
- Measured: Complete update pipeline (signal + repaint)
- Result: Qt shows MORE updates to user (100 FPS vs 35 FPS)

### Combined Conclusion
1. **Qt Signal overhead is small** (2-5 µs)
2. **But Qt's integration is smart** (better coalescing)
3. **For UI updates**: Qt Signals WIN (better user experience)
4. **For non-UI data flow**: Either is fine (both fast enough)

---

## Decision Matrix

| Use Case | Qt Signals | C++ Callbacks | Observer |
|----------|-----------|---------------|----------|
| Thread-safe needed | ✅ | ❌ | ❌ |
| UI updates | ✅ | ❌ | ❌ |
| Ultra-low latency | ❌ | ✅ | ⚡ |
| Cross-thread | ✅ | ❌ | ❌ |
| Standard Qt app | ✅ | ❌ | ❌ |
| Hot path >50k/sec | ❌ | ✅ | ⚡ |
| Need flexibility | ⚡ | ❌ | ✅ |
| Easy maintenance | ✅ | ❌ | ⚡ |

**Legend**: ✅ Best choice | ⚡ Good choice | ❌ Not recommended

---

## Expected Output Example

```
========================================
Signal vs Callback Communication Test
========================================
Configuration:
  Subscribers: 5
  Messages: 10000
  Test runs: 5
========================================

Testing Qt Signals/Slots...
  Run 1/5 - Avg: 2.34 µs
  Run 2/5 - Avg: 2.41 µs
  ...

Testing C++ Callbacks...
  Run 1/5 - Avg: 0.87 µs
  Run 2/5 - Avg: 0.92 µs
  ...

Testing Observer Pattern...
  Run 1/5 - Avg: 1.56 µs
  Run 2/5 - Avg: 1.61 µs
  ...

========================================
BENCHMARK RESULTS
========================================

Method                        | Avg (µs) | Median (µs) | Min (ns) | Max (µs) | Total (ms) | Msgs
------------------------------+----------+-------------+----------+----------+------------+--------
Qt Signals/Slots              |     2.38 |        2.35 |      850 |     45.2 |        119 |  50000
C++ Callbacks                 |     0.89 |        0.86 |      420 |     12.3 |         44 |  50000
Observer Pattern              |     1.58 |        1.55 |      680 |     28.7 |         79 |  50000

========================================
ANALYSIS
========================================

Per-message latency comparison:
  Qt Signals:     2.38 µs
  C++ Callbacks:  0.89 µs (2.7x faster)
  Observer:       1.58 µs (1.5x faster than Qt)

Total time for 10000 messages to 5 subscribers:
  Qt Signals:     119 ms
  C++ Callbacks:  44 ms (2.7x faster)
  Observer:       79 ms (1.5x faster than Qt)

========================================
RECOMMENDATIONS
========================================

✅ Qt Signals are FAST ENOUGH
   - Difference: <3x slower than raw callbacks
   - Both methods deliver in <5 µs
   - Recommendation: Use Qt Signals for:
     • Cross-thread communication (thread-safe)
     • Loose coupling between components
     • When you need Qt's connection management
     • GUI applications (standard Qt pattern)

⚡ Use C++ Callbacks when:
   - Same-thread communication only
   - Need absolute minimum latency
   - High-frequency updates (>10000/sec)
   - No Qt dependency desired

========================================
CONCLUSION
========================================

For your trading terminal application:

✅ RECOMMENDED: Qt Signals/Slots
   Reason: 2.38 µs latency is negligible
           compared to network latency (1000-5000 µs)
           and UI refresh rate (16667 µs for 60 FPS)

   Use for: UDP Parser → FeedHandler → MarketWatch
            Cross-component communication
            Thread-safe data delivery

⚡ OPTIONAL: C++ Callbacks for ultra-hot paths
   Use ONLY if profiling shows Qt signals as bottleneck
   Example: Inner loops processing >50000 msgs/sec
```

---

## Next Steps

1. **Run the test** - Get actual numbers for your hardware
2. **Document results** - Save output for reference
3. **Make decision** - Based on YOUR measurements, not assumptions
4. **Profile production** - See if Qt signals show up in profiler
5. **Optimize only if needed** - Don't complicate without data

---

**Remember**: 
- 📊 Measure on YOUR hardware
- 🎯 2-5 µs is negligible for trading app
- ✅ Qt Signals likely fast enough
- ⚡ Callbacks only if proven bottleneck
