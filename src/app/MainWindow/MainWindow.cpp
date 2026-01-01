#include "app/MainWindow.h"
#include "nsefo_callback.h"
#include "nsecm_callback.h"
#include "core/widgets/CustomMDIArea.h"
#include "core/widgets/CustomMDISubWindow.h"
#include "app/ScripBar.h"
#include "core/widgets/InfoBar.h"
#include "services/PriceCache.h"
#include "services/FeedHandler.h"
#include "services/TradingDataService.h"
#include "utils/ConfigLoader.h"
#include "utils/LatencyTracker.h"
#include <QDebug>
#include <QTimer>
#include <QSettings>
#include <QStatusBar>
#include <QDockWidget>
#include <QShortcut>
#include "services/UdpBroadcastService.h"
#include "views/MarketWatchWindow.h" // Needed for getActiveMarketWatch cast

MainWindow::MainWindow(QWidget *parent)
    : CustomMainWindow(parent)
    , m_xtsMarketDataClient(nullptr)
    , m_xtsInteractiveClient(nullptr)
    , m_tradingDataService(nullptr)
    , m_configLoader(nullptr)
{
    setTitle("Trading Terminal");
    resize(1600, 900);
    setMinimumSize(800, 600);

    // Setup content FIRST (creates layout and widgets)
    setupContent();
    
    // Setup keyboard shortcuts
    setupShortcuts();

    // Connect centralized UDP broadcast service
    connect(&UdpBroadcastService::instance(), &UdpBroadcastService::tickReceived,
            this, &MainWindow::onTickReceived);
    
    // setupNetwork() will be called once configLoader is set
    // to ensure we have the correct multicast IPs and ports.
    
    // Restore visibility preferences
    QSettings s("TradingCompany", "TradingTerminal");
    if (m_infoDock)
        m_infoDock->setVisible(s.value("mainwindow/info_visible", true).toBool());
    if (m_statusBar)
        m_statusBar->setVisible(s.value("mainwindow/status_visible", true).toBool());
}

MainWindow::~MainWindow()
{
    stopBroadcastReceiver();
}

void MainWindow::setXTSClients(XTSMarketDataClient *mdClient, XTSInteractiveClient *iaClient) {
    m_xtsMarketDataClient = mdClient;
    m_xtsInteractiveClient = iaClient;
    
    if (m_xtsMarketDataClient) {
        connect(m_xtsMarketDataClient, &XTSMarketDataClient::tickReceived, 
                this, &MainWindow::onTickReceived);
    }
    
    if (m_scripBar && m_xtsMarketDataClient) {
        m_scripBar->setXTSClient(m_xtsMarketDataClient);
    }
}

void MainWindow::setTradingDataService(TradingDataService *service) {
    m_tradingDataService = service;
}

void MainWindow::setConfigLoader(ConfigLoader *loader) {
    m_configLoader = loader;
    
    // Once config is loaded, we can start the UDP broadcast receivers
    // if auto-start is enabled.
    setupNetwork();
}

void MainWindow::refreshScripBar() {
    if (m_scripBar) m_scripBar->refreshSymbols();
}

void MainWindow::closeEvent(QCloseEvent *event) {
    QSettings s("TradingCompany", "TradingTerminal");
    s.setValue("mainwindow/state", saveState());
    
    if (m_infoDock)
        s.setValue("mainwindow/info_visible", m_infoDock->isVisible());
    if (m_statusBar)
        s.setValue("mainwindow/status_visible", m_statusBar->isVisible());
        
    CustomMainWindow::closeEvent(event);
}

MarketWatchWindow* MainWindow::getActiveMarketWatch() const {
    // Get the currently active MDI sub-window
    CustomMDISubWindow *activeSubWindow = m_mdiArea ? m_mdiArea->activeWindow() : nullptr;
    if (!activeSubWindow) {
        return nullptr;
    }
    
    // Check if the active sub-window is a MarketWatch
    if (activeSubWindow->windowType() == "MarketWatch") {
        QWidget *widget = activeSubWindow->contentWidget();
        return qobject_cast<MarketWatchWindow*>(widget);
    }
    
    // If not, search through all MDI windows for the first MarketWatch
    QList<CustomMDISubWindow*> windows = m_mdiArea->windowList();
    for (CustomMDISubWindow *win : windows) {
        if (win->windowType() == "MarketWatch") {
            QWidget *widget = win->contentWidget();
            return qobject_cast<MarketWatchWindow*>(widget);
        }
    }
    
    return nullptr;
}

void MainWindow::onTickReceived(const XTS::Tick &tick)
{
    // Debug logging for BSE tokens
    if (tick.exchangeSegment == 12 || tick.exchangeSegment == 11) {
        static int mainWindowBseCount = 0;
        if (mainWindowBseCount++ < 10) {
            qDebug() << "[MainWindow] BSE Tick received - Segment:" << tick.exchangeSegment 
                     << "Token:" << tick.exchangeInstrumentID << "LTP:" << tick.lastTradedPrice;
        }
    }
    
    // Direct callback architecture
    FeedHandler::instance().onTickReceived(tick);
}


// UDP Broadcast Logic
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
    if (config.bseFoPort == 0) config.bseFoPort = 26002;
    if (config.bseCmIp.empty()) config.bseCmIp = "239.1.2.4"; // Standard BSE CM Multicast
    if (config.bseCmPort == 0) config.bseCmPort = 26001;

    // TODO: Add NSE CM methods to ConfigLoader if they don't exist
    // For now we start with what we have
    
    UdpBroadcastService::instance().start(config);

    if (m_statusBar) {
        m_statusBar->showMessage("Market Data Receivers: INITIALIZING...", 3000);
    }
}

void MainWindow::stopBroadcastReceiver() {
    UdpBroadcastService::instance().stop();
    if (m_statusBar) m_statusBar->showMessage("Market Data Receivers: STOPPED");
}

void MainWindow::onUdpTickReceived(const XTS::Tick& tick) {
    // This is now redundant as tickReceived is connected to onTickReceived
    // but we keep it for now if any specialized logic is needed.
    // Actually, let's just forward it.
    onTickReceived(tick);
}

void MainWindow::showPreferences() {
    // Placeholder for preferences dialog
    qDebug() << "[MainWindow] showPreferences requested";
}

// setupShortcuts() is defined in core/GlobalShortcuts.cpp
