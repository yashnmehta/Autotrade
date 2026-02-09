# Thread Safety Audit - Trading Terminal C++

**Classification**: 🟠 HIGH PRIORITY  
**Impact**: Data corruption, race conditions, crashes  
**Estimated Fix Time**: 3-5 days  
**Risk Level**: High in production, Medium in development

---

## 📊 Executive Summary

**Thread Safety Status**: ⚠️ **MOSTLY SAFE** with critical gaps

**Summary**: Your codebase demonstrates good understanding of thread safety with extensive use of mutexes and proper Qt signal/slot patterns. However, 2 critical issues create data race opportunities.

### Key Findings:

✅ **Strengths**:
- Good use of `std::shared_mutex` for reader-writer scenarios
- Proper `Qt::QueuedConnection` for cross-thread signals
- No direct UI manipulation from worker threads

❌ **Critical Issues**:
- **RACE-001**: Price store pointer escape after lock release
- **RACE-002**: Non-thread-safe singleton initialization patterns

⚠️ **Moderate Concerns**:
- No documented lock ordering (deadlock risk)
- Mixed locking primitives (QMutex vs std::mutex)

---

## 🔍 Issue Analysis

### RACE-001: Price Store Pointer Escape (CRITICAL)

**Severity**: 🔴 **CRITICAL**  
**Likelihood**: High (occurs on every price update)  
**Impact**: Data corruption, incorrect Greeks, trading losses

---

#### The Problem

**Location**: 
- `lib/cpp_broacast_nsefo/include/nsefo_price_store.h:169-184`
- `lib/cpp_broadcast_bsefo/include/bse_price_store.h:43-48`

**Vulnerable Code**:
```cpp
const UnifiedTokenState* getUnifiedState(uint32_t token) const {
    if (token < MIN_TOKEN || token > MAX_TOKEN) return nullptr;
    
    std::shared_lock lock(mutex_);  // ✅ Acquire shared lock
    const auto* rowPtr = store_[token - MIN_TOKEN];
    if (!rowPtr) return nullptr;
    return (rowPtr->token == token) ? rowPtr : nullptr;
    // ❌ PROBLEM: Lock released here, but pointer still accessible!
}  // ← Lock destructor runs here

// Caller code (UNSAFE)
const auto* data = nsefo::g_nseFoPriceStore.getUnifiedState(token);
if (data) {
    // ❌ TORN READ: data->ltp might be from tick N
    //              data->volume might be from tick M
    double ltp = data->ltp;        // Reading unprotected memory
    int volume = data->volume;     // Another thread might be writing
}
```

---

#### Race Condition Scenario

```
Thread A (UI/Greeks Reader)          Thread B (UDP Writer)
════════════════════════════════     ═══════════════════════════
Call getUnifiedState(12345)
  Acquire shared_lock ✅
  Get pointer to data at 0x1234
  Release lock ✅
  Return pointer 0x1234
                                     Call updateTouchline(12345)
Start reading data:                    Acquire unique_lock ✅
  ltp = data->ltp                      data->ltp = 105.00  ⚠️
    (reads 100.50 - OLD)
  volume = data->volume                data->volume = 50000  ⚠️
    (reads 50000 - NEW!)                 data->high = 106.00
                                         Release lock

Result: Thread A sees inconsistent state
  - LTP from previous tick (100.50)
  - Volume from current tick (50000)
  - INCONSISTENT: Violates atomicity
```

---

#### Impact Analysis

**1. Greeks Calculation Corruption**:
```cpp
// GreeksCalculationService.cpp
auto data = nsefo::g_nseFoPriceStore.getUnifiedState(token);
double optionPrice = data->ltp;           // Might be torn read
double underlyingPrice = getUnderlying(); // Fresh read
// Now calculating Greeks with mixed time-series data!
```

**2. UI Display Issues**:
```cpp
// OptionChainWindow rendering
auto data = nsefo::g_nseFoPriceStore.getUnifiedState(token);
ui->ltp->setText(QString::number(data->ltp));      // From tick N
ui->volume->setText(QString::number(data->volume)); // From tick M
// User sees impossible combination
```

**3. Trading Decision Risks**:
```cpp
// ATM strike selection
auto data = nsefo::g_nseFoPriceStore.getUnifiedState(futureToken);
double futurePrice = data->ltp;  // Torn read
// If torn during high volatility, wrong strike selected
```

---

#### The Fix: Snapshot Pattern

