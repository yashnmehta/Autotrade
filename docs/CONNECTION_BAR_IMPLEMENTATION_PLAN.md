
# Connection Bar — Full Implementation Roadmap

**Date:** February 27, 2026  
**Priority:** HIGH — Core infrastructure for market data connectivity  
**Reference:** Autotrader-Reborn "Broadcast Server Reconnect" dialog (screenshot)

---

## 📸 Current State Analysis

### What Exists Today

```
┌──────────────────────────────────────────────────────────────────────────────────────────┐
│ Connection Status: Disconnected                                                          │ ← Connection Bar (cosmetic only)
└──────────────────────────────────────────────────────────────────────────────────────────┘
```

The current `createConnectionBar()` in `MainWindow/UI.cpp` is a **static label** — it says "Disconnected" and never changes. There is no interactive UI for managing connections.

### What Exists Under the Hood (Already Working)

| Component | File | Role |
|---|---|---|
| **LoginFlowService** | `services/LoginFlowService.h` | XTS REST login (MD + IA), master download, position/order/trade sync |
| **XTSMarketDataClient** | `api/xts/XTSMarketDataClient.h` | WebSocket for market data (subscribe/tick events) |
| **XTSInteractiveClient** | `api/xts/XTSInteractiveClient.h` | WebSocket for orders/positions/trades |
| **UdpBroadcastService** | `services/UdpBroadcastService.h` | NSE FO/CM + BSE FO/CM multicast UDP receivers |
| **XTSFeedBridge** | `services/XTSFeedBridge.h` | Routes subscriptions to XTS REST API (XTS_ONLY mode) |
| **FeedHandler** | `services/FeedHandler.h` | Centralized pub/sub for tick distribution |
| **ConfigLoader** | `utils/ConfigLoader.h` | Reads `config.ini` (UDP IPs, ports, feed mode, XTS URLs) |
| **IMarketDataProvider** | `api/IMarketDataProvider.h` | Abstract interface (XTS / UDP / Hybrid) |

### Current Connection Flow

```
App Start → SplashScreen → LoginWindow (modal)
                              │
                              ├─ User clicks "Login"
                              │   ├─ LoginFlowService.executeLogin()
                              │   │   ├─ XTS MD REST login → WebSocket connect
                              │   │   ├─ XTS IA REST login → WebSocket connect
                              │   │   ├─ Master download/cache load
                              │   │   └─ Position/Order/Trade REST sync + event buffering
                              │   └─ Login complete → Continue button appears
                              │
                              ├─ User clicks "Continue"
                              │   ├─ MainWindow.show()
                              │   ├─ MainWindow.setupNetwork()
                              │   │   ├─ Check feed mode (hybrid / xts_only)
                              │   │   ├─ Start UdpBroadcastService (if hybrid)
                              │   │   ├─ Initialize XTSFeedBridge
                              │   │   └─ Initialize GreeksCalculationService
                              │   └─ Load workspace, IndicesView
                              │
                              └─ App runs — NO WAY to reconnect/reconfigure
```

### Current Config (`config.ini`)

```ini
[XTS]
url           = http://192.168.102.9:3000
mdurl         = http://192.168.102.9:3000/marketdataapi
disable_ssl   = true
broadcastmode = Partial

[UDP]
nse_fo_multicast_ip   = 233.1.2.5
nse_fo_port           = 34331
nse_cm_multicast_ip   = 233.1.2.5
nse_cm_port           = 8223
bse_fo_multicast_ip   = 239.1.2.5
bse_fo_port           = 27002
bse_cm_multicast_ip   = 239.1.2.5
bse_cm_port           = 27001

[FEED]
mode = hybrid   # hybrid | xts_only
```

---

## 🎯 Use Cases

### UC-1: See Connection Status at a Glance
> As a trader, I want to see whether XTS WebSocket and UDP feeds are connected in the connection bar, so I know if my price data is live.

**Current:** Static "Disconnected" label — never updates.  
**Needed:** Real-time status indicators per connection type.

### UC-2: Reconnect/Disconnect UDP Receivers
> As a trader, I want to start/stop UDP receivers from the connection bar without using the Data menu.

**Current:** `Data → Start NSE Broadcast Receiver` / `Stop NSE Broadcast Receiver`.  
**Needed:** Quick toggle in the connection bar.

### UC-3: Change UDP Server IP/Port at Runtime
> As a trader connecting to different broadcast servers (primary vs. secondary), I want to change the UDP multicast IP and port without restarting the app.

**Current:** Only from `config.ini` (requires app restart).  
**Needed:** A settings dialog (like the reference "Broadcast Server Reconnect" screenshot).

