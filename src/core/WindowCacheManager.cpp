#include "core/WindowCacheManager.h"
#include "api/xts/XTSMarketDataClient.h"
#include "app/MainWindow.h"
#include "core/WindowConstants.h"
#include "core/widgets/CustomMDIArea.h"
#include "core/widgets/CustomMDISubWindow.h"
#include "models/domain/WindowContext.h"
#include "services/UdpBroadcastService.h" // ⚡ For real-time UDP updates
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
  if (m_initialized)
    return;
  if (!mainWindow)
    return;

  m_mainWindow = mainWindow;
  qDebug() << "[WindowCacheManager] Starting staggered window cache "
              "initialization...";

  // Phase 1: Create Buy Window immediately
  createBuyWindow();

  // Phase 2: Stagger remaining windows to keep UI responsive
  QTimer::singleShot(50, this, [this]() {
    createSellWindow();

    QTimer::singleShot(50, this, [this]() {
      createSnapQuoteWindows();
      m_initialized = true;
      qDebug() << "[WindowCacheManager] ✓ Window cache background "
                  "initialization complete";
    });
  });

  // Pre-load saved position
  QSettings settings("TradingCompany", "TradingTerminal");
  if (settings.contains("orderwindow/last_x") &&
      settings.contains("orderwindow/last_y")) {
    m_lastOrderWindowPos = QPoint(settings.value("orderwindow/last_x").toInt(),
                                  settings.value("orderwindow/last_y").toInt());
    m_hasSavedPosition = true;
  }
}

void WindowCacheManager::createBuyWindow() {
  auto mdiArea = m_mainWindow->mdiArea();
  if (!mdiArea)
    return;

  m_cachedBuyMdiWindow = new CustomMDISubWindow("Buy Order", mdiArea);
  m_cachedBuyMdiWindow->setWindowType("BuyWindow");
  m_cachedBuyMdiWindow->setCached(true);
  m_cachedBuyWindow = new BuyWindow(m_cachedBuyMdiWindow);
  m_cachedBuyMdiWindow->setContentWidget(m_cachedBuyWindow);

  // Only apply default size if user hasn't saved a geometry preference
  QSettings buySettings("TradingCompany", "TradingTerminal");
  QString buyOpenWith = buySettings.value("Customize/BuyWindow/openWith", "default").toString();
  if (buyOpenWith == "default" || !buySettings.contains("WindowState/BuyWindow/geometry")) {
    m_cachedBuyMdiWindow->resize(925, 155);
  }

  // ⚡ Add window without activating or showing (it's cached)
  mdiArea->addWindow(m_cachedBuyMdiWindow, false);

  // Move off-screen
  m_cachedBuyMdiWindow->show();
  m_cachedBuyMdiWindow->move(WindowConstants::OFF_SCREEN_X,
                             WindowConstants::OFF_SCREEN_Y);
  m_cachedBuyMdiWindow->lower();

  if (m_cachedBuyWindow) {
    QObject::connect(m_cachedBuyWindow, &BaseOrderWindow::orderSubmitted,
                     m_mainWindow, &MainWindow::placeOrder);
    QObject::connect(m_cachedBuyWindow,
                     &BaseOrderWindow::orderModificationSubmitted, m_mainWindow,
                     &MainWindow::modifyOrder);
  }
  m_buyWindowNeedsReset = false;
}

void WindowCacheManager::createSellWindow() {
  auto mdiArea = m_mainWindow->mdiArea();
  if (!mdiArea)
    return;

  m_cachedSellMdiWindow = new CustomMDISubWindow("Sell Order", mdiArea);
  m_cachedSellMdiWindow->setWindowType("SellWindow");
  m_cachedSellMdiWindow->setCached(true);
  m_cachedSellWindow = new SellWindow(m_cachedSellMdiWindow);
  m_cachedSellMdiWindow->setContentWidget(m_cachedSellWindow);

  // Only apply default size if user hasn't saved a geometry preference
  QSettings sellSettings("TradingCompany", "TradingTerminal");
  QString sellOpenWith = sellSettings.value("Customize/SellWindow/openWith", "default").toString();
  if (sellOpenWith == "default" || !sellSettings.contains("WindowState/SellWindow/geometry")) {
    m_cachedSellMdiWindow->resize(925, 155);
  }

  mdiArea->addWindow(m_cachedSellMdiWindow, false);

  m_cachedSellMdiWindow->show();
  m_cachedSellMdiWindow->move(WindowConstants::OFF_SCREEN_X,
                              WindowConstants::OFF_SCREEN_Y);
  m_cachedSellMdiWindow->lower();

  if (m_cachedSellWindow) {
    QObject::connect(m_cachedSellWindow, &BaseOrderWindow::orderSubmitted,
                     m_mainWindow, &MainWindow::placeOrder);
    QObject::connect(m_cachedSellWindow,
                     &BaseOrderWindow::orderModificationSubmitted, m_mainWindow,
                     &MainWindow::modifyOrder);
  }
  m_sellWindowNeedsReset = false;
}

