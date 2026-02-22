# Application Initialization & Login Flow — Full Analysis

> **Date**: 2026-02-21  
> **Scope**: `main.cpp`, `SplashScreen`, `LoginFlowService`, `TradingDataService`

---

## 1. Complete Flow Map

```
main()
│
├─ [Qt Setup]
│   ├─ QCoreApplication attributes (HiDPI)
│   ├─ QApplication created
│   ├─ setupFileLogging()
│   ├─ qRegisterMetaType<> for all cross-thread types
│   │     XTS::Tick/Order/Trade/Position
│   │     UDP::MarketTick/IndexTick/CircuitLimitTick
│   │     GreeksResult, ChartData::Candle/Timeframe
│   │     LicenseManager::CheckResult          ← NEW
│   ├─ TA-Lib initialize()
│   └─ app.setApplicationName/OrganizationName/Version
│
├─ PHASE 1 — SPLASH SCREEN
│   │
│   ├─ SplashScreen created & shown (ctor)
│   │   └─ loadPreferences()                   ← RUNS IN CTOR
│   │       PreferencesManager::instance()     ← QSettings singleton loaded here
│   │       reads PriceCache mode flag
│   │       setProgress(20)
│   │
│   ├─ [main.cpp] Config load (setProgress 5→10)
│   │   ConfigLoader searches 7 candidate paths for config.ini
│   │   Loads: credentials, XTS URL, UDP multicast IPs, etc.
│   │
│   ├─ [main.cpp] License check (setProgress 12→15)  ← NEW
│   │   LicenseManager::instance().initialize(config)
│   │   LicenseManager::checkLicense()
│   │   if INVALID → QMessageBox + app.quit()
│   │   if VALID   → continue
│   │
│   ├─ [main.cpp] splash->preloadMasters()     (setProgress 30→100)
│   │   MasterDataState checked (skip if already loading/loaded)
│   │   If cache file exists → MasterLoaderWorker::loadFromCache() [worker thread]
│   │   5-second fallback timeout (splash closes anyway)
│   │   On complete → onMasterLoadingComplete()
│   │       MasterDataState::setMastersLoaded(count)
│   │       PriceStoreGateway::instance().initialize(...)   ← builds zero-copy stores
│   │       m_masterLoadingComplete = true
│   │       checkIfReadyToClose()
│   │
│   ├─ [1.5s timer] m_minimumTimeElapsed = true
│   │   checkIfReadyToClose()
│   │
│   └─ [readyToClose signal] → 300ms delay → emit readyToClose()
│
├─ PHASE 2 — LOGIN WINDOW
│   │
│   ├─ SplashScreen closed & deleted
│   ├─ LoginWindow created
│   │   Pre-filled from config (mdAppKey, mdSecretKey, iaAppKey, iaSecretKey, loginID)
│   ├─ LoginFlowService created
│   ├─ TradingDataService created
│   ├─ loginService->setTradingDataService(tradingDataService)
│   ├─ MainWindow created (hidden)
│   │
│   ├─ Callbacks wired:
│   │   setStatusCallback  → updates loginWindow MD/IA status labels
│   │   setErrorCallback   → shows error in loginWindow, re-enables button
│   │   setCompleteCallback → shows "Continue" button, enables login button
│   │
│   ├─ loginService mastersLoaded signal → mainWindow->refreshScripBar()
│   │
│   └─ loginWindow->exec()   ← MODAL — blocks here until accept/reject
│
├─ PHASE 3 — LOGIN EXECUTION  (on Login button click)
│   │
│   ├─ Credentials read from UI
│   ├─ loginWindow disables button, shows "Connecting..."
│   ├─ loginService->executeLogin(...)
│   │
│   ├─ [executeLogin internals]
│   │   ├─ Old clients cleaned up (disconnectWebSocket, delete)
│   │   ├─ XTSMarketDataClient created   (mdBaseURL = baseURL + "/apimarketdata")
│   │   ├─ XTSInteractiveClient created  (iaBaseURL = baseURL)
│   │   │
│   │   ├─ Interactive events wired to TradingDataService:
│   │   │   iaClient::orderEvent    → tradingDataService::onOrderEvent
│   │   │   iaClient::tradeEvent    → tradingDataService::onTradeEvent
│   │   │   iaClient::positionEvent → tradingDataService::onPositionEvent
│   │   │
│   │   ├─ mdClient->login()   (async)
│   │   └─ iaClient->login()   (async)
│   │
│   ├─ Both login signals fire independently:
│   │   mdClient loginCompleted → m_mdLoggedIn = true  → if both done → startMastersPhase()
│   │   iaClient loginCompleted → m_iaLoggedIn = true  → if both done → startMastersPhase()
│   │   (whichever completes second triggers masters phase)
│
├─ PHASE 4 — MASTERS PHASE  (startMastersPhase)
│   │
│   ├─ Check MasterDataState:
│   │   CASE A: Still loading by splash → wait for mastersReady/loadingError signals
│   │   CASE B: User requested download (downloadMasters=true) → startMasterDownload()
│   │   CASE C: Already loaded by splash → use cache, emit mastersLoaded, continue
│   │   CASE D: Not loaded → loadFromCache() via MasterLoaderWorker
│   │
│   ├─ [If download] startMasterDownload()
│   │   mdClient->downloadMasterContracts(["NSEFO","NSECM","BSEFO","BSECM"])
│   │   On success → masterLoader->loadFromMemoryOnly(csvData) [NO file I/O]
│   │   On failure → continueLoginAfterMasters() (non-fatal)
│   │
│   └─ handleMasterLoadingComplete(contractCount)
│       MasterDataState::setMastersLoaded(count)
│       PriceStoreGateway::instance().initialize(...)   ← RE-initializes stores
│       emit mastersLoaded()
│       continueLoginAfterMasters()
│
├─ PHASE 5 — WEBSOCKET & DATA SYNC  (continueLoginAfterMasters)
│   │
│   ├─ mdClient->connectWebSocket()   (async)
│   │
│   └─ On wsConnectionStatusChanged(success):
│       if !success → log error, STOP
│       if success:
│           ├─ iaClient->getPositions(preferredView)    [REST GET]
│           │   Fallback: toggle NetWise ↔ DayWise on failure
│           │   → tradingDataService->setPositions(positions)
│           │   → emit positionsUpdated(positions)
│           │
│           ├─ iaClient->getOrders(...)                 [REST GET]
│           │   → tradingDataService->setOrders(orders)
│           │   → emit ordersUpdated(orders)
│           │
│           ├─ iaClient->connectWebSocket(...)          [WS connect]
│           │   Interactive socket for live events
│           │
│           └─ iaClient->getTrades(...)                 [REST GET]
│               → tradingDataService->setTrades(trades)
│               → emit tradesUpdated(trades)
│               → updateStatus("complete", 100)
│               → emit loginComplete()
│               → completeCallback()  ← back in main.cpp
│
├─ PHASE 6 — LOGIN COMPLETE CALLBACK  (in main.cpp)
│   │
│   ├─ CandleAggregator::instance().initialize(true)
│   ├─ mainWindow->setXTSClients(mdClient, iaClient)
│   ├─ mainWindow->setTradingDataService(tradingDataService)
│   ├─ mainWindow->setConfigLoader(config)
│   └─ loginWindow->showContinueButton()
│
└─ PHASE 7 — CONTINUE → MAIN WINDOW  (on Continue button click)
    │
    ├─ mainWindow->show() / raise() / activateWindow()   ← FIRST (prevent Qt auto-quit)
    ├─ loginWindow->accept()                              ← SECOND (close modal)
    ├─ [10ms QTimer] loadWorkspaceByName(defaultWorkspace)
    └─ [50ms QTimer]
        ├─ mainWindow->createIndicesView()
        └─ WindowCacheManager::instance().initialize(mainWindow)
```

