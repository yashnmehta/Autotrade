# Codebase Analysis & Refactoring Plan

**Date**: 2026-02-27  
**Status**: Phase 1 COMPLETE ✅ | Phase 2 COMPLETE ✅ | Phase 3 COMPLETE ✅ (3.1–3.4 done, 3.5 deferred)  
**Scope**: Full codebase audit — structure, naming, separation of concerns, file placement

---

## 1. USE CASE UNDERSTANDING

### What This Application Is

A **professional-grade, low-latency trading terminal** for Indian equity and derivatives markets (NSE + BSE), built in C++20 with Qt 5.15.2. It's comparable to platforms like Zerodha Kite, Sharekhan TradeTiger, or Symphony Presto — but as a native desktop application optimized for co-located / office environments.

### Core Capabilities

| Domain | Features |
|--------|----------|
| **Market Data** | UDP multicast receiver (NSE FO/CM, BSE FO/CM) + XTS WebSocket fallback, zero-copy PriceStore, 5-level depth |
| **Trading** | Buy/Sell order placement, modification, cancellation via XTS Interactive API |
| **Watchlists** | Multi-tab MarketWatch with drag/drop, column profiles, pinned rows |
| **Options** | OptionChain, ATM Watch (±N strikes), Black-Scholes Greeks, IV solver |
| **Charting** | TradingView (Chromium/QtWebEngine), native QtCharts, real-time CandleAggregator |
| **Strategy** | Template builder (indicators + conditions), deployment, live execution engine |
| **Books** | OrderBook, TradeBook, PositionBook with real-time WebSocket updates |
| **Search** | Global multi-token search across all exchange master files |
| **UI Framework** | Custom frameless MDI (Multiple Document Interface) with taskbar, titlebars, workspace persistence |

### Data Flow Architecture

```
UDP Multicast ──→ C++ Receivers ──→ PriceStore (zero-copy) ──→ FeedHandler ──→ UI Widgets
                                                                   ↑
XTS WebSocket ──→ NativeWebSocketClient ──────────────────────────┘
                                                                   
XTS REST API ──→ NativeHTTPClient ──→ LoginFlowService ──→ TradingDataService ──→ Books UI
```

---

## 2. CURRENT DIRECTORY STRUCTURE (AS-IS)

```
Autotrade/
├── CMakeLists.txt           ← Single monolithic 700+ line build file
├── configs/config.ini       ← Runtime config (credentials, UDP IPs, feature flags)
├── include/                 ← Header-only layer (mirrors src/)
│   ├── socket_platform.h    ← ⚠️ ORPHAN: Should be in include/api/ or include/platform/
│   ├── api/                 ← XTS API clients, types, interfaces
│   ├── app/                 ← MainWindow header
│   ├── core/                ← WindowCacheManager + WindowConstants
│   │   └── widgets/         ← Custom Qt widgets (MDI, TitleBar, etc.)
│   ├── data/                ← PriceStore, CandleData, UnifiedPriceState
│   ├── indicators/          ← TA-Lib wrapper
│   ├── models/              ← Qt Models + WindowContext + Profile types
│   ├── repository/          ← Master contract repos + Greeks + IV
│   ├── search/              ← SearchTokenizer (DUPLICATE of src/search/)
│   ├── services/            ← Business logic services
│   ├── strategy/            ← Strategy subsystem (model/runtime/builder/manager)
│   ├── udp/                 ← UDP type enums
│   ├── utils/               ← Utility headers (ATMCalculator, LockFreeQueue, etc.)
│   └── views/               ← View/window headers + helpers
├── src/                     ← Implementation files
│   ├── main.cpp             ← ⚠️ 280+ lines of bootstrap logic
│   ├── api/                 ← API clients + minilzo.c/h (⚠️ LZO files misplaced)
│   ├── app/MainWindow/      ← MainWindow split into 5 files (good pattern)
│   ├── core/                ← Core widgets + global shortcuts/connections
│   ├── data/                ← PriceStoreGateway, SymbolCacheManager
│   ├── indicators/          ← TALibIndicators impl
│   ├── models/              ← Qt model implementations
│   ├── repository/          ← Repo impls + Greeks/IV (⚠️ Greeks in wrong layer)
│   ├── search/              ← SearchTokenizer.cpp + .h (⚠️ DUPLICATE header)
│   ├── services/            ← All business services
│   ├── strategy/            ← 4 sub-packages (good decomposition)
│   ├── ui/                  ← Dialogs (Login, Splash, Settings)
│   ├── utils/               ← Utilities + FileLogger.h (⚠️ header in src/)
│   └── views/               ← Trading windows + helpers
├── resources/
│   ├── forms/               ← Qt Designer .ui files
│   │   └── strategy/        ← Strategy-specific .ui files
│   ├── html/                ← TradingView HTML
│   └── tradingview/         ← Charting library assets
├── lib/                     ← Third-party / internal libraries
│   ├── common/              ← Shared LZO decompression
│   ├── cpp_broacast_nsefo/  ← ⚠️ TYPO: "broacast" → "broadcast"
│   ├── cpp_broadcast_nsecm/
│   ├── cpp_broadcast_bsefo/
│   ├── imgui/               ← ⚠️ Unused? No imgui references found
│   └── lzo-2.10/            ← Bundled LZO compression library
├── MasterFiles/             ← CSV master contract files
├── profiles/                ← Column profile presets (JSON/TXT)
├── tests/                   ← 4 test files (minimal coverage)
├── scripts/                 ← Build/setup scripts
├── docs/                    ← Documentation (cleaned up)
└── ref_code/                ← Reference implementations (ZIP/RAR)
```

---

## 3. ISSUES IDENTIFIED

### 3.1 Structural Issues