**Already Implemented** ✅:
```cpp
[[nodiscard]] UnifiedTokenState getUnifiedSnapshot(uint32_t token) const {
    if (token < MIN_TOKEN || token > MAX_TOKEN) 
        return UnifiedTokenState{};
    
    std::shared_lock lock(mutex_);  // ✅ Acquire lock
    const auto* rowPtr = store_[token - MIN_TOKEN];
    if (!rowPtr || rowPtr->token != token) 
        return UnifiedTokenState{};
    
    return *rowPtr;  // ✅ Copy entire struct under lock
}  // ✅ Lock released after copy
```

**Safe Usage**:
```cpp
// ✅ SAFE: All fields are from same consistent state
auto snapshot = nsefo::g_nseFoPriceStore.getUnifiedSnapshot(token);
if (snapshot.token != 0) {  // Check if valid
    double ltp = snapshot.ltp;        // Consistent
    int volume = snapshot.volume;     // Consistent
    double change = snapshot.netChange;  // All from same tick
}
```

---

#### Remediation Plan

**Step 1**: Find all unsafe usages
```bash
cd /path/to/trading_terminal_cpp
grep -r "getUnifiedState" --include="*.cpp" --include="*.h"

# Expected OUTPUT:
# lib/cpp_broacast_nsefo/include/nsefo_price_store.h:169
# src/services/UdpBroadcastService.cpp:445  (NEEDS FIX)
# src/services/GreeksCalculationService.cpp:250  (Already using snapshot ✅)
# src/ui/OptionChainWindow.cpp:890  (NEEDS FIX)
```

**Step 2**: Replace pattern systematically
```cpp
// ❌ OLD (unsafe)
const auto* data = nsefo::g_nseFoPriceStore.getUnifiedState(token);
if (data) {
    double ltp = data->ltp;
}

// ✅ NEW (safe)
auto snapshot = nsefo::g_nseFoPriceStore.getUnifiedSnapshot(token);
if (snapshot.token != 0) {  // token==0 means not found
    double ltp = snapshot.ltp;
}
```

**Step 3**: Deprecate unsafe API
```cpp
// nsefo_price_store.h
[[deprecated("Use getUnifiedSnapshot() for thread-safe access")]]
const UnifiedTokenState* getUnifiedState(uint32_t token) const;

// Enable compiler warning
// CMakeLists.txt: add -Werror=deprecated-declarations
```

**Step 4**: Add static analysis check
```bash
# Create .clang-tidy config
Checks: '-*,modernize-use-nodiscard,readability-avoid-const-params-in-decls'
WarningsAsErrors: 'modernize-*'

# Run clang-tidy
clang-tidy src/**/*.cpp -- -std=c++17
```

---

### RACE-002: Singleton Initialization Races (HIGH)

**Severity**: 🟠 **HIGH**  
**Likelihood**: Low (only on startup, rare timing window)  
**Impact**: Multiple instances, memory leaks, crashes

---

#### Thread-Safe Singletons (GOOD ✅)

**Pattern: Meyer's Singleton** (C++11 guarantees thread safety):
```cpp
// ✅ THREAD-SAFE
GreeksCalculationService& GreeksCalculationService::instance() {
    static GreeksCalculationService inst;  // C++11: magic Static
    return inst;
}

// Why it's safe:
// - C++11 guarantees static local initialization is thread-safe
// - Guard variable prevents re-initialization
// - Only one thread can initialize, others block
```

**Found in** (all safe ✅):
- `GreeksCalculationService::instance()`
- `UdpBroadcastService::instance()`
- `FeedHandler::instance()`
- `WindowManager::instance()`
- `PreferencesManager::instance()`
- `SoundManager::instance()`

---

#### UNSAFE Singletons (NEEDS FIX ❌)

**Pattern: Check-Then-Act** (Classic TOCTOU race):
```cpp
// ❌ NOT THREAD-SAFE
RepositoryManager* RepositoryManager::s_instance = nullptr;

RepositoryManager* RepositoryManager::getInstance() {
    if (!s_instance) {  // ⚠️ CHECK (not atomic with ACT)
        s_instance = new RepositoryManager();  // ⚠️ ACT
    }
    return s_instance;
}

// Race scenario:
// Thread A: checks s_instance (nullptr) ✓
// Thread B: checks s_instance (nullptr) ✓
// Thread A: creates instance A
// Thread B: creates instance B  ← MEMORY LEAK + DOUBLE INIT
// Thread A: s_instance = A
// Thread B: s_instance = B  ← OVERWRITES A (leaked)
```

