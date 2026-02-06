#include "core/WindowCacheManager.h"
#include "api/XTSMarketDataClient.h"
#include "app/MainWindow.h"
#include "core/widgets/CustomMDIArea.h"
#include "core/widgets/CustomMDISubWindow.h"
#include "models/WindowContext.h"
#include "services/UdpBroadcastService.h" // ⚡ For real-time UDP updates
#include "utils/WindowManager.h"
#include "views/BuyWindow.h"
#include "views/SellWindow.h"
#include "views/SnapQuoteWindow.h"
#include <QDateTime> // Added for performance logging
#include <QDebug>
#include <QElapsedTimer>
#include <QSettings>
#include <QTimer>

WindowCacheManager &WindowCacheManager::instance() {
  static WindowCacheManager instance;
  return instance;
}

WindowCacheManager::WindowCacheManager() {}

WindowCacheManager::~WindowCacheManager() {}

void WindowCacheManager::initialize(MainWindow *mainWindow) {
  // ⏱️ PERFORMANCE LOG: Track WindowCacheManager initialization CPU usage
  QElapsedTimer initTimer;
  initTimer.start();
  qint64 startTime = QDateTime::currentMSecsSinceEpoch();

  qDebug() << "========================================";
  qDebug() << "[PERF] [WINDOW_CACHE_INIT] START";
  qDebug() << "  Timestamp:" << startTime;
  qDebug() << "========================================";

  if (m_initialized) {
    qDebug() << "[WindowCacheManager] Already initialized";
    return;
  }

  if (!mainWindow) {
    qWarning() << "[WindowCacheManager] Cannot initialize: MainWindow is null";
    return;
  }

  m_mainWindow = mainWindow;

  qDebug() << "[WindowCacheManager] Initializing window cache...";
  qint64 t0 = initTimer.elapsed();

  createCachedWindows();
  qint64 t1 = initTimer.elapsed();

  m_initialized = true;
  qDebug() << "[WindowCacheManager] ✓ Window cache initialized successfully";

  // Pre-load saved position to avoid disk I/O on hotkey press
  QSettings settings("TradingCompany", "TradingTerminal");
  if (settings.contains("orderwindow/last_x") &&
      settings.contains("orderwindow/last_y")) {
    int x = settings.value("orderwindow/last_x").toInt();
    int y = settings.value("orderwindow/last_y").toInt();
    m_lastOrderWindowPos = QPoint(x, y);
    m_hasSavedPosition = true;
    qDebug() << "[WindowCacheManager] Pre-loaded window position:"
             << m_lastOrderWindowPos;
  } else {
    m_hasSavedPosition = false;
    qDebug()
        << "[WindowCacheManager] No saved position found (will use default)";
  }
  qint64 t2 = initTimer.elapsed();

  qint64 totalTime = initTimer.elapsed();
  qDebug() << "========================================";
  qDebug() << "[PERF] [WINDOW_CACHE_INIT] COMPLETE";
  qDebug() << "  TOTAL TIME:" << totalTime << "ms";
  qDebug() << "  Breakdown:";
  qDebug() << "    - Pre-setup:" << t0 << "ms";
  qDebug() << "    - Create cached windows (Buy/Sell/3xSnapQuote):" << (t1 - t0)
           << "ms";
  qDebug() << "    - Load window position:" << (t2 - t1) << "ms";
  qDebug() << "========================================";
}

