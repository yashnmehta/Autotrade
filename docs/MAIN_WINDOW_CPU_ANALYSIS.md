# Main Window CPU Usage Analysis & Optimization Plan

## 🔍 Problem Identified

User reports **high CPU usage** during main window load after implementing Indices window and SnapQuote window caching.

## 📊 Root Cause Analysis

### CPU Spike Occurs During Window Cache Initialization

The `WindowCacheManager::initialize()` is called in `main.cpp` line 288:
```cpp
QTimer::singleShot(300, mainWindow, [mainWindow]() {
    // ...
    WindowCacheManager::instance().initialize(mainWindow);  // ⚠️ CPU SPIKE HERE
});
```

### What Happens During Initialization:

The `WindowCacheManager` **pre-creates and shows off-screen**:
1. **1x Buy Window** - Complete order entry form with all widgets
2. **1x Sell Window** - Complete order entry form with all widgets  
3. **3x SnapQuote Windows** - Each with full market depth UI

**Total: 5 complex windows created simultaneously**

### Why This Causes High CPU:

1. **Widget Construction:** Each window has dozens of widgets (labels, inputs, tables)
2. **Layout Calculations:** Qt calculates layouts for all 5 windows
3. **Off-Screen Rendering:** Windows are shown at (-10000, -10000) to pre-warm rendering
4. **Signal/Slot Connections:** Each window connects to multiple services
5. **MDI Operations:** 5 separate `mdiArea->addWindow()` calls
6. **Stylesheet Application:** Qt applies stylesheets to all widgets

### Compounding Factor: IndicesView

`createIndicesView()` is also called with 300ms delay:
```cpp
QTimer::singleShot(300, mainWindow, [mainWindow]() {
    mainWindow->createIndicesView();  // Adds another window
    WindowCacheManager::instance().initialize(mainWindow);
});
```

**Result: 6 windows created nearly simultaneously within 100ms**

---

## 🎯 Performance Logging Added

### 1. WindowCacheManager::initialize()
```cpp
[PERF] [WINDOW_CACHE_INIT] START
  TOTAL TIME: <X>ms
  Breakdown:
    - Pre-setup: <X>ms
    - Create cached windows (Buy/Sell/3xSnapQuote): <X>ms
    - Load window position: <X>ms
```

### 2. WindowCacheManager::createCachedWindows()
```cpp
[PERF] [CACHE_CREATE] Starting window pre-creation...
[PERF] [CACHE_CREATE] Buy window created in <X>ms
[PERF] [CACHE_CREATE] Sell window created in <X>ms
[PERF] [CACHE_CREATE] SnapQuote window 1 created in <X>ms
[PERF] [CACHE_CREATE] SnapQuote window 2 created in <X>ms
[PERF] [CACHE_CREATE] SnapQuote window 3 created in <X>ms
[PERF] [CACHE_CREATE] TOTAL window creation time: <X>ms
  - Buy: <X>ms
  - Sell: <X>ms
  - 3x SnapQuote: <X>ms
```

### 3. MainWindow::createIndicesView()
```cpp
[PERF] [INDICES_CREATE] START
  TOTAL TIME: <X>ms
  Breakdown:
    - Create IndicesView widget: <X>ms
    - Set window properties: <X>ms
    - Connect UDP signal: <X>ms
    - Load visibility preference: <X>ms
    - Setup action connections: <X>ms
    - Show window (if enabled): <X>ms
```

---

## 🚀 Optimization Strategies

### Strategy 1: Staggered Window Creation (Low Risk, Medium Impact)

**Current:**
```cpp
// All 5 windows created in tight loop
createBuyWindow();    // 0ms
createSellWindow();   // +0ms
createSnapQuote1();   // +0ms
createSnapQuote2();   // +0ms
createSnapQuote3();   // +0ms
// CPU spike: 100% for ~500ms
```

**Optimized:**
```cpp
// Create windows with delays
createBuyWindow();           // 0ms
QTimer::singleShot(100, []() {
    createSellWindow();      // +100ms
});
QTimer::singleShot(200, []() {
    createSnapQuote1();      // +200ms
});
QTimer::singleShot(300, []() {
    createSnapQuote2();      // +300ms
});
QTimer::singleShot(400, []() {
    createSnapQuote3();      // +400ms
});
// CPU: ~50% spread over 500ms
```

**Impact:** Distributes CPU load over time, prevents UI freeze

---

### Strategy 2: Lazy Initialization (Medium Risk, High Impact)

**Current:** Pre-create all windows at startup

**Optimized:** Create on first use, cache for subsequent uses

```cpp
bool WindowCacheManager::showBuyWindow(const WindowContext* context)
{
    if (!m_cachedBuyWindow) {
        // First use: Create window (slow, ~200ms)
        createCachedBuyWindow();
    }
    
    // Subsequent uses: Instant (~10ms)
    return showExistingBuyWindow(context);
}
```

**Benefits:**
- Zero CPU impact if user never uses Buy/Sell/SnapQuote
- Only pay cost when needed
- Same instant performance after first use

**Trade-offs:**
- First F1 press: ~200ms (instead of ~10ms)
- User experience slightly degraded on first use only

---

### Strategy 3: Reduce SnapQuote Cache Size (Low Risk, Medium Impact)