### UC-4: See XTS WebSocket Connection Health
> As a trader, I want to see if XTS MD and IA WebSockets are connected/disconnected/reconnecting.

**Current:** Only shown during login flow, then lost.  
**Needed:** Persistent status in connection bar.

### UC-5: Switch Default Market Data Source
> As a trader switching between office (UDP access) and home (internet only), I want to switch between Hybrid and XTS-only mode without editing config.ini.

**Current:** `config.ini → [FEED] mode = hybrid | xts_only`  
**Needed:** Dropdown or toggle in the connection bar.

### UC-6: View Connection Statistics
> As a trader, I want to see packets/sec, tick counts, and latency for each feed to verify data quality.

**Current:** `UdpBroadcastService::getStats()` exists but is not exposed in UI.  
**Needed:** Tooltip or expandable stats panel.

---

## 🏗️ Architecture Design

### Component Hierarchy

```
┌─────────────────────────────────────────────────────────────────────────────────┐
│ MainWindow                                                                       │
│ ┌─────────────────────────────────────────────────────────────────────────────┐ │
│ │ ConnectionBar (QWidget — embedded in QToolBar)                              │ │
│ │ ┌────────┐ ┌──────────┐ ┌───────────┐ ┌──────────────┐ ┌───────────────┐  │ │
│ │ │FeedMode│ │ XTS MD ● │ │ XTS IA  ● │ │ UDP NSEFO  ● │ │ UDP NSECM  ● │  │ │
│ │ │Hybrid ▼│ │Connected │ │Connected  │ │Active 1.2k/s │ │Active  800/s │  │ │
│ │ └────────┘ └──────────┘ └───────────┘ └──────────────┘ └───────────────┘  │ │
│ │                                        ┌──────────────┐ ┌───────────────┐  │ │
│ │                          [⚙ Settings]  │ UDP BSEFO  ● │ │ UDP BSECM  ● │  │ │
│ │                                        │Active  200/s │ │Active  100/s │  │ │
│ │                                        └──────────────┘ └───────────────┘  │ │
│ └─────────────────────────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────────────────────┘
```

### Key Design Decisions

1. **ConnectionBar** = New standalone `QWidget` class (replaces inline code in UI.cpp)
2. **ConnectionStatusManager** = New service singleton that tracks all connection states
3. **BroadcastSettingsDialog** = New `.ui`-based dialog for UDP server configuration (mimics reference screenshot)
4. **Feed mode** = Dropdown in connection bar, persisted to `config.ini`
5. **Status indicators** = Colored dots (🟢 Connected / 🔴 Disconnected / 🟡 Reconnecting)

---

## 📋 Implementation Roadmap

### PHASE 1: ConnectionStatusManager (Service Layer) 
**Effort:** 1-2 hours | **Files:** 2 new

The brain — aggregates connection states from all services into a single observable model.

```
include/services/ConnectionStatusManager.h  (NEW)
src/services/ConnectionStatusManager.cpp     (NEW)
```

**Responsibilities:**
- Track state for: XTS MD, XTS IA, UDP NSEFO, UDP NSECM, UDP BSEFO, UDP BSECM
- Emit signals on state change: `connectionStateChanged(ConnectionId, State)`
- Provide stats: `getStats()` → packets/sec, latency, uptime
- Expose current feed mode and allow runtime switching

**State Machine per Connection:**

```
   ┌──────────┐   start()   ┌────────────┐   success   ┌───────────┐
   │Disconnected│─────────→│ Connecting  │────────────→│ Connected  │
   └──────────┘            └────────────┘              └───────────┘
        ↑                       │                           │
        │          error/       │                  error/   │
        │          timeout      │                  dropped  │
        │                       ↓                           ↓
        │               ┌──────────────┐           ┌──────────────┐
        └───────────────│    Error     │←──────────│Reconnecting  │
                        └──────────────┘           └──────────────┘
```

**Enum Definitions:**

```cpp
enum class ConnectionId {
    XTS_MARKET_DATA,    // XTS MD WebSocket
    XTS_INTERACTIVE,    // XTS IA WebSocket
    UDP_NSEFO,          // NSE F&O UDP multicast
    UDP_NSECM,          // NSE CM UDP multicast
    UDP_BSEFO,          // BSE F&O UDP multicast
    UDP_BSECM           // BSE CM UDP multicast
};

enum class ConnectionState {
    Disconnected,       // Not started / stopped
    Connecting,         // Attempting connection
    Connected,          // Active and receiving data
    Reconnecting,       // Lost connection, attempting recovery
    Error               // Failed (with error message)
};
```