---

## 2. Where Each Component is Initialized

| Component | When | Location |
|---|---|---|
| `PreferencesManager` | SplashScreen **ctor** | `SplashScreen::loadPreferences()` |
| `ConfigLoader` | After splash shown | `main.cpp` Phase 1 |
| `LicenseManager` | After config load | `main.cpp` Phase 1b |
| Masters (cache) | During splash | `SplashScreen::preloadMasters()` |
| `PriceStoreGateway` | After masters load | `SplashScreen::onMasterLoadingComplete()` |
| `XTSMarketDataClient` | On login button | `LoginFlowService::executeLogin()` |
| `XTSInteractiveClient` | On login button | `LoginFlowService::executeLogin()` |
| `TradingDataService` | Before login window | `main.cpp` Phase 2 |
| WebSocket (MD) | After both logins | `LoginFlowService::continueLoginAfterMasters()` |
| Positions (REST) | After MD WS connect | `LoginFlowService::continueLoginAfterMasters()` |
| Orders (REST) | After positions | `LoginFlowService::continueLoginAfterMasters()` |
| WebSocket (IA) | After orders | `LoginFlowService::continueLoginAfterMasters()` |
| Trades (REST) | After IA WS | `LoginFlowService::continueLoginAfterMasters()` |
| Masters (download) | After REST data (if ticked) | `LoginFlowService::startMasterDownload()` |
| `PriceStoreGateway` re-init | After downloaded masters | `LoginFlowService::handleMasterLoadingComplete()` |
| `CandleAggregator` | Login complete callback | `main.cpp` Phase 6 |
| `WindowCacheManager` | After workspace loaded | `main.cpp` Phase 7 (+50ms) |

