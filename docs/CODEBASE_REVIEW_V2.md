# Trading Terminal C++ — Comprehensive Codebase Review v2

**Date**: 28 February 2026  
**Reviewer**: GitHub Copilot (Unbiased Deep Analysis)  
**Scope**: Full codebase — every module, every pattern, every risk  
**Codebase Stats**: ~150 source/header files, ~30K+ LOC, Qt 5.15.2 / C++20 / MSVC / Ninja  

---

## Table of Contents

- [A. EXECUTIVE VERDICT](#a-executive-verdict)
- [B. ARCHITECTURE SCORECARD](#b-architecture-scorecard)
- [C. MODULE-BY-MODULE DEEP DIVE](#c-module-by-module-deep-dive)
  - [C1. App Layer (Bootstrap, MainWindow, WindowFactory, WorkspaceManager)](#c1-app-layer)
  - [C2. Services Layer (FeedHandler, XTSFeedBridge, UdpBroadcastService, etc.)](#c2-services-layer)
  - [C3. Repository Layer (RepositoryManager, Segment Repos)](#c3-repository-layer)
  - [C4. Strategy Engine (Template, Runtime, Persistence, Manager)](#c4-strategy-engine)
  - [C5. Data Layer (PriceStoreGateway, UnifiedPriceState, CandleData)](#c5-data-layer)
  - [C6. Models (Qt Models, Profiles, Domain)](#c6-models)
  - [C7. Views (MarketWatch, OptionChain, ATMWatch, Books, etc.)](#c7-views)
  - [C8. API Layer (XTS Clients, Transport, UDP)](#c8-api-layer)
  - [C9. Quant (Greeks, IV, TimeToExpiry)](#c9-quant)
  - [C10. Core Widgets (Custom MDI, TitleBar, MarketWatch base, etc.)](#c10-core-widgets)
  - [C11. Utils (Config, Logger, License, Preferences, etc.)](#c11-utils)
  - [C12. Lib (Broadcast Receivers, LZO)](#c12-lib)
  - [C13. Build System & Tests](#c13-build-system--tests)
- [D. CROSS-CUTTING CONCERNS](#d-cross-cutting-concerns)
  - [D1. Singleton Epidemic](#d1-singleton-epidemic)
  - [D2. Memory Management](#d2-memory-management)
  - [D3. Thread Safety](#d3-thread-safety)
  - [D4. Error Handling & Resilience](#d4-error-handling--resilience)
  - [D5. Dead Code & Technical Debt](#d5-dead-code--technical-debt)
  - [D6. Logging Discipline](#d6-logging-discipline)
- [E. CRITICAL BUGS (Ship-Stoppers)](#e-critical-bugs)
- [F. PHASED IMPROVEMENT PLAN](#f-phased-improvement-plan)
- [G. TRACKING MATRIX](#g-tracking-matrix)

---

## A. EXECUTIVE VERDICT

### What's Good (Genuinely)

The codebase has **clear strengths** that should be preserved:

| Strength | Evidence |
|---|---|
| **Clean bootstrap sequence** | `AppBootstrap` is well-phased: meta-types → TA-Lib → splash → config → license → login → main window. Easy to follow. |
| **Canonical `ExchangeSegment` enum** | Single source of truth in `core/ExchangeSegment.h` with utility functions. Eliminates magic numbers. |
| **WindowFactory extraction** | `MainWindow` delegates window creation to `WindowFactory` — proper SRP. |
| **Composite-key FeedHandler** | `(segment << 32) | token` key is correct for multi-exchange. Publisher-subscriber with `TokenPublisher` per token is efficient. |
| **XTSFeedBridge design** | Rate limiting, LRU eviction, batch processing, cooldown — this is production-grade API management. |
| **View decomposition** | Large views (MarketWatch, OptionChain, ATMWatch, SnapQuote) split into `UI.cpp`, `Data.cpp`, `Actions.cpp` — manageable file sizes. |
| **Strategy template/instance model** | Clean separation: `StrategyTemplate` (reusable blueprint) → `StrategyInstance` (deployed, bound to real instruments). |
| **UnifiedPriceState** | Single struct for all market data fields — good canonical representation. |
| **ConnectionStatusManager** | Tracks all 6 connection endpoints with lifecycle states, statistics, and primary source switching. |
| **Qt .ui file approach** | Layouts defined in Designer files, not programmatically — single source of truth for UI. |

### What's Concerning

| Concern | Severity | Impact |
|---|---|---|
| **19 singletons** | 🔴 Critical | Untestable, hidden dependencies, undefined init/destroy order |
| **Hardcoded expiry dates** | 🔴 Critical | `"27JAN2026"` in production code — will break when date passes |
| **Strategy runtime is partially stub** | 🔴 Critical | `StrategyBase::subscribe()` is a TODO, `CandleAggregator::onTick()` doesn't match by token |
| **FileLogger with static globals** | 🔴 Critical | Static `QFile*` and `QMutex` in a header = ODR violation in multi-TU builds |
| **Missing test coverage** | 🔴 Critical | 1 active test out of 4 test files; zero tests for services, models, strategy |
| **Memory ownership ambiguity** | 🟡 High | Mix of raw `new`, `deleteLater()`, `std::unique_ptr`, and bare `delete` |
| **Thread safety gaps** | 🟡 High | FeedHandler emits signals on IO thread; some views don't marshal to UI thread |
| **RepositoryManager is 559-line header** | 🟡 Medium | God-class with 50+ public methods, 20+ data members, 2 different lock types |

### Overall Assessment

**Maturity Level: Late Prototype / Early Production**

The application works for its primary use case (manual trading terminal with market data feeds), but the strategy automation subsystem is incomplete, testability is near-zero, and the singleton architecture will become a maintenance bottleneck. The codebase needs **targeted hardening** before it can be considered production-grade for automated trading.

---

## B. ARCHITECTURE SCORECARD

| Dimension | Score | Notes |
|---|---|---|
| **Modularity** | 7/10 | Good directory structure; modules are logically separated. Undermined by singleton coupling. |
| **Separation of Concerns** | 7/10 | WindowFactory/WorkspaceManager extraction is good. MainWindow still does network setup and order placement. |
| **Testability** | 2/10 | 19 singletons, no DI, no interfaces for services. Essentially untestable without major refactoring. |
| **Thread Safety** | 5/10 | Mutexes present but inconsistently applied. FeedHandler callbacks run on IO thread. |
| **Error Handling** | 4/10 | Errors logged but rarely propagated. No structured error types outside LoginFlowService. |
| **Memory Safety** | 5/10 | Qt parent-child helps, but many raw pointers with unclear ownership. |
| **Code Hygiene** | 6/10 | Dead code, commented-out debug lines, stale archive files. |
| **Documentation** | 7/10 | Good header doc comments. Many architectural docs in `docs/`. Some outdated. |
| **Performance** | 8/10 | Optimized feed path, composite keys, pre-built caches, LRU eviction. |
| **Build System** | 6/10 | CMake works but has legacy paths, missing `cpp_broadcast_bsecm` lib dir, inconsistent standards. |

---

## C. MODULE-BY-MODULE DEEP DIVE

### C1. App Layer

**Files**: `AppBootstrap.cpp/h`, `MainWindow/` (5 files), `WindowFactory.cpp/h`, `WorkspaceManager.cpp/h`, `ScripBar.cpp/h`, `SnapQuoteScripBar.cpp/h`

#### ✅ Strengths
- Bootstrap sequence is clean and well-documented with phase comments
- `WindowFactory` properly extracted from `MainWindow` — true SRP
- Modal login dialog pattern prevents accessing main UI before authentication
- Workspace save/restore delegated to `WorkspaceManager`

#### 🔴 Issues

**1. Hardcoded Expiry Dates in MainWindow.cpp (CRITICAL)**
```
Lines 161-162:
atm.addWatch("NIFTY", "27JAN2026", source);
atm.addWatch("BANKNIFTY", "27JAN2026", source);
```
This date has already passed (current date: Feb 28, 2026). The ATM Watch will show stale/empty data. **Must use `RepositoryManager::getCurrentExpiry()` dynamically.**

**2. MainWindow still handles order placement (400+ lines)**
`placeOrder()`, `modifyOrder()`, `cancelOrder()` are each 80-120 lines of callback-heavy code with nested `QMetaObject::invokeMethod`. This should be extracted to an `OrderService` or handled by `TradingDataService`.

**3. `onTickReceived()` does XTS→UDP conversion + index tick routing (~80 lines)**
This business logic doesn't belong in the main window. Should be in `FeedHandler` or a dedicated `TickRouter`.

**4. Config path search has 8 candidate paths**
`loadConfiguration()` checks 8 different paths including macOS bundle paths and deep nesting. This is fragile. Should use a single env var or a well-defined search order.

**5. AppBootstrap doesn't clean up LoginFlowService/TradingDataService**
`m_loginService` and `m_tradingDataService` are created in `showLoginWindow()` but `cleanup()` only shuts down TA-Lib. These objects leak on application exit.

---

### C2. Services Layer

**Files**: `FeedHandler`, `XTSFeedBridge`, `UdpBroadcastService`, `CandleAggregator`, `TradingDataService`, `LoginFlowService`, `ConnectionStatusManager`, `GreeksCalculationService`, `ATMWatchManager`, `TokenSubscriptionManager`, `HistoricalDataStore`, `MasterLoaderWorker`, `MasterDataState`

#### ✅ Strengths
- `FeedHandler` composite-key pub/sub is efficient and correct
- `XTSFeedBridge` rate limiting, LRU eviction, and cooldown are production-quality
- `ConnectionStatusManager` tracks 6 endpoints with granular state machine
- `TradingDataService` is properly thread-safe with per-collection mutexes

#### 🔴 Issues

**1. CandleAggregator::onTick() doesn't match by token (CRITICAL for strategy)**
The candle aggregator receives ticks but doesn't properly route them to the correct symbol's candle builder by token. This means candle data can be attributed to wrong instruments. Strategy backtesting/live execution depends on correct candles.

**2. FeedHandler emits Qt signals on IO thread**
`onUdpTickReceived()` calls `pub->publish()` which emits `udpTickUpdated`. If the receiver is a UI widget, this crosses thread boundaries without `Qt::QueuedConnection`. The `subscribe()` template method uses `connect()` with default connection type — this is **auto-detected** by Qt (queued if cross-thread), but only if the receiver lives on the main thread AND has a running event loop. Safe in practice but fragile by design.

**3. FeedHandler also emits 4 broadcast signals (`depthUpdateReceived`, etc.) on every tick**
These global signals are emitted for EVERY tick regardless of whether anyone is connected. With 10K+ ticks/sec from UDP, this creates significant signal overhead even with zero receivers.

**4. `timestampFeedHandler` is set twice in `onUdpTickReceived()`**
Lines ~130 and ~137 in FeedHandler.cpp both set `trackedTick.timestampFeedHandler = LatencyTracker::now()`. The second write overwrites the first — minor but indicates copy-paste oversight.

**5. LoginFlowService creates XTS clients with raw `new` and manually `delete`s them**
```cpp
delete m_mdClient;    // line 44
delete m_iaClient;    // line 45
// ... later in executeLogin():
delete m_mdClient;    // line 171
delete m_iaClient;    // line 176
```
Double-delete risk if `executeLogin()` is called concurrently. Should use `std::unique_ptr`.

**6. 13 services, 10 are singletons**
`FeedHandler`, `XTSFeedBridge`, `UdpBroadcastService`, `CandleAggregator`, `ConnectionStatusManager`, `GreeksCalculationService`, `ATMWatchManager`, `HistoricalDataStore`, `PriceStoreGateway`, `SymbolCacheManager` — all singletons. This makes isolated testing impossible.

---

### C3. Repository Layer

**Files**: `RepositoryManager.h` (559 lines), `NSEFORepository`, `NSECMRepository`, `BSEFORepository`, `BSECMRepository`, `NSEFORepositoryPreSorted`, `MasterFileParser`, `ContractData.h`, `ScripMaster.h`

#### ✅ Strengths
- Pre-built caches (`buildExpiryCache()`) for O(1) strike/token lookup — excellent for ATM Watch
- Dual-index approach: `m_symbolExpiryStrikes`, `m_strikeToTokens`, `m_symbolToAssetToken`
- `NSEFORepositoryPreSorted` with symbol-indexed sorted data for fast search
- `resolveIndexAssetTokens()` correctly handles the -1 sentinel problem

#### 🔴 Issues

**1. RepositoryManager is a 559-line god-header with 50+ public methods**
This class does: loading, searching, caching, updating, statistics, segment mapping, path resolution, and signal emission. Needs decomposition into:
- `MasterLoader` (load/save/parse)
- `ContractSearchService` (search/filter)
- `ExpiryCacheService` (ATM Watch caches)
- `RepositoryManager` (thin coordinator)

**2. Two different locking mechanisms**
`QReadWriteLock m_repositoryLock` AND `std::shared_mutex m_expiryCacheMutex` protect different data in the same class. This is confusing and increases deadlock risk if methods that hold one lock call methods that need the other.

**3. `ScripMaster.h` is completely dead code**
Zero includes anywhere. Should be deleted.

**4. `getContractByToken()` has 3 overloads with different signatures**
```cpp
const ContractData* getContractByToken(int exchangeSegmentID, int64_t token) const;
const ContractData* getContractByToken(const QString &segmentKey, int64_t token) const;
const ContractData* getContractByToken(const QString &exchange, const QString &segment, int64_t token) const;
```
The 3-argument version just calls `getSegmentKey()` then delegates to the 2-argument version. Unnecessary API surface.

**5. `getMastersDirectory()` is a static method with platform-specific logic**
Hardcoded paths like `~/TradingTerminal/Masters`. Should use `QStandardPaths`.

---

### C4. Strategy Engine

**Files**: 
- Model: `StrategyTemplate.h` (392 lines), `StrategyInstance.h`, `ConditionNode.h`
- Builder: 9 files (ConditionBuilder, IndicatorCatalog, IndicatorRow, ParamEditor, SymbolBinding, etc.)
- Runtime: `TemplateStrategy`, `StrategyBase`, `FormulaEngine`, `IndicatorEngine`, `LiveFormulaContext`, `OptionsExecutionEngine`, `OrderExecutionEngine`, `StrategyFactory`
- Manager: `StrategyService`, `StrategyManagerWindow`, `CreateStrategyDialog`, `StrategyTableModel`, `StrategyFilterProxyModel`, `ModifyParametersDialog`
- Persistence: `StrategyRepository` (SQLite), `StrategyTemplateRepository` (SQLite)

#### ✅ Strengths
- Template/Instance separation is architecturally sound
- `ConditionNode` tree with AND/OR branching is flexible
- `FormulaEngine` with `ParamTrigger` (EveryTick, OnCandleClose, OnEntry, etc.) is well-designed
- `IndicatorCatalog` loads from `indicator_defaults.json` — data-driven configuration
- `SymbolBinding` with strike selection modes (ATM-relative, premium-based, fixed) is comprehensive
- Expression parameter system (`__expr__:formula`) is powerful

#### 🔴 Issues

**1. StrategyBase::subscribe() is a TODO stub (CRITICAL)**
```cpp
void StrategyBase::subscribe() {
    // TODO: Implement proper token lookup.
    ...
}
```
The core mechanism for subscribing a running strategy to market data is unimplemented. `TemplateStrategy::start()` calls this — meaning no strategy can actually receive live data.

**2. StrategyBase::unsubscribe() is completely empty**
```cpp
void StrategyBase::unsubscribe() {
    // Empty — resources not released
}
```
Running strategies will leak FeedHandler subscriptions.

**3. Strategy model directory is empty**
`src/strategy/model/` exists but has zero files. All model types are header-only in `include/strategy/model/`. The empty dir should be removed.

**4. StrategyService::onUpdateTick() — MTM computation is a TODO**
```cpp
// TODO (Phase 2): Compute MTM from FeedHandler
```
Strategy P&L tracking doesn't work.

**5. `StrategyTemplate.h` is 392 lines of structs**
While header-only models are fine, this file defines 10+ types including enums, structs, inline functions, and type aliases. Should be split: `StrategyEnums.h`, `SymbolDefinition.h`, `IndicatorDefinition.h`, `TemplateParam.h`, `RiskDefaults.h`, `StrategyTemplate.h`.

**6. Backward-compatibility aliases accumulate technical debt**
```cpp
using SymbolSegment = ::ExchangeSegment;
using TradeSymbolType = ::ExchangeSegment;
```
Plus `SymbolDefinition` has an unused `tradeType` member. These should be removed with a find-and-replace pass.

---

### C5. Data Layer

**Files**: `UnifiedPriceState.h` (122 lines), `CandleData.h`, `PriceStoreGateway.cpp/h`, `SymbolCacheManager.cpp/h`

#### ✅ Strengths
- `UnifiedState` is a comprehensive, cache-friendly struct covering all market data
- `PriceStoreGateway` singleton for unified access
- `CandleData` with proper OHLCV + timeframe enum

#### 🟡 Issues

**1. `UnifiedState` uses fixed-size char arrays**
```cpp
char symbol[32] = {0};
char displayName[64] = {0};
```
Risk of buffer overflow if exchange provides longer names. Consider `QString` or at minimum `std::array<char, N>` with bounds-checked copy.

**2. `PriceStoreGateway` is another singleton**
Should be injected into services that need it.

---

### C6. Models

**Files**: `MarketWatchModel`, `OrderModel`, `PositionModel`, `TradeModel`, `PinnedRowProxyModel`, `MarketWatchColumnProfile`, `TokenAddressBook`, `GenericProfileManager`, `WindowContext`

#### ✅ Strengths
- Qt model-view pattern correctly applied with `QAbstractTableModel` subclasses
- `PinnedRowProxyModel` for pinning rows — nice feature
- `GenericProfileManager` for column profile persistence
- `IMarketWatchViewCallback` interface for low-latency updates (bypassing Qt signals)

#### 🟡 Issues

**1. TokenAddressBook has both `.cpp` and exists in models but also referenced from views**
Cross-module dependency — views reach into models layer directly.

**2. No model unit tests**
`MarketWatchModel`, `OrderModel`, `PositionModel` have complex `data()` and `setData()` implementations with no test coverage.

---

### C7. Views

**Files**: MarketWatch (4 files), OptionChain (4 files), ATMWatch (4 files), SnapQuote (4 files), OrderBook, PositionWindow, TradeBook, BuyWindow, SellWindow, IndicesView, AllIndicesWindow, MarketMovementWindow, OptionCalculatorWindow, PreferenceDialog (5 files), and helpers

#### ✅ Strengths
- Consistent decomposition: `Window.cpp`, `UI.cpp`, `Data.cpp`, `Actions.cpp`
- `BaseBookWindow` and `BaseOrderWindow` provide shared functionality for book/order views
- `.ui` files used for dialogs — proper separation of layout and logic
- Context-aware window opening via `WindowContext`

#### 🟡 Issues

**1. Archive `.bak` files in source tree**
`src/views/archive/ATMWatchWindow.cpp.bak` and `OptionChainWindow.cpp.bak` should be removed.

**2. Copy operations are TODO stubs in multiple book windows**
```cpp
// In TradeBookWindow.cpp, OrderBookWindow.cpp, PositionWindow.cpp:
// TODO: copy selected rows
```
Ctrl+C doesn't work in these windows.

**3. MarketWatchWindow is 390 lines in header alone**
The header includes `<unordered_map>`, `<QMap>`, `<QTimer>`, `<QSettings>` — implementation details leaking into the header. Use forward declarations and pimpl.

---

### C8. API Layer

**Files**: `XTSMarketDataClient`, `XTSInteractiveClient`, `SocketIOInteractiveClient`, `NativeWebSocketClient` (in transport/), `UDPBroadcastProvider`, `XTSTypes.h`, `IMarketDataProvider.h`, `NSEProtocol.h`

#### ✅ Strengths
- `IMarketDataProvider` interface exists — good abstraction
- `XTSTypes.h` has well-structured order/position/trade types
- `SocketIOInteractiveClient` for real-time order events
- `NativeWebSocketClient` using Boost.Beast — no Qt WebSocket dependency for perf-critical path

#### 🟡 Issues

**1. Duplicate tick structures: `XTS::Tick` vs `UDP::MarketTick`**
Both carry nearly identical data. `MainWindow::onTickReceived()` manually converts XTS→UDP. Should unify into a single `MarketTick` type.

**2. `NSEProtocol.h` — unclear if used**
Only referenced in includes; may be legacy.

**3. NativeWebSocketClient subscription tracking is a TODO**
```cpp
.subscriptions = 0 // TODO: track subscriptions
```

---

### C9. Quant

**Files**: `Greeks.h/cpp`, `IVCalculator.h/cpp`, `TimeToExpiry.h/cpp`, `ATMCalculator.h`

#### ✅ Strengths
- Clean Black-Scholes implementation with structured input/output
- Newton-Raphson IV solver
- `ATMCalculator` with multiple ATM selection strategies (nearest, straddle, interpolated)
- `TimeToExpiry` handles Indian market hours correctly

#### 🟡 Issues

**1. `ATMCalculator.h` has no corresponding `.cpp`**
Header-only is fine for templates, but this has non-trivial logic that should be in a `.cpp`.

**2. No validation in Greeks calculator**
`calculate()` doesn't validate inputs (negative time, zero strike, zero vol). Division by zero is possible.

---

### C10. Core Widgets

**Files**: `CustomMainWindow`, `CustomMDIArea`, `CustomMDISubWindow`, `CustomMDIChild` (dead), `CustomTitleBar`, `CustomMarketWatch`, `CustomNetPosition`, `CustomOrderBook`, `CustomTradeBook`, `CustomScripComboBox`, `InfoBar`, `MDITaskBar`

#### ✅ Strengths
- `CustomMainWindow` provides frameless window with resize grips and title bar
- `CustomMDISubWindow` has window type tracking for workspace persistence
- `MDITaskBar` provides window switching UI

#### 🟡 Issues

**1. `CustomMDIChild` is dead code** — superseded by `CustomMDISubWindow`. Delete both `.h` and `.cpp`.

**2. "Custom" prefix on everything adds no information**
`CustomMainWindow`, `CustomMDIArea`, `CustomMDISubWindow`, `CustomTitleBar`, `CustomScripComboBox`, `CustomMarketWatch`, `CustomNetPosition`, `CustomTradeBook`, `CustomOrderBook` — every single class has "Custom". Since they're ALL custom, the prefix is meaningless. Consider: `TradingMainWindow`, `TradingMDIArea`, etc. or just drop the prefix.

**3. `GlobalShortcuts.cpp` and `GlobalConnections.cpp` are in `src/core/` but define MainWindow methods**
These files implement methods of `MainWindow` but live in the `core/` directory. This is misleading — they should be in `src/app/MainWindow/`.

---

### C11. Utils

**Files**: `ConfigLoader`, `FileLogger.h`, `LicenseManager`, `PreferencesManager`, `WindowManager`, `DateUtils`, `MemoryProfiler`, `SoundManager`, `ClipboardHelpers`, `SelectionHelpers`, `BookWindowHelper.h`, `TableProfileHelper.h`, `WindowSettingsHelper.h`, `LatencyTracker.h`, `LockFreeQueue.h`

#### 🔴 Critical Issues

**1. FileLogger.h defines functions AND static variables in a header**
```cpp
static QFile *logFile = nullptr;      // ODR violation
static QMutex logMutex;               // ODR violation
void messageHandler(...) { ... }      // ODR violation if included in multiple TUs
void setupFileLogging() { ... }       // ODR violation
void cleanupFileLogging() { ... }     // ODR violation
```
If this header is included in more than one `.cpp` file, each TU gets its own copy of `logFile` and `logMutex`, leading to:
- Log file opened multiple times
- Mutex doesn't actually synchronize
- Undefined behavior per C++ standard

**Currently** this works because only `AppBootstrap.cpp` calls `setupFileLogging()`, but it's a time bomb. Must be split into `.h` (declarations) + `.cpp` (definitions).

**2. `LockFreeQueue.h` is completely unused**
227 lines, zero `#include` references. Well-implemented SPSC queue but dead code.

**3. Header-only helpers with unclear usage**
- `BookWindowHelper.h` — no corresponding `.cpp`, grep shows limited usage
- `TableProfileHelper.h` — same pattern
- `WindowSettingsHelper.h` — same pattern

These may be legitimate header-only utilities, but should be audited for actual usage.

**4. `LatencyTracker.h` — used in FeedHandler but results never displayed/logged**
The latency tracking infrastructure exists but no UI or log output ever shows the measurements.

---

### C12. Lib

**Files**: `lib/cpp_broadcast_nsefo/`, `lib/cpp_broadcast_nsecm/`, `lib/cpp_broadcast_bsefo/`, `lib/lzo-2.10/`, `lib/common/`

#### 🟡 Issues

**1. `cpp_broadcast_bsecm` directory is referenced in CMake but doesn't exist**
```cmake
# In CMakeLists.txt line 258:
${CMAKE_SOURCE_DIR}/lib/cpp_broadcast_bsecm/include
```
But `lib/cpp_broadcast_bsecm/` does NOT exist as a directory. The BSE CM receiver implementation is bundled inside `cpp_broadcast_bsefo/` (which handles both BSE FO and CM via `bse_receiver.h`). The include path is harmless (CMake doesn't error on missing include dirs) but is misleading.

**2. `cpp_broadcast_bsefo/` contains stale autogen files from different machine**
```
cpp_broadcast_bsefo_autogen/deps references:
/Users/yashmehta/Desktop/go_proj/trading_terminal_cpp/...
```
These are absolute paths from a different developer's machine. Should be in `.gitignore`.

---

### C13. Build System & Tests

#### Build System

**CMakeLists.txt**: 280 lines, well-structured with optional components.

**Issues:**
1. **C++ standard inconsistency**: Root sets `CMAKE_CXX_STANDARD 20`, but `tests/CMakeLists.txt` sets `CXX_STANDARD 17`. Pick one.
2. **OpenSSL paths hardcoded for Windows**: `C:/Program Files/OpenSSL-Win64` — won't work on other machines without the same install path.
3. **TA-Lib is `OFF` by default**: Users must explicitly enable it, but the code conditionally compiles with `#ifdef HAVE_TALIB`. Should document this clearly or default to ON when found.
4. **No `install` target**: No way to package the application.

#### Tests

**Only 1 active test** (`test_search_tokenizer`) out of 4 test files:

| Test File | Status |
|---|---|
| `test_search_tokenizer_simple.cpp` | ✅ Active (in CMake) |
| `test_search_tokenizer.cpp` | ❌ Not in CMake — dead |
| `test_global_search.cpp` | ❌ Not in CMake — dead |
| `test_options_execution.cpp` | ❌ Not in CMake — dead |

**Critical testing gaps:**
- Zero tests for `FeedHandler`, `XTSFeedBridge`, `CandleAggregator`
- Zero tests for `RepositoryManager` (loading, searching, caching)
- Zero tests for `StrategyTemplate` serialization/deserialization
- Zero tests for `TemplateStrategy` condition evaluation
- Zero tests for `FormulaEngine` expression parsing
- Zero tests for `Greeks` / `IVCalculator` calculations
- Zero tests for `OrderModel`, `PositionModel`, `TradeModel`
- Zero tests for `TradingDataService` thread safety

---

## D. CROSS-CUTTING CONCERNS

### D1. Singleton Epidemic

**19 singletons identified:**

| # | Singleton | Module | Thread-Safe? |
|---|---|---|---|
| 1 | `FeedHandler::instance()` | services | ✅ `std::mutex` |
| 2 | `XTSFeedBridge::instance()` | services | ✅ `QMutex` |
| 3 | `UdpBroadcastService::instance()` | services | Partial |
| 4 | `CandleAggregator::instance()` | services | ✅ `QMutex` |
| 5 | `ConnectionStatusManager::instance()` | services | ✅ `QMutex` |
| 6 | `GreeksCalculationService::instance()` | services | Partial |
| 7 | `ATMWatchManager::getInstance()` | services | Unknown |
| 8 | `HistoricalDataStore::instance()` | services | Unknown |
| 9 | `TokenSubscriptionManager::instance()` | services | Unknown |
| 10 | `PriceStoreGateway::instance()` | data | Unknown |
| 11 | `SymbolCacheManager::instance()` | data | Unknown |
| 12 | `RepositoryManager::getInstance()` | repository | ✅ RWLock |
| 13 | `StrategyService::instance()` | strategy | ✅ `QMutex` |
| 14 | `StrategyTemplateRepository::instance()` | strategy | Unknown |
| 15 | `IndicatorCatalog::instance()` | strategy | Unknown |
| 16 | `PreferencesManager::instance()` | utils | Unknown |
| 17 | `SoundManager::instance()` | utils | Unknown |
| 18 | `WindowManager::instance()` | utils | Unknown |
| 19 | `LicenseManager::instance()` | utils | Unknown |
| 20* | `WindowCacheManager::instance()` | core | Unknown |

**Impact**: 
- **Unit testing is effectively impossible** — every service grabs its dependencies via `::instance()`, so you can't inject mocks.
- **Initialization order is implicit** — `setConfigLoader()` triggers `GreeksCalculationService::instance()`, `ConnectionStatusManager::instance()`, etc. If called in wrong order, you get crashes.
- **Destruction order is undefined** — static local singletons are destroyed in reverse construction order. If `FeedHandler` is destroyed before `UdpBroadcastService`, the UDP callbacks will crash.

**Recommendation**: Phase 1: Create a `ServiceRegistry` that owns all service instances with explicit lifetime. Phase 2: Pass services via constructor injection.

---

### D2. Memory Management

| Pattern | Count | Risk |
|---|---|---|
| Raw `new` with Qt parent | ~50+ | 🟢 Low — Qt parent-child handles cleanup |
| Raw `new` without parent | ~10 | 🟡 Medium — depends on manual cleanup |
| `std::unique_ptr` | ~5 | 🟢 Low — correct RAII |
| `deleteLater()` | ~22 | 🟢 Low — correct for cross-thread Qt objects |
| Manual `delete m_xxx` | ~7 | 🟡 Medium — double-delete risk |
| Singleton static locals | ~19 | 🟡 Medium — destruction order issues |

**Specific risks:**
1. `LoginFlowService` does manual `delete m_mdClient` / `delete m_iaClient` in both destructor AND `executeLogin()` — double-delete if concurrent.
2. `AppBootstrap` doesn't delete `m_loginService` or `m_tradingDataService` in `cleanup()`.
3. `FeedHandler` manually deletes `TokenPublisher*` in destructor — should use `std::unique_ptr`.
4. `ParamEditorDialog::rebuildParamRows()` calls `delete m_paletteContent->layout()` — undefined behavior per Qt docs. Should use `QWidget::setLayout(nullptr)` or reparent.

---

### D3. Thread Safety

| Issue | Location | Severity |
|---|---|---|
| FeedHandler signals emitted on IO thread | `FeedHandler::onUdpTickReceived()` | 🟡 Works due to Qt auto-connection but fragile |
| LoginFlowService raw pointer delete without lock | `executeLogin()` lines 171-176 | 🔴 Race condition |
| RepositoryManager dual-lock (QReadWriteLock + shared_mutex) | `RepositoryManager.h` | 🟡 Confusing, deadlock risk |
| UdpBroadcastService subscription set uses `shared_mutex` | `subscribeToken()` | 🟢 Correct |
| XTSFeedBridge queue processing on timer thread | `processPendingQueue()` | 🟢 Mutex protected |
| CandleAggregator `onTick()` and `checkCandleCompletion()` | Both access builders | ✅ QMutex |
| Static `logFile` / `logMutex` in FileLogger.h | ODR violation | 🔴 Each TU has its own copy |

---

### D4. Error Handling & Resilience

**Good patterns:**
- `LoginFlowService::FetchError` struct with `anyFailed()` / `summary()` — excellent
- License check with informative error dialog
- XTSFeedBridge rate limit detection and cooldown

**Missing patterns:**
- No structured error type for order placement failures (just QString messages in callbacks)
- No circuit breaker for rapid feed disconnection/reconnection
- No graceful degradation if RepositoryManager fails to load masters (app continues with null data)
- Strategy engine has no error recovery — if `loadTemplate()` fails, strategy is silently dead
- No health check / heartbeat for UDP receivers
- No logging of performance degradation (e.g., if FeedHandler queue grows)

---

### D5. Dead Code & Technical Debt

| Item | Type | Action |
|---|---|---|
| `include/repository/ScripMaster.h` | Dead header | **DELETE** |
| `include/utils/LockFreeQueue.h` | Dead header (well-implemented but unused) | Move to `ref_code/` |
| `include/core/widgets/CustomMDIChild.h/.cpp` | Dead class | **DELETE** |
| `src/views/archive/*.bak` | Backup files | **DELETE** |
| `src/strategy/model/` (empty dir) | Empty directory | **DELETE** |
| `tests/test_global_search.cpp` | Dead test file | Wire into CMake or delete |
| `tests/test_options_execution.cpp` | Dead test file | Wire into CMake or delete |
| `SymbolSegment` / `TradeSymbolType` aliases | Backward-compat aliases | Replace with `ExchangeSegment` |
| `SymbolDefinition::tradeType` member | Unused member | Remove |
| `ref_code/*.zip` / `*.rar` | Binary archives (~45MB) | Move to external storage, `.gitignore` |
| 8 TODO comments in production code | Incomplete features | Prioritize completion or document as known limitations |

---

### D6. Logging Discipline

**Problems:**
1. **Debug-level messages silenced entirely** in `messageHandler()` — means you lose ALL `qDebug()` output. This is too aggressive.
2. **`fprintf(stderr, ...)` used alongside `qDebug()`/`qWarning()`** in AppBootstrap — bypasses the logging framework.
3. **Commented-out debug logging** in FeedHandler.cpp (BSE tick counts) — should be behind a log level, not commented code.
4. **No log rotation** — `setupFileLogging()` creates a new file per session but never cleans up old ones. Disk will fill over weeks.
5. **Log file created in relative `logs/` directory** — depends on working directory. Should use `QStandardPaths`.

---

## E. CRITICAL BUGS (Ship-Stoppers)

These issues will cause visible failures in production:

| # | Bug | Impact | Fix Effort |
|---|---|---|---|
| **E1** | Hardcoded `"27JAN2026"` expiry in MainWindow.cpp | ATM Watch shows empty/stale data after this date passes | 15 min |
| **E2** | `StrategyBase::subscribe()` is a TODO stub | No strategy can receive live market data | 2-4 hours |
| **E3** | `StrategyBase::unsubscribe()` is empty | FeedHandler subscriptions leak when strategies stop | 30 min |
| **E4** | `CandleAggregator::onTick()` doesn't match by token | Candle data attributed to wrong instruments | 2 hours |
| **E5** | `StrategyService::onUpdateTick()` MTM is TODO | Strategy P&L is always 0 | 2-4 hours |
| **E6** | FileLogger.h ODR violation | Logging silently broken if header included in multiple TUs | 1 hour |
| **E7** | `LoginFlowService` raw delete without lock | Race condition / potential crash on re-login | 30 min |
| **E8** | Copy operations are TODO in 3 book windows | Ctrl+C doesn't work in TradeBook, OrderBook, PositionWindow | 1-2 hours |

---

## F. PHASED IMPROVEMENT PLAN

### Phase 0: Emergency Fixes (Day 1) ⏱ ~4 hours

Fix ship-stopping bugs that affect daily usage.

| ID | Task | Ref | Est. | Status |
|---|---|---|---|---|
| P0.1 | Replace hardcoded `"27JAN2026"` with `RepositoryManager::getCurrentExpiry()` | E1 | 15 min | ✅ |
| P0.2 | Fix FileLogger.h — move to `.h` + `.cpp` split | E6 | 1 hr | ✅ |
| P0.3 | Fix LoginFlowService double-delete — remove Qt parent from manually-managed clients, null-after-delete, disconnect before delete | E7 | 30 min | ✅ |
| P0.4 | Fix duplicate `timestampFeedHandler` assignment in FeedHandler | D3 | 5 min | ✅ |
| P0.5 | Implement Ctrl+C copy in BaseBookWindow (inherited by all book windows) | E8 | 1.5 hr | ✅ |
| P0.6 | Clean up AppBootstrap leak — delete `m_loginService`/`m_tradingDataService`/`m_config`/`m_mainWindow` in cleanup() | D2 | 15 min | ✅ |

### Phase 1: Dead Code Cleanup (Day 2) ⏱ ~2 hours

Remove confirmed dead code to reduce noise. Files moved to `archive/dead_code/` (not deleted).

| ID | Task | Ref | Est. | Status |
|---|---|---|---|---|
| P1.1 | Move `ScripMaster.h` → `archive/dead_code/` + remove from CMakeLists | D5 | 5 min | ✅ |
| P1.2 | Move `CustomMDIChild.h/.cpp` → `archive/dead_code/` + remove from CMakeLists | D5 | 5 min | ✅ |
| P1.3 | Move `LockFreeQueue.h` → `archive/dead_code/` + remove from CMakeLists | D5 | 5 min | ✅ |
| P1.4 | ~~Delete `src/views/archive/*.bak`~~ — already clean, no `.bak` files found | D5 | 2 min | ✅ N/A |
| P1.5 | Delete empty `src/strategy/model/` | D5 | 1 min | ✅ |
| P1.6 | Move 3 unwired test files → `archive/dead_code/tests/` | D5 | 30 min | ✅ |
| P1.7 | ~~Remove backward-compat aliases~~ — DEFERRED: `tradeType` alias still used by `StrategyTemplateRepository` for loading legacy saved JSON | D5 | 30 min | ⏸ |
| P1.8 | Add `archive/` to `.gitignore` | D5 | 2 min | ✅ |
| P1.9 | Fix ghost `cpp_broadcast_bsecm` include path in CMake (2 files) | C12 | 5 min | ✅ |
| P1.10 | ~~Remove stale autogen files~~ — none found, already clean | C12 | 5 min | ✅ N/A |

### Phase 2: Strategy Engine Completion (Week 1) ⏱ ~3-5 days

Make the strategy engine actually functional.

| ID | Task | Ref | Est. | Status |
|---|---|---|---|---|
| P2.1 | Implement `StrategyBase::subscribe()` — proper token lookup + FeedHandler subscription | E2 | 3 hr | ✅ |
| P2.2 | Implement `StrategyBase::unsubscribe()` — cleanup FeedHandler subscriptions | E3 | 1 hr | ✅ |
| P2.3 | Fix `CandleAggregator::onTick()` — proper token-to-symbol matching | E4 | 2 hr | ✅ |
| P2.4 | Implement `StrategyService::onUpdateTick()` MTM computation | E5 | 4 hr | ✅ |
| P2.5 | Add unit tests for `TemplateStrategy` condition evaluation | C4 | 4 hr | ✅ |
| P2.6 | Add unit tests for `FormulaEngine` expression parsing | C4 | 3 hr | ✅ |
| P2.7 | End-to-end test: create template → deploy → receive tick → evaluate → signal | C4 | 8 hr | ⬜ |

### Phase 3: Testing Foundation (Week 2) ⏱ ~5 days

Build test infrastructure that enables safe refactoring.

| ID | Task | Ref | Est. | Status |
|---|---|---|---|---|
| P3.1 | Create `ServiceRegistry` class — explicit service ownership + lifetime | D1 | 4 hr | ⬜ |
| P3.2 | Create mock `IFeedHandler` interface; refactor `FeedHandler` to implement it | D1 | 4 hr | ⬜ |
| P3.3 | Create mock `IRepositoryManager` interface | D1 | 3 hr | ⬜ |
| P3.4 | Unit tests for `Greeks::calculate()` — edge cases (zero vol, zero time, deep ITM/OTM) | C9 | 2 hr | ⬜ |
| P3.5 | Unit tests for `IVCalculator` — known option prices → expected IV | C9 | 2 hr | ⬜ |
| P3.6 | Unit tests for `RepositoryManager` — load, search, cache hits | C3 | 4 hr | ⬜ |
| P3.7 | Unit tests for `MarketWatchModel` — add/remove/duplicate/blank rows | C6 | 3 hr | ⬜ |
| P3.8 | Unit tests for `TradingDataService` — thread-safe concurrent access | C2 | 3 hr | ⬜ |
| P3.9 | Set up CI pipeline (GitHub Actions) — build + test on every PR | C13 | 4 hr | ⬜ |
| P3.10 | Fix C++ standard inconsistency (C++20 vs C++17 in tests) | C13 | 15 min | ✅ |

### Phase 4: Architecture Refactoring (Weeks 3-4) ⏱ ~10 days

Structural improvements for long-term maintainability.

| ID | Task | Ref | Est. | Status |
|---|---|---|---|---|
| P4.1 | Extract `OrderService` from MainWindow (placeOrder/modifyOrder/cancelOrder) | C1 | 8 hr | ⬜ |
| P4.2 | Extract `TickRouter` from MainWindow (XTS→UDP conversion + index routing) | C1 | 4 hr | ⬜ |
| P4.3 | Decompose RepositoryManager into Loader + SearchService + CacheService | C3 | 12 hr | ⬜ |
| P4.4 | Unify `XTS::Tick` and `UDP::MarketTick` into single `MarketTick` type | C8 | 8 hr | ⬜ |
| P4.5 | Split `StrategyTemplate.h` (392 lines) into separate model files | C4 | 3 hr | ⬜ |
| P4.6 | Move `GlobalShortcuts.cpp` and `GlobalConnections.cpp` to `src/app/MainWindow/` | C10 | 30 min | ⬜ |
| P4.7 | Add log rotation (max 10 files × 50MB) and use `QStandardPaths` for log dir | D6 | 3 hr | ⬜ |
| P4.8 | Remove FeedHandler broadcast signals (depthUpdate, tradeUpdate, etc.) — use per-token pub/sub only | C2 | 2 hr | ⬜ |
| P4.9 | Add input validation to `Greeks::calculate()` | C9 | 1 hr | ⬜ |
| P4.10 | Consolidate RepositoryManager's dual locking into single `std::shared_mutex` | C3 | 2 hr | ⬜ |

### Phase 5: Polish & Production Hardening (Weeks 5-6)

| ID | Task | Ref | Est. | Status |
|---|---|---|---|---|
| P5.1 | Add circuit breaker for UDP disconnection detection | D4 | 4 hr | ⬜ |
| P5.2 | Add health check / heartbeat for all 6 connections | D4 | 4 hr | ⬜ |
| P5.3 | Expose LatencyTracker measurements in status bar / diagnostic window | C11 | 4 hr | ⬜ |
| P5.4 | Add structured error types for order failures | D4 | 4 hr | ⬜ |
| P5.5 | Implement CMake `install` target for packaging | C13 | 4 hr | ⬜ |
| P5.6 | Add integration tests for login flow | C13 | 8 hr | ⬜ |
| P5.7 | Replace `Custom*` prefix with meaningful names | C10 | 4 hr | ⬜ |
| P5.8 | Document all 19 singletons' initialization order in `docs/` | D1 | 2 hr | ⬜ |
| P5.9 | Add pimpl to `MarketWatchWindow.h` (390-line header) | C7 | 3 hr | ⬜ |
| P5.10 | Profile and benchmark FeedHandler under 50K ticks/sec load | C2 | 4 hr | ⬜ |

---

## G. TRACKING MATRIX

### Summary by Phase

| Phase | Items | Est. Total | Priority | Depends On |
|---|---|---|---|---|
| **Phase 0**: Emergency Fixes | 6 | ~4 hours | 🔴 IMMEDIATE | — |
| **Phase 1**: Dead Code Cleanup | 10 | ~2 hours | 🟡 This Week | — |
| **Phase 2**: Strategy Engine | 7 | ~25 hours | 🔴 High | Phase 0 |
| **Phase 3**: Testing Foundation | 10 | ~30 hours | 🔴 High | Phase 1 |
| **Phase 4**: Architecture Refactoring | 10 | ~44 hours | 🟡 Medium | Phase 3 |
| **Phase 5**: Production Hardening | 10 | ~37 hours | 🟢 Normal | Phase 4 |

### Effort Breakdown

| Effort | Hours |
|---|---|
| Phase 0 (Emergency) | 4 |
| Phase 1 (Cleanup) | 2 |
| Phase 2 (Strategy) | 25 |
| Phase 3 (Testing) | 30 |
| Phase 4 (Architecture) | 44 |
| Phase 5 (Hardening) | 37 |
| **TOTAL** | **~142 hours** |

### Risk Register

| Risk | Probability | Impact | Mitigation |
|---|---|---|---|
| Strategy engine goes live with stub `subscribe()` | High | 🔴 Strategies silently do nothing | Phase 2 priority |
| FileLogger ODR violation if new include added | Medium | 🔴 Corrupted logging, hard to debug | Phase 0 fix |
| Hardcoded expiry causes ATM Watch failure | Already happening | 🔴 User-visible bug | Phase 0 fix |
| Singleton destruction crashes on shutdown | Low | 🟡 Crash on exit (not during trading) | Phase 3 ServiceRegistry |
| Refactoring introduces regressions | Medium | 🟡 Feature breakage | Phase 3 tests first |

---

## Appendix: File Inventory

### Source Files by Module (LOC estimates)

| Module | .cpp Files | .h Files | Est. LOC | Complexity |
|---|---|---|---|---|
| app/ | 8 | 6 | ~3,000 | Medium |
| services/ | 14 | 13 | ~5,000 | High |
| repository/ | 7 | 9 | ~4,000 | High |
| strategy/ | 20 | 18 | ~6,000 | Very High |
| views/ | 25+ | 25 | ~7,000 | Medium |
| core/widgets/ | 12 | 12 | ~3,000 | Medium |
| models/ | 7 | 10 | ~2,000 | Low |
| api/ | 5 | 7 | ~2,000 | Medium |
| quant/ | 3 | 4 | ~800 | Low |
| data/ | 2 | 4 | ~500 | Low |
| ui/ | 8 | 7 | ~2,500 | Medium |
| utils/ | 10 | 15 | ~2,000 | Low |
| search/ | 1 | 1 | ~300 | Low |
| lib/ | 10+ | 10+ | ~5,000 | Low (3rd party) |
| **Total** | **~140** | **~140** | **~43,000** | — |

### Singleton Dependency Graph

```
AppBootstrap
  ├── LicenseManager::instance()
  └── MainWindow
       ├── FeedHandler::instance()
       ├── StrategyService::instance()
       ├── WindowManager::instance()
       ├── ATMWatchManager::getInstance()
       ├── GreeksCalculationService::instance()
       ├── UdpBroadcastService::instance()
       ├── XTSFeedBridge::instance()
       ├── ConnectionStatusManager::instance()
       ├── RepositoryManager::getInstance()
       ├── PreferencesManager::instance()
       ├── WindowCacheManager::instance()
       ├── CandleAggregator::instance()
       ├── PriceStoreGateway::instance()
       └── SymbolCacheManager::instance()
```

All 19 singletons are reachable from `MainWindow` — it knows about everything.

---

*This document is a living review. Update the Status column (⬜→🟨→✅) as items are completed.*
*Last updated: 28 February 2026*