**Found in** (needs fix ❌):
- `RepositoryManager::getInstance()` - `include/repository/RepositoryManager.h:64`
- `MasterDataState::getInstance()` - `include/services/MasterDataState.h:40`
- `ATMWatchManager::getInstance()` - `include/services/ATMWatchManager.h:52`

---

#### Fix Options

**Option 1: Meyer's Singleton (Recommended)**
```cpp
// ✅ BEST: Simple, thread-safe, standard C++11
class RepositoryManager {
public:
    static RepositoryManager& getInstance() {
        static RepositoryManager instance;
        return instance;
    }
    
    // Delete copy/move
    RepositoryManager(const RepositoryManager&) = delete;
    RepositoryManager& operator=(const RepositoryManager&) = delete;
    
private:
    RepositoryManager() = default;
};

// Usage (no pointer, no cleanup needed)
RepositoryManager& repo = RepositoryManager::getInstance();
```

**Option 2: std::call_once (If pointer needed)**
```cpp
// ✅ GOOD: Thread-safe with pointer
class RepositoryManager {
public:
    static RepositoryManager* getInstance() {
        std::call_once(s_initFlag, []() {
            s_instance = new RepositoryManager();
        });
        return s_instance;
    }
    
private:
    static std::once_flag s_initFlag;
    static RepositoryManager* s_instance;
};

// In .cpp
std::once_flag RepositoryManager::s_initFlag;
RepositoryManager* RepositoryManager::s_instance = nullptr;
```

**Option 3: std::atomic (Low-level)**
```cpp
// ✅ OK: But more complex, avoid unless needed
class RepositoryManager {
public:
    static RepositoryManager* getInstance() {
        RepositoryManager* tmp = s_instance.load(std::memory_order_acquire);
        if (!tmp) {
            std::lock_guard<std::mutex> lock(s_mutex);
            tmp = s_instance.load(std::memory_order_relaxed);
            if (!tmp) {
                tmp = new RepositoryManager();
                s_instance.store(tmp, std::memory_order_release);
            }
        }
        return tmp;
    }
    
private:
    static std::atomic<RepositoryManager*> s_instance;
    static std::mutex s_mutex;
};
```

---

### LOCK-001: No Documented Lock Ordering (MEDIUM)

**Severity**: 🟡 **MEDIUM**  
**Likelihood**: Low (no deadlocks observed yet)  
**Impact**: Deadlock risk as codebase grows

---

#### The Problem

**Observation**: Multiple mutexes exist, but no documented acquisition order.

**Found Mutexes**:
1. `nsefo::g_nseFoPriceStore.mutex_` (std::shared_mutex)
2. `UdpBroadcastService::m_subscriptionMutex` (std::mutex)
3. `TradingDataService::m_positionsMutex` (QMutex)
4. `TradingDataService::m_ordersMutex` (QMutex)
5. `RepositoryManager::m_expiryCacheMutex` (std::shared_mutex)
6. `PositionWindow::m_updateMutex` (QMutex)

**Deadlock Scenario** (hypothetical):
```cpp
// Thread A (UI)
void onButtonClick() {
    QMutexLocker lock1(&m_updateMutex);  // ← Lock 1
    auto data = nsefo::g_nseFoPriceStore.getUnifiedSnapshot(token); // ← Lock 2
}

// Thread B (UDP)
void onUdpTick() {
    std::unique_lock lock1(nsefo::g_nseFoPriceStore.mutex_);  // ← Lock 2
    emit updateUI();  // → Eventually acquires m_updateMutex  // ← Lock 1
}

// Result: Thread A waits for Lock 2, Thread B waits for Lock 1 = DEADLOCK
```

---

#### Recommended Lock Hierarchy

Define clear ordering:
```cpp
// docs/LOCK_ORDER_HIERARCHY.md

Lock Hierarchy (acquire in this order):
═══════════════════════════════════════

Level 1 (Highest): Configuration/Singleton Initialization
  - RepositoryManager::s_instanceMutex

Level 2: Data Stores (can hold multiple at same level)
  - nsefo::g_nseFoPriceStore.mutex_
  - nsecm::g_nseCmPriceStore.mutex_
  - bse::g_bseFoPriceStore.mutex_

Level 3: Service Layer
  - GreeksCalculationService::m_cacheMutex
  - UdpBroadcastService::m_subscriptionMutex

Level 4: UI Layer (Lowest priority)
  - PositionWindow::m_updateMutex
  - TradingDataService::m_positionsMutex

RULE: Never acquire higher-level lock while holding lower-level lock!
```