---

## 3. Issues Found

### 3.1 ⚠️ Preferences load BEFORE config — ordering mismatch

**File**: `SplashScreen.cpp` ctor  
**Problem**: `loadPreferences()` runs **inside the SplashScreen constructor**, which is called before `ConfigLoader` is even searched for. This means `PreferencesManager` loads from `QSettings` (OS user profile) — which is correct for user prefs — but if any preference _should_ be seeded from `config.ini` on first run, that's not possible today.  
**Current impact**: Low — `PreferencesManager` uses `QSettings` which is independent of `config.ini`.  
**Recommended fix**: Document the separation explicitly (preferences = per-user QSettings; config = per-deployment INI). Both are fine to load early, but the ordering should be made intentional in `main.cpp`, not hidden in the splash ctor.

---

### 3.2 ⚠️ Data sync ordering: Trades fetched AFTER IA WebSocket connect

**File**: `LoginFlowService::continueLoginAfterMasters()` (lines ~430–530)  
**Problem**: The fetch order is:
```
getPositions → getOrders → connectWebSocket(IA) → getTrades
```
This means **live order/position events can arrive via WebSocket before the REST snapshot is complete** (specifically, trades can arrive live before `getTrades` finishes). If a trade event fires between `connectWebSocket(IA)` and `setTrades()`, it will be applied to an empty `m_trades` vector and then **immediately overwritten** when `setTrades()` runs.

**Recommended fix**: 
```
getPositions → getOrders → getTrades    ← all REST first
connectWebSocket(IA)                    ← live stream last
```

---

### 3.3 ⚠️ PriceStoreGateway initialized twice on master download

**Problem**: `PriceStoreGateway::instance().initialize(...)` is called in **two places**:
1. `SplashScreen::onMasterLoadingComplete()` — after cache load
2. `LoginFlowService::handleMasterLoadingComplete()` — after download/re-load

If the user does NOT tick "Download Masters", path (1) runs during splash and path (2) never runs — correct.  
If the user DOES tick "Download Masters", path (1) runs during splash with stale token list, then path (2) re-runs with fresh tokens — correct but wasteful.  
**Risk**: If splash's initialization starts serving UDP ticks between the two calls, there's a brief window where stores have old token maps. This is acceptable only if the re-init is atomic.  
**Recommendation**: Verify `PriceStoreGateway::initialize()` is safe to call twice (re-entrant/idempotent) and add a log warning on second call.

---

### 3.4 ⚠️ `mainWindow` created before login — potential resource waste

**File**: `main.cpp` Phase 2  
**Problem**: `MainWindow *mainWindow = new MainWindow(nullptr)` is created immediately when the login window is shown, even before any credentials are entered. `MainWindow` likely owns heavy resources (views, docks, market watch tables, etc.).  
**Recommendation**: Defer `MainWindow` creation to the `completeCallback` (Phase 6), after login succeeds. This also prevents the case where the user cancels login and a half-initialized `MainWindow` must be torn down.