void WindowCacheManager::createSnapQuoteWindows() {
  auto mdiArea = m_mainWindow->mdiArea();
  if (!mdiArea)
    return;

  for (int i = 0; i < CACHED_SNAPQUOTE_COUNT; i++) {
    SnapQuoteWindowEntry entry;
    QString title = QString("Snap Quote %1").arg(i + 1);
    entry.mdiWindow = new CustomMDISubWindow(title, mdiArea);
    entry.mdiWindow->setWindowType("SnapQuote");
    entry.mdiWindow->setCached(true);
    entry.mdiWindow->setProperty("snapQuoteIndex", i);

    entry.window = new SnapQuoteWindow(entry.mdiWindow);
    entry.window->setScripBarDisplayMode(true);
    entry.mdiWindow->setContentWidget(entry.window);
    entry.mdiWindow->resize(860, 300);

    mdiArea->addWindow(entry.mdiWindow, false);

    entry.mdiWindow->show();
    entry.mdiWindow->move(WindowConstants::OFF_SCREEN_X - (i * 100),
                          WindowConstants::OFF_SCREEN_Y);
    entry.mdiWindow->lower();

    entry.needsReset = false;
    entry.isVisible = false;
    entry.lastToken = -1;
    entry.lastUsedTime = QDateTime::currentDateTime().addSecs(-i);

    m_snapQuoteWindows.append(entry);
  }
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
WindowCacheManager::showBuyWindow(const WindowContext *context,
                                  QWidget *initiatingWindow) {
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

  // Set initiating window on MDI wrapper for focus restoration
  if (initiatingWindow) {
    m_cachedBuyMdiWindow->setInitiatingWindow(initiatingWindow);
  } else {
    qWarning() << "[WindowCacheManager][FOCUS-DEBUG] showBuyWindow called with NO initiating window!";
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
WindowCacheManager::showSellWindow(const WindowContext *context,
                                   QWidget *initiatingWindow) {
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

  // Set initiating window on MDI wrapper for focus restoration
  if (initiatingWindow) {
    m_cachedSellMdiWindow->setInitiatingWindow(initiatingWindow);
  } else {
    qWarning() << "[WindowCacheManager][FOCUS-DEBUG] showSellWindow called with NO initiating window!";
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
WindowCacheManager::showSnapQuoteWindow(const WindowContext *context,
                                        QWidget *initiatingWindow) {
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

  // Debug pool state only if really needed (removed for performance)

  // ⚡ STEP 1: Check if token is already displayed in any window (reuse same
  // window)
  if (requestedToken != -1) {
    for (int i = 0; i < m_snapQuoteWindows.size(); i++) {
      if (m_snapQuoteWindows[i].lastToken == requestedToken &&
          m_snapQuoteWindows[i].isVisible) {
        selectedIndex = i;
        break;
      }
    }
  }

  // ⚡ STEP 2: If not found, look for first unused window (not yet visible)
  if (selectedIndex == -1) {
    for (int i = 0; i < m_snapQuoteWindows.size(); i++) {
      if (!m_snapQuoteWindows[i].isVisible) {
        selectedIndex = i;
        break;
      }
    }
  }

  // ⚡ STEP 3: All windows in use - find LRU
  if (selectedIndex == -1) {
    selectedIndex = findLeastRecentlyUsedSnapQuoteWindow();
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

  // Set initiating window on MDI wrapper for focus restoration
  if (initiatingWindow && entry.mdiWindow) {
    entry.mdiWindow->setInitiatingWindow(initiatingWindow);
  } else if (!initiatingWindow) {
    qWarning() << "[WindowCacheManager][FOCUS-DEBUG] showSnapQuoteWindow called with NO initiating window!";
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

  qDebug() << "[WindowCacheManager] Buy window marked as closed (available for "
              "reuse)";

  // Note: We don't unregister from WindowManager here because cached windows
  // are not actually destroyed - just moved off-screen. The window remains
  // in the stack for proper focus restoration when user closes it.
  // Unregister only happens in the destructor when window is truly destroyed.
}

void WindowCacheManager::markSellWindowClosed() {
  m_sellWindowNeedsReset = true;
  m_lastSellToken = -1; // Clear cached token

  qDebug() << "[WindowCacheManager] Sell window marked as closed (available "
              "for reuse)";

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