void WindowCacheManager::createCachedWindows() {
  // ⏱️ PERFORMANCE LOG: Track per-window creation time
  QElapsedTimer windowTimer;
  windowTimer.start();

  qDebug() << "[PERF] [CACHE_CREATE] Starting window pre-creation...";

  if (!m_mainWindow || !m_mainWindow->mdiArea()) {
    qWarning() << "[WindowCacheManager] Cannot create cached windows: MDI area "
                  "not ready";
    return;
  }

  auto mdiArea = m_mainWindow->mdiArea();

  // Pre-create Buy Window
  qint64 buyStart = windowTimer.elapsed();
  m_cachedBuyMdiWindow = new CustomMDISubWindow("Buy Order", mdiArea);
  m_cachedBuyMdiWindow->setWindowType("BuyWindow");
  m_cachedBuyMdiWindow->setCached(true); // Mark as cached window
  m_cachedBuyWindow = new BuyWindow(m_cachedBuyMdiWindow);
  m_cachedBuyMdiWindow->setContentWidget(m_cachedBuyWindow);
  m_cachedBuyMdiWindow->resize(1220, 200);
  mdiArea->addWindow(m_cachedBuyMdiWindow);

  // ⚡ CRITICAL: Show off-screen immediately (not hide!) for instant first show
  m_cachedBuyMdiWindow->show();
  m_cachedBuyMdiWindow->move(-10000, -10000);
  m_cachedBuyMdiWindow->lower();

  // Connect signals to MainWindow (Crucial for cached windows!)
  if (m_mainWindow && m_cachedBuyWindow) {
    QObject::connect(m_cachedBuyWindow, &BaseOrderWindow::orderSubmitted,
                     m_mainWindow, &MainWindow::placeOrder);
    QObject::connect(m_cachedBuyWindow,
                     &BaseOrderWindow::orderModificationSubmitted, m_mainWindow,
                     &MainWindow::modifyOrder);
  }

  // Windows are pre-initialized, don't need reset on first show
  m_buyWindowNeedsReset = false;

  qint64 buyTime = windowTimer.elapsed() - buyStart;
  qDebug() << "[PERF] [CACHE_CREATE] Buy window created in" << buyTime << "ms";

  // Pre-create Sell Window
  qint64 sellStart = windowTimer.elapsed();
  m_cachedSellMdiWindow = new CustomMDISubWindow("Sell Order", mdiArea);
  m_cachedSellMdiWindow->setWindowType("SellWindow");
  m_cachedSellMdiWindow->setCached(true); // Mark as cached window
  m_cachedSellWindow = new SellWindow(m_cachedSellMdiWindow);
  m_cachedSellMdiWindow->setContentWidget(m_cachedSellWindow);
  m_cachedSellMdiWindow->resize(1220, 200);
  mdiArea->addWindow(m_cachedSellMdiWindow);

  // ⚡ CRITICAL: Show off-screen immediately (not hide!) for instant first show
  m_cachedSellMdiWindow->show();
  m_cachedSellMdiWindow->move(-10000, -10000);
  m_cachedSellMdiWindow->lower();

  // Connect signals to MainWindow (Crucial for cached windows!)
  if (m_mainWindow && m_cachedSellWindow) {
    QObject::connect(m_cachedSellWindow, &BaseOrderWindow::orderSubmitted,
                     m_mainWindow, &MainWindow::placeOrder);
    QObject::connect(m_cachedSellWindow,
                     &BaseOrderWindow::orderModificationSubmitted, m_mainWindow,
                     &MainWindow::modifyOrder);
  }

  // Windows are pre-initialized, don't need reset on first show
  m_sellWindowNeedsReset = false;

  qint64 sellTime = windowTimer.elapsed() - sellStart;
  qDebug() << "[PERF] [CACHE_CREATE] Sell window created in" << sellTime
           << "ms";

  // ⚡ Pre-create 3 SnapQuote Windows for multi-window support
  qint64 snapStart = windowTimer.elapsed();
  for (int i = 0; i < CACHED_SNAPQUOTE_COUNT; i++) {
    qint64 singleSnapStart = windowTimer.elapsed();

    SnapQuoteWindowEntry entry;

    QString title = QString("Snap Quote %1").arg(i + 1);
    entry.mdiWindow = new CustomMDISubWindow(title, mdiArea);
    entry.mdiWindow->setWindowType("SnapQuote");
    entry.mdiWindow->setCached(true); // Mark as cached window
    entry.mdiWindow->setProperty("snapQuoteIndex",
                                 i); // Store index for close tracking

    entry.window = new SnapQuoteWindow(entry.mdiWindow);

    // ⚡ CRITICAL: Set ScripBar to DisplayMode for instant setScripDetails()
    // (<1ms) SnapQuote only needs to display the selected scrip, not search
    entry.window->setScripBarDisplayMode(true);

    entry.mdiWindow->setContentWidget(entry.window);
    entry.mdiWindow->resize(860, 300);
    mdiArea->addWindow(entry.mdiWindow);

    // ⚡ CRITICAL: Show off-screen immediately (not hide!) for instant first
    // show This ensures even the FIRST user-triggered show is instant (< 20ms)!
    entry.mdiWindow->show();
    entry.mdiWindow->move(-10000 - (i * 100), -10000); // Stagger positions
    entry.mdiWindow->lower();

    // ⚡ Connect to UDP broadcast service for real-time updates
    QObject::connect(&UdpBroadcastService::instance(),
                     &UdpBroadcastService::udpTickReceived, entry.window,
                     &SnapQuoteWindow::onTickUpdate);

    // Initialize entry metadata
    entry.needsReset = false; // Windows are pre-initialized
    entry.isVisible = false;
    entry.lastToken = -1;
    entry.lastUsedTime = QDateTime::currentDateTime().addSecs(
        -i); // Stagger times for initial LRU order

    m_snapQuoteWindows.append(entry);

    qint64 singleSnapTime = windowTimer.elapsed() - singleSnapStart;
    qDebug() << "[PERF] [CACHE_CREATE] SnapQuote window" << (i + 1)
             << "created in" << singleSnapTime << "ms";
  }

  qint64 totalSnapTime = windowTimer.elapsed() - snapStart;
  qint64 totalTime = windowTimer.elapsed();

  qDebug() << "[PERF] [CACHE_CREATE] ✓ All" << (int)CACHED_SNAPQUOTE_COUNT
           << "SnapQuote windows created in" << totalSnapTime << "ms";
  qDebug() << "[PERF] [CACHE_CREATE] TOTAL window creation time:" << totalTime
           << "ms";
  qDebug() << "  - Buy:" << buyTime << "ms";
  qDebug() << "  - Sell:" << sellTime << "ms";
  qDebug() << "  - 3x SnapQuote:" << totalSnapTime << "ms";
}