---

### 3.5 ⚠️ No timeout / retry on REST data fetches

**File**: `LoginFlowService::continueLoginAfterMasters()`  
**Problem**: `getPositions`, `getOrders`, `getTrades` callbacks are pure fire-and-forget. If any of them silently times out (no callback ever fires), the login flow **stalls permanently** — the complete callback is never invoked and the "Continue" button never appears.  
**Recommendation**: Add a `QTimer` (e.g., 15 seconds) that fires `completeCallback` even if REST fetches are slow/failed, with a warning logged.

---

### 3.6 ℹ️ `loadPreferences()` called in SplashScreen ctor — hidden side effect

The splash ctor calls `PreferencesManager::instance()` which lazily constructs the singleton. This is fine but means any other early caller (e.g., before SplashScreen is created) would get an uninitialized preference state.  
**Recommendation**: Move `loadPreferences()` call to `main.cpp` explicitly, before `new SplashScreen()`, so the initialization order is visible at the top level.

---

### 3.7 ℹ️ `config` pointer captured by lambda — lifetime concern

**File**: `main.cpp`  
**Problem**: `config` (raw `ConfigLoader*`) is captured by value in several lambdas (`readyToClose`, `setOnLoginClicked`, `setCompleteCallback`). These lambdas outlive the scope where `config` was allocated on the heap with `new`. If the lambdas fire after `config` is deleted (unlikely but possible if Qt event queue is backed up), it's a use-after-free.  
**Recommendation**: Use `QSharedPointer<ConfigLoader>` or ensure `config` lifetime is tied to `QApplication`.

---

## 4. Recommended Corrected Ordering (main.cpp)

```cpp
// ── PHASE 1: Qt infrastructure ────────────────────────────────────────────
QApplication app(...)
setupFileLogging()
qRegisterMetaType<>()           // all types
TALibIndicators::initialize()
app.setApplicationName(...)

// ── PHASE 2: Splash Screen ────────────────────────────────────────────────
SplashScreen *splash = new SplashScreen();
// SplashScreen ctor MUST NOT call loadPreferences() anymore ↓

// ── PHASE 3: Load preferences (explicit, visible) ─────────────────────────
PreferencesManager::instance();     // force init / load QSettings
splash->setStatus("Loading preferences...");

// ── PHASE 4: Load config ──────────────────────────────────────────────────
ConfigLoader *config = loadConfig();   // current logic
splash->setProgress(10);

// ── PHASE 5: License check ────────────────────────────────────────────────
LicenseManager::instance().initialize(config);
auto licResult = LicenseManager::instance().checkLicense();
if (!licResult.valid) { /* show error, quit */ }
splash->setProgress(15);

// ── PHASE 6: Preload masters (async, non-blocking) ────────────────────────
splash->preloadMasters();

// ── PHASE 7: readyToClose → show LoginWindow ─────────────────────────────
// ... existing login window setup ...

// ── PHASE 8: Login button ─────────────────────────────────────────────────
// MD login + IA login (parallel, async)

// ── PHASE 9: Masters phase ────────────────────────────────────────────────
// Use cached / wait for splash / download

// ── PHASE 10: REST data sync (ALL before WebSocket) ──────────────────────  ← FIX 3.2
// getPositions → getOrders → getTrades

// ── PHASE 11: WebSocket (IA) connect ─────────────────────────────────────  ← FIX 3.2
// iaClient->connectWebSocket()

// ── PHASE 12: Login complete ──────────────────────────────────────────────
// CandleAggregator::initialize()
// mainWindow = new MainWindow()                                              ← FIX 3.4
// mainWindow->setXTSClients() / setTradingDataService() / setConfigLoader()
// loginWindow->showContinueButton()

// ── PHASE 13: Continue → Main Window ─────────────────────────────────────
// mainWindow->show()
// loadWorkspace()
// createIndicesView()
// WindowCacheManager::initialize()
```

---

## 5. WebSocket Live-Event vs REST Snapshot Race (Detail)

### The gap problem — why "REST first" is still wrong

