#include "core/WindowCacheManager.h"
#include "app/MainWindow.h"
#include "core/widgets/CustomMDIArea.h"
#include "core/widgets/CustomMDISubWindow.h"
#include "views/BuyWindow.h"
#include "views/SellWindow.h"
#include "models/WindowContext.h"
#include <QDebug>
#include <QSettings>

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
    
    qDebug() << "[WindowCacheManager] Sell window pre-created";
}

void WindowCacheManager::resetBuyWindow()
{
    if (m_cachedBuyWindow) {
        m_cachedBuyWindow->resetToNewOrderMode();
    }
}

void WindowCacheManager::resetSellWindow()
{
    if (m_cachedSellWindow) {
        m_cachedSellWindow->resetToNewOrderMode();
    }
}

bool WindowCacheManager::showBuyWindow(const WindowContext* context)
{
    if (!m_initialized || !m_cachedBuyMdiWindow || !m_cachedBuyWindow) {
        qDebug() << "[WindowCacheManager] Buy window cache not available";
        return false;
    }
    
    // Hide sell window if visible
    if (m_cachedSellMdiWindow && m_cachedSellMdiWindow->isVisible()) {
        resetSellWindow();
        m_cachedSellMdiWindow->hide();
    }
    
    // Load context or reset
    if (context && context->isValid()) {
        m_cachedBuyWindow->loadFromContext(*context);
    } else {
        resetBuyWindow();
    }
    
    // Restore last saved position
    QSettings settings("TradingCompany", "TradingTerminal");
    if (settings.contains("orderwindow/last_x") && settings.contains("orderwindow/last_y")) {
        int x = settings.value("orderwindow/last_x").toInt();
        int y = settings.value("orderwindow/last_y").toInt();
        m_cachedBuyMdiWindow->move(x, y);
    } else {
        // Default position: bottom-right
        auto mdiArea = m_mainWindow->mdiArea();
        if (mdiArea) {
            QSize mdiSize = mdiArea->size();
            int x = mdiSize.width() - 1220 - 20;
            int y = mdiSize.height() - 200 - 20;
            m_cachedBuyMdiWindow->move(x, y);
        }
    }
    
    // Show and activate
    m_cachedBuyMdiWindow->show();
    m_cachedBuyMdiWindow->activateWindow();
    
    // Apply focus to appropriate field based on user preference
    m_cachedBuyWindow->applyDefaultFocus();
    
    qDebug() << "[WindowCacheManager] ✓ Buy window shown from cache";
    return true;
}

bool WindowCacheManager::showSellWindow(const WindowContext* context)
{
    if (!m_initialized || !m_cachedSellMdiWindow || !m_cachedSellWindow) {
        qDebug() << "[WindowCacheManager] Sell window cache not available";
        return false;
    }
    
    // Hide buy window if visible
    if (m_cachedBuyMdiWindow && m_cachedBuyMdiWindow->isVisible()) {
        resetBuyWindow();
        m_cachedBuyMdiWindow->hide();
    }
    
    // Load context or reset
    if (context && context->isValid()) {
        m_cachedSellWindow->loadFromContext(*context);
    } else {
        resetSellWindow();
    }
    
    // Restore last saved position
    QSettings settings("TradingCompany", "TradingTerminal");
    if (settings.contains("orderwindow/last_x") && settings.contains("orderwindow/last_y")) {
        int x = settings.value("orderwindow/last_x").toInt();
        int y = settings.value("orderwindow/last_y").toInt();
        m_cachedSellMdiWindow->move(x, y);
    } else {
        // Default position: bottom-right
        auto mdiArea = m_mainWindow->mdiArea();
        if (mdiArea) {
            QSize mdiSize = mdiArea->size();
            int x = mdiSize.width() - 1220 - 20;
            int y = mdiSize.height() - 200 - 20;
            m_cachedSellMdiWindow->move(x, y);
        }
    }
    
    // Show and activate
    m_cachedSellMdiWindow->show();
    m_cachedSellMdiWindow->activateWindow();
    
    // Apply focus to appropriate field based on user preference
    m_cachedSellWindow->applyDefaultFocus();
    
    qDebug() << "[WindowCacheManager] ✓ Sell window shown from cache";
    return true;
}