void WindowCacheManager::setXTSClientForSnapQuote(XTSMarketDataClient *client) {
  if (!client)
    return;

  for (int i = 0; i < m_snapQuoteWindows.size(); i++) {
    if (m_snapQuoteWindows[i].window) {
      m_snapQuoteWindows[i].window->setXTSClient(client);
    }
  }

  qDebug() << "[WindowCacheManager] XTS client set for all"
           << m_snapQuoteWindows.size() << "cached SnapQuote windows";
}

void WindowCacheManager::resetBuyWindow() {
  if (m_cachedBuyWindow) {
    m_cachedBuyWindow->resetToNewOrderMode(
        true); // Fast mode: skip UI updates for hidden window
  }
}

void WindowCacheManager::resetSellWindow() {
  if (m_cachedSellWindow) {
    m_cachedSellWindow->resetToNewOrderMode(
        true); // Fast mode: skip UI updates for hidden window
  }
}

void WindowCacheManager::resetSnapQuoteWindow(int index) {
  if (index >= 0 && index < m_snapQuoteWindows.size()) {
    if (m_snapQuoteWindows[index].window) {
      // Clear the previous scrip details
      m_snapQuoteWindows[index].window->setScripDetails("", "", 0, "", "");
      m_snapQuoteWindows[index].lastToken = -1;
      m_snapQuoteWindows[index].needsReset = true;
    }
  }
}

int WindowCacheManager::findLeastRecentlyUsedSnapQuoteWindow() {
  if (m_snapQuoteWindows.isEmpty()) {
    return -1;
  }

  int lruIndex = 0;
  QDateTime oldestTime = m_snapQuoteWindows[0].lastUsedTime;

  for (int i = 1; i < m_snapQuoteWindows.size(); i++) {
    if (m_snapQuoteWindows[i].lastUsedTime < oldestTime) {
      oldestTime = m_snapQuoteWindows[i].lastUsedTime;
      lruIndex = i;
    }
  }

  qDebug() << "[WindowCacheManager] LRU window index:" << lruIndex
           << "lastUsed:" << oldestTime.toString("hh:mm:ss");

  return lruIndex;
}