```
❌ Naive "REST first" (previous fix — still has a gap):
────────────────────────────────────────────────────────
t=0   getPositions() REST request sent
      ... server processing, network RTT ...        ← WINDOW where broker
      ... order fills, events fire on exchange ...  ←   events ARE LOST
t=N   REST response arrives → setPositions()
t=N+1 connectWebSocket(IA)                          ← only NOW do we listen
      ← events from t=0..N are GONE FOREVER
```

Any order that was filled, any position that changed, any trade that executed
between your REST request and your WebSocket connect is silently dropped.
On a slow connection or busy market this window can be **hundreds of
milliseconds** — easily enough to miss fills.

### The correct solution — Snapshot + Replay

```
✅ Snapshot + Replay (current implementation):
───────────────────────────────────────────────
t=0   connectWebSocket(IA)         ← open live stream FIRST
      events arriving → BUFFERED in memory (not yet applied)

t=0   getPositions() ─┐
t=0   getOrders()    ─┤ fired IN PARALLEL  ← REST snapshots in flight
t=0   getTrades()    ─┘

      ... server processing, network RTT ...
      ← live events keep buffering: bufferedOrders[], bufferedTrades[]

t=N   ALL 3 REST responses arrive
      → setPositions(snapshot)      ← apply authoritative baseline
      → setOrders(snapshot)
      → setTrades(snapshot)
      → replay bufferedPositionEvents  ← delta upserts on top of baseline
      → replay bufferedOrderEvents
      → replay bufferedTradeEvents
      m_snapshotApplied = true
      ← from now on, live events go directly to TradingDataService

Result: ZERO event loss. Correct final state regardless of REST latency.
```

### Why replay converges correctly

`TradingDataService` event handlers are **upserts**:
- Orders: matched by `appOrderID`, replaced if found, appended if new
- Trades: always appended (log semantics — never overwritten)
- Positions: matched by `(exchangeInstrumentID + productType + exchangeSegment)`, replaced or appended

So replaying a live event on top of a REST snapshot produces the correct
final state — the same answer you'd get if the broker sent the event again
after the snapshot arrived.


---

## 6. TradingDataService WebSocket Event Handlers (Review)

| Handler | Logic | Issue |
|---|---|---|
| `onOrderEvent` | Find by `appOrderID`, update or append | ✅ Correct — upsert by ID |
| `onTradeEvent` | Always append (trades are a log) | ✅ Correct — additive |
| `onPositionEvent` | Find by `exchangeInstrumentID + productType + exchangeSegment`, update or append | ✅ Correct — upsert by composite key |

All three handlers correctly **emit the full updated collection** after mutation — UI subscribers always get a full refresh, avoiding partial-state bugs.  

One gap: `onPositionEvent` does **not** log the event (unlike `onOrderEvent` and `onTradeEvent`). Minor.

---

## 7. Summary: What's Good, What Needs Fixing

### ✅ Working Well
- Dual parallel API login (MD + IA fire simultaneously)
- Event-driven splash close (not timer-polled)
- `MasterDataState` shared singleton correctly prevents double-loading
- In-memory master load (no disk I/O on fresh download)
- Thread-safe `TradingDataService` with `QMutex` per collection
- UI thread marshalling in `updateStatus()` / `notifyError()`
- `QMetaObject::invokeMethod(..., Qt::QueuedConnection)` used correctly
- LicenseManager stub in correct position (after config, before masters)
- **Snapshot + Replay pattern** — IA WebSocket opened first, events buffered, REST fired in parallel, replay on merge (zero event loss)
- **Structured fetch error callback** — timeout or failure shows a Retry dialog with exact details, not a silent silent proceed
- REST requests for positions/orders/trades fired **in parallel**, not sequentially chained

### 🔴 Must Fix
*(All previously identified must-fixes are now resolved)*

### 🟡 Should Fix
3. **Defer MainWindow construction** to login-complete callback (Issue 3.4)
4. **Move `loadPreferences()` to `main.cpp`** — make ordering explicit (Issue 3.6)
5. **Verify `PriceStoreGateway` double-init safety** (Issue 3.3)

### ℹ️ Low Priority
6. Use `QSharedPointer<ConfigLoader>` to guard lifetime (Issue 3.7)
7. Add `onPositionEvent` debug log to match other handlers *(done)*
