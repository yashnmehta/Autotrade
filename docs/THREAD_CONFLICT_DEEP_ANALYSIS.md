# 🔍 Deep Thread Conflict Analysis & Resolution

**Date:** January 15, 2026  
**Status:** 🔴 CRITICAL ISSUE IDENTIFIED  
**Priority:** P0 - Blocking Production

---

## 🚨 ROOT CAUSE IDENTIFIED

### The Problem

**IndicesView appears DURING login, not AFTER main window is shown.**

### Timeline Analysis

```
1. Application starts
2. MainWindow constructor called
   └─> setupContent()
       └─> ❌ REMOVED: createIndicesView() was here at line 122
3. MainWindow hidden
4. Login window shown
5. User logs in
6. Login completes
7. setConfigLoader() called ✅
   └─> QTimer::singleShot(200ms) to create IndicesView ✅
8. ❌ PROBLEM: Continue button clicked BEFORE 200ms expires!
9. mainWindow->show() called
10. 200ms timer fires
11. IndicesView created and shown

**THE RACE CONDITION:**
- setConfigLoader() schedules IndicesView creation for 200ms later
- Continue button click happens immediately after login
- User sees mainWindow->show() 
- 200ms later, IndicesView pops up (appears to be during rendering)
```

### The Critical Bug

**Line 204 in main.cpp:**
```cpp
loginService->setCompleteCallback([...]{
    // ... setup code ...
    mainWindow->setConfigLoader(config);  // ← Schedules 200ms timer
    
    // Show continue button immediately ← USER CAN CLICK NOW!
    loginWindow->showContinueButton();
});
```

**Line 251 in main.cpp:**
```cpp
loginWindow->setOnContinueClicked([...]{
    mainWindow->show();  // ← Happens before 200ms timer!
});
```

**Result:** IndicesView creation is scheduled but mainWindow->show() happens before the timer fires, creating a race condition where IndicesView appears to pop up during main window rendering.

---

## 🎯 Solution Strategy

### Approach 1: Move IndicesView Creation to AFTER mainWindow->show()

Instead of scheduling in `setConfigLoader()`, schedule it in the continue button callback AFTER the main window is shown.

```cpp
loginWindow->setOnContinueClicked([...]{
    mainWindow->show();
    mainWindow->raise();
    mainWindow->activateWindow();
    
    // ✅ NOW schedule IndicesView creation (after window is visible)
    QTimer::singleShot(300, mainWindow, [mainWindow]() {
        if (!mainWindow->hasIndicesView()) {
            mainWindow->createIndicesView();
        }
    });
    
    loginWindow->accept();
    loginWindow->deleteLater();
});
```

### Approach 2: Create on showEvent

Override `showEvent()` in MainWindow to create IndicesView when window is actually shown:

```cpp
void MainWindow::showEvent(QShowEvent *event) {
    CustomMainWindow::showEvent(event);
    
    if (!m_indicesViewCreated && m_configLoader) {
        QTimer::singleShot(300, this, [this]() {
            createIndicesView();
            m_indicesViewCreated = true;
        });
    }
}
```

---

## 🔧 Recommended Fix: Approach 1 (Cleaner)

Move the IndicesView creation out of `setConfigLoader()` and into the continue button callback.

---

## 📊 Thread Analysis

### Thread Conflicts Found:

#### 1. **UDP Receiver Threads**
- **Location:** UdpBroadcastService.cpp lines 682, 707, 731, 753
- **Type:** std::thread (4 threads for NSE FO/CM, BSE FO/CM)
- **Status:** ✅ SAFE - Uses Qt signals with QueuedConnection
- **Data Flow:** 
  ```
  std::thread (UDP) → emit signal → Qt::QueuedConnection → Main Thread
  ```

#### 2. **WebSocket Threads**
- **Location:** NativeWebSocketClient.cpp lines 83, 188, 383
- **Type:** std::thread (IO thread + heartbeat thread)
- **Status:** ✅ SAFE - Uses Qt signals
- **Data Flow:**
  ```
  std::thread (WebSocket) → emit signal → Main Thread
  ```

