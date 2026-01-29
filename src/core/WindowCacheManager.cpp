#include "core/WindowCacheManager.h"
#include "app/MainWindow.h"
#include "core/widgets/CustomMDIArea.h"
#include "core/widgets/CustomMDISubWindow.h"
#include "views/BuyWindow.h"
#include "views/SellWindow.h"
#include "models/WindowContext.h"
#include <QDebug>
#include <QSettings>
#include <QElapsedTimer>
#include <QTimer>
#include <QDateTime> // Added for performance logging

WindowCacheManager& WindowCacheManager::instance()
{
    static WindowCacheManager instance;
    return instance;
}

WindowCacheManager::WindowCacheManager()
{
}

WindowCacheManager::~WindowCacheManager()
{
}

void WindowCacheManager::initialize(MainWindow* mainWindow)
{
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
    createCachedWindows();
    
    m_initialized = true;
    qDebug() << "[WindowCacheManager] ✓ Window cache initialized successfully";

    // Pre-load saved position to avoid disk I/O on hotkey press
    QSettings settings("TradingCompany", "TradingTerminal");
    if (settings.contains("orderwindow/last_x") && settings.contains("orderwindow/last_y")) {
        int x = settings.value("orderwindow/last_x").toInt();
        int y = settings.value("orderwindow/last_y").toInt();
        m_lastOrderWindowPos = QPoint(x, y);
        m_hasSavedPosition = true;
        qDebug() << "[WindowCacheManager] Pre-loaded window position:" << m_lastOrderWindowPos;
    } else {
        m_hasSavedPosition = false;
        qDebug() << "[WindowCacheManager] No saved position found (will use default)";
    }
}

void WindowCacheManager::createCachedWindows()
{
    if (!m_mainWindow || !m_mainWindow->mdiArea()) {
        qWarning() << "[WindowCacheManager] Cannot create cached windows: MDI area not ready";
        return;
    }
    
    auto mdiArea = m_mainWindow->mdiArea();
    
    // Pre-create Buy Window
    m_cachedBuyMdiWindow = new CustomMDISubWindow("Buy Order", mdiArea);
    m_cachedBuyMdiWindow->setWindowType("BuyWindow");
    m_cachedBuyMdiWindow->setCached(true);  // Mark as cached window
    m_cachedBuyWindow = new BuyWindow(m_cachedBuyMdiWindow);
    m_cachedBuyMdiWindow->setContentWidget(m_cachedBuyWindow);
    m_cachedBuyMdiWindow->resize(1220, 200);
    mdiArea->addWindow(m_cachedBuyMdiWindow);
    m_cachedBuyMdiWindow->hide();
    
    // Connect signals to MainWindow (Crucial for cached windows!)
    if (m_mainWindow && m_cachedBuyWindow) {
        QObject::connect(m_cachedBuyWindow, &BaseOrderWindow::orderSubmitted, 
                        m_mainWindow, &MainWindow::placeOrder);
        QObject::connect(m_cachedBuyWindow, &BaseOrderWindow::orderModificationSubmitted, 
                        m_mainWindow, &MainWindow::modifyOrder);
    }
    
    // Windows are pre-initialized, don't need reset on first show
    m_buyWindowNeedsReset = false;
    
    qDebug() << "[WindowCacheManager] Buy window pre-created";
    
    // Pre-create Sell Window
    m_cachedSellMdiWindow = new CustomMDISubWindow("Sell Order", mdiArea);
    m_cachedSellMdiWindow->setWindowType("SellWindow");
    m_cachedSellMdiWindow->setCached(true);  // Mark as cached window
    m_cachedSellWindow = new SellWindow(m_cachedSellMdiWindow);
    m_cachedSellMdiWindow->setContentWidget(m_cachedSellWindow);
    m_cachedSellMdiWindow->resize(1220, 200);
    mdiArea->addWindow(m_cachedSellMdiWindow);
    m_cachedSellMdiWindow->hide();
    
    // Connect signals to MainWindow (Crucial for cached windows!)
    if (m_mainWindow && m_cachedSellWindow) {
        QObject::connect(m_cachedSellWindow, &BaseOrderWindow::orderSubmitted, 
                        m_mainWindow, &MainWindow::placeOrder);
        QObject::connect(m_cachedSellWindow, &BaseOrderWindow::orderModificationSubmitted, 
                        m_mainWindow, &MainWindow::modifyOrder);
    }
    
    // Windows are pre-initialized, don't need reset on first show
    m_sellWindowNeedsReset = false;
    
    qDebug() << "[WindowCacheManager] Sell window pre-created";
}

void WindowCacheManager::resetBuyWindow()
{
    if (m_cachedBuyWindow) {
        m_cachedBuyWindow->resetToNewOrderMode(true);  // Fast mode: skip UI updates for hidden window
    }
}

void WindowCacheManager::resetSellWindow()
{
    if (m_cachedSellWindow) {
        m_cachedSellWindow->resetToNewOrderMode(true);  // Fast mode: skip UI updates for hidden window
    }
}