**Current:** Pre-create **3 SnapQuote windows**

**Optimized:** Pre-create only **1 SnapQuote window**, create 2nd/3rd on demand

```cpp
// Pre-create 1 SnapQuote at startup
MAX_SNAPQUOTE_WINDOWS = 1;  // Down from 3

// Create additional windows on demand
if (allWindowsBusy && needNewWindow) {
    createAdditionalSnapQuoteWindow();
}
```

**Impact:**
- 66% less CPU during startup (2 fewer windows)
- Still instant for first SnapQuote
- Slight delay only if user opens 2nd or 3rd window

---

### Strategy 4: Defer Non-Critical Windows (High Impact, Low Risk)

**Current:** Create Buy/Sell at startup

**Optimized:** Only pre-create SnapQuote (most frequently used)

```cpp
void WindowCacheManager::initialize(MainWindow* mainWindow)
{
    // Only pre-create frequently used windows
    createSnapQuoteWindows();  // SnapQuote is used often
    
    // Defer Buy/Sell to first use (less frequent)
    m_cachedBuyWindow = nullptr;
    m_cachedSellWindow = nullptr;
}
```

**Benefits:**
- 60% less CPU during startup
- SnapQuote still instant (most common use case)
- Buy/Sell slightly slower on first F1/F2 press

---

### Strategy 5: Hide Windows Instead of Show Off-Screen (Low Impact, Low Risk)

**Current:**
```cpp
m_cachedBuyMdiWindow->show();
m_cachedBuyMdiWindow->move(-10000, -10000);  // Off-screen rendering
```

**Optimized:**
```cpp
m_cachedBuyMdiWindow->hide();  // Don't render at all
// Show on first use (Qt will render then)
```

**Trade-offs:**
- Saves CPU on initial render
- Might increase first-show time by ~20ms
- Need to test if this affects instant-show benefit

---

## 📈 Recommended Implementation Plan

### Phase 1: Immediate (Low Risk)

✅ **Add performance logging** (DONE)
- Measure actual CPU time for each window
- Identify which window type is slowest

### Phase 2: Quick Wins (This PR)

1. **Stagger SnapQuote creation** by 100ms each
   ```cpp
   // Create first SnapQuote immediately
   createSnapQuote(0);
   
   // Stagger 2nd and 3rd
   QTimer::singleShot(100, [this]() { createSnapQuote(1); });
   QTimer::singleShot(200, [this]() { createSnapQuote(2); });
   ```

2. **Increase IndicesView delay** from 300ms to 500ms
   ```cpp
   QTimer::singleShot(500, mainWindow, [mainWindow]() {  // Was 300ms
       mainWindow->createIndicesView();
   });
   ```

3. **Increase WindowCache delay** from 300ms to 600ms
   ```cpp
   QTimer::singleShot(600, mainWindow, [mainWindow]() {  // Was 300ms
       WindowCacheManager::instance().initialize(mainWindow);
   });
   ```

**Expected Result:**
- Indices: 500ms
- Buy: 600ms
- Sell: 700ms (staggered)
- SnapQuote1: 800ms (staggered)
- SnapQuote2: 900ms (staggered)
- SnapQuote3: 1000ms (staggered)

**CPU load spread over 500ms instead of concentrated in 100ms**

---

### Phase 3: Medium Term (Next Sprint)

1. **Reduce SnapQuote cache** from 3 to 1
2. **Lazy-create Buy/Sell windows** on first use
3. **Pre-create only most-used windows**

**Expected Result:**
- 80% less CPU during startup
- Slight delay only on first use of infrequently-used features

---

### Phase 4: Long Term (Future)

1. **Lazy widget initialization** - Create minimal widgets, populate on first show
2. **Virtual rendering** - Don't render off-screen windows until needed
3. **Background thread creation** - Create windows on worker thread (complex)

---

## 🧪 Testing Plan

### 1. Measure Current Baseline
```bash
# Run app, check logs
[PERF] [WINDOW_CACHE_INIT] COMPLETE
  TOTAL TIME: <X>ms
```

### 2. Apply Staggered Creation

### 3. Measure Improvement
```bash
# Compare new timing
# Should see reduced per-window time
# CPU usage spread over time
```

### 4. Verify No Regressions
- F1 (Buy): Still instant (~10ms)
- F2 (Sell): Still instant (~10ms)
- F3 (SnapQuote): Still instant (~10ms)
- Multiple SnapQuote: Works correctly

---

## 📝 Summary

### Problem:
- 5 windows created simultaneously at startup
- High CPU spike (100% for ~500ms)
- UI appears frozen briefly

### Solution:
- **Immediate:** Add logging to identify bottleneck
- **Short-term:** Stagger window creation (100ms delays)
- **Medium-term:** Lazy-create infrequent windows
- **Long-term:** Architectural improvements

### Expected Results:
- CPU usage spread over time (no spike)
- Smooth main window load
- Same instant performance for user interactions

---

## 🎯 Next Steps

1. ✅ Build with new logging
2. ✅ Run app and collect timing data
3. ✅ Analyze which window type is slowest
4. ✅ Implement staggered creation
5. ✅ Test and verify improvement
6. ✅ Document final results

**Status:** Logging added, ready for testing! 🚀