| # | Issue | Severity | Location |
|---|-------|----------|----------|
| S1 | **Duplicate header**: `SearchTokenizer.h` exists in BOTH `src/search/` and `include/search/` | 🔴 High | search/ |
| S2 | **Header in src/**: `FileLogger.h` is in `src/utils/` instead of `include/utils/` | 🟡 Medium | utils/ |
| S3 | **Orphan header**: `socket_platform.h` sits at `include/` root | 🟡 Medium | include/ |
| S4 | **minilzo in api/**: `minilzo.c` and `minilzo.h` are in `src/api/` — they're a compression library, not API code | 🟡 Medium | api/ |
| S5 | **Greeks in repository/**: `Greeks.h`, `Greeks.cpp`, `IVCalculator.h`, `IVCalculator.cpp` are under `repository/` but are calculation engines, not data access | 🟡 Medium | repository/ |
| S6 | **Typo in directory name**: `lib/cpp_broacast_nsefo/` (missing 'd' in "broadcast") | 🟡 Medium | lib/ |
| S7 | **Possibly unused lib**: `lib/imgui/` — no references to ImGui in the codebase | 🟢 Low | lib/ |
| S8 | **`src/strategy/model/` is empty**: All model headers are in `include/strategy/model/` but no .cpp files — this is fine for header-only structs but the directory is misleading | 🟢 Low | strategy/ |

### 3.2 Layering / Separation of Concerns Issues

| # | Issue | Severity | Details |
|---|-------|----------|---------|
| L1 | **`main.cpp` is a God function** (280+ lines): Contains login flow orchestration, retry logic, error dialogs, service wiring — should be an `AppBootstrap` or `ApplicationController` class | 🔴 High | main.cpp |
| L2 | **`MainWindow` does too much**: 210-line header, creates 15+ window types, manages broadcast receivers, handles price subscriptions, workspace save/load — classic God Object | 🔴 High | MainWindow.h |
| L3 | **`views/` mixes windows and helpers**: `GenericTableFilter`, `MarketWatchHelpers`, `PositionHelpers` are utility code living alongside window classes | 🟡 Medium | views/helpers/ |
| L4 | **`models/` mixes concerns**: Contains Qt data models (MarketWatchModel, OrderModel), profile management (GenericProfileManager, MarketWatchColumnProfile), callback interfaces (IMarketWatchViewCallback), and domain context (WindowContext) | 🟡 Medium | models/ |
| L5 | **`ui/` vs `views/` boundary is unclear**: `ui/` has LoginWindow, SplashScreen, ATMWatchSettingsDialog, GlobalSearchWidget, chart widgets. `views/` has ATMWatchWindow, OptionChainWindow, BuyWindow, etc. The distinction is inconsistent | 🟡 Medium | ui/, views/ |
| L6 | **`repository/` contains calculation logic**: Greeks and IVCalculator are pure math — they have zero data access concern | 🟡 Medium | repository/ |
| L7 | **`api/` mixes transport and protocol**: Contains HTTP client, WebSocket client, Socket.IO client, UDP provider, AND XTS-specific types/clients. The transport layer (HTTP/WS) is coupled to the XTS protocol layer | 🟡 Medium | api/ |
| L8 | **Strategy builder contains UI AND persistence**: `StrategyTemplateRepository.cpp` (data access) sits alongside `ConditionBuilderWidget.cpp` (Qt widget) in `strategy/builder/` | 🟡 Medium | strategy/builder/ |

### 3.3 Naming & Convention Issues

| # | Issue | Details |
|---|-------|---------|
| N1 | **Inconsistent naming**: `UdpBroadcastService` vs `UDPBroadcastProvider` vs `UDPTypes` (camelCase vs UPPERCASE acronyms) |
| N2 | **`Custom*` prefix overuse**: `CustomMainWindow`, `CustomMDIArea`, `CustomMDISubWindow`, `CustomMDIChild`, `CustomTitleBar`, `CustomScripComboBox`, `CustomMarketWatch`, `CustomNetPosition`, `CustomTradeBook`, `CustomOrderBook` — the "Custom" prefix adds no information since ALL of these are custom |
| N3 | ✅ **RESOLVED — Mixed exchange segment enums**: Unified into `::ExchangeSegment` (core/ExchangeSegment.h) with XTS API values. Old enums are backward-compat aliases. |
| N4 | **`ScripBar` vs `SnapQuoteScripBar`**: Unclear relationship — are these variants or parent/child? |

### 3.4 Build System Issues

| # | Issue | Details |
|---|-------|---------|
| B1 | **Monolithic CMakeLists.txt**: 700+ lines, manually lists every file — fragile, hard to maintain. Should use per-directory CMakeLists or at least `file(GLOB_RECURSE ...)` with explicit exclusions |
| B2 | **Lots of empty `set()` variables**: Many `set(SOURCES ...)` blocks in CMakeLists.txt appear truncated/empty in the summary — may indicate stale or auto-generated content |
| B3 | **No separation into CMake targets**: Everything compiles into a single `TradingTerminal` executable. The broadcast libs, strategy engine, and core widgets could be static libraries for faster incremental builds |
| B4 | **Tests disabled**: `BUILD_TESTS` is ON but there's only a message, no actual test target. 4 test files exist but aren't built |

### 3.5 Missing / Incomplete

| # | What's Missing | Impact |
|---|----------------|--------|
| M1 | **No `include/udp/` broadcast wrapper headers**: The `include/udp/` only has `UDPTypes.h` and `UDPEnums.h`. The actual receiver headers are in `lib/*/include/` and directly included via `include_directories()` — leaky abstraction |
| M2 | **No interface for TradingDataService**: Direct concrete dependency everywhere — makes testing impossible |
| M3 | **No dependency injection**: All services are singletons (`::instance()`) — FeedHandler, PriceStoreGateway, CandleAggregator, XTSFeedBridge, UdpBroadcastService, RepositoryManager, ATMWatchManager. This creates hidden coupling |
| M4 | **No error handling abstraction**: Error reporting is ad-hoc (QMessageBox in main.cpp, qWarning scattered) |

---

## 4. REFACTORING PLAN

### Phase 1: Quick Wins (Low Risk, High Impact) 🟢

These changes require NO logic changes — just file moves and renames.

#### 1.1 Fix Duplicate/Misplaced Headers

```
ACTION: Delete src/search/SearchTokenizer.h (keep include/search/SearchTokenizer.h)
ACTION: Move src/utils/FileLogger.h → include/utils/FileLogger.h
ACTION: Move include/socket_platform.h → include/platform/socket_platform.h
        (create include/platform/ directory)