CustomMDISubWindow *
WindowCacheManager::showBuyWindow(const WindowContext *context, QWidget* initiatingWindow) {
  static int cacheHitCounter = 0;
  cacheHitCounter++;
  qDebug() << "[PERF] [CACHE_SHOW_BUY] #" << cacheHitCounter
           << " START Time:" << QDateTime::currentMSecsSinceEpoch();

  QElapsedTimer timer;
  timer.start();

  if (!m_initialized || !m_cachedBuyMdiWindow || !m_cachedBuyWindow) {
    qDebug() << "[WindowCacheManager] Buy window cache not available";
    return nullptr;
  }
  
  // Register with WindowManager, passing the initiating window for focus restoration
  if (initiatingWindow) {
    WindowManager::instance().registerWindow(m_cachedBuyWindow, "Buy Window", initiatingWindow);
    qDebug() << "[WindowCacheManager] Registered Buy window with initiating window";
  }

  qint64 t1 = timer.elapsed();

  // Ultra-fast path: Move sell window off-screen instead of hiding (much
  // faster!)
  if (m_cachedSellMdiWindow && m_cachedSellMdiWindow->isVisible()) {
    m_cachedSellMdiWindow->move(-10000, -10000); // Move off-screen (instant!)
    m_cachedSellMdiWindow->lower();              // Send to back
  }

  qint64 t2 = timer.elapsed();

  // ALWAYS restore position FIRST (critical for off-screen strategy)
  // OPTIMIZATION: Use in-memory cache instead of QSettings disk read
  if (m_hasSavedPosition) {
    // Sanity check: if position is off-screen, use default instead
    if (m_lastOrderWindowPos.x() < -1000 || m_lastOrderWindowPos.y() < -1000) {
      auto mdiArea = m_mainWindow->mdiArea();
      if (mdiArea) {
        QSize mdiSize = mdiArea->size();
        int x = mdiSize.width() - 1220 - 20;
        int y = mdiSize.height() - 200 - 20;
        m_cachedBuyMdiWindow->move(x, y);
        // Don't update cache here, let user move it
      }
    } else {
      m_cachedBuyMdiWindow->move(m_lastOrderWindowPos);
    }
  } else {
    // Default position: bottom-right
    auto mdiArea = m_mainWindow->mdiArea();
    if (mdiArea) {
      QSize mdiSize = mdiArea->size();
      int x = mdiSize.width() - 1220 - 20;
      int y = mdiSize.height() - 200 - 20;
      // qDebug() << "[WindowCacheManager] Using default position:" << QPoint(x,
      // y);
      m_cachedBuyMdiWindow->move(x, y);
    }
  }

  qint64 t3 = timer.elapsed();

  // Smart context loading: only reload if context changed or window needs reset
  if (context && context->isValid()) {
    int currentToken =
        context->token; // Assuming WindowContext has a token field

    if (m_buyWindowNeedsReset || currentToken != m_lastBuyToken) {
      // Context changed OR window was closed - need to reload
      m_cachedBuyWindow->loadFromContext(*context);
      m_lastBuyToken = currentToken;
      m_buyWindowNeedsReset = false;
      qDebug() << "[WindowCacheManager] Loaded NEW context for token:"
               << currentToken;
    } else {
      // Same context, window still has data - skip expensive reload!
      qDebug() << "[WindowCacheManager] Skipping reload - same token:"
               << currentToken;
    }
  } else if (m_buyWindowNeedsReset) {
    // No context and window needs reset - clear it
    resetBuyWindow();
    m_lastBuyToken = -1;
    m_buyWindowNeedsReset = false;
  }

  qint64 t4 = timer.elapsed();

  // EVENT COALESCING: Cancel pending Sell activation if user switched to Buy
  m_pendingActivation = PendingWindow::Buy;

  // Show immediately (fast, gives instant visual feedback)
  m_cachedBuyMdiWindow->show();
  if (m_cachedBuyWindow)
    m_cachedBuyWindow->show();

  qint64 t5 = timer.elapsed();

  // Defer raise/activate to next event loop iteration (prevents queue buildup)
  QTimer::singleShot(0, this, [this]() {
    // Only activate if Buy is still the pending window (not canceled by F2)
    if (m_pendingActivation == PendingWindow::Buy && m_cachedBuyMdiWindow) {
      m_cachedBuyMdiWindow->raise();
      m_cachedBuyMdiWindow->activateWindow();
      m_pendingActivation = PendingWindow::None;

      // Apply focus after activation
      if (m_cachedBuyWindow) {
        m_cachedBuyWindow->applyDefaultFocus();
      }
    }
  });

  qint64 totalTime = timer.elapsed();

  qDebug() << "[WindowCacheManager] ✓ Buy window shown from cache";
  qDebug() << "[WindowCacheManager] Timing breakdown:";
  qDebug() << "  Init check:" << t1 << "ms";
  qDebug() << "  Hide sell:" << (t2 - t1) << "ms";
  qDebug() << "  Position restore:" << (t3 - t2) << "ms";
  qDebug() << "  Load context/reset:" << (t4 - t3) << "ms";
  qDebug() << "  Show/activate:" << (t5 - t4) << "ms";
  qDebug() << "  Apply focus (async):" << (totalTime - t5) << "ms";
  qDebug() << "  TOTAL:" << totalTime << "ms";

  return m_cachedBuyMdiWindow;
}