**Integration Points:**
- Listen to `UdpBroadcastService::receiverStatusChanged(ExchangeReceiver, bool)`
- Listen to `XTSMarketDataClient` WebSocket state signals
- Listen to `XTSInteractiveClient` WebSocket state signals
- Listen to `XTSFeedBridge::feedModeChanged(FeedMode)`

---

### PHASE 2: ConnectionBar Widget (UI Layer)
**Effort:** 2-3 hours | **Files:** 3 new + 1 modified

The visual — replaces the static label with an interactive, live-updating bar.

```
include/app/ConnectionBar.h              (NEW)
src/app/ConnectionBar.cpp                (NEW)
resources/forms/ConnectionBar.ui         (NEW - optional, could be code-only)
src/app/MainWindow/UI.cpp               (MODIFIED - swap old widget for new)
```

**Layout:**

```
┌───────────────────────────────────────────────────────────────────────────────┐
│ Feed: [Hybrid ▼]  │  XTS MD: ● Connected  │  XTS IA: ● Connected  │  ⚙     │
│                    │  UDP:    ● 4/4 Active  (2.1k ticks/s)         │        │
└───────────────────────────────────────────────────────────────────────────────┘
```

**Widgets:**
| Widget | Type | Role |
|---|---|---|
| `m_feedModeCombo` | QComboBox | "Hybrid" / "XTS Only" selector |
| `m_xtsStatusLabel` | QLabel | "XTS MD: ● Connected" with colored dot |
| `m_iaStatusLabel` | QLabel | "XTS IA: ● Connected" with colored dot |
| `m_udpStatusLabel` | QLabel | "UDP: ● 4/4 Active (2.1k ticks/s)" |
| `m_settingsButton` | QToolButton | Opens BroadcastSettingsDialog |
| (optional) `m_reconnectButton` | QPushButton | Quick UDP reconnect |

**Features:**
- Status dot colors: 🟢`#16a34a` Connected, 🔴`#dc2626` Disconnected, 🟡`#f59e0b` Reconnecting
- Tooltip on hover: detailed per-receiver stats
- Feed mode change: calls `XTSFeedBridge::setFeedMode()` + restarts/stops UDP
- Settings button: opens Phase 3 dialog
- Compact: Fixed height 28px (matches existing toolbar)

**Styling:**
- Follow light theme palette from coding instructions
- Background: `#f8fafc`, Border: `#e2e8f0`, Text: `#475569`

---

### PHASE 3: BroadcastSettingsDialog (UDP Configuration)
**Effort:** 2-3 hours | **Files:** 3 new

Interactive UDP server configuration — based on the reference "Broadcast Server Reconnect" screenshot.

```
include/ui/BroadcastSettingsDialog.h     (NEW)
src/ui/BroadcastSettingsDialog.cpp       (NEW)
resources/forms/BroadcastSettingsDialog.ui (NEW)
```

**Layout (inspired by reference screenshot):**

```
┌─────────────────────────────────────────────────────────────┐
│  Broadcast Server Configuration                        ✕   │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  ┌─ NSE F&O ────────────────────────────────────────────┐  │
│  │ Multicast IP: [233] . [1] . [2] . [5]               │  │
│  │ UDP Port:     [34331        ]                        │  │
│  │ Status:       ● Active (1,200 pkt/s)                 │  │
│  │               [Start]  [Stop]                        │  │
│  └──────────────────────────────────────────────────────┘  │
│                                                             │
│  ┌─ NSE CM ─────────────────────────────────────────────┐  │
│  │ Multicast IP: [233] . [1] . [2] . [5]               │  │
│  │ UDP Port:     [8223         ]                        │  │
│  │ Status:       ● Active (800 pkt/s)                   │  │
│  │               [Start]  [Stop]                        │  │
│  └──────────────────────────────────────────────────────┘  │
│                                                             │
│  ┌─ BSE F&O ────────────────────────────────────────────┐  │
│  │ Multicast IP: [239] . [1] . [2] . [5]               │  │
│  │ UDP Port:     [27002        ]                        │  │
│  │ Status:       ● Active (200 pkt/s)                   │  │
│  │               [Start]  [Stop]                        │  │
│  └──────────────────────────────────────────────────────┘  │
│                                                             │
│  ┌─ BSE CM ─────────────────────────────────────────────┐  │
│  │ Multicast IP: [239] . [1] . [2] . [5]               │  │
│  │ UDP Port:     [27001        ]                        │  │
│  │ Status:       ● Active (100 pkt/s)                   │  │
│  │               [Start]  [Stop]                        │  │
│  └──────────────────────────────────────────────────────┘  │
│                                                             │
│  ┌─ Feed Mode ──────────────────────────────────────────┐  │
│  │ Default Source: [Hybrid (UDP + XTS)          ▼]      │  │
│  │ XTS Base URL:   [http://192.168.102.9:3000   ]       │  │
│  └──────────────────────────────────────────────────────┘  │
│                                                             │
│               [Connect All]  [Disconnect All]  [Save]       │
└─────────────────────────────────────────────────────────────┘
```

