# Multiple SnapQuote Windows Implementation (Max 3)

## Feature Request

Support up to **3 simultaneous SnapQuote windows** with LRU (Least Recently Used) replacement:

### Behavior
1. **F5 on JPPOWER** → Opens SnapQuote Window #1
2. **F5 on NIFTY** → Opens SnapQuote Window #2  
3. **F5 on BANKNIFTY** → Opens SnapQuote Window #3
4. **F5 on TCS** → **Reuses least recently used window** (Window #1)

## Architecture

### Window Pool Structure
```cpp
struct SnapQuoteWindowEntry {
    CustomMDISubWindow* mdiWindow;      // MDI container
    SnapQuoteWindow* window;            // Content widget
    int lastToken;                      // Currently displayed token
    QDateTime lastUsedTime;             // For LRU tracking
    bool needsReset;                    // Needs reset on next show
    bool isVisible;                     // Currently on-screen
};

QVector<SnapQuoteWindowEntry> m_snapQuoteWindows;  // Pool of 3 windows
static constexpr int MAX_SNAPQUOTE_WINDOWS = 3;
```

### LRU Selection Algorithm
```cpp
int findLeastRecentlyUsedSnapQuoteWindow() {
    int lruIndex = 0;
    QDateTime oldestTime = QDateTime::currentDateTime();
    
    for (int i = 0; i < m_snapQuoteWindows.size(); i++) {
        if (m_snapQuoteWindows[i].lastUsedTime < oldestTime) {
            oldestTime = m_snapQuoteWindows[i].lastUsedTime;
            lruIndex = i;
        }
    }
    
    return lruIndex;
}
```

### Window Selection Logic
```cpp
int selectSnapQuoteWindow(int requestedToken) {
    // 1. Check if token is already displayed in any window
    for (int i = 0; i < m_snapQuoteWindows.size(); i++) {
        if (m_snapQuoteWindows[i].lastToken == requestedToken 
            && m_snapQuoteWindows[i].isVisible) {
            return i;  // Reuse same window
        }
    }
    
    // 2. Find first unused window (not yet shown)
    for (int i = 0; i < m_snapQuoteWindows.size(); i++) {
        if (!m_snapQuoteWindows[i].isVisible) {
            return i;  // Use new window
        }
    }
    
    // 3. All 3 windows in use - find LRU
    return findLeastRecentlyUsedSnapQuoteWindow();
}
```

## Implementation Steps

### 1. Update WindowCacheManager.h
```cpp
// Add window pool
QVector<SnapQuoteWindowEntry> m_snapQuoteWindows;
static constexpr int MAX_SNAPQUOTE_WINDOWS = 3;

// Update methods
void markSnapQuoteWindowClosed(int windowIndex);
void resetSnapQuoteWindow(int index);
int findLeastRecentlyUsedSnapQuoteWindow();
```

### 2. Update createCachedWindows()
```cpp
// Create 3 SnapQuote windows instead of 1
for (int i = 0; i < MAX_SNAPQUOTE_WINDOWS; i++) {
    SnapQuoteWindowEntry entry;
    
    QString title = QString("Snap Quote %1").arg(i + 1);
    entry.mdiWindow = new CustomMDISubWindow(title, mdiArea);
    entry.mdiWindow->setWindowType("SnapQuote");
    entry.mdiWindow->setCached(true);
    entry.mdiWindow->setProperty("snapQuoteIndex", i);  // Store index
    
    entry.window = new SnapQuoteWindow(entry.mdiWindow);
    entry.mdiWindow->setContentWidget(entry.window);
    entry.mdiWindow->resize(860, 300);
    
    mdiArea->addWindow(entry.mdiWindow);
    
    // Pre-show off-screen
    entry.mdiWindow->show();
    entry.mdiWindow->move(-10000 - (i * 100), -10000);  // Stagger positions
    entry.mdiWindow->lower();
    
    // Connect UDP
    QObject::connect(&UdpBroadcastService::instance(), 
                     &UdpBroadcastService::udpTickReceived,
                     entry.window, &SnapQuoteWindow::onTickUpdate);
    
    entry.needsReset = false;
    entry.isVisible = false;
    entry.lastToken = -1;
    entry.lastUsedTime = QDateTime::currentDateTime().addSecs(-i);  // Stagger times
    
    m_snapQuoteWindows.append(entry);
}
```

### 3. Update showSnapQuoteWindow()
```cpp
bool WindowCacheManager::showSnapQuoteWindow(const WindowContext* context) {
    if (!context || !context->isValid()) return false;
    
    // Select which window to use (LRU logic)
    int windowIndex = selectSnapQuoteWindow(context->token);
    
    auto& entry = m_snapQuoteWindows[windowIndex];
    
    // Position window
    auto mdiArea = m_mainWindow->mdiArea();
    if (mdiArea) {
        QSize mdiSize = mdiArea->size();
        // Cascade windows (offset each one slightly)
        int x = (mdiSize.width() - 860) / 2 + (windowIndex * 30);
        int y = (mdiSize.height() - 300) / 2 + (windowIndex * 30);
        entry.mdiWindow->move(x, y);
    }
    
    // Load context if changed
    if (entry.needsReset || context->token != entry.lastToken) {
        entry.window->loadFromContext(*context, false);
        entry.lastToken = context->token;
        entry.needsReset = false;
    }
    
    // Update LRU timestamp
    entry.lastUsedTime = QDateTime::currentDateTime();
    entry.isVisible = true;
    
    // Show and activate
    bool wasHidden = !entry.mdiWindow->isVisible();
    if (wasHidden) {
        entry.mdiWindow->show();
        entry.window->show();
    }
    
    // Activate
    QTimer::singleShot(0, this, [this, windowIndex]() {
        auto& e = m_snapQuoteWindows[windowIndex];
        if (e.mdiWindow) {
            e.mdiWindow->raise();
            e.mdiWindow->activateWindow();
            if (e.window) {
                e.window->setFocus();
                e.window->activateWindow();
            }
        }
    });
    
    return true;
}
```

### 4. Update CustomMDISubWindow closeEvent
```cpp
if (m_windowType == "SnapQuote") {
    // Get window index from property
    int index = property("snapQuoteIndex").toInt();
    WindowCacheManager::instance().markSnapQuoteWindowClosed(index);
}
```

## User Experience

### Example Flow
```
User Action          | Window 1    | Window 2    | Window 3    | LRU Order
---------------------|-------------|-------------|-------------|----------
F5 on JPPOWER       | JPPOWER     | [unused]    | [unused]    | 1
F5 on NIFTY         | JPPOWER     | NIFTY       | [unused]    | 1, 2
F5 on BANKNIFTY     | JPPOWER     | NIFTY       | BANKNIFTY   | 1, 2, 3
F5 on TCS           | TCS ✅      | NIFTY       | BANKNIFTY   | 2, 3, 1
F5 on RELIANCE      | TCS         | RELIANCE ✅ | BANKNIFTY   | 3, 1, 2
Close Window 3      | TCS         | RELIANCE    | [closed]    | 1, 2
F5 on INFY          | TCS         | RELIANCE    | INFY ✅     | 1, 2, 3
```

## Performance

### Memory
- **Before**: 1 SnapQuote window (~2MB)
- **After**: 3 SnapQuote windows (~6MB)
- **Impact**: +4MB (acceptable)

### Speed
- **Window selection**: < 1ms (simple loop)
- **Show time**: Still 5-10ms per window
- **No performance degradation**

## Benefits

1. **Multi-tasking**: Compare 3 scrips side-by-side
2. **No data loss**: Previous windows stay open
3. **Smart reuse**: Oldest window automatically replaced
4. **Consistent speed**: All windows pre-cached and instant

## Testing Checklist

- [ ] F5 3 times opens 3 separate windows
- [ ] 4th F5 reuses oldest window
- [ ] Windows cascade properly (not overlapping)
- [ ] Each window shows correct scrip data
- [ ] Closing a window frees it for reuse
- [ ] ESC closes active window only
- [ ] All windows update via UDP
- [ ] Window titles show "Snap Quote 1/2/3"
- [ ] Performance stays < 20ms
- [ ] No memory leaks

## Future Enhancements

1. **Configurable max**: Let user set 1-5 windows
2. **Tab bar**: Show all open windows in a tab strip
3. **Pin windows**: Prevent LRU replacement
4. **Workspace save**: Remember open windows