CustomMDISubWindow *
WindowCacheManager::showSellWindow(const WindowContext *context, QWidget* initiatingWindow) {
  static int cacheHitCounter = 0;
  cacheHitCounter++;
  qDebug() << "[PERF] [CACHE_SHOW_SELL] #" << cacheHitCounter
           << " START Time:" << QDateTime::currentMSecsSinceEpoch();

  QElapsedTimer timer;
  timer.start();

  if (!m_initialized || !m_cachedSellMdiWindow || !m_cachedSellWindow) {
    qDebug() << "[WindowCacheManager] Sell window cache not available";
    return nullptr;
  }
  
  // Register with WindowManager, passing the initiating window for focus restoration
  if (initiatingWindow) {
    WindowManager::instance().registerWindow(m_cachedSellWindow, "Sell Window", initiatingWindow);
    qDebug() << "[WindowCacheManager] Registered Sell window with initiating window";
  }

  qint64 t1 = timer.elapsed();

  // Ultra-fast path: Move buy window off-screen instead of hiding (much
  // faster!)
  if (m_cachedBuyMdiWindow && m_cachedBuyMdiWindow->isVisible()) {
    m_cachedBuyMdiWindow->move(-10000, -10000); // Move off-screen (instant!)
    m_cachedBuyMdiWindow->lower();              // Send to back
  }

  qint64 t2 = timer.elapsed();

  // ALWAYS restore position FIRST (critical for off-screen strategy)
  // OPTIMIZATION: Use in-memory cache instead of QSettings disk read
  if (m_hasSavedPosition) {
    // Sanity check: if position is off-screen, use default instead
    if (m_lastOrderWindowPos.x() < -1000 || m_lastOrderWindowPos.y() < -1000) {
      auto mdiArea = m_mainWindow->mdiArea();
      if (mdiArea) {
        QSize mdiSize = mdiArea->size();
        int x = mdiSize.width() - 1220 - 20;
        int y = mdiSize.height() - 200 - 20;
        m_cachedSellMdiWindow->move(x, y);
      }
    } else {
      m_cachedSellMdiWindow->move(m_lastOrderWindowPos);
    }
  } else {
    // Default position: bottom-right
    auto mdiArea = m_mainWindow->mdiArea();
    if (mdiArea) {
      QSize mdiSize = mdiArea->size();
      int x = mdiSize.width() - 1220 - 20;
      int y = mdiSize.height() - 200 - 20;
      // qDebug() << "[WindowCacheManager] Using default position:" << QPoint(x,
      // y);
      m_cachedSellMdiWindow->move(x, y);
    }
  }

  qint64 t3 = timer.elapsed();

  // Smart context loading: only reload if context changed or window needs reset
  if (context && context->isValid()) {
    int currentToken =
        context->token; // Assuming WindowContext has a token field

    if (m_sellWindowNeedsReset || currentToken != m_lastSellToken) {
      // Context changed OR window was closed - need to reload
      m_cachedSellWindow->loadFromContext(*context);
      m_lastSellToken = currentToken;
      m_sellWindowNeedsReset = false;
      qDebug() << "[WindowCacheManager] Loaded NEW context for token:"
               << currentToken;
    } else {
      // Same context, window still has data - skip expensive reload!
      qDebug() << "[WindowCacheManager] Skipping reload - same token:"
               << currentToken;
    }
  } else if (m_sellWindowNeedsReset) {
    // No context and window needs reset - clear it
    resetSellWindow();
    m_lastSellToken = -1;
    m_sellWindowNeedsReset = false;
  }

  qint64 t4 = timer.elapsed();

  // EVENT COALESCING: Cancel pending Buy activation if user switched to Sell
  m_pendingActivation = PendingWindow::Sell;

  // Show immediately (fast, gives instant visual feedback)
  m_cachedSellMdiWindow->show();
  if (m_cachedSellWindow)
    m_cachedSellWindow->show();

  qint64 t5 = timer.elapsed();

  // Defer raise/activate to next event loop iteration (prevents queue buildup)
  QTimer::singleShot(0, this, [this]() {
    // Only activate if Sell is still the pending window (not canceled by F1)
    if (m_pendingActivation == PendingWindow::Sell && m_cachedSellMdiWindow) {
      m_cachedSellMdiWindow->raise();
      m_cachedSellMdiWindow->activateWindow();
      m_pendingActivation = PendingWindow::None;

      // Apply focus after activation
      if (m_cachedSellWindow) {
        m_cachedSellWindow->applyDefaultFocus();
      }
    }
  });

  qint64 totalTime = timer.elapsed();

  qDebug() << "[WindowCacheManager] ✓ Sell window shown from cache";
  qDebug() << "[WindowCacheManager] Timing breakdown:";
  qDebug() << "  Init check:" << t1 << "ms";
  qDebug() << "  Hide buy:" << (t2 - t1) << "ms";
  qDebug() << "  Position restore:" << (t3 - t2) << "ms";
  qDebug() << "  Load context/reset:" << (t4 - t3) << "ms";
  qDebug() << "  Show/activate:" << (t5 - t4) << "ms";
  qDebug() << "  Apply focus (async):" << (totalTime - t5) << "ms";
  qDebug() << "  TOTAL:" << totalTime << "ms";

  return m_cachedSellMdiWindow;
}