#### 3. **Master Loader Thread**
- **Location:** MasterLoaderWorker.cpp (inherits QThread)
- **Type:** QThread
- **Status:** ✅ SAFE - Uses Qt signals
- **Potential Issue:** ⚠️ Thread may still be running when LoginFlowService callback fires
- **Risk:** Masters may load AFTER setConfigLoader() is called

#### 4. **XTS API HTTP Threads**
- **Location:** XTSMarketDataClient.cpp line 810, XTSInteractiveClient.cpp line 547
- **Type:** std::thread (detached HTTP requests)
- **Status:** ⚠️ POTENTIALLY UNSAFE
- **Issue:** Callbacks may fire from background threads
- **Fix Needed:** Wrap callbacks with QMetaObject::invokeMethod

---

## 🔍 Additional Issues Found

### Issue 1: processEvents() in LoginWindow

**File:** `src/ui/LoginWindow.cpp` line 201  
**Status:** ✅ FIXED (removed in previous fix)

### Issue 2: Masters Loading Race Condition

**Problem:** Masters may still be loading when `setConfigLoader()` is called.

**Evidence:**
```cpp
// In LoginFlowService.cpp lines 120-140
if (masterState->areMastersLoaded()) {
    emit mastersLoaded();
    continueLoginAfterMasters();  // ← Calls m_completeCallback
    return;
}

if (masterState->isLoading()) {
    // Connect to mastersReady signal
    // Callback happens when loading completes
}
```

**Risk:** If masters load asynchronously, `setConfigLoader()` might be called before masters are ready, but IndicesView creation expects data to be available.

**Fix:** Ensure IndicesView creation happens AFTER `mastersLoaded` signal.

### Issue 3: XTS API Callback Thread Safety

**Problem:** HTTP requests use detached threads with callbacks.

**Location:** `XTSMarketDataClient.cpp`
```cpp
std::thread([this, url, token, body, callback]() {
    // ... HTTP request ...
    callback(success, message);  // ← May be on background thread!
}).detach();
```

**Fix Needed:** Wrap callback invocation:
```cpp
QMetaObject::invokeMethod(this, [callback, success, message]() {
    callback(success, message);
}, Qt::QueuedConnection);
```

---

## 🎯 Implementation Plan

### Priority 1: Fix IndicesView Race Condition
- [ ] Remove QTimer from `setConfigLoader()`
- [ ] Add QTimer to continue button callback (after mainWindow->show())
- [ ] Test that IndicesView only appears after window is fully rendered

### Priority 2: Fix XTS API Thread Safety
- [ ] Wrap all callback invocations in QMetaObject::invokeMethod
- [ ] Ensure callbacks run on main thread

### Priority 3: Add Master Loading Guard
- [ ] Connect to `mastersLoaded` signal in MainWindow
- [ ] Only enable market data features after signal fires

---

## 🧪 Testing Plan

### Test 1: IndicesView Timing
1. Launch application
2. Login
3. Click Continue button
4. ✅ Verify main window appears instantly
5. ✅ Verify IndicesView appears 300ms later (not during rendering)

### Test 2: Thread Safety
1. Add debug logging to all thread callbacks
2. Verify all UI updates happen on main thread
3. Check with Qt's thread checker

### Test 3: Master Loading
1. Test with slow network (simulate delay)
2. Verify UI doesn't freeze
3. Verify IndicesView waits for masters to load

---

## 📝 Code Changes Required

### File 1: src/main.cpp
Remove scheduling from setConfigLoader callback, add to continue callback

### File 2: src/app/MainWindow/MainWindow.cpp
Remove QTimer from setConfigLoader()

### File 3: src/api/XTSMarketDataClient.cpp
Add QMetaObject::invokeMethod around callbacks

### File 4: src/api/XTSInteractiveClient.cpp
Add QMetaObject::invokeMethod around callbacks

---

**Status:** Analysis complete, implementing fixes now...