**Enforcement**:
```cpp
// Use clang thread safety analysis
class [[clang::capability("mutex")]] PriceStore {
    mutable std::shared_mutex mutex_ [[clang::guarded_by(mutex_)]];
    
public:
    [[clang::requires_capability(mutex_)]]
    void updateTouchline(const UnifiedTokenState& data);
};
```

---

## 📊 Thread Safety Scorecard

| Component | Thread Safety | Issues | Priority |
|-----------|---------------|--------|----------|
| **Price Stores** | ⚠️ 3/5 | Pointer escape | **P1** |
| **Singletons** | ⚠️ 3/5 | Unsafe init | **P1** |
| **Qt Signals** | ✅ 5/5 | None | - |
| **Greeks Service** | ✅ 4/5 | Minor caching | P2 |
| **UDP Readers** | ✅ 5/5 | None | - |
| **UI Components** | ✅ 4/5 | No issues | - |

---

## ✅ Testing Recommendations

### 1. Thread Sanitizer (TSan)

```bash
# Compile with TSan
cmake -DCMAKE_CXX_FLAGS="-fsanitize=thread -g -O1" ..
make

# Run
./TradingTerminal

# TSan will report:
# ==================
# WARNING: ThreadSanitizer: data race (pid=1234)
#   Read of size 8 at 0x7fff1234abcd by thread T2:
#     #0 getUnifiedState() nsefo_price_store.h:180
# ==================
```

### 2. Stress Test

```cpp
// tests/stress_test_price_store.cpp
#include <gtest/gtest.h>
#include <thread>
#include <vector>

TEST(PriceStoreStressTest, ConcurrentReadWrite) {
    const int NUM_READERS = 50;
    const int NUM_WRITERS = 10;
    const int ITERATIONS = 10000;
    
    std::vector<std::thread> threads;
    std::atomic<int> errors{0};
    
    // Spawn writers
    for (int i = 0; i < NUM_WRITERS; i++) {
        threads.emplace_back([&]() {
            for (int j = 0; j < ITERATIONS; j++) {
                UnifiedTokenState state{};
                state.token = 42546;
                state.ltp = 100.0 + j;
                state.volume = j;
                nsefo::g_nseFoPriceStore.updateTouchline(state);
            }
        });
    }
    
    // Spawn readers
    for (int i = 0; i < NUM_READERS; i++) {
        threads.emplace_back([&]() {
            for (int j = 0; j < ITERATIONS; j++) {
                auto snap = nsefo::g_nseFoPriceStore.getUnifiedSnapshot(42546);
                
                // Consistency check: ltp should correlate with volume
                if (snap.token == 42546) {
                    double expected_volume = snap.ltp - 100.0;
                    if (std::abs(snap.volume - expected_volume) > 1.0) {
                        errors++;  // Torn read detected!
                    }
                }
            }
        });
    }
    
    for (auto& t : threads) t.join();
    
    EXPECT_EQ(errors.load(), 0) << "Detected torn reads!";
}
```

### 3. Helgrind (Valgrind tool)

```bash
# Linux only
valgrind --tool=helgrind --log-file=helgrin.log ./TradingTerminal

# Check log for race warnings
grep "Possible data race" helgrind.log
```

---

## 📚 Reference

### Best Practices Applied ✅

1. **Shared Mutex for Readers**: Using `std::shared_mutex` correctly
2. **Qt Threading**: Proper `Qt::QueuedConnection` usage
3. **RAII Locking**: Using lock guards, not manual lock/unlock

### Common Pitfalls Avoided ✅

1. ✅ Not calling UI methods from worker threads
2. ✅ Using Qt signals instead of direct callbacks for cross-thread
3. ✅ No global mutable state (mostly)

### Remaining Gaps ⚠️

1. ⚠️ Pointer escape from locked regions
2. ⚠️ Inconsistent singleton patterns
3. ⚠️ No lock ordering documentation

---

## 📋 Action Items

### Week 1 (P1 - Critical)
- [ ] Replace all `getUnifiedState()` with `getUnifiedSnapshot()`
- [ ] Fix RepositoryManager singleton
- [ ] Fix MasterDataState singleton
- [ ] Fix ATMWatchManager singleton

### Week 2 (P2 - Important)
- [ ] Document lock hierarchy in `docs/LOCK_ORDER_HIERARCHY.md`
- [ ] Add thread safety tests
- [ ] Run ThreadSanitizer on test suite

### Month 2 (P3 - Improvement)
- [ ] Deprecate unsafe APIs
- [ ] Add Clang thread safety annotations
- [ ] Implement lock contention monitoring

---

**Document Version**: 1.0  
**Last Updated**: February 8, 2026  
**Next Audit**: After implementing P1 fixes