CustomMDISubWindow *
WindowCacheManager::showSnapQuoteWindow(const WindowContext *context, QWidget* initiatingWindow) {
  static int cacheHitCounter = 0;
  cacheHitCounter++;
  qDebug() << "[PERF] [CACHE_SHOW_SNAPQUOTE] #" << cacheHitCounter
           << " START Time:" << QDateTime::currentMSecsSinceEpoch();

  QElapsedTimer timer;
  timer.start();

  if (!m_initialized || m_snapQuoteWindows.isEmpty()) {
    qDebug() << "[WindowCacheManager] SnapQuote window cache not available";
    return nullptr;
  }

  if (!context || !context->isValid()) {
    qDebug() << "[WindowCacheManager] Invalid context provided - returning "
                "first available or LRU window";
    // During workspace restore, context might be null. We still need a window.
    // Try to find a window to return even without a context.
  }

  int requestedToken = context ? context->token : -1;
  int selectedIndex = -1;

  // ⚡ DEBUG: State of snap quote pool
  qDebug() << "[WindowCacheManager] SnapQuote Pool Size:"
           << m_snapQuoteWindows.size();
  for (int i = 0; i < m_snapQuoteWindows.size(); i++) {
    qDebug() << "  Window" << i
             << ": isVisible=" << m_snapQuoteWindows[i].isVisible
             << "lastToken=" << m_snapQuoteWindows[i].lastToken;
  }

  // ⚡ STEP 1: Check if token is already displayed in any window (reuse same
  // window)
  if (requestedToken != -1) {
    for (int i = 0; i < m_snapQuoteWindows.size(); i++) {
      if (m_snapQuoteWindows[i].lastToken == requestedToken &&
          m_snapQuoteWindows[i].isVisible) {
        selectedIndex = i;
        qDebug() << "[WindowCacheManager] Token" << requestedToken
                 << "already in window" << (i + 1) << "- reusing";
        break;
      }
    }
  }

  // ⚡ STEP 2: If not found, look for first unused window (not yet visible)
  if (selectedIndex == -1) {
    for (int i = 0; i < m_snapQuoteWindows.size(); i++) {
      if (!m_snapQuoteWindows[i].isVisible) {
        selectedIndex = i;
        qDebug() << "[WindowCacheManager] Selected index" << i
                 << "(Unused window)";
        break;
      }
    }
  }

  // ⚡ STEP 3: All windows in use - find LRU
  if (selectedIndex == -1) {
    selectedIndex = findLeastRecentlyUsedSnapQuoteWindow();
    qDebug() << "[WindowCacheManager] Selected index" << selectedIndex
             << "(LRU reuse)";
  }

  if (selectedIndex == -1) {
    qWarning() << "[WindowCacheManager] Failed to select SnapQuote window!";
    return nullptr;
  }

  auto &entry = m_snapQuoteWindows[selectedIndex];

  qint64 t1 = timer.elapsed();

  // Position window (cascade windows with offset)
  auto mdiArea = m_mainWindow->mdiArea();
  if (mdiArea) {
    QSize mdiSize = mdiArea->size();
    int x =
        (mdiSize.width() - 860) / 2 + (selectedIndex * 30); // Cascade offset
    int y = (mdiSize.height() - 300) / 2 + (selectedIndex * 30);
    entry.mdiWindow->move(x, y);
  }

  qint64 t2 = timer.elapsed();

  // Smart context loading: only reload if context changed or window needs reset
  if (context && (entry.needsReset || requestedToken != entry.lastToken)) {
    // Context changed OR window was closed - need to reload
    // ⚡ Pass false to skip XTS API call - use GStore data instead (MUCH
    // faster!)
    entry.window->loadFromContext(*context, false);
    entry.lastToken = requestedToken;
    entry.needsReset = false;
    qDebug() << "[WindowCacheManager] Loaded NEW context for SnapQuote token:"
             << requestedToken << "(no API call)";
  } else {
    // Same context, window still has data - skip expensive reload!
    qDebug() << "[WindowCacheManager] Skipping reload - same SnapQuote token:"
             << requestedToken;
  }

  qint64 t3 = timer.elapsed();

  // Update LRU timestamp BEFORE showing (critical for next LRU calculation)
  entry.lastUsedTime = QDateTime::currentDateTime();
  entry.isVisible = true;
  
  // Register with WindowManager, passing the initiating window for focus restoration
  if (initiatingWindow && entry.window) {
    QString windowName = QString("Window_%1").arg(reinterpret_cast<quintptr>(entry.window));
    WindowManager::instance().registerWindow(entry.window, windowName, initiatingWindow);
    qDebug() << "[WindowCacheManager] Registered SnapQuote window" << (selectedIndex + 1) << "with initiating window";
  }

  qint64 t4 = timer.elapsed();

  // ⚡ ULTRA-FAST PATH: Keep window visible, just reposition (no show/hide
  // overhead!)
  bool wasHidden = !entry.mdiWindow->isVisible();

  if (wasHidden) {
    // First time showing after hide() - need to call show() once
    entry.mdiWindow->show();
    if (entry.window)
      entry.window->show();
    qDebug() << "[WindowCacheManager] ⚡ Window" << (selectedIndex + 1)
             << "was hidden, calling show()";
  } else {
    // Already visible (just repositioned)
    qDebug() << "[WindowCacheManager] ⚡ Window" << (selectedIndex + 1)
             << "already visible, instant reposition!";
  }

  qint64 t5 = timer.elapsed();

  // ⚡ Defer raise/activate to next event loop (ensures proper focus)
  QTimer::singleShot(0, this, [this, selectedIndex]() {
    if (selectedIndex >= 0 && selectedIndex < m_snapQuoteWindows.size()) {
      auto &e = m_snapQuoteWindows[selectedIndex];
      if (e.mdiWindow) {
        e.mdiWindow->raise();
        e.mdiWindow->activateWindow();

        // Set focus to content widget for keyboard input
        if (e.window) {
          e.window->setFocus();
          e.window->activateWindow();
        }
      }
    }
  });

  qint64 totalTime = timer.elapsed();

  qDebug() << "[WindowCacheManager] ⚡ SnapQuote window" << (selectedIndex + 1)
           << "shown from cache";
  qDebug() << "[WindowCacheManager] Timing breakdown:";
  qDebug() << "  Window selection:" << t1 << "ms";
  qDebug() << "  Position restore:" << (t2 - t1) << "ms";
  qDebug() << "  Load context/reset (⚡ ScripBar deferred):" << (t3 - t2)
           << "ms";
  qDebug() << "  Update LRU timestamp:" << (t4 - t3) << "ms";
  qDebug() << "  Show/raise/activate:" << (t5 - t4) << "ms"
           << (wasHidden ? " (show() called)" : " (instant!)");
  qDebug() << "  TOTAL:" << totalTime << "ms (⚡ Target: 10-20ms)";

  return entry.mdiWindow;
}

