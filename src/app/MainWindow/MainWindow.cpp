#include "app/MainWindow.h"
#include "api/XTSInteractiveClient.h"
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

    // Connect centralized UDP broadcast service (legacy XTS::Tick)
    connect(&UdpBroadcastService::instance(), &UdpBroadcastService::tickReceived,
            this, &MainWindow::onTickReceived);
    
    // Connect new UDP::MarketTick signal directly to FeedHandler
    connect(&UdpBroadcastService::instance(), &UdpBroadcastService::udpTickReceived,
            &FeedHandler::instance(), &FeedHandler::onUdpTickReceived);
    
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
            // qDebug() << "[MainWindow] BSE Tick received - Segment:" << tick.exchangeSegment 
            //          << "Token:" << tick.exchangeInstrumentID << "LTP:" << tick.lastTradedPrice;
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

#include "views/PreferenceDialog.h"

// ... (existing includes)

void MainWindow::showPreferences() {
    PreferenceDialog dialog(this);
    dialog.exec();
}

void MainWindow::placeOrder(const XTS::OrderParams &params) {
    if (!m_xtsInteractiveClient || !m_xtsInteractiveClient->isLoggedIn()) {
        if (m_statusBar) m_statusBar->showMessage("Error: Interactive API not logged in");
        return;
    }

    QJsonObject orderJson;
    orderJson["exchangeSegment"] = params.exchangeSegment;
    orderJson["exchangeInstrumentID"] = params.exchangeInstrumentID;
    orderJson["productType"] = params.productType;
    orderJson["orderType"] = params.orderType;
    orderJson["orderSide"] = params.orderSide;
    orderJson["timeInForce"] = params.timeInForce;
    orderJson["disclosedQuantity"] = params.disclosedQuantity;
    orderJson["orderQuantity"] = params.orderQuantity;
    orderJson["limitPrice"] = params.limitPrice;
    orderJson["stopPrice"] = params.stopPrice;
    orderJson["orderUniqueIdentifier"] = params.orderUniqueIdentifier;
    
    // Use stored/default clientID if not provided
    QString clientID = params.clientID;
    if (clientID.isEmpty()) clientID = m_xtsInteractiveClient->getClientID();
    orderJson["clientID"] = clientID;

    qDebug() << "[MainWindow] Placing order:" << orderJson;

    m_xtsInteractiveClient->placeOrder(orderJson, [this](bool success, const QString &orderID, const QString &message) {
        // CRITICAL: This callback runs from HTTP background thread!
        // Must use invokeMethod to safely update UI on main thread
        QMetaObject::invokeMethod(this, [this, success, orderID, message]() {
            if (success) {
                QString msg = QString("Order Placed Successfully. Order ID: %1").arg(orderID);
                if (m_statusBar) m_statusBar->showMessage(msg, 5000);
                QMessageBox::information(this, "Order Placed", msg);
                
                // Refresh orders via HTTP polling (since Interactive socket may not be stable)
                // Use a short delay to allow server to process the order
                QTimer::singleShot(5, this, [this]() {
                    if (m_xtsInteractiveClient && m_tradingDataService) {
                        // Refresh Orders
                        m_xtsInteractiveClient->getOrders([this](bool ordersSuccess, const QVector<XTS::Order> &orders, const QString &) {
                            QMetaObject::invokeMethod(this, [this, ordersSuccess, orders]() {
                                if (ordersSuccess && m_tradingDataService) {
                                    m_tradingDataService->setOrders(orders);
                                    qDebug() << "[MainWindow] Orders refreshed via HTTP:" << orders.size();
                                }
                            }, Qt::QueuedConnection);
                        });
                        
                        // Refresh Trades (for Trade Book)
                        m_xtsInteractiveClient->getTrades([this](bool tradesSuccess, const QVector<XTS::Trade> &trades, const QString &) {
                            QMetaObject::invokeMethod(this, [this, tradesSuccess, trades]() {
                                if (tradesSuccess && m_tradingDataService) {
                                    m_tradingDataService->setTrades(trades);
                                    qDebug() << "[MainWindow] Trades refreshed via HTTP:" << trades.size();
                                }
                            }, Qt::QueuedConnection);
                        });
                        
                        // Refresh Positions (for Net Position)
                        m_xtsInteractiveClient->getPositions("NetWise", [this](bool posSuccess, const QVector<XTS::Position> &positions, const QString &) {
                            QMetaObject::invokeMethod(this, [this, posSuccess, positions]() {
                                if (posSuccess && m_tradingDataService) {
                                    m_tradingDataService->setPositions(positions);
                                    qDebug() << "[MainWindow] Positions refreshed via HTTP:" << positions.size();
                                }
                            }, Qt::QueuedConnection);
                        });
                    }
                });
            } else {
                QString msg = QString("Order Failed: %1").arg(message);
                if (m_statusBar) m_statusBar->showMessage(msg, 5000);
                QMessageBox::critical(this, "Order Failed", msg);
            }
        }, Qt::QueuedConnection);
    });
}

