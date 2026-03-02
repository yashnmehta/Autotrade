# Comprehensive Codebase Review — Trading Terminal C++

**Date**: 28 February 2026
**Scope**: Full codebase audit — dead code, redundancy, bugs, architecture issues, and recommendations
**Codebase**: ~120+ source files, ~80+ headers, Qt 5.15.2 / C++20 / MSVC / Ninja

---

## Table of Contents

1. [Executive Summary](#1-executive-summary)
2. [Dead Code & Unused Artifacts](#2-dead-code--unused-artifacts)
3. [Redundancy & Duplication](#3-redundancy--duplication)
4. [Potential Bugs & Issues](#4-potential-bugs--issues)
5. [Memory Management Concerns](#5-memory-management-concerns)
6. [Thread Safety Issues](#6-thread-safety-issues)
7. [Architecture & Design Concerns](#7-architecture--design-concerns)
8. [Build System Issues](#8-build-system-issues)
9. [Testing Gaps](#9-testing-gaps)
10. [Logging & Debug Hygiene](#10-logging--debug-hygiene)
11. [Performance Concerns](#11-performance-concerns)
12. [Recommendations Summary](#12-recommendations-summary)

---

## 1. Executive Summary

The Trading Terminal codebase is a **large, feature-rich Qt C++ application** with a well-organized modular structure (api, core, services, views, strategy, repository, models, etc.). The architecture demonstrates solid engineering decisions: a canonical `ExchangeSegment` enum, separation of concerns via `WindowFactory` / `WorkspaceManager`, and a layered feed handler system supporting both UDP multicast and XTS WebSocket.

However, the audit reveals **several categories of issues** that, if left unaddressed, could impact maintainability, reliability, and performance:

| Category | Count | Severity |
|---|---|---|
| Dead Code / Unused Files | 12+ items | 🟡 Medium |
| Redundancy / Duplication | 8+ patterns | 🟡 Medium |
| Potential Bugs | 10+ issues | 🔴 High |
| Memory Management | 6+ concerns | 🔴 High |
| Thread Safety | 5+ issues | 🔴 High |
| Architecture Debt | 8+ items | 🟡 Medium |
| Build System | 3+ issues | 🟢 Low |
| Testing Gaps | Critical | 🔴 High |
| Debug Log Noise | Pervasive | 🟡 Medium |

---

## 2. Dead Code & Unused Artifacts

### 2.1 🔴 Completely Unused Header Files

| File | Evidence | Action |
|---|---|---|
| `include/repository/ScripMaster.h` | Zero `#include` references anywhere in `src/` or other headers. Defines a `MasterRepository` class and `Scrip` struct that are **never used**. Replaced by `ContractData` + `RepositoryManager`. | **DELETE** |
| `include/utils/LockFreeQueue.h` | Zero `#include` references in any `.cpp` or `.h` file. 227-line SPSC queue implementation. Only referenced in documentation/planning docs. | **DELETE** or move to `ref_code/` |
| `include/core/widgets/CustomMDIChild.h` | Only included by its own `.cpp` file. No other code creates or uses `CustomMDIChild`. Superseded by `CustomMDISubWindow`. | **DELETE** both `.h` and `.cpp` |

### 2.2 🟡 Archive / Backup Files in Source Tree

| File | Issue |
|---|---|
| `src/views/archive/ATMWatchWindow.cpp.bak` | Stale backup file polluting the source tree |
| `src/views/archive/OptionChainWindow.cpp.bak` | Stale backup file polluting the source tree |
| `ref_code/*.zip`, `ref_code/*.rar` | Binary reference archives (45+ MB) committed to repo |

**Action**: Remove `.bak` files. Move `ref_code/` to `.gitignore` or external storage.

### 2.3 🟡 Unused Test Files Not in Build

| File | Issue |
|---|---|
| `tests/test_global_search.cpp` | Not listed in `tests/CMakeLists.txt` — never compiled or run |
| `tests/test_options_execution.cpp` | Not listed in `tests/CMakeLists.txt` — never compiled or run |

**Action**: Either wire these into the test build or delete them.

### 2.4 🟡 Empty Source Directory

| Directory | Issue |
|---|---|
| `src/strategy/model/` | Empty folder. All model structs are header-only in `include/strategy/model/`. The CMake module doesn't reference it. |

**Action**: Remove empty directory.

### 2.5 🟡 Stub Implementations (Incomplete Code)

| File | Issue |
|---|---|
| `src/strategy/runtime/StrategyBase.cpp` | `subscribe()` method is a stub — has `// TODO: Implement proper token lookup` and empty segment branches. `unsubscribe()` is completely empty. |
| `src/services/CandleAggregator.cpp` | `onTick()` matches by segment only (not by token/symbol) — comment says "in full implementation, also match by token/symbol". Candle data goes to wrong instruments. |

---

## 3. Redundancy & Duplication

### 3.1 🟡 Duplicate Data Structures: `XTS::Tick` vs `UDP::MarketTick`

Both structures hold virtually identical market data but are maintained as **separate types**:

- `XTS::Tick` (in `include/api/xts/XTSTypes.h`) — 70+ bytes, used for WebSocket/REST data
- `UDP::MarketTick` (in `include/udp/UDPTypes.h`) — 200+ bytes, used for UDP multicast data

Both have: LTP, OHLC, volume, OI, 5-level depth, latency timestamps.

**Impact**: Every UI component that handles both data sources needs separate code paths. `FeedHandler` only publishes `UDP::MarketTick`, meaning XTS ticks must be converted.

**Recommendation**: Create a unified `MarketTick` type or add a conversion constructor. The `IMarketDataProvider` interface already defines a `Quote` struct that could serve as the canonical type, but it is **also unused** (see 3.2).

### 3.2 🟡 Unused Provider Abstraction Layer

`IMarketDataProvider` interface + `UDPBroadcastProvider` implementation exist but are **never used by the main application**:
- `MainWindow` connects directly to `UdpBroadcastService` (singleton)
- `FeedHandler` (singleton) is the actual pub-sub layer
- `XTSFeedBridge` handles XTS REST subscriptions directly

The `Quote` struct in `IMarketDataProvider.h` is a third price data type, adding to the redundancy.

**Recommendation**: Either complete the provider abstraction and route all data through it, or delete `IMarketDataProvider.h`, `UDPBroadcastProvider.h/.cpp`, and `NSEProtocol.h` to reduce confusion.

### 3.3 🟡 Duplicate Repository Load Patterns

The four `loadXXX()` methods in `RepositoryManager.cpp` (lines 224-345) — `loadNSEFO`, `loadNSECM`, `loadBSEFO`, `loadBSECM` — share **95% identical code**:

```
1. Try processed_csv subdirectory
2. Try root directory
3. Try master file with fallback name
4. Load CSV or fall back to master file
```

Each is a 25-line function with the same structure, differing only in filenames.

**Recommendation**: Extract a single parameterized `loadSegment(segmentName, csvName, masterName, repository)` method.

### 3.4 🟡 Duplicate Depth Level Structs

Three separate `DepthLevel` definitions exist:
1. `XTS::DepthLevel` in `XTSTypes.h` — `{double, int64_t, int}`
2. `UDP::DepthLevel` in `UDPTypes.h` — `{double, uint64_t, uint32_t}`
3. `Quote` struct (in `IMarketDataProvider.h`) has no depth at all

**Recommendation**: Consolidate to one canonical `DepthLevel` struct.

### 3.5 🟡 Dual Singleton Patterns for `RepositoryManager`

`RepositoryManager::getInstance()` uses both a **static local** and a **static member pointer**:

```cpp
static RepositoryManager localInstance;
if (!s_instance) {
    s_instance = &localInstance;
}
return s_instance;
```

This is needlessly complex. The raw pointer `s_instance` exists only for "API compatibility" per the comment, but all callers go through `getInstance()`.

**Recommendation**: Remove `s_instance` and use `static RepositoryManager instance;` directly.

### 3.6 🟡 `ExchangeReceiver` Type Alias

`UdpBroadcastService.h` defines `using ExchangeReceiver = ::ExchangeSegment;` with a `DEPRECATED` comment. It's still used as the parameter type in `startReceiver()`, `stopReceiver()`, `isReceiverActive()`, etc.

**Recommendation**: Replace all `ExchangeReceiver` usage with `ExchangeSegment` and remove the alias.

---

## 4. Potential Bugs & Issues

### 4.1 🔴 `FileLogger.h` — Function Definitions in Header, Static Globals

`include/utils/FileLogger.h` defines **non-inline free functions** (`setupFileLogging()`, `cleanupFileLogging()`, `messageHandler()`) and **static global variables** (`logFile`, `logMutex`) directly in the header:

```cpp
static QFile *logFile = nullptr;
static QMutex logMutex;

void messageHandler(QtMsgType type, ...) { ... }
void setupFileLogging() { ... }
void cleanupFileLogging() { ... }
```

**Problems**:
1. If this header is included in multiple translation units, each gets its own copy of `logFile` and `logMutex` — **the mutex doesn't actually protect anything** across TUs.
2. The free functions will cause **linker errors (multiple definition)** if included in more than one `.cpp`.
3. Currently works only because `AppBootstrap.cpp` is the sole includer.

**Fix**: Move function bodies to a `.cpp` file. Use `extern` declarations in the header. Make `logFile`/`logMutex` file-scoped in the `.cpp`.

### 4.2 🔴 `FeedHandler::onUdpTickReceived` — Double Timestamp Assignment

```cpp
UDP::MarketTick trackedTick = tick;
trackedTick.timestampFeedHandler = LatencyTracker::now();  // ← First assignment (line ~128)
// ... 10 lines of debug code ...
trackedTick.timestampFeedHandler = LatencyTracker::now();  // ← Second assignment (line ~138)
```

The timestamp is set twice, wasting a `LatencyTracker::now()` call and measuring the wrong interval (includes debug log overhead).

**Fix**: Remove the duplicate assignment.

### 4.3 🔴 `CandleAggregator::onTick` — Incorrect Token Matching

The tick handler matches candle builders by **segment only**, not by specific token:

```cpp
if (static_cast<int>(tick.exchangeSegment) == segment) {
    builder.update(tick);  // ← ALL builders for this segment receive ALL ticks!
}
```

If you subscribe to NIFTY and BANKNIFTY candles (both segment 2), **both receive every NSE FO tick**, corrupting candle data.

**Fix**: Match by `(segment, token)` pair using the `MarketTick::token` field.

### 4.4 🔴 `TradingDataService::onOrderEvent` — O(N) Linear Search

Order updates scan the entire order list linearly:
```cpp
for (int i = 0; i < m_orders.size(); ++i) {
    if (m_orders[i].appOrderID == order.appOrderID) {
```

With hundreds of orders, this becomes slow during high-volume trading.

**Fix**: Use a `QHash<int64_t, int>` (appOrderID → index) for O(1) lookups. Same for `onPositionEvent`.

### 4.5 🔴 `TradingDataService` — Full Data Copy on Every Event

Every single order/trade/position event triggers a **full copy** of the entire vector:
```cpp
currentOrders = m_orders; // efficient copy-on-write
```

Despite the comment about "copy-on-write", this emits a signal with the **entire** order list every time a single order changes. With 500 orders and frequent updates, this creates massive memory churn.

**Fix**: Emit individual change events (`orderAdded`, `orderUpdated`) with just the changed order.

### 4.6 🟡 `StrategyBase::subscribe()` — Completely Non-Functional

The subscribe method has empty branches and a TODO comment:
```cpp
if (m_instance.segment == 1) {        // NSECM
                                      // Not implemented fully
} else if (m_instance.segment == 2) { // NSEFO
    // TODO: Implement proper token lookup.
}
```

Any strategy that tries to subscribe to market data will silently do nothing.

### 4.7 🟡 `AppBootstrap::checkLicense` — Race Condition on Failure

When the license check fails, a `QTimer::singleShot(300, ...)` is used to show the error dialog and quit. But `run()` continues executing past `checkLicense()` to call `m_splash->preloadMasters()` and connect signals — setting up infrastructure for a session that will be killed 300ms later.

**Fix**: Return early from `run()` after `checkLicense()` detects failure, or set a flag to skip subsequent initialization.

### 4.8 🟡 `RepositoryManager` — Missing `#pragma once` / Include Guards Are Fine But...

The `RepositoryManager.h` has a peculiar access specifier issue:

```cpp
private:
public:    // ← Switches back to public mid-section
  ~RepositoryManager();
private:
  RepositoryManager();
```

The destructor is public but the constructor is private. While this is valid for singletons, the `private: public:` sequence is confusing and likely unintentional. The destructor should be private for a proper singleton.

### 4.9 🟡 `AppBootstrap::onFetchError` — Code Duplication with `onLoginComplete`

The "Continue Anyway" path in `onFetchError()` duplicates 5 lines from `onLoginComplete()`:
```cpp
CandleAggregator::instance().initialize(true);
m_mainWindow->setXTSClients(...);
m_mainWindow->setTradingDataService(...);
m_mainWindow->setConfigLoader(m_config);
m_loginWindow->showContinueButton();
```

**Fix**: Extract a shared `finalizeLogin()` method.

### 4.10 🟡 `qRegisterMetaType<ExchangeSegment>("ExchangeReceiver")`

In `AppBootstrap::registerMetaTypes()`:
```cpp
qRegisterMetaType<ExchangeSegment>("ExchangeReceiver");
```

This registers `ExchangeSegment` under the legacy name `"ExchangeReceiver"`. This works but is misleading — if any signal uses the string `"ExchangeReceiver"` for queued connections, they silently get an `ExchangeSegment`. Should be cleaned up.

---

## 5. Memory Management Concerns

### 5.1 🔴 `TALibIndicators` — Raw `new[]` Without RAII

Every indicator calculation allocates raw arrays:
```cpp
double *outReal = new double[size];
// ... TA-Lib call ...
delete[] outReal;
```

If the TA-Lib call throws or an early return is added later, **memory leaks**.

Affected functions: `calculateRSI`, `calculateMACD`, `calculateStochastic`, `calculateATR`, `calculateADX`, `calculateCCI`, `calculateOBV`, `calculateWilliamsR`, `calculateROC`, `calculateMomentum`, `calculateVWAP`, `calculateBollingerBands`, `calculateParabolicSAR`, `calculateAROON`, `calculateMFI` — **15+ functions**.

**Fix**: Use `std::unique_ptr<double[]>` or `std::vector<double>`:
```cpp
auto outReal = std::make_unique<double[]>(size);
```

### 5.2 🟡 `AppBootstrap` — Owning Raw Pointers Without Clear Ownership

```cpp
m_config = new ConfigLoader();         // Owned by AppBootstrap
m_loginService = new LoginFlowService(); // Owned by AppBootstrap
m_tradingDataService = new TradingDataService(); // Passed to MainWindow
m_mainWindow = new MainWindow(nullptr);  // Top-level widget
```

`m_tradingDataService` is created in `showLoginWindow()` but passed to `m_mainWindow`. If the window takes ownership, `AppBootstrap`'s cleanup() would double-free. Ownership semantics are unclear.

**Fix**: Use `std::unique_ptr` or Qt's parent ownership for clear lifetime management.

### 5.3 🟡 `LoginFlowService::~LoginFlowService` — Manual Delete of API Clients

```cpp
delete m_mdClient;
delete m_iaClient;
```

These are raw-deleted in the destructor. If any exception or early return happens between creation and destruction, they leak. Also, other code may hold references to these clients (e.g., `MainWindow::m_xtsMarketDataClient`).

**Fix**: Use `std::unique_ptr` and `QPointer` for shared references.

### 5.4 🟡 `FeedHandler` — Raw `new` for TokenPublisher

```cpp
TokenPublisher* pub = new TokenPublisher(compositeKey);
m_publishers[compositeKey] = pub;
```

Manual `new` with manual `delete` in destructor. If `getOrCreatePublisher` is called for an existing key (impossible due to the check, but fragile), the old publisher leaks.

**Fix**: Use `std::unique_ptr<TokenPublisher>` in the map.

### 5.5 🟡 `FileLogger.h` — `logFile` is `new`'d but Global

```cpp
logFile = new QFile(logFileName);
```

This is a globally-scoped raw `new` that is only cleaned up if `cleanupFileLogging()` is explicitly called. If the app crashes or `cleanupFileLogging()` is missed, it leaks (minor, but sloppy).

### 5.6 🟢 `LockFreeQueue` — Uses `std::unique_ptr<T[]>` ✓

The lock-free queue correctly uses smart pointers. However, since it's completely unused (see §2.1), this is moot.

---

## 6. Thread Safety Issues

### 6.1 🔴 `FeedHandler` — Mixed Mutex Types

`FeedHandler` uses `std::mutex` for `m_publishers`, but emits Qt signals (`depthUpdateReceived`, `tradeUpdateReceived`, etc.) from the UDP receiver thread. These signals are connected via `Qt::AutoConnection` (the default), which means:

- If the receiving slot is in the same thread → **direct call** (fine)
- If the receiving slot is in the GUI thread → **queued connection** (fine, but the `MarketTick` is copied)

The issue is that `onUdpTickReceived()` calls `pub->publish(trackedTick)` **inside** the lock scope, then emits type-specific signals **outside** the lock. But `publish()` emits `udpTickUpdated` which may invoke slots that call back into `FeedHandler` (e.g., another `subscribe()` call), causing a **deadlock** since `std::mutex` is not recursive.

**Fix**: Use `std::recursive_mutex`, or emit signals outside the lock (current code partially does this but `pub->publish()` is called while context suggests lock is released — verify).

Actually, re-reading the code: the lock IS released before `pub->publish(trackedTick)` because `pub` is fetched inside the lock block `{}` and publish happens after. This is correct but fragile — adding code inside the lock that calls publish would deadlock.

### 6.2 🔴 `RepositoryManager` — Dual Locking Mechanisms

`RepositoryManager` uses BOTH:
- `mutable QReadWriteLock m_repositoryLock;` (Qt-style, declared but unclear where used)
- `mutable std::shared_mutex m_expiryCacheMutex;` (C++ standard, used for expiry cache)

And in `loadAll()`, there's no locking at all during the load phase. If any thread reads repository data while `loadAll()` is running (e.g., a pre-existing ScripBar), they get partially-loaded data.

**Fix**: Consistently use one locking mechanism. Add write-lock around the entire `loadAll()` operation.

### 6.3 🟡 `UdpBroadcastService::shouldEmitSignal` — Lock in Hot Path

```cpp
inline bool shouldEmitSignal(uint32_t token) const {
    if (!m_filteringEnabled) return true;
    std::shared_lock lock(m_subscriptionMutex);
    return m_subscribedTokens.find(token) != m_subscribedTokens.end();
}
```

This shared lock is taken on **every single UDP tick** (potentially 10,000+/sec). While `shared_lock` allows concurrent reads, the lock/unlock overhead adds ~50ns per tick.

**Fix**: Consider a lock-free data structure (e.g., `std::atomic<std::shared_ptr<const std::unordered_set<uint32_t>>>`) with publish-on-write semantics.

### 6.4 🟡 `ConnectionStatusManager` — `QMutex` vs `QMutexLocker` Consistency

Some methods use `QMutexLocker lock(&m_mutex)`, which is correct. But `udpSummaryString()` calls `udpConnectedCount()` and `udpTotalCount()` which each acquire the mutex independently:

```cpp
QString ConnectionStatusManager::udpSummaryString() const {
    int connected = udpConnectedCount();  // Acquires m_mutex
    int total = udpTotalCount();           // Acquires m_mutex again
    double pps = totalUdpPacketsPerSec();  // Acquires m_mutex yet again
```

Between these calls, the state could change, yielding inconsistent summaries (e.g., `connected > total`).

**Fix**: Lock once, compute all values, then format.

### 6.5 🟡 `CandleAggregator` — QMutex in Timer Callback

`checkCandleCompletion()` is called by a `QTimer` (GUI thread) and acquires `m_mutex`. But `onTick()` is called from the UDP thread and also acquires `m_mutex`. This is correct if `QMutex` is not recursive and no re-entrant calls happen, but the `emit candleCompleted(...)` inside the lock could trigger slot code that calls back into the aggregator.

---

## 7. Architecture & Design Concerns

### 7.1 🟡 Singleton Overuse — 17 Singletons

The codebase has **17 singleton classes**:

| Singleton | Purpose |
|---|---|
| `RepositoryManager` | Contract data |
| `FeedHandler` | Pub-sub for ticks |
| `UdpBroadcastService` | UDP receivers |
| `XTSFeedBridge` | XTS subscription management |
| `ConnectionStatusManager` | Connection monitoring |
| `CandleAggregator` | OHLC candle building |
| `HistoricalDataStore` | Historical data cache |
| `PriceStoreGateway` | Distributed price stores |
| `SymbolCacheManager` | Symbol cache |
| `GreeksCalculationService` | Options Greeks |
| `WindowManager` | Window focus stack |
| `WindowCacheManager` | Window object pool |
| `PreferencesManager` | User preferences |
| `SoundManager` | Audio alerts |
| `LicenseManager` | License validation |
| `IndicatorCatalog` | Strategy indicators |
| `StrategyTemplateRepository` | Template storage |

**Impact**:
- Tight coupling — components access global state directly
- Unit testing is extremely difficult (can't inject mocks)
- Initialization order dependencies are implicit and fragile
- Shutdown order can cause use-after-free

**Recommendation**: Gradually introduce dependency injection. Start with a `ServiceLocator` or `AppContext` that holds references and is passed explicitly.

### 7.2 🟡 God Class Tendencies

`RepositoryManager` is **2089 lines** and handles:
- Loading 4 exchange segments from CSV/master files
- Index master parsing
- Asset token resolution
- Price store initialization
- Expiry cache building
- Global search (via `searchScripsGlobal`)
- ATM calculations
- Strike/token lookups

**Recommendation**: Extract `MasterFileLoader`, `ExpiryCache`, and `InstrumentSearch` as separate classes.

### 7.3 🟡 `MainWindow` Dual Role

`MainWindow` acts as both:
1. The application's primary window (UI setup, menus, toolbars)
2. A service mediator (wiring `FeedHandler`, `XTSFeedBridge`, order placement)

Order placement (`placeOrder`, `modifyOrder`, `cancelOrder`) and XTS client management don't belong in a UI class.

**Recommendation**: Extract an `OrderService` or `TradingService` class.

### 7.4 🟡 Inconsistent File Organization for Split Views

Some views are split into multiple files (e.g., `MarketWatchWindow/` has `Actions.cpp`, `Data.cpp`, `UI.cpp`, `MarketWatchWindow.cpp`), while others are monolithic (e.g., `AllIndicesWindow.cpp`, `MarketMovementWindow.cpp`). The split pattern is good but applied inconsistently.

### 7.5 🟡 `MasterDataState` — Extra Synchronization Layer

`MasterDataState` exists alongside `RepositoryManager::mastersLoaded` signal. Both communicate the same event (masters are ready). This creates confusion about which to use.

### 7.6 🟡 `FormulaEngine` — 1031-Line Recursive Descent Parser

While functionally impressive, this is a hand-written tokenizer + parser + evaluator. For a trading terminal, using a well-tested expression library (e.g., `exprtk`, `muParser`) would reduce risk and maintenance burden.

### 7.7 🟡 `WindowFactory` + `WindowCacheManager` Overlap

Both classes participate in window creation. `WindowFactory` creates windows; `WindowCacheManager` caches and reuses them. The boundary is unclear — some windows go through the cache, others through the factory directly.

### 7.8 🟢 Positive: Clean Module Separation via CMake

The per-module static library approach (`core_widgets`, `api_layer`, `services`, etc.) is well-structured and enables fast incremental builds.

---

## 8. Build System Issues

### 8.1 🟡 Multiple Unused Build Directories

The CMake configuration mentions and the project contains:
- `build/` — Active build directory  
- `build_ninja/` — Documented as the correct build directory (per coding instructions)
- `build_msvc/` — Legacy MSVC solution files

These should be consolidated.

### 8.2 🟡 MinGW Fallback Code Still Present

The `CMakeLists.txt` contains ~40 lines of MinGW-specific logic (DLL copying, static linking flags) even though the project **requires MSVC** for QtWebEngine. This dead build code adds confusion.

### 8.3 🟡 Hardcoded Paths

```cmake
set(OPENSSL_ROOT_DIR "C:/Program Files/OpenSSL-Win64" ...)
```

OpenSSL paths are hardcoded to a specific Windows installation path. This should use `find_package(OpenSSL)` as the primary method.

### 8.4 🟢 C++20 Standard ✓

Using `CMAKE_CXX_STANDARD 20` is appropriate and enables structured bindings, `std::optional`, etc.

---

## 9. Testing Gaps

### 9.1 🔴 Critical: Near-Zero Test Coverage

Only **1 test target** is compiled and run: `test_search_tokenizer` (tests the search tokenizer).

Untested critical components:
- **Order execution** (`OrderExecutionEngine`, `OptionsExecutionEngine`)
- **Strategy runtime** (`StrategyBase`, `TemplateStrategy`, `FormulaEngine`)
- **Feed handling** (`FeedHandler`, `XTSFeedBridge`, `UdpBroadcastService`)
- **Repository loading** (`RepositoryManager`, `NSEFORepositoryPreSorted`)
- **Data models** (`MarketWatchModel`, `OrderModel`, `PositionModel`)
- **Greeks calculation** (`GreeksCalculationService`, `IVCalculator`)
- **Candle aggregation** (`CandleAggregator`)

### 9.2 🔴 Orphaned Test Files

`test_global_search.cpp` and `test_options_execution.cpp` exist but are **not compiled** — they're not in `tests/CMakeLists.txt`.

### 9.3 🟡 No Integration Tests

No test verifies the end-to-end flow: login → masters loading → market data → UI update.

**Recommendation**: Prioritize unit tests for:
1. `FormulaEngine` (complex parsing logic)
2. `RepositoryManager::searchScripsGlobal` (search correctness)
3. `TradingDataService::onOrderEvent` (order matching)
4. `ATMCalculator` (strike calculations)
5. Greeks calculations (`IVCalculator`, `Greeks`)

---

## 10. Logging & Debug Hygiene

### 10.1 🟡 Excessive `FOCUS-DEBUG` Logging

**19 instances** of `qWarning() << "[...][FOCUS-DEBUG]"` across the codebase. These are development-time debug logs using `qWarning` (not `qDebug`), meaning they're **always printed** even in release builds and can't be silenced by the `FileLogger`'s debug filter.

Affected files:
- `WindowManager.cpp` (5 instances)
- `CustomMDISubWindow.cpp` (7 instances)
- `CustomMDIArea.cpp` (2 instances)
- `WindowFactory.cpp` (1 instance)
- `WindowCacheManager.cpp` (3 instances)
- `WindowCycling.cpp` (1 instance)

**Fix**: Either:
1. Change to `qDebug()` so the `FileLogger` can filter them
2. Guard with `#ifdef DEBUG_FOCUS` preprocessor flag
3. Remove entirely if focus management is stable

### 10.2 🟡 Commented-Out Debug Logs

Multiple files have blocks of commented-out debug logging:

```cpp
// qDebug() << "[FeedHandler] BSE UDP Tick - Segment:" << ...
// qDebug() << "[FeedHandler] ⚠ No UDP subscriber for BSE - Segment:" << ...
```

These pollute the code. Either use a proper log-level system or remove them.

### 10.3 🟡 Static Debug Counters

**9 instances** of `static int xxxCount = 0;` used to limit debug output to first N occurrences. These counters:
- Never reset (accumulate across the app lifetime)
- Are not thread-safe (data races on increment)
- Silently suppress ALL logs after the threshold

**Fix**: Use a proper rate-limited logging utility.

### 10.4 🟡 `fprintf(stderr, ...)` Mixed with Qt Logging

`AppBootstrap.cpp` uses 20+ `fprintf(stderr, ...)` calls alongside `qDebug()`. This bypasses the Qt message handler entirely and won't appear in log files.

---

## 11. Performance Concerns

### 11.1 🟡 `RepositoryManager::searchScripsGlobal` — Potential O(N) on 100K+ Contracts

Global search iterates through all contracts. With 100K+ contracts loaded, complex multi-token queries could be slow. The `NSEFORepositoryPreSorted` helps but only for NSEFO.

**Recommendation**: Build a prefix trie or inverted index for symbol names.

### 11.2 🟡 `MarketWatchModel` — Full Data Emit per Tick

If the market watch model emits `dataChanged` for every tick, the view re-renders the entire visible region. With 50+ rows and 1000+ ticks/sec, this causes UI lag.

**Recommendation**: Batch tick updates and emit `dataChanged` at most 30fps (one timer-driven update per 33ms).

### 11.3 🟡 `FeedHandler` — Emits 4 Global Signals per Tick

Every tick emits one of `depthUpdateReceived`, `tradeUpdateReceived`, `touchlineUpdateReceived`, or `marketWatchReceived`. These are broadcast to ALL connected slots regardless of token. Components not interested in a specific tick still receive and discard it.

**Recommendation**: Only emit type-specific signals if there are subscribers (track subscriber count per signal type).

### 11.4 🟢 `LockFreeQueue` — Good Implementation But Unused

The SPSC queue implementation is well-designed with proper memory ordering. If integrated, it could significantly reduce latency between the UDP thread and the UI thread. Currently wasted.

---

## 12. Recommendations Summary

### Priority 1 — Fix Now (Bugs / Safety)

| # | Issue | Section | Effort |
|---|---|---|---|
| P1.1 | Fix `CandleAggregator::onTick` token matching | §4.3 | 30 min |
| P1.2 | Move `FileLogger.h` functions to `.cpp` file | §4.1 | 1 hour |
| P1.3 | Replace raw `new[]` in `TALibIndicators` with `unique_ptr` | §5.1 | 1 hour |
| P1.4 | Remove duplicate timestamp in `FeedHandler::onUdpTickReceived` | §4.2 | 5 min |
| P1.5 | Fix `AppBootstrap::checkLicense` early return | §4.7 | 15 min |

### Priority 2 — Clean Up (Dead Code / Hygiene)

| # | Issue | Section | Effort |
|---|---|---|---|
| P2.1 | Delete `ScripMaster.h`, `LockFreeQueue.h`, `CustomMDIChild.h/.cpp` | §2.1 | 15 min |
| P2.2 | Delete `.bak` files in `src/views/archive/` | §2.2 | 5 min |
| P2.3 | Wire or delete orphaned test files | §2.3 | 30 min |
| P2.4 | Change `FOCUS-DEBUG` logs from `qWarning` to `qDebug` | §10.1 | 30 min |
| P2.5 | Remove commented-out debug logs | §10.2 | 30 min |
| P2.6 | Remove `ExchangeReceiver` type alias | §3.6 | 30 min |

### Priority 3 — Refactor (Architecture / Design)

| # | Issue | Section | Effort |
|---|---|---|---|
| P3.1 | Unify `XTS::Tick` and `UDP::MarketTick` | §3.1 | 1 day |
| P3.2 | Extract `loadSegment()` from duplicated load methods | §3.3 | 1 hour |
| P3.3 | Add O(1) order lookup in `TradingDataService` | §4.4 | 2 hours |
| P3.4 | Extract `OrderService` from `MainWindow` | §7.3 | 4 hours |
| P3.5 | Break up `RepositoryManager` (2089 lines) | §7.2 | 1 day |
| P3.6 | Decide on `IMarketDataProvider` — complete or delete | §3.2 | 2 hours |

### Priority 4 — Strategic (Testing / Long-term)

| # | Issue | Section | Effort |
|---|---|---|---|
| P4.1 | Add unit tests for `FormulaEngine` | §9.1 | 1 day |
| P4.2 | Add unit tests for `ATMCalculator` + Greeks | §9.1 | 1 day |
| P4.3 | Add integration test for login → data flow | §9.3 | 2 days |
| P4.4 | Introduce dependency injection for top-5 singletons | §7.1 | 3 days |
| P4.5 | Implement tick batching in `MarketWatchModel` | §11.2 | 4 hours |
| P4.6 | Integrate `LockFreeQueue` for UDP→UI pipeline | §11.4 | 1 day |

---

## Appendix A: File Inventory — Dead/Unused

```
SAFE TO DELETE:
  include/repository/ScripMaster.h           ← Never included
  include/utils/LockFreeQueue.h              ← Never included (or move to ref_code/)
  include/core/widgets/CustomMDIChild.h      ← Only self-references
  src/core/widgets/CustomMDIChild.cpp        ← Only self-references
  src/views/archive/ATMWatchWindow.cpp.bak   ← Stale backup
  src/views/archive/OptionChainWindow.cpp.bak ← Stale backup
  src/strategy/model/                        ← Empty directory
  
EVALUATE (may be needed for planned features):
  include/api/IMarketDataProvider.h          ← Unused abstraction
  include/api/UDPBroadcastProvider.h         ← Unused implementation
  src/api/UDPBroadcastProvider.cpp           ← Unused implementation
  include/api/NSEProtocol.h                  ← Only used by UDPBroadcastProvider
  tests/test_global_search.cpp              ← Not in build
  tests/test_options_execution.cpp          ← Not in build
```

## Appendix B: Singleton Registry

| Singleton | Header | Thread-Safe? | Cleanup? |
|---|---|---|---|
| `RepositoryManager` | `repository/RepositoryManager.h` | ⚠ Partial | Static local + raw ptr |
| `FeedHandler` | `services/FeedHandler.h` | ✅ `std::mutex` | Manual delete in dtor |
| `UdpBroadcastService` | `services/UdpBroadcastService.h` | ✅ `shared_mutex` | Joins threads |
| `XTSFeedBridge` | `services/XTSFeedBridge.h` | ✅ `QMutex` | QTimer cleanup |
| `ConnectionStatusManager` | `services/ConnectionStatusManager.h` | ✅ `QMutex` | disconnect() in dtor |
| `CandleAggregator` | `services/CandleAggregator.h` | ✅ `QMutex` | Stops timer |
| `HistoricalDataStore` | `services/HistoricalDataStore.h` | ? | ? |
| `PriceStoreGateway` | `data/PriceStoreGateway.h` | ? | Default dtor |
| `SymbolCacheManager` | `data/SymbolCacheManager.h` | ✅ `QMutex` | ? |
| `GreeksCalculationService` | `services/GreeksCalculationService.h` | ? | ? |
| `WindowManager` | `utils/WindowManager.h` | ❌ GUI-only | Clears maps |
| `WindowCacheManager` | `core/WindowCacheManager.h` | ❌ GUI-only | ? |
| `PreferencesManager` | `utils/PreferencesManager.h` | ? | ? |
| `SoundManager` | `utils/SoundManager.h` | ? | ? |
| `LicenseManager` | `utils/LicenseManager.h` | ? | ? |
| `IndicatorCatalog` | `strategy/builder/IndicatorCatalog.h` | ? | ? |
| `StrategyTemplateRepository` | `strategy/persistence/StrategyTemplateRepository.h` | ? | ? |

---

*End of Review*