bool WindowCacheManager::showBuyWindow(const WindowContext* context)
{
    static int cacheHitCounter = 0;
    cacheHitCounter++;
    qDebug() << "[PERF] [CACHE_SHOW_BUY] #" << cacheHitCounter << " START Time:" << QDateTime::currentMSecsSinceEpoch();

    QElapsedTimer timer;
    timer.start();
    
    if (!m_initialized || !m_cachedBuyMdiWindow || !m_cachedBuyWindow) {
        qDebug() << "[WindowCacheManager] Buy window cache not available";
        return false;
    }
    
    qint64 t1 = timer.elapsed();
    
    // Ultra-fast path: Move sell window off-screen instead of hiding (much faster!)
    if (m_cachedSellMdiWindow && m_cachedSellMdiWindow->isVisible()) {
        m_cachedSellMdiWindow->move(-10000, -10000);  // Move off-screen (instant!)
        m_cachedSellMdiWindow->lower();  // Send to back
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
            // qDebug() << "[WindowCacheManager] Using default position:" << QPoint(x, y);
            m_cachedBuyMdiWindow->move(x, y);
        }
    }
    
    qint64 t3 = timer.elapsed();
    
    // Smart context loading: only reload if context changed or window needs reset
    if (context && context->isValid()) {
        int currentToken = context->token;  // Assuming WindowContext has a token field
        
        if (m_buyWindowNeedsReset || currentToken != m_lastBuyToken) {
            // Context changed OR window was closed - need to reload
            m_cachedBuyWindow->loadFromContext(*context);
            m_lastBuyToken = currentToken;
            m_buyWindowNeedsReset = false;
            qDebug() << "[WindowCacheManager] Loaded NEW context for token:" << currentToken;
        } else {
            // Same context, window still has data - skip expensive reload!
            qDebug() << "[WindowCacheManager] Skipping reload - same token:" << currentToken;
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
    if (m_cachedBuyWindow) m_cachedBuyWindow->show();
    
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
    qDebug() << "  Hide sell:" << (t2-t1) << "ms";
    qDebug() << "  Position restore:" << (t3-t2) << "ms";
    qDebug() << "  Load context/reset:" << (t4-t3) << "ms";
    qDebug() << "  Show/activate:" << (t5-t4) << "ms";
    qDebug() << "  Apply focus (async):" << (totalTime-t5) << "ms";
    qDebug() << "  TOTAL:" << totalTime << "ms";
    
    return true;
}

bool WindowCacheManager::showSellWindow(const WindowContext* context)
{
    static int cacheHitCounter = 0;
    cacheHitCounter++;
    qDebug() << "[PERF] [CACHE_SHOW_SELL] #" << cacheHitCounter << " START Time:" << QDateTime::currentMSecsSinceEpoch();

    QElapsedTimer timer;
    timer.start();
    
    if (!m_initialized || !m_cachedSellMdiWindow || !m_cachedSellWindow) {
        qDebug() << "[WindowCacheManager] Sell window cache not available";
        return false;
    }
    
    qint64 t1 = timer.elapsed();
    
    // Ultra-fast path: Move buy window off-screen instead of hiding (much faster!)
    if (m_cachedBuyMdiWindow && m_cachedBuyMdiWindow->isVisible()) {
        m_cachedBuyMdiWindow->move(-10000, -10000);  // Move off-screen (instant!)
        m_cachedBuyMdiWindow->lower();  // Send to back
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
            // qDebug() << "[WindowCacheManager] Using default position:" << QPoint(x, y);
            m_cachedSellMdiWindow->move(x, y);
        }
    }
    
    qint64 t3 = timer.elapsed();
    
    // Smart context loading: only reload if context changed or window needs reset
    if (context && context->isValid()) {
        int currentToken = context->token;  // Assuming WindowContext has a token field
        
        if (m_sellWindowNeedsReset || currentToken != m_lastSellToken) {
            // Context changed OR window was closed - need to reload
            m_cachedSellWindow->loadFromContext(*context);
            m_lastSellToken = currentToken;
            m_sellWindowNeedsReset = false;
            qDebug() << "[WindowCacheManager] Loaded NEW context for token:" << currentToken;
        } else {
            // Same context, window still has data - skip expensive reload!
            qDebug() << "[WindowCacheManager] Skipping reload - same token:" << currentToken;
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
    if (m_cachedSellWindow) m_cachedSellWindow->show();
    
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
    qDebug() << "  Hide buy:" << (t2-t1) << "ms";
    qDebug() << "  Position restore:" << (t3-t2) << "ms";
    qDebug() << "  Load context/reset:" << (t4-t3) << "ms";
    qDebug() << "  Show/activate:" << (t5-t4) << "ms";
    qDebug() << "  Apply focus (async):" << (totalTime-t5) << "ms";
    qDebug() << "  TOTAL:" << totalTime << "ms";
    
    return true;
}

void WindowCacheManager::saveOrderWindowPosition(const QPoint& pos)
{
    qDebug() << "[WindowCacheManager] Caching new position:" << pos;
    // Update memory cache immediately
    m_lastOrderWindowPos = pos;
    m_hasSavedPosition = true;
    
    // Save to disk (can be lazy, but QSettings is safely reentrant)
    QSettings s("TradingCompany", "TradingTerminal");
    s.setValue("orderwindow/last_x", pos.x());
    s.setValue("orderwindow/last_y", pos.y());
}