void WindowCacheManager::markSnapQuoteWindowClosed(int windowIndex) {
  if (windowIndex >= 0 && windowIndex < m_snapQuoteWindows.size()) {
    auto &entry = m_snapQuoteWindows[windowIndex];
    entry.isVisible = false;
    entry.needsReset = true;
    entry.lastToken = -1;

    qDebug() << "[WindowCacheManager] SnapQuote window" << (windowIndex + 1)
             << "marked as closed (available for reuse)";
    
    // Note: We don't unregister from WindowManager here because cached windows
    // are not actually destroyed - just moved off-screen. The window remains
    // in the stack for proper focus restoration when user closes it.
    // Unregister only happens in the destructor when window is truly destroyed.
  }
}

  void WindowCacheManager::markBuyWindowClosed() {
  m_buyWindowNeedsReset = true;
  m_lastBuyToken = -1; // Clear cached token
  
  qDebug() << "[WindowCacheManager] Buy window marked as closed (available for reuse)";
  
  // Note: We don't unregister from WindowManager here because cached windows
  // are not actually destroyed - just moved off-screen. The window remains
  // in the stack for proper focus restoration when user closes it.
  // Unregister only happens in the destructor when window is truly destroyed.
}

void WindowCacheManager::markSellWindowClosed() {
  m_sellWindowNeedsReset = true;
  m_lastSellToken = -1; // Clear cached token
  
  qDebug() << "[WindowCacheManager] Sell window marked as closed (available for reuse)";
  
  // Note: We don't unregister from WindowManager here because cached windows
  // are not actually destroyed - just moved off-screen. The window remains
  // in the stack for proper focus restoration when user closes it.
  // Unregister only happens in the destructor when window is truly destroyed.
}

void WindowCacheManager::saveOrderWindowPosition(const QPoint &pos) {
  qDebug() << "[WindowCacheManager] Caching new position:" << pos;
  // Update memory cache immediately
  m_lastOrderWindowPos = pos;
  m_hasSavedPosition = true;

  // Save to disk (can be lazy, but QSettings is safely reentrant)
  QSettings s("TradingCompany", "TradingTerminal");
  s.setValue("orderwindow/last_x", pos.x());
  s.setValue("orderwindow/last_y", pos.y());
}
