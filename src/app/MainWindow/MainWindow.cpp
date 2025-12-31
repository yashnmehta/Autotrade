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
#include <thread>
#include "multicast_receiver.h"
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
    
    // Initialize network (auto-start broadcast receiver if enabled)
    setupNetwork();
    
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
    FeedHandler::instance().onTickReceived(tick);
}


// UDP Broadcast Logic
void MainWindow::startBroadcastReceiver() {
    qDebug() << "[UDP] Starting NSE broadcast receivers (FO + CM)...";
    
    // Default values (fallback)
    std::string nseFoIp = "233.1.2.5";
    int nseFoPort = 34330; 
    std::string nseCmIp = "233.1.2.5";
    int nseCmPort = 34001;
    
    // Load from ConfigLoader
    if (m_configLoader) {
        nseFoIp = m_configLoader->getNSEFOMulticastIP().toStdString();
        nseFoPort = m_configLoader->getNSEFOPort();
        
        QJsonObject udp = m_configLoader->getUDPConfig();
        QJsonObject exchanges = udp["udp"].toObject()["exchanges"].toObject();
        
        if (exchanges.contains("NSECM")) {
            QJsonObject nseCm = exchanges["NSECM"].toObject();
            nseCmIp = nseCm["multicastGroup"].toString().toStdString();
            nseCmPort = nseCm["port"].toInt();
        }
        
        qDebug() << "[UDP] Config loaded for FO:" << QString::fromStdString(nseFoIp) << ":" << nseFoPort;
        qDebug() << "[UDP] Config loaded for CM:" << QString::fromStdString(nseCmIp) << ":" << nseCmPort;
    } else {
        qWarning() << "[UDP] ConfigLoader is missing! Using built-in defaults.";
    }
    
    // ==========================================
    // 0. STOP EXISTING THREADS FIRST (Crucial to prevent race)
    // ==========================================
    if (m_udpReceiver) m_udpReceiver->stop();
    if (m_udpThread.joinable()) m_udpThread.join();
    m_udpReceiver.reset();

    if (m_udpReceiverCM) m_udpReceiverCM->stop();
    if (m_udpThreadCM.joinable()) m_udpThreadCM.join();
    m_udpReceiverCM.reset();

    // ==========================================
    // 1. NSE FO RECEIVER SETUP
    // ==========================================
    try {
        m_udpReceiver = std::make_unique<nsefo::MulticastReceiver>(nseFoIp, nseFoPort);
        if (m_udpReceiver->isValid()) {
            // Register FO Callbacks
            nsefo::MarketDataCallbackRegistry::instance().registerTouchlineCallback(
                [this](const nsefo::TouchlineData& data) {
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
                    tick.averagePrice = data.avgPrice;
                    tick.refNo = data.refNo;
                    tick.timestampUdpRecv = data.timestampRecv;
                    tick.timestampParsed = data.timestampParsed;
                    QMetaObject::invokeMethod(this, "onUdpTickReceived", Qt::QueuedConnection, Q_ARG(XTS::Tick, tick));
                }
            );

            nsefo::MarketDataCallbackRegistry::instance().registerMarketDepthCallback(
                [this](const nsefo::MarketDepthData& data) {
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
                    QMetaObject::invokeMethod(this, "onUdpTickReceived", Qt::QueuedConnection, Q_ARG(XTS::Tick, tick));
                }
            );

            nsefo::MarketDataCallbackRegistry::instance().registerTickerCallback(
                 [this](const nsefo::TickerData& data) {
                    if (data.fillVolume > 0) {
                        XTS::Tick tick;
                        tick.exchangeSegment = 2;
                        tick.exchangeInstrumentID = data.token;
                        tick.lastTradedQuantity = (int)data.fillVolume;
                        tick.lastTradedPrice = data.fillPrice;
                        tick.openInterest = data.openInterest;
                        tick.refNo = data.refNo;
                        tick.timestampUdpRecv = data.timestampRecv;
                        tick.timestampParsed = data.timestampParsed;
                        QMetaObject::invokeMethod(this, "onUdpTickReceived", Qt::QueuedConnection, Q_ARG(XTS::Tick, tick));
                    }
                 }
            );

            // Start FO Thread
            m_udpThread = std::thread([this]() {
                try {
                    if (m_udpReceiver) m_udpReceiver->start();
                } catch (const std::exception& e) {
                    qCritical() << "[UDP] NSE FO Thread Error:" << e.what();
                }
            });
        }
    } catch (const std::exception& e) {
        qCritical() << "[UDP] Failed to initialize FO receiver:" << e.what();
        if (m_statusBar) m_statusBar->showMessage(QString("NSE FO Error: %1").arg(e.what()), 5000);
    }

    // ==========================================
    // 2. NSE CM RECEIVER SETUP
    // ==========================================
    try {
        m_udpReceiverCM = std::make_unique<nsecm::MulticastReceiver>(nseCmIp, nseCmPort);
        if (m_udpReceiverCM->isValid()) {
            // Register CM Callbacks
            nsecm::MarketDataCallbackRegistry::instance().registerTouchlineCallback(
                [this](const nsecm::TouchlineData& data) {
                    XTS::Tick tick;
                    tick.exchangeSegment = 1; // NSECM
                    tick.exchangeInstrumentID = data.token;
                    tick.lastTradedPrice = data.ltp;
                    tick.lastTradedQuantity = data.lastTradeQty;
                    tick.volume = (uint32_t)data.volume;
                    tick.open = data.open;
                    tick.high = data.high;
                    tick.low = data.low;
                    tick.close = data.close;
                    tick.lastUpdateTime = data.lastTradeTime;
                    tick.averagePrice = data.avgPrice;
                    tick.refNo = data.refNo;
                    tick.timestampUdpRecv = data.timestampRecv;
                    tick.timestampParsed = data.timestampParsed;
                    QMetaObject::invokeMethod(this, "onUdpTickReceived", Qt::QueuedConnection, Q_ARG(XTS::Tick, tick));
                }
            );

            nsecm::MarketDataCallbackRegistry::instance().registerMarketDepthCallback(
                [this](const nsecm::MarketDepthData& data) {
                    XTS::Tick tick;
                    tick.exchangeSegment = 1;
                    tick.exchangeInstrumentID = data.token;
                    if (data.bids[0].quantity > 0) {
                        tick.bidPrice = data.bids[0].price;
                        tick.bidQuantity = (int)data.bids[0].quantity;
                    }
                    if (data.asks[0].quantity > 0) {
                        tick.askPrice = data.asks[0].price;
                        tick.askQuantity = (int)data.asks[0].quantity;
                    }
                    tick.totalBuyQuantity = (int)data.totalBuyQty;
                    tick.totalSellQuantity = (int)data.totalSellQty;
                    tick.refNo = data.refNo;
                    tick.timestampUdpRecv = data.timestampRecv;
                    tick.timestampParsed = data.timestampParsed;
                    QMetaObject::invokeMethod(this, "onUdpTickReceived", Qt::QueuedConnection, Q_ARG(XTS::Tick, tick));
                }
            );

            // Start CM Thread
            m_udpThreadCM = std::thread([this]() {
                try {
                    if (m_udpReceiverCM) m_udpReceiverCM->start();
                } catch (const std::exception& e) {
                    qCritical() << "[UDP] NSE CM Thread Error:" << e.what();
                }
            });
        }
    } catch (const std::exception& e) {
        qCritical() << "[UDP] Failed to initialize CM receiver:" << e.what();
        if (m_statusBar) m_statusBar->showMessage(QString("NSE CM Error: %1").arg(e.what()), 5000);
    }
    
    if (m_statusBar && m_udpReceiver && m_udpReceiverCM) {
        m_statusBar->showMessage("NSE Broadcast Receivers: FO & CM ACTIVE", 2000);
    }
}

void MainWindow::stopBroadcastReceiver() {
    if (m_udpReceiver) m_udpReceiver->stop();
    if (m_udpThread.joinable()) m_udpThread.join();
    m_udpReceiver.reset();

    if (m_udpReceiverCM) m_udpReceiverCM->stop();
    if (m_udpThreadCM.joinable()) m_udpThreadCM.join();
    m_udpReceiverCM.reset();

    if (m_statusBar) m_statusBar->showMessage("NSE Broadcast Receivers: STOPPED");
}

void MainWindow::onUdpTickReceived(const XTS::Tick& tick) {
    XTS::Tick processedTick = tick;
    processedTick.timestampDequeued = LatencyTracker::now();
    FeedHandler::instance().onTickReceived(processedTick);
}

void MainWindow::showPreferences() {
    // Placeholder for preferences dialog
    qDebug() << "[MainWindow] showPreferences requested";
}

// setupShortcuts() is defined in core/GlobalShortcuts.cpp
