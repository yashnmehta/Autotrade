#include "app/MainWindow.h"
#include "market_data_callback.h"
#include "core/widgets/CustomMDIArea.h"
#include "core/widgets/CustomMDISubWindow.h"
#include "app/ScripBar.h"
#include "core/widgets/InfoBar.h"
#include "services/PriceCache.h"
#include "services/FeedHandler.h"
#include "services/TradingDataService.h"
#include "utils/LatencyTracker.h"
#include <QDebug>
#include <QTimer>
#include <QSettings>
#include <QStatusBar>
#include <QDockWidget>
#include <QShortcut>
#include <thread>
#include "multicast_receiver.h"
#include "views/MarketWatchWindow.h" // Needed for getActiveMarketWatch cast

MainWindow::MainWindow(QWidget *parent)
    : CustomMainWindow(parent)
    , m_xtsMarketDataClient(nullptr)
    , m_xtsInteractiveClient(nullptr)
    , m_tradingDataService(nullptr)
    , m_tickDrainTimer(nullptr)
    , m_udpTickQueue(65536) // Power of 2 buffer for high-throughput
{
    setTitle("Trading Terminal");
    resize(1600, 900);
    setMinimumSize(800, 600);

    // Setup content FIRST (creates layout and widgets)
    setupContent();
    
    // Setup keyboard shortcuts
    setupShortcuts();
    
    // Setup UDP tick drain timer (1ms = 1000 Hz)
    m_tickDrainTimer = new QTimer(this);
    m_tickDrainTimer->setInterval(1);  // 1ms for real-time performance
    m_tickDrainTimer->setTimerType(Qt::PreciseTimer);
    connect(m_tickDrainTimer, &QTimer::timeout, this, &MainWindow::drainTickQueue);
    m_tickDrainTimer->start();
    
    qDebug() << "[MainWindow] ✅ UDP tick drain timer started (1ms interval)";

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
    // Direct callback architecture
    PriceCache::instance().updatePrice((int)tick.exchangeInstrumentID, tick);
    FeedHandler::instance().onTickReceived(tick);
}

// UDP Broadcast Logic
void MainWindow::startBroadcastReceiver() {
    qDebug() << "[UDP] Starting NSE broadcast receiver...";
    
    // UDP Config
    std::string multicastIP = "233.1.2.5";
    int port = 34331;
    
    m_udpReceiver = std::make_unique<nsefo::MulticastReceiver>(multicastIP, port);
    
    if (!m_udpReceiver->isValid()) {
        qWarning() << "[UDP] ❌ Failed to initialize receiver!";
        return;
    }
    
    // Register Callbacks
    MarketDataCallbackRegistry::instance().registerTouchlineCallback(
        [this](const TouchlineData& data) {
            m_msg7200Count++;
            XTS::Tick tick;
            tick.exchangeSegment = 2; // NSEFO
            tick.exchangeInstrumentID = data.token;
            tick.lastTradedPrice = data.ltp;
            tick.lastTradedQuantity = data.lastTradeQty;
            tick.volume = data.volume;
            tick.open = data.open;
            tick.high = data.high;
            tick.low = data.low;
            tick.close = data.close;
            tick.lastUpdateTime = data.lastTradeTime;
            
            // Latency tracking
            tick.refNo = data.refNo;
            tick.timestampUdpRecv = data.timestampRecv;
            tick.timestampParsed = data.timestampParsed;
            tick.timestampQueued = LatencyTracker::now();
            tick.timestampDequeued = LatencyTracker::now();
            
            QMetaObject::invokeMethod(this, [this, tick]() {
                 FeedHandler::instance().onTickReceived(tick);
            }, Qt::AutoConnection);
        }
    );
    
    MarketDataCallbackRegistry::instance().registerMarketDepthCallback(
        [this](const MarketDepthData& data) {
            m_depthCount++;            
            XTS::Tick tick;
            tick.exchangeSegment = 2;
            tick.exchangeInstrumentID = data.token;
            
            if (data.bids[0].quantity > 0) {
                tick.bidPrice = data.bids[0].price;
                tick.bidQuantity = data.bids[0].quantity;
            }
            if (data.asks[0].quantity > 0) {
                tick.askPrice = data.asks[0].price;
                tick.askQuantity = data.asks[0].quantity;
            }
            tick.totalBuyQuantity = (int)data.totalBuyQty;
            tick.totalSellQuantity = (int)data.totalSellQty;
            
            tick.refNo = data.refNo;
            tick.timestampUdpRecv = data.timestampRecv;
            tick.timestampParsed = data.timestampParsed;
            tick.timestampQueued = LatencyTracker::now();
            tick.timestampDequeued = LatencyTracker::now();
            
            QMetaObject::invokeMethod(this, [this, tick]() {
                FeedHandler::instance().onTickReceived(tick);
            }, Qt::AutoConnection);
        }
    );
    
    MarketDataCallbackRegistry::instance().registerTickerCallback(
         [this](const TickerData& data) {
            m_msg7202Count++;
            if (data.fillVolume > 0) {
                XTS::Tick tick;
                tick.exchangeSegment = 2;
                tick.exchangeInstrumentID = data.token;
                tick.volume = data.fillVolume;
                
                tick.refNo = data.refNo;
                tick.timestampUdpRecv = data.timestampRecv;
                tick.timestampParsed = data.timestampParsed;
                tick.timestampQueued = LatencyTracker::now();
                tick.timestampDequeued = LatencyTracker::now();
                
                QMetaObject::invokeMethod(this, [this, tick]() {
                    FeedHandler::instance().onTickReceived(tick);
                }, Qt::AutoConnection);
            }
         }
    );
    
    // Start background thread
    std::thread udpThread([this]() {
        m_udpReceiver->start();
    });
    udpThread.detach();
    
    if (m_statusBar) m_statusBar->showMessage("NSE Broadcast Receiver: ACTIVE");
}

void MainWindow::stopBroadcastReceiver() {
    if (m_udpReceiver) {
        m_udpReceiver->stop();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        m_udpReceiver.reset();
        if (m_statusBar) m_statusBar->showMessage("NSE Broadcast Receiver: STOPPED");
    }
}

void MainWindow::drainTickQueue() {
    int count = 0;
    const int MAX_BATCH = 1000;
    while (count < MAX_BATCH) {
        auto tick = m_udpTickQueue.dequeue();
        if (!tick.has_value()) break;
        
        tick->timestampDequeued = LatencyTracker::now();
        FeedHandler::instance().onTickReceived(*tick);
        count++;
    }
}

void MainWindow::onUdpTickReceived(const XTS::Tick& tick) {
    // Deprecated, use direct callback
    FeedHandler::instance().onTickReceived(tick);
}

void MainWindow::showPreferences() {
    // Placeholder for preferences dialog
    qDebug() << "[MainWindow] showPreferences requested";
}

// setupShortcuts() is defined in core/GlobalShortcuts.cpp
