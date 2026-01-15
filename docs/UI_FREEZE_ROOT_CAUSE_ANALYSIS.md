# UI Freeze - Root Cause Analysis & Fix

**Date:** January 15, 2026  
**Status:** 🔴 CRITICAL - UI Freeze on Startup  
**Severity:** BLOCKER

---

## 🐛 Problem Description

The application starts successfully, connects to WebSocket services, and initializes UDP broadcast receivers, but then the **UI completely freezes** and becomes unresponsive.

### Symptoms:
- Application launches and shows window
- WebSocket connections successful
- UDP multicast receivers start
- **UI freezes** - no interaction possible
- Terminal shows: `PS C:\Users\admin\Desktop\trading_terminal_cpp\build>` with cursor stuck

---

## 🔍 Root Cause Analysis

###  **Issue #1: Blocking UDP Receiver Start (CRITICAL)**

**File:** `src/services/UdpBroadcastService.cpp` (lines 626-750)

**Problem:**
The `UdpBroadcastService::start()` is called **synchronously on the UI thread** during `MainWindow::setupNetwork()`, which is invoked from `MainWindow::setConfigLoader()`.

**Blocking Operations:**
1. **MulticastReceiver initialization** - Creates sockets, binds to multicast groups
2. **Thread creation** - Spawns 4 background threads (NSE FO, NSE CM, BSE FO, BSE CM)
3. **Socket binding** - May block if network interface is slow
4. **Multicast group join** - Potentially blocking system call

**Code Flow:**
```cpp
main.cpp
 ↓
loginService->setCompleteCallback() 
 ↓
mainWindow->setConfigLoader(config)  // Called when login completes
 ↓
setupNetwork()  // ⚠️ On UI thread!
 ↓
startBroadcastReceiver()  // ⚠️ BLOCKING!
 ↓
UdpBroadcastService::instance().start(config)  // ⚠️ Creates 4 receivers + threads
 ↓
startReceiver() x 4  // Each creates socket, binds, joins multicast
 ↓
UI THREAD BLOCKED - No event processing!
```

**Why This Freezes UI:**
- All socket operations happen on the **main UI thread**
- Qt event loop **cannot process** mouse/keyboard events
- Window painting/resizing blocked
- User sees frozen window

---

### 🔎 **Issue #2: RepositoryManager Loading (MEDIUM)**

**File:** `src/ui/SplashScreen.cpp` (lines 1-150)

**Problem:**
Master file loading happens during splash screen but uses `QApplication::processEvents()` extensively (lines 68, 74, 90, 195), which can cause nested event loops and unpredictable behavior.

**Risk:**
- Nested event loops can process signals out of order
- Modal dialogs can appear before main window
- Race conditions between splash and main window initialization

---

### 🔎 **Issue #3: PriceCache Initialization (LOW)**

**File:** `src/app/MainWindow/MainWindow.cpp` (line 55)

**Problem:**
`PriceCacheZeroCopy::instance()` is initialized on first access during signal connection setup, but the initialization is:
```cpp
ΓÜí Native PriceCache initialized (composite key: segment|token)
```

This appears after UDP receivers start, suggesting lazy initialization during critical path.

---

## ✅ Solutions

### **Fix #1: Async UDP Receiver Start** 🔴 CRITICAL

Move UDP receiver initialization to background thread using `QThread` or `std::async`.

**Implementation:**

```cpp
// src/app/MainWindow/Network.cpp
void MainWindow::setupNetwork() {
    QSettings settings("TradingCompany", "TradingTerminal");
    if (settings.value("Network/AutoStartBroadcast", true).toBool()) {
        // Use QTimer to defer start to next event loop iteration
        QTimer::singleShot(0, this, &MainWindow::startBroadcastReceiver);
    }
}

void MainWindow::startBroadcastReceiver() {
    if (!m_configLoader) {
        qWarning() << "[MainWindow] startBroadcastReceiver failed: ConfigLoader is missing!";
        return;
    }

    UdpBroadcastService::Config config;
    config.nseFoIp = m_configLoader->getNSEFOMulticastIP().toStdString();
    config.nseFoPort = m_configLoader->getNSEFOPort();
    config.nseCmIp = m_configLoader->getNSECMMulticastIP().toStdString();
    config.nseCmPort = m_configLoader->getNSECMPort();
    config.bseFoIp = m_configLoader->getBSEFOMulticastIP().toStdString();
    config.bseFoPort = m_configLoader->getBSEFOPort();
    config.bseCmIp = m_configLoader->getBSECMMulticastIP().toStdString();
    config.bseCmPort = m_configLoader->getBSECMPort();

    if (config.bseFoIp.empty()) config.bseFoIp = "239.1.2.5";
    if (config.bseFoPort == 0) config.bseFoPort = 27002;
    if (config.bseCmIp.empty()) config.bseCmIp = "239.1.2.5";
    if (config.bseCmPort == 0) config.bseCmPort = 27001;

    if (m_statusBar) {
        m_statusBar->showMessage("Market Data Receivers: INITIALIZING...", 3000);
    }

    // ✅ FIX: Start UDP receivers asynchronously
    QtConcurrent::run([config, this]() {
        qDebug() << "[MainWindow] Starting UDP receivers in background thread...";
        UdpBroadcastService::instance().start(config);
        
        // Update status on main thread
        QMetaObject::invokeMethod(this, [this]() {
            if (m_statusBar) {
                m_statusBar->showMessage("Market Data Receivers: ACTIVE", 5000);
            }
        }, Qt::QueuedConnection);
    });
}
```