**Features:**
- Per-segment: IP (4 octets), Port, Status, Start/Stop buttons
- Live status with packet count per second
- Connect All / Disconnect All for quick operations
- Save: Writes changes back to `config.ini`
- Validates IP format and port range
- Shows real-time status (refreshes every 1s via QTimer)

---

### PHASE 4: Wire Everything Together
**Effort:** 1-2 hours | **Files:** 4-5 modified

```
src/app/MainWindow/UI.cpp               (MODIFIED - use new ConnectionBar)
src/app/MainWindow/MainWindow.cpp        (MODIFIED - wire signals)
src/app/MainWindow/Network.cpp           (MODIFIED - integrate ConnectionStatusManager)
include/app/MainWindow.h                 (MODIFIED - add ConnectionBar member)
src/app/AppBootstrap.cpp                 (MODIFIED - pass connection state to bar)
```

**Key Wiring:**

```cpp
// MainWindow::setupContent()
m_connectionBar = new ConnectionBar(this);
m_connectionToolBar->addWidget(m_connectionBar);

// MainWindow::setupConnections()  
connect(&ConnectionStatusManager::instance(), &ConnectionStatusManager::stateChanged,
        m_connectionBar, &ConnectionBar::onConnectionStateChanged);

connect(m_connectionBar, &ConnectionBar::feedModeChanged,
        this, &MainWindow::onFeedModeChanged);

connect(m_connectionBar, &ConnectionBar::settingsRequested,
        this, &MainWindow::showBroadcastSettings);

// After login complete
ConnectionStatusManager::instance().setXtsConnected(ConnectionId::XTS_MARKET_DATA, true);
ConnectionStatusManager::instance().setXtsConnected(ConnectionId::XTS_INTERACTIVE, true);
```

---

### PHASE 5: Persistence & Polish
**Effort:** 1-2 hours | **Files:** 2-3 modified

- **Save UDP config changes** to `config.ini` via ConfigLoader (add `setValue()` method)
- **Save feed mode** to `config.ini` and `QSettings`
- **Auto-reconnect** on config change (stop → update config → start)
- **Stats refresh** timer in BroadcastSettingsDialog (1s interval)
- **Keyboard shortcut**: `Ctrl+K` → open BroadcastSettingsDialog

---

## 📊 Complete File Manifest

### New Files (8)

| File | Type | Description |
|---|---|---|
| `include/services/ConnectionStatusManager.h` | Header | Connection state aggregator |
| `src/services/ConnectionStatusManager.cpp` | Source | State tracking, signals |
| `include/app/ConnectionBar.h` | Header | Connection bar widget |
| `src/app/ConnectionBar.cpp` | Source | Bar UI, indicators, feed mode |
| `include/ui/BroadcastSettingsDialog.h` | Header | UDP settings dialog |
| `src/ui/BroadcastSettingsDialog.cpp` | Source | Dialog logic, validation |
| `resources/forms/BroadcastSettingsDialog.ui` | Qt UI | Dialog layout |
| `docs/CONNECTION_BAR_ARCHITECTURE.md` | Docs | Architecture reference |

### Modified Files (6-7)

| File | Change |
|---|---|
| `src/app/MainWindow/UI.cpp` | Replace `createConnectionBar()` with new `ConnectionBar` widget |
| `src/app/MainWindow/MainWindow.cpp` | Wire signals, add settings slot |
| `src/app/MainWindow/Network.cpp` | Update ConnectionStatusManager on network events |
| `include/app/MainWindow.h` | Add `ConnectionBar*` member, new slots |
| `src/app/AppBootstrap.cpp` | Pass XTS state to ConnectionStatusManager after login |
| `CMakeLists.txt` or `src/CMakeLists.txt` | Add new source files + .ui |
| `resources/resources.qrc` | Add new .ui if needed |

---

## ⏱️ Estimated Timeline

| Phase | Effort | Dependency |
|---|---|---|
| Phase 1: ConnectionStatusManager | 1-2 hrs | None |
| Phase 2: ConnectionBar widget | 2-3 hrs | Phase 1 |
| Phase 3: BroadcastSettingsDialog | 2-3 hrs | Phase 1 |
| Phase 4: Wiring | 1-2 hrs | Phase 2 + 3 |
| Phase 5: Persistence & polish | 1-2 hrs | Phase 4 |
| **Total** | **7-12 hrs** | |