void MainWindow::modifyOrder(const XTS::ModifyOrderParams &params) {
    if (!m_xtsInteractiveClient || !m_xtsInteractiveClient->isLoggedIn()) {
        if (m_statusBar) m_statusBar->showMessage("Error: Interactive API not logged in");
        return;
    }

    qDebug() << "[MainWindow] Modifying order:" << params.appOrderID;

    m_xtsInteractiveClient->modifyOrder(params, [this, params](bool success, const QString &orderID, const QString &message) {
        QMetaObject::invokeMethod(this, [this, success, orderID, message, params]() {
            if (success) {
                QString msg = QString("Order Modified Successfully. Order ID: %1").arg(orderID);
                if (m_statusBar) m_statusBar->showMessage(msg, 5000);
                QMessageBox::information(this, "Order Modified", msg);
                
                // Refresh orders after modification
                QTimer::singleShot(5, this, [this]() {
                    if (m_xtsInteractiveClient && m_tradingDataService) {
                        m_xtsInteractiveClient->getOrders([this](bool ordersSuccess, const QVector<XTS::Order> &orders, const QString &) {
                            QMetaObject::invokeMethod(this, [this, ordersSuccess, orders]() {
                                if (ordersSuccess && m_tradingDataService) {
                                    m_tradingDataService->setOrders(orders);
                                }
                            }, Qt::QueuedConnection);
                        });
                    }
                });
            } else {
                QString msg = QString("Modify Order Failed: %1").arg(message);
                if (m_statusBar) m_statusBar->showMessage(msg, 5000);
                QMessageBox::critical(this, "Modify Failed", msg);
            }
        }, Qt::QueuedConnection);
    });
}

void MainWindow::cancelOrder(int64_t appOrderID) {
    if (!m_xtsInteractiveClient || !m_xtsInteractiveClient->isLoggedIn()) {
        if (m_statusBar) m_statusBar->showMessage("Error: Interactive API not logged in");
        return;
    }

    qDebug() << "[MainWindow] Cancelling order:" << appOrderID;

    m_xtsInteractiveClient->cancelOrder(appOrderID, [this, appOrderID](bool success, const QString &message) {
        QMetaObject::invokeMethod(this, [this, success, message, appOrderID]() {
            if (success) {
                QString msg = QString("Order Cancelled Successfully. Order ID: %1").arg(appOrderID);
                if (m_statusBar) m_statusBar->showMessage(msg, 5000);
                
                // Refresh orders after cancellation
                QTimer::singleShot(5, this, [this]() {
                    if (m_xtsInteractiveClient && m_tradingDataService) {
                        m_xtsInteractiveClient->getOrders([this](bool ordersSuccess, const QVector<XTS::Order> &orders, const QString &) {
                            QMetaObject::invokeMethod(this, [this, ordersSuccess, orders]() {
                                if (ordersSuccess && m_tradingDataService) {
                                    m_tradingDataService->setOrders(orders);
                                }
                            }, Qt::QueuedConnection);
                        });
                    }
                });
            } else {
                QString msg = QString("Cancel Order Failed: %1").arg(message);
                if (m_statusBar) m_statusBar->showMessage(msg, 5000);
                QMessageBox::critical(this, "Cancel Failed", msg);
            }
        }, Qt::QueuedConnection);
    });
}

#include "views/BuyWindow.h"
#include "views/SellWindow.h"
#include "core/widgets/CustomMDISubWindow.h"

void MainWindow::openBuyWindowForModification(const XTS::Order &order) {
    // Close any existing buy/sell window
    closeWindowsByType("BuyWindow");
    closeWindowsByType("SellWindow");

    CustomMDISubWindow *window = new CustomMDISubWindow("Modify Buy Order", m_mdiArea);
    window->setWindowType("BuyWindow");

    BuyWindow *buyWindow = new BuyWindow(window);
    buyWindow->loadFromOrder(order);  // Load order data for modification
    
    // Connect the modification signal
    connect(buyWindow, &BaseOrderWindow::orderModificationSubmitted, this, &MainWindow::modifyOrder);
    
    window->setContentWidget(buyWindow);
    window->resize(1220, 260);
    connectWindowSignals(window);
    m_mdiArea->addWindow(window);
    window->activateWindow();
}

void MainWindow::openSellWindowForModification(const XTS::Order &order) {
    // Close any existing buy/sell window
    closeWindowsByType("BuyWindow");
    closeWindowsByType("SellWindow");

    CustomMDISubWindow *window = new CustomMDISubWindow("Modify Sell Order", m_mdiArea);
    window->setWindowType("SellWindow");

    SellWindow *sellWindow = new SellWindow(window);
    sellWindow->loadFromOrder(order);  // Load order data for modification
    
    // Connect the modification signal
    connect(sellWindow, &BaseOrderWindow::orderModificationSubmitted, this, &MainWindow::modifyOrder);
    
    window->setContentWidget(sellWindow);
    window->resize(1220, 260);
    connectWindowSignals(window);
    m_mdiArea->addWindow(window);
    window->activateWindow();
}

// setupShortcuts() is defined in core/GlobalShortcuts.cpp