```

#### 1.2 Move minilzo Out of api/

```
ACTION: Move src/api/minilzo.c → lib/common/minilzo.c
ACTION: Move src/api/minilzo.h → lib/common/include/minilzo.h
UPDATE: CMakeLists.txt to reference new paths
```

#### 1.3 Move Greeks/IV to Correct Layer

```
ACTION: Move src/repository/Greeks.cpp → src/services/Greeks.cpp
ACTION: Move src/repository/IVCalculator.cpp → src/services/IVCalculator.cpp
ACTION: Move include/repository/Greeks.h → include/services/Greeks.h
ACTION: Move include/repository/IVCalculator.h → include/services/IVCalculator.h
UPDATE: All #include paths
```

Alternatively, create a dedicated `calculations/` or `quant/` module:

```
include/quant/Greeks.h
include/quant/IVCalculator.h
include/quant/ATMCalculator.h  (move from utils/)
src/quant/Greeks.cpp
src/quant/IVCalculator.cpp
```

#### 1.4 Fix Directory Typo

```
ACTION: Rename lib/cpp_broacast_nsefo/ → lib/cpp_broadcast_nsefo/
UPDATE: All CMakeLists.txt references and include_directories paths
```

#### 1.5 Remove Unused Library

```
ACTION: Verify lib/imgui/ is unused (grep for imgui includes)
ACTION: If unused, remove lib/imgui/ entirely
```

---

### Phase 2: Boundary Clarification (Medium Risk) 🟡

#### 2.1 Clarify ui/ vs views/ Split

**Proposed convention:**

- `ui/` = **Application-level dialogs** (one-shot / modal): Login, Splash, Settings, Preferences, GlobalSearch
- `views/` = **MDI trading windows** (persistent, data-driven): MarketWatch, OrderBook, PositionWindow, OptionChain, Charts
- `core/widgets/` = **Reusable framework widgets** (MDI infrastructure): stays as-is

**Actions:**

```
MOVE: src/ui/ATMWatchSettingsDialog → src/views/ATMWatchSettingsDialog  
      (it's a trading-domain dialog, not app-level)
MOVE: src/ui/OptionCalculatorWindow → src/views/OptionCalculatorWindow  
      (it's a trading tool window)
KEEP: src/ui/LoginWindow, SplashScreen, GlobalSearchWidget  
      (these are app-level)
KEEP: src/ui/TradingViewChartWidget, IndicatorChartWidget  
      (chart widgets are embeddable components — could go to core/widgets/ or stay in ui/)
```

#### 2.2 Split models/ Into Sub-concerns

```
include/models/
├── qt/                          ← Qt Item Models (MVC pattern)
│   ├── MarketWatchModel.h
│   ├── OrderModel.h
│   ├── PositionModel.h
│   ├── TradeModel.h
│   ├── PinnedRowProxyModel.h
│   └── StrategyTableModel.h     (move from strategy/manager/)
├── profiles/                    ← Column/layout profile management
│   ├── GenericProfileManager.h
│   ├── GenericTableProfile.h
│   └── MarketWatchColumnProfile.h
├── domain/                      ← Trading domain types
│   ├── WindowContext.h
│   └── TokenAddressBook.h
└── interfaces/                  ← Callback interfaces
    └── IMarketWatchViewCallback.h
```

#### 2.3 Split api/ Into Transport + Protocol

```
include/api/
├── transport/                   ← Generic network transport (reusable)
│   ├── NativeHTTPClient.h
│   └── NativeWebSocketClient.h
├── xts/                         ← XTS-specific protocol (broker-specific)
│   ├── XTSTypes.h
│   ├── XTSMarketDataClient.h
│   ├── XTSInteractiveClient.h
│   └── SocketIOInteractiveClient.h
├── udp/                         ← UDP broadcast provider
│   └── UDPBroadcastProvider.h
├── IMarketDataProvider.h        ← Abstract interface (stays at root)
└── (socket_platform.h moved to platform/)
```

#### 2.4 Separate Strategy Builder Persistence from UI

```
strategy/builder/ currently has:
  - StrategyTemplateRepository.cpp  ← This is DATA ACCESS, not UI

ACTION: Move to strategy/model/ or create strategy/persistence/:
  include/strategy/persistence/StrategyTemplateRepository.h
  src/strategy/persistence/StrategyTemplateRepository.cpp
  
  include/strategy/persistence/StrategyRepository.h  (from manager/)
  src/strategy/persistence/StrategyRepository.cpp     (from manager/)
```

---

### Phase 3: Architecture Improvements (Higher Risk, High Reward) 🔴

#### 3.1 Extract AppBootstrap from main.cpp

Create `src/app/AppBootstrap.cpp` / `include/app/AppBootstrap.h`:

```cpp
class AppBootstrap : public QObject {
    Q_OBJECT
public:
    int run(int argc, char *argv[]);
    
private:
    void registerMetaTypes();
    void loadConfiguration();
    void checkLicense();
    void showSplashScreen();
    void showLoginWindow();
    void createMainWindow();
    void wireServices();
    
    // Dependencies
    ConfigLoader *m_config;
    LoginFlowService *m_loginService;
    TradingDataService *m_tradingDataService;
    MainWindow *m_mainWindow;
};
```

`main.cpp` becomes:

```cpp
int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    AppBootstrap bootstrap;
    return bootstrap.run(argc, argv);
}
```

#### 3.2 Decompose MainWindow (Extract Window Factory)

Create `src/app/WindowFactory.cpp`:

```cpp
class WindowFactory : public QObject {
    Q_OBJECT
public:
    CustomMDISubWindow* create(const QString &windowType, 
                                const WindowContext &context = {});
    
private:
    CustomMDISubWindow* createMarketWatch();
    CustomMDISubWindow* createBuyWindow(const WindowContext &ctx);
    CustomMDISubWindow* createOptionChain(const WindowContext &ctx);
    // ... etc
};
```

Create `src/app/WorkspaceManager.cpp`:

```cpp
class WorkspaceManager : public QObject {
    Q_OBJECT
public:
    void saveWorkspace(const QString &name);
    void loadWorkspace(const QString &name);
    void manageWorkspaces();
    QStringList listWorkspaces();
};
```

This reduces MainWindow to ~100 lines focused on layout and menu setup.

#### 3.3 Unify Exchange Segment Enum

Create a single canonical enum and conversion utilities:

```cpp
// include/core/ExchangeSegment.h
enum class ExchangeSegment : int {
    NSECM = 1,
    NSEFO = 2,
    NSECD = 3,
    BSECM = 11,
    BSEFO = 12,
    MCXFO = 51,
    BSECD = 61
};

namespace ExchangeSegmentUtil {
    QString toString(ExchangeSegment seg);
    ExchangeSegment fromXTS(int xtsCode);
    ExchangeSegment fromUDP(int udpCode);
    bool isDerivative(ExchangeSegment seg);
    bool isEquity(ExchangeSegment seg);
}
```

Then deprecate all other duplicate enums.

#### 3.4 Create Quant Module

Consolidate all mathematical/quantitative code:

```
include/quant/
├── Greeks.h                 ← Black-Scholes Greeks calculator
├── IVCalculator.h           ← Implied Volatility solver
├── ATMCalculator.h          ← ATM strike calculator (from utils/)
├── OptionPricing.h          ← Future: other pricing models
└── TimeToExpiry.h           ← Expiry date calculations

src/quant/
├── Greeks.cpp
├── IVCalculator.cpp
└── ATMCalculator.cpp        ← (if it has implementation)
```

#### 3.5 Per-Module CMakeLists.txt

Split the monolithic CMakeLists.txt:

```
CMakeLists.txt               ← Top-level: project config, find_package, add_subdirectory
src/CMakeLists.txt            ← Main executable target
src/core/CMakeLists.txt       ← Static lib: core_widgets
src/api/CMakeLists.txt        ← Static lib: api_layer
src/services/CMakeLists.txt   ← Static lib: services
src/strategy/CMakeLists.txt   ← Static lib: strategy_engine
src/views/CMakeLists.txt      ← Object lib: views
src/repository/CMakeLists.txt ← Static lib: repository
lib/CMakeLists.txt            ← Broadcast receiver libs
```

Benefits:
- Faster incremental builds (only rebuild changed module)
- Clearer dependency graph
- Easier to add unit tests per module

---

## 5. PROPOSED TARGET STRUCTURE

```
Autotrade/
├── CMakeLists.txt                    ← Slim top-level
├── configs/
│   └── config.ini
├── include/
│   ├── platform/                     ← Cross-platform abstractions
│   │   └── socket_platform.h
│   ├── core/                         ← Framework layer (rarely changes)
│   │   ├── ExchangeSegment.h         ← NEW: Canonical exchange enum
│   │   ├── WindowCacheManager.h
│   │   ├── WindowConstants.h
│   │   └── widgets/                  ← Custom MDI/widget framework
│   ├── api/                          ← Network + broker API
│   │   ├── transport/                ← Generic HTTP/WS clients
│   │   ├── xts/                      ← XTS-specific clients + types
│   │   ├── udp/                      ← UDP broadcast provider
│   │   └── IMarketDataProvider.h
│   ├── data/                         ← Zero-copy data stores
│   │   ├── PriceStoreGateway.h
│   │   ├── UnifiedPriceState.h
│   │   ├── CandleData.h
│   │   └── SymbolCacheManager.h
│   ├── models/                       ← Data models
│   │   ├── qt/                       ← Qt MVC models
│   │   ├── profiles/                 ← Column/table profiles
│   │   ├── domain/                   ← Trading domain types
│   │   └── interfaces/               ← Callback interfaces
│   ├── repository/                   ← Master contract data access
│   │   ├── RepositoryManager.h
│   │   ├── ContractData.h
│   │   ├── ScripMaster.h
│   │   ├── NSE*Repository.h
│   │   ├── BSE*Repository.h
│   │   └── MasterFileParser.h
│   ├── quant/                        ← NEW: Mathematical calculations
│   │   ├── Greeks.h
│   │   ├── IVCalculator.h
│   │   └── ATMCalculator.h
│   ├── services/                     ← Business logic services
│   │   ├── FeedHandler.h
│   │   ├── LoginFlowService.h
│   │   ├── TradingDataService.h
│   │   ├── CandleAggregator.h
│   │   ├── GreeksCalculationService.h
│   │   ├── ATMWatchManager.h
│   │   ├── XTSFeedBridge.h
│   │   ├── UdpBroadcastService.h
│   │   └── TokenSubscriptionManager.h
│   ├── search/                       ← Symbol search
│   │   └── SearchTokenizer.h
│   ├── indicators/                   ← TA-Lib wrapper
│   │   └── TALibIndicators.h
│   ├── strategy/                     ← Strategy subsystem
│   │   ├── model/                    ← Data models (header-only)
│   │   ├── runtime/                  ← Execution engines
│   │   ├── builder/                  ← Template authoring UI
│   │   └── persistence/                 ← NEW: Strategy storage
│   ├── app/                          ← Application layer
│   │   ├── MainWindow.h
│   │   ├── AppBootstrap.h            ← NEW
│   │   ├── WindowFactory.h           ← NEW
│   │   ├── WorkspaceManager.h        ← NEW
│   │   ├── ScripBar.h
│   │   └── SnapQuoteScripBar.h
│   ├── ui/                           ← App-level dialogs
│   │   ├── LoginWindow.h
│   │   ├── SplashScreen.h
│   │   └── GlobalSearchWidget.h
│   ├── views/                        ← MDI trading windows
│   │   ├── ATMWatchWindow.h
│   │   ├── OptionChainWindow.h
│   │   ├── MarketWatchWindow.h
│   │   ├── BuyWindow.h / SellWindow.h
│   │   ├── *BookWindow.h
│   │   ├── *ChartWidget.h
│   │   ├── OptionCalculatorWindow.h
│   │   ├── PreferenceDialog.h
│   │   └── helpers/
│   ├── udp/                          ← UDP type definitions
│   │   ├── UDPTypes.h
│   │   └── UDPEnums.h
│   └── utils/                        ← Utilities
│       ├── ConfigLoader.h
│       ├── FileLogger.h              ← MOVED from src/
│       ├── PreferencesManager.h
│       ├── LockFreeQueue.h
│       ├── LatencyTracker.h
│       └── ...
├── src/                              ← Implementations (mirrors include/)
│   ├── main.cpp                      ← Slim (10 lines)
│   ├── app/
│   │   ├── AppBootstrap.cpp          ← NEW: All bootstrap logic
│   │   ├── MainWindow/               ← Existing split (good)
│   │   ├── WindowFactory.cpp         ← NEW
│   │   └── WorkspaceManager.cpp      ← NEW
│   ├── quant/                        ← NEW
│   │   ├── Greeks.cpp
│   │   └── IVCalculator.cpp
│   └── ... (mirrors include/)
├── lib/
│   ├── common/
│   │   ├── minilzo.c                 ← MOVED from src/api/
│   │   └── include/
│   │       ├── lzo_decompress.h
│   │       └── minilzo.h             ← MOVED from src/api/
│   ├── cpp_broadcast_nsefo/          ← RENAMED (typo fix)
│   ├── cpp_broadcast_nsecm/
│   ├── cpp_broadcast_bsefo/
│   └── lzo-2.10/
├── resources/
├── MasterFiles/
├── profiles/
├── tests/
├── scripts/
└── docs/
```

---

## 6. PRIORITY MATRIX

| Phase | Task | Risk | Effort | Impact | Priority |
|-------|------|------|--------|--------|----------|
| 1 | Delete duplicate `SearchTokenizer.h` | 🟢 None | 5 min | Prevents confusion | P0 |
| 1 | Move `FileLogger.h` to `include/` | 🟢 None | 5 min | Consistency | P0 |
| 1 | Fix `socket_platform.h` location | 🟢 None | 5 min | Consistency | P0 |
| 1 | Fix `broacast` typo | 🟢 Low | 15 min | Professionalism | P0 |
| 1 | Move minilzo out of api/ | 🟢 Low | 15 min | Cleaner api/ | P1 |
| 1 | Remove unused imgui/ | 🟢 None | 5 min | Less clutter | P1 |
| 1 | Fix broken `test_iv_debug` CMake target | 🟢 None | 5 min | Prevents build error | P1 |
| 1 | Fix `DateUtils` misplaced in `REPOSITORY_SOURCES` | 🟢 None | 5 min | Correct CMake grouping | P1 |
| 2 | Move Greeks/IV to quant/ | 🟡 Low | 30 min | Correct layering | P1 |
| 2 | Clarify ui/ vs views/ | 🟡 Low | 30 min | Developer clarity | P2 |
| 2 | Split models/ sub-dirs | 🟡 Medium | 1 hr | Navigability | P2 |
| 2 | Split api/ transport/protocol | 🟡 Medium | 1 hr | Maintainability | P2 |
| 3 | ✅ Extract AppBootstrap | 🔴 Medium | 2 hr | ✅ Testability, clarity | P2 |
| 3 | ✅ Extract WindowFactory | 🔴 Medium | 3 hr | ✅ MainWindow simplification | P2 |
| 3 | ✅ Unify ExchangeSegment | 🔴 High | 4 hr | ✅ Eliminated confusion | P3 |
| 3 | ✅ Per-module CMakeLists | 🔴 High | 4 hr | ✅ Faster incremental builds | P3 |
| 3 | Dependency injection | 🔴 Very High | 8+ hr | Testability | P3 (deferred) |

---

## 7. MIGRATION STRATEGY

### Golden Rule: **One Phase at a Time, Verify Build After Each Step**

1. **Phase 1** can be done in a single PR — all changes are file moves + CMake updates
2. **Phase 2** should be split into 3-4 PRs (one per subsection)  
3. **Phase 3** each task is its own PR with careful testing

### For Each File Move:

1. Move the file
2. Update `CMakeLists.txt`
3. Update all `#include` directives (use `grep -r "oldpath"` to find them)
4. Build & verify
5. Commit

### Safety Net:

- Always build after each change: `cd build_ninja && .\build.bat`
- Run any existing tests
- Keep a rollback commit ready

---

## 8. WHAT NOT TO CHANGE

| Component | Reason |
|-----------|--------|
| `core/widgets/*` | Stable MDI framework, well-tested, rarely changes |
| `lib/cpp_broadcast_*/` | Low-level C libraries with their own include structure |
| `resources/forms/*.ui` | Qt Designer files — binary-ish, don't restructure |
| `strategy/` 4-part split | Already well-decomposed (model/runtime/builder/manager) |
| `MainWindow/` 5-file split | Good pattern (MainWindow.cpp, UI.cpp, Windows.cpp, Network.cpp, WindowCycling.cpp) |
| `views/*Window/` splits | MarketWatchWindow/ and SnapQuoteWindow/ 4-file splits are good |

---

**Next Step**: Start with Phase 1 Quick Wins. Want me to execute them?

---

## 9. PHASE 1 EXECUTION LOG ✅

**Executed**: 2026-02-27  
**Result**: All Phase 1 quick wins completed successfully

### 9.1 Changes Made

| # | Task | Status | Details |
|---|------|--------|---------|
| P1.1 | Delete duplicate `SearchTokenizer.h` | ✅ Done | Deleted `src/search/SearchTokenizer.h`. Updated `include/search/SearchTokenizer.h` to match (public enums). Updated `.cpp` include to `"search/SearchTokenizer.h"`. Updated CMake `SEARCH_HEADERS`. Added `include/search` to CMake `include_directories`. |
| P1.2 | Move `FileLogger.h` to `include/utils/` | ✅ Done | Moved `src/utils/FileLogger.h` → `include/utils/FileLogger.h`. No include-path changes needed (already resolved via `include/`). |
| P1.3 | Move `socket_platform.h` to `include/platform/` | ✅ Done | Created `include/platform/` directory. Moved file. Added `include/platform` to CMake `include_directories` so broadcast libs still find it as `"socket_platform.h"`. |
| P1.4 | Move `minilzo` out of `src/api/` | ✅ Done | Moved `src/api/minilzo.c` → `lib/common/src/minilzo.c`. Moved `src/api/minilzo.h` → `lib/common/include/minilzo.h`. Files were dead code (not in any CMake source list). |
| P1.5 | Remove unused `lib/imgui/` | ✅ Done | Confirmed zero references. Directory was empty. Deleted. |
| P1.6 | Fix broken `test_iv_debug` CMake target | ✅ Done | `test_iv_debug.cpp` referenced at project root but didn't exist. Commented out the broken target definition. |
| P1.7 | Fix `DateUtils` misplaced in `REPOSITORY_SOURCES` | ✅ Done | Moved `src/utils/DateUtils.cpp` and `include/utils/DateUtils.h` from `REPOSITORY_SOURCES/HEADERS` to `UTILS_SOURCES/HEADERS` in CMake. |

### 9.2 Additional Issues Found During Execution

| # | Issue | Severity | Details |
|---|-------|----------|---------|
| A1 | **`test_iv_debug.cpp` missing**: CMake references `test_iv_debug.cpp` at project root but file doesn't exist — would cause build failure if `BUILD_TESTS=ON` | 🔴 High | Fixed by commenting out |
| A2 | **`DateUtils` in wrong CMake group**: `src/utils/DateUtils.cpp` and `include/utils/DateUtils.h` were listed under `REPOSITORY_SOURCES/HEADERS` instead of `UTILS_SOURCES/HEADERS` | 🟡 Medium | Fixed |
| A3 | **`SocketIOInteractiveClient` disabled but files present**: `src/api/SocketIOInteractiveClient.cpp` and `include/api/SocketIOInteractiveClient.h` exist but are commented out in CMake — dead code | 🟢 Low | Can be removed in future cleanup |
| A4 | **`UDPBroadcastProvider.cpp` not in `API_SOURCES`**: `src/api/UDPBroadcastProvider.cpp` exists but isn't in the `API_SOURCES` CMake list — may be dead code or included elsewhere | 🟡 Medium | Needs investigation |
| A5 | **`NSEProtocol.h` not in any header list**: `include/api/NSEProtocol.h` exists but is not listed in `API_HEADERS` | 🟢 Low | Header-only, auto-found by AUTOMOC/include |
| A6 | **`MemoryProfiler.h` not in CMake**: `include/utils/MemoryProfiler.h` not listed in `UTILS_HEADERS` but `.cpp` is in `UTILS_SOURCES` | 🟢 Low | No build impact (AUTOMOC finds it) |
| A7 | **Multiple model headers missing from `MODEL_HEADERS`**: `GenericProfileManager.h`, `GenericTableProfile.h`, `IMarketWatchViewCallback.h`, `WindowContext.h` not in CMake `MODEL_HEADERS` list | 🟡 Medium | AUTOMOC handles it, but inconsistent |
| A8 | **`WindowContext.h` listed under `UTILS_HEADERS`**: Domain model header listed with utilities | 🟢 Low | Organizational inconsistency |
| A9 | **Strategy `model/` dir empty in `src/`**: `src/strategy/model/` exists but is empty — misleading for developers | 🟢 Low | Consider removing empty dir |
| A10 | **`lib/cpp_broacast_nsefo/` typo NOT fixed**: Kept for now — renaming requires updating all CMake paths, broadcast source file references, and testing | 🟡 Medium | Deferred to Phase 2 (requires careful find-replace across CMake) |

---

## 10. PHASE 2 EXECUTION LOG ✅

**Executed**: 2026-02-27  
**Result**: All Phase 2 boundary clarification tasks completed successfully. Build verified (187/187 units clean).

### 10.1 Changes Made

| # | Task | Status | Details |
|---|------|--------|---------|
| P2.1 | Fix `broacast` typo | ✅ Done | Renamed `lib/cpp_broacast_nsefo/` → `lib/cpp_broadcast_nsefo/`. Updated all CMake references. |
| P2.2 | Create `quant/` module | ✅ Done | Moved `Greeks.h/.cpp`, `IVCalculator.h/.cpp` from `repository/` and `ATMCalculator.h` from `utils/` to `include/quant/` and `src/quant/`. Updated all consumer includes. Added `QUANT_SOURCES/HEADERS` section to CMake. |
| P2.3 | Separate Strategy persistence | ✅ Done | Moved `StrategyTemplateRepository` from `strategy/builder/` and `StrategyRepository` from `strategy/manager/` to `strategy/persistence/`. Created `STRATEGY_PERSISTENCE_SOURCES/HEADERS` in CMake. Updated all consumer includes. Added `include/strategy/persistence` to `include_directories`. |
| P2.4 | Split `models/` sub-dirs | ✅ Done | Created `models/qt/` (MarketWatchModel, OrderModel, TradeModel, PositionModel, PinnedRowProxyModel), `models/profiles/` (GenericProfileManager, GenericTableProfile, MarketWatchColumnProfile), `models/domain/` (WindowContext, TokenAddressBook), `models/interfaces/` (IMarketWatchViewCallback). Updated all `#include` paths across entire codebase. Updated CMake `MODEL_HEADERS`. Added subdirs to `include_directories`. |
| P2.5 | Clarify `ui/` vs `views/` | ✅ Done | Moved `ATMWatchSettingsDialog` and `OptionCalculatorWindow` from `ui/` → `views/` (trading-domain dialogs). Updated all `#include` paths. Updated CMake `UI_SOURCES/HEADERS` → `VIEW_SOURCES/HEADERS`. `ui/` now only contains app-level dialogs: Login, Splash, GlobalSearch (+chart widgets). |
| P2.6 | Split `api/` transport/protocol | ✅ Done | Created `api/transport/` (NativeHTTPClient, NativeWebSocketClient) and `api/xts/` (XTSTypes, XTSMarketDataClient, XTSInteractiveClient, SocketIOInteractiveClient). `api/` root retains broker-agnostic interfaces (IMarketDataProvider, UDPBroadcastProvider, NSEProtocol). Updated all `#include` paths including internal cross-references. Updated CMake `API_SOURCES/HEADERS`. Added subdirs to `include_directories`. |

### 10.2 Build Verification

```
CMake configure: ✅ Success
Ninja build: ✅ 187/187 units compiled, linked successfully
```

### 10.3 Updated Directory Structure (Post Phase 2)

```
include/
├── platform/socket_platform.h
├── core/
│   ├── WindowCacheManager.h, WindowConstants.h
│   └── widgets/                    ← Custom MDI framework
├── api/
│   ├── IMarketDataProvider.h       ← Broker-agnostic interface
│   ├── UDPBroadcastProvider.h
│   ├── NSEProtocol.h
│   ├── transport/                  ← Generic network layer
│   │   ├── NativeHTTPClient.h
│   │   └── NativeWebSocketClient.h
│   └── xts/                        ← XTS broker-specific
│       ├── XTSTypes.h
│       ├── XTSMarketDataClient.h
│       ├── XTSInteractiveClient.h
│       └── SocketIOInteractiveClient.h
├── models/
│   ├── qt/                          ← Qt MVC models
│   │   ├── MarketWatchModel.h
│   │   ├── OrderModel.h, TradeModel.h, PositionModel.h
│   │   └── PinnedRowProxyModel.h
│   ├── profiles/                    ← Column/layout profiles
│   │   ├── GenericProfileManager.h
│   │   ├── GenericTableProfile.h
│   │   └── MarketWatchColumnProfile.h
│   ├── domain/                      ← Trading domain types
│   │   ├── WindowContext.h
│   │   └── TokenAddressBook.h
│   └── interfaces/                  ← Callback interfaces
│       └── IMarketWatchViewCallback.h
├── quant/                           ← Mathematical calculations
│   ├── Greeks.h
│   ├── IVCalculator.h
│   └── ATMCalculator.h
├── strategy/
│   ├── model/                       ← Data models (header-only)
│   ├── runtime/                     ← Execution engines
│   ├── builder/                     ← Template authoring UI
│   ├── manager/                     ← Deployment dashboard
│   └── persistence/                 ← Data access layer
│       ├── StrategyTemplateRepository.h
│       └── StrategyRepository.h
├── ui/                              ← App-level dialogs only
│   ├── LoginWindow.h, SplashScreen.h
│   ├── GlobalSearchWidget.h
│   ├── TradingViewChartWidget.h     ← Chart widgets
│   └── IndicatorChartWidget.h
├── views/                           ← MDI trading windows
│   ├── ATMWatchWindow.h, ATMWatchSettingsDialog.h  ← Moved from ui/
│   ├── OptionChainWindow.h, OptionCalculatorWindow.h  ← Moved from ui/
│   ├── MarketWatchWindow.h, SnapQuoteWindow.h
│   ├── BuyWindow.h, SellWindow.h
│   ├── OrderBookWindow.h, TradeBookWindow.h, PositionWindow.h
│   └── helpers/
├── services/                        ← Business logic
├── repository/                      ← Master contract data access
├── data/                            ← Zero-copy price stores
├── search/                          ← Symbol search
├── indicators/                      ← TA-Lib wrapper
├── udp/                             ← UDP type definitions
└── utils/                           ← Utilities
```

---

## 11. PHASE 3 EXECUTION LOG (In Progress)

### 11.1 Extract AppBootstrap from main.cpp ✅

**Executed**: 2026-02-27  
**Result**: Successfully extracted 529 lines from `main.cpp` into `AppBootstrap` class.

| Item | Before | After |
|------|--------|-------|
| `main.cpp` | 529 lines (God function) | 15 lines (clean entry point) |
| `AppBootstrap.h` | N/A | 75 lines (class declaration) |
| `AppBootstrap.cpp` | N/A | 490 lines (all bootstrap logic) |

**Key design decisions:**
- `AppBootstrap` is a `QObject` subclass (needed for `connect()` with `SplashScreen::readyToClose`)
- Takes `QApplication*` in constructor — avoids creating QApplication itself
- All lambdas replaced with proper member functions: `onSplashReady()`, `onLoginComplete()`, `onFetchError()`, `onLoginClicked()`, `onContinueClicked()`
- Members (`m_config`, `m_splash`, `m_loginWindow`, `m_loginService`, `m_tradingDataService`, `m_mainWindow`) stored as class members instead of captured in nested lambdas
- `cleanup()` handles TA-Lib shutdown and file logging cleanup
- Build verified: ✅ 10/10 units compiled and linked

---

## Phase 3.2 Execution Log — Decompose MainWindow

**Objective:** Extract `WindowFactory` and `WorkspaceManager` from MainWindow to follow SRP.

### What was done

1. **Created `include/app/WindowFactory.h`** — Header declaring factory interface for all MDI window types
2. **Created `src/app/WindowFactory.cpp`** (~1300 lines) — All `createXxxWindow()` methods, `connectWindowSignals()`, `getBestWindowContext()`, `getActiveMarketWatch()`, `countWindowsOfType()`, `closeWindowsByType()`, order modification windows, context-aware/widget-aware creation, `onAddToWatchRequested()`
3. **Created `include/app/WorkspaceManager.h`** — Header declaring workspace lifecycle management
4. **Created `src/app/WorkspaceManager.cpp`** (~230 lines) — `saveCurrentWorkspace()`, `loadWorkspace()`, `loadWorkspaceByName()`, `manageWorkspaces()`, `onRestoreWindowRequested()`
5. **Updated `include/app/MainWindow.h`** — Added `WindowFactory*` and `WorkspaceManager*` members, accessors; removed `connectWindowSignals`, `getActiveMarketWatch`, `getBestWindowContext`, order modification APIs, context-aware creation slots, `onRestoreWindowRequested`, `countWindowsOfType`, `closeWindowsByType`; kept public order routing (`placeOrder`, `modifyOrder`, `cancelOrder`)
6. **Replaced `src/app/MainWindow/Windows.cpp`** — Was ~1378 lines, now ~107 lines of thin delegator slots forwarding to `WindowFactory` and `WorkspaceManager`
7. **Updated `src/app/MainWindow/MainWindow.cpp`** — Constructor creates `WindowFactory` and `WorkspaceManager`, wires `restoreWindowRequested` to `WorkspaceManager`, propagates XTS/TradingDataService to `WindowFactory`, removed `getActiveMarketWatch()` and order modification methods
8. **Updated `src/app/MainWindow/UI.cpp`** — Removed `saveCurrentWorkspace()`, `loadWorkspace()`, `loadWorkspaceByName()`, `manageWorkspaces()` (moved to `WorkspaceManager`); removed duplicate `restoreWindowRequested` connection (now in constructor)
9. **Updated `CMakeLists.txt`** — Added `WindowFactory.cpp`, `WorkspaceManager.cpp`, `WindowFactory.h`, `WorkspaceManager.h`

### Line count changes

| File | Before | After | Delta |
|------|--------|-------|-------|
| `MainWindow.h` | 210 | 175 | -35 |
| `MainWindow/Windows.cpp` | 1378 | 107 | **-1271** |
| `MainWindow/MainWindow.cpp` | 658 | 540 | -118 |
| `MainWindow/UI.cpp` | 648 | 540 | -108 |
| `WindowFactory.h` | N/A | 100 | +100 |
| `WindowFactory.cpp` | N/A | 1300 | +1300 |
| `WorkspaceManager.h` | N/A | 55 | +55 |
| `WorkspaceManager.cpp` | N/A | 230 | +230 |

**Net: MainWindow family shrank by ~1530 lines** (from ~2894 to ~1362).
Window creation and workspace management are now independently testable.

- Build verified: ✅ 14/14 units compiled and linked (macOS, Ninja)

### Remaining Phase 3 Tasks

3. **3.3 Unify ExchangeSegment enum** → ✅ **COMPLETE**
   - Created `include/core/ExchangeSegment.h` — canonical enum with XTS API values (1,2,3,11,12,51,61)
   - XTS is the single source of truth; all other enums are now backward-compat aliases
   - `XTS::ExchangeSegment` → alias to `::ExchangeSegment` (XTSTypes.h)
   - `UDP::ExchangeSegment` → alias to `::ExchangeSegment` (UDPEnums.h)
   - `SymbolSegment` / `TradeSymbolType` → alias to `::ExchangeSegment` (StrategyTemplate.h)
   - `ExchangeReceiver` → alias to `::ExchangeSegment` (UdpBroadcastService.h)
   - Added `segmentFromComboIndex()` / `segmentToComboIndex()` helpers for UI combo mapping
   - Added `ExchangeSegmentUtil` namespace: `toString`, `fromString`, `fromInt`, `toInt`,
     `isDerivative`, `isEquity`, `isNSE`, `isBSE`, `isValid`, `exchangeName`, `segmentSuffix`
   - Updated `StrategyTemplateBuilderDialog.cpp` and `StrategyTemplateRepository.cpp`
   - Added `default:` cases in `UdpBroadcastService.cpp` switch statements
   - Build verified: ✅ 64/64 units compiled and linked, zero warnings (macOS, Ninja)
4. **3.4 Per-module CMakeLists.txt** → ✅ **COMPLETE**
   - Split 1223-line monolithic `CMakeLists.txt` into 16 per-module CMake files
   - Top-level `CMakeLists.txt`: ~310 lines (project config, find_package, options, add_subdirectory, post-build DLL deploy)
   - `src/CMakeLists.txt`: Main executable target, links all module libraries, broadcast sources, .ui forms, .qrc resources
   - 13 static library modules, each with their own `CMakeLists.txt`:
     - `src/core/` → `libcore_widgets.a` (MDI framework widgets)
     - `src/api/` → `libapi_layer.a` (HTTP/WS transport + XTS protocol)
     - `src/services/` → `libservices.a` (business logic services)
     - `src/repository/` → `librepository.a` (master contract data access)
     - `src/quant/` → `libquant.a` (Greeks, IV, ATM calculations)
     - `src/models/` → `libmodels.a` (Qt MVC models, profiles, domain types)
     - `src/strategy/` → `libstrategy_engine.a` (template builder + runtime + manager + persistence)
     - `src/views/` → `libviews.a` (MDI trading windows + helpers)
     - `src/ui/` → `libui_dialogs.a` (Login, Splash, GlobalSearch, chart widgets)
     - `src/data/` → `libdata_layer.a` (PriceStoreGateway, SymbolCacheManager)
     - `src/search/` → `libsearch.a` (SearchTokenizer)
     - `src/indicators/` → `libindicators.a` (TA-Lib wrapper)
     - `src/utils/` → `libutils.a` (ConfigLoader, PreferencesManager, etc.)
   - `lib/CMakeLists.txt`: common_lzo + BSE FO broadcast + bundled LZO
   - `tests/CMakeLists.txt`: test_search_tokenizer
   - Incremental build benefit verified: single-file change → 4 ninja steps (MOC → compile → link lib → link exe)
   - Build verified: ✅ 239/239 units compiled and linked (macOS, Ninja)
5. **3.5 Dependency injection** → Deferred (P3, very high risk, 8+ hr effort)