---

## 🔌 Signal/Slot Diagram

```
                    ┌─────────────────────────┐
                    │ ConnectionStatusManager  │ (Singleton Service)
                    │   - tracks 6 connections │
                    │   - aggregates stats     │
                    └────────┬────────────────┘
                             │ stateChanged(id, state)
                    ┌────────┼──────────────────────────────────────┐
                    ↓        ↓                                      ↓
            ┌──────────────┐ ┌──────────────────────────┐ ┌────────────────┐
            │ ConnectionBar │ │ BroadcastSettingsDialog   │ │ StatusBar      │
            │ (live dots)   │ │ (detailed per-receiver)  │ │ (summary msg)  │
            └──────┬───────┘ └──────┬───────────────────┘ └────────────────┘
                   │                │
    feedModeChanged│    connectAll/ │ disconnectAll/
                   │    startReceiver/stopReceiver
                   ↓                ↓
         ┌──────────────────────────────┐
         │      MainWindow              │
         │  onFeedModeChanged()         │
         │  showBroadcastSettings()     │
         └──────────┬───────────────────┘
                    │
           ┌────────┼────────────────┐
           ↓        ↓                ↓
    ┌────────────┐ ┌───────────┐ ┌────────────┐
    │ XTSFeed    │ │ UdpBroad  │ │ ConfigLoader│
    │ Bridge     │ │ castSvc   │ │ (save ini) │
    └────────────┘ └───────────┘ └────────────┘
```

---

## 🎨 Visual Design Mockup

### ConnectionBar (Compact — 28px toolbar)

```
┌─────────────────────────────────────────────────────────────────────────────────────────┐
│ Feed: [Hybrid ▼]  │  XTS: 🟢 MD  🟢 IA  │  UDP: 🟢 NSEFO  🟢 NSECM  ⚪ BSEFO  ⚪ BSECM  │  ⚙ │
└─────────────────────────────────────────────────────────────────────────────────────────────────┘
```

**Legend:**
- 🟢 = Connected/Active (`#16a34a`)
- 🔴 = Error (`#dc2626`)  
- 🟡 = Connecting/Reconnecting (`#f59e0b`)
- ⚪ = Disabled/Not configured (`#94a3b8`)

### Tooltip (on hover over "UDP: 🟢 NSEFO"):

```
┌──────────────────────────────────┐
│ NSE F&O UDP Broadcast            │
│ ─────────────────────────────── │
│ IP:       233.1.2.5              │
│ Port:     34331                  │
│ Status:   Connected              │
│ Packets:  1,247/sec              │
│ Uptime:   2h 15m                 │
│ Last Tick: 12:04:23.456          │
└──────────────────────────────────┘
```

---

## 🧪 Testing Strategy

### Unit Tests
- `ConnectionStatusManager`: state transitions, signal emission
- IP validation (4 octets, 0-255 range)
- Port validation (1-65535)

### Manual Tests
1. Start app → Connection bar shows "Disconnected" for all
2. Login → XTS MD/IA switch to "Connected"
3. Network → UDP receivers start → UDP dots go green
4. Open settings → change UDP IP → Apply → receivers restart
5. Switch feed mode Hybrid → XTS Only → UDP dots go gray
6. Kill server → XTS dots go red → auto-reconnect → yellow → green
7. Save config → restart app → settings persisted

---

## ❓ Open Questions

1. **Should feed mode change require app restart?**  
   → Proposed: No. Hot-switch via `XTSFeedBridge::setFeedMode()` + start/stop UDP.

2. **Should we support Primary/Secondary UDP servers (like reference)?**  
   → Proposed: Phase 5 enhancement. Start with single server per segment.

3. **Should the bar auto-hide in XTS-only mode?**  
   → Proposed: No. Show all indicators, but UDP dots show "Disabled" (gray).

4. **Where to persist runtime config changes?**  
   → Proposed: `config.ini` for UDP/feed settings, `QSettings` for UI preferences.

---

## 🚀 Recommended Implementation Order

```
Phase 1 → Phase 2 → Phase 4 → Phase 3 → Phase 5
(service)  (bar UI)  (wire)    (dialog)   (polish)
```

Start with the service layer (Phase 1) and the connection bar (Phase 2), wire them together (Phase 4) for immediate visual feedback, then add the detailed settings dialog (Phase 3) and persistence (Phase 5).

**Ready to start Phase 1?**