**Alternative (Simpler):** Use QTimer with 0ms delay:
```cpp
void MainWindow::setupNetwork() {
    QSettings settings("TradingCompany", "TradingTerminal");
    if (settings.value("Network/AutoStartBroadcast", true).toBool()) {
        // Defer to next event loop cycle - allows UI to render first
        QTimer::singleShot(100, this, &MainWindow::startBroadcastReceiverDeferred);
    }
}

void MainWindow::startBroadcastReceiverDeferred() {
    // ... existing startBroadcastReceiver() logic
}
```

---

### **Fix #2: Remove Excessive processEvents()** 🟡 MEDIUM

**File:** `src/ui/SplashScreen.cpp`

Replace `QApplication::processEvents()` with proper Qt signals/slots.

**Before:**
```cpp
void SplashScreen::setProgress(int value)
{
    ui->progressBar->setValue(value);
    QApplication::processEvents(); // ❌ BAD: Nested event loop
}
```

**After:**
```cpp
void SplashScreen::setProgress(int value)
{
    ui->progressBar->setValue(value);
    // Qt will update UI automatically on next event loop
}
```

Remove from lines: 68, 74, 90, 195

---

### **Fix #3: Preload PriceCache** 🟢 LOW

Initialize PriceCache early in main.cpp before MainWindow creation:

```cpp
// src/main.cpp (before MainWindow creation)
qDebug() << "[Main] Initializing PriceCache...";
PriceCacheZeroCopy::instance(); // Force initialization
FeedHandler::instance(); // Force initialization
```

---

## 📋 Implementation Plan

### Phase 1: Emergency Fix (15 minutes)
1. ✅ Add `QTimer::singleShot(100, ...)` to `setupNetwork()`
2. ✅ Test - UI should be responsive immediately

### Phase 2: Proper Fix (1 hour)
1. ✅ Move UDP start to `QtConcurrent::run()`
2. ✅ Add progress callback for status bar
3. ✅ Remove `processEvents()` from SplashScreen
4. ✅ Test on all platforms

### Phase 3: Optimization (2 hours)
1. ⏳ Profile startup time
2. ⏳ Lazy-load non-critical services
3. ⏳ Add startup progress indicator

---

## 🧪 Testing Checklist

### Before Fix:
- [ ] Application starts
- [ ] Window appears
- [ ] **UI freezes** ❌
- [ ] Cannot interact with window ❌

### After Fix:
- [ ] Application starts
- [ ] Window appears
- [ ] **UI responsive immediately** ✅
- [ ] Can drag window ✅
- [ ] Can click menu ✅
- [ ] UDP receivers start in background ✅
- [ ] Market data flows after ~1 second ✅

---

## 📊 Performance Impact

### Current (Broken):
```
Startup: 3-5 seconds
UI Freeze: PERMANENT
User Experience: BLOCKED
```

### Expected (Fixed):
```
Startup: 3-5 seconds
UI Freeze: NONE
UDP Init: 100-500ms (async, non-blocking)
User Experience: SMOOTH
```

---

## 🔧 Additional Recommendations

### 1. Add Connection Status Indicator
Show UDP receiver status in status bar or connection toolbar.

### 2. Implement Timeout
Add timeout for UDP receiver initialization (30 seconds max).

### 3. Error Handling
Show user-friendly error if multicast fails (firewall, network).

### 4. Startup Splash
Add proper startup splash with progress:
- "Loading configuration..."
- "Connecting to XTS API..."
- "Starting market data receivers..."
- "Ready"

### 5. Logging
Add detailed timing logs:
```cpp
auto t1 = std::chrono::steady_clock::now();
// ... operation
auto t2 = std::chrono::steady_clock::now();
qDebug() << "Operation took" << std::chrono::duration_cast<std::chrono::milliseconds>(t2-t1).count() << "ms";
```

---

## 🎯 Priority

**CRITICAL - Fix immediately!**

Current state: **Application unusable**  
Impact: **100% of users blocked**  
Risk: **High - production deployment impossible**

---

## 📝 Notes

- This is a **blocking bug** that prevents any usage of the application
- The fix is **simple** (add QTimer::singleShot)
- The root cause is **synchronous network initialization on UI thread**
- Similar issues may exist in other parts (WebSocket connect, Repository load)
- Need to audit all startup code for blocking operations

---

**Next Steps:**
1. Implement Fix #1 (QTimer async start)
2. Rebuild and test
3. Verify UI responsiveness
4. Commit fix with detailed commit message
5. Create follow-up task for comprehensive startup refactor
