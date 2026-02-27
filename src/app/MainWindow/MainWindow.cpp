#include "app/MainWindow.h"
#include "app/WindowFactory.h"
#include "app/WorkspaceManager.h"
#include "api/xts/XTSInteractiveClient.h"
#include "app/ScripBar.h"
#include "core/widgets/CustomMDIArea.h"
#include "core/widgets/CustomMDISubWindow.h"
#include "core/widgets/InfoBar.h"
#include "nsecm_callback.h"
#include "nsefo_callback.h"
#ifdef HAVE_TRADINGVIEW
#include "ui/TradingViewChartWidget.h"
#endif

#include "core/WindowCacheManager.h"
#include "repository/RepositoryManager.h"
#include "services/ATMWatchManager.h"
#include "services/FeedHandler.h"
#include "services/GreeksCalculationService.h"
#include "strategy/manager/StrategyService.h"
#include "services/TradingDataService.h"
#include "services/UdpBroadcastService.h"
#include "services/XTSFeedBridge.h"
#include "services/ConnectionStatusManager.h"
#include "utils/ConfigLoader.h"
#include "utils/LatencyTracker.h"
#include "utils/WindowManager.h"
#include "views/IndicesView.h"
#include "views/MarketWatchWindow.h"
#include <QAction>
#include <QDebug>
#include <QDockWidget>
#include <QSettings>
#include <QShortcut>
#include <QStatusBar>
#include <QTimer>


MainWindow::MainWindow(QWidget *parent)
    : CustomMainWindow(parent), m_windowFactory(nullptr),
      m_workspaceManager(nullptr), m_xtsMarketDataClient(nullptr),
      m_xtsInteractiveClient(nullptr), m_tradingDataService(nullptr),
      m_configLoader(nullptr), m_indicesDock(nullptr), m_indicesView(nullptr),
      m_allIndicesWindow(nullptr) {
  setTitle("Trading Terminal");
  resize(1600, 900);
  setMinimumSize(800, 600);

  // Setup content FIRST (creates layout, widgets, and m_mdiArea)
  setupContent();

  // Create extracted collaborators (requires m_mdiArea from setupContent)
  m_windowFactory = new WindowFactory(this, m_mdiArea, this);
  m_workspaceManager =
      new WorkspaceManager(this, m_mdiArea, m_windowFactory, this);

  // Wire workspace restore signal to WorkspaceManager instead of MainWindow
  connect(m_mdiArea, &CustomMDIArea::restoreWindowRequested,
          m_workspaceManager, &WorkspaceManager::onRestoreWindowRequested);

  // Setup keyboard shortcuts
  setupShortcuts();

  // CRITICAL FIX: Do NOT connect udpTickReceived ->
  // FeedHandler::onUdpTickReceived UdpBroadcastService already calls
  // FeedHandler directly in its callbacks. connecting it here causes double
  // processing of every tick!

  // CRITICAL FIX: Do NOT connect udpTickReceived -> GreeksCalculationService
  // UdpBroadcastService already calls GreeksService internally.
  // connecting it here causes double calculation of Greeks!

  // Connect FeedHandler price subscription requests to MainWindow router (for
  // new PriceCache)
  connect(&FeedHandler::instance(), &FeedHandler::requestPriceSubscription,
          this, &MainWindow::onPriceSubscriptionRequest,
          Qt::QueuedConnection); // Async, thread-safe

  // Connect strategy order requests to order placement pipeline
  connect(&StrategyService::instance(), &StrategyService::orderRequested,
          this, &MainWindow::placeOrder, Qt::QueuedConnection);

  // setupNetwork() will be called once configLoader is set
  // to ensure we have the correct multicast IPs and ports.

  // Restore visibility preferences AND sync menu actions
  QSettings s("TradingCompany", "TradingTerminal");

  bool infoVisible = s.value("mainwindow/info_visible", true).toBool();
  if (m_infoDock)
    m_infoDock->setVisible(infoVisible);
  if (m_infoBarAction)
    m_infoBarAction->setChecked(infoVisible);

  bool statusVisible = s.value("mainwindow/status_visible", true).toBool();
  if (m_statusBar)
    m_statusBar->setVisible(statusVisible);
  if (m_statusBarAction)
    m_statusBarAction->setChecked(statusVisible);

  // in setConfigLoader() to ensure market data windows only appear after
  // authentication. This improves startup time and ensures proper
  // initialization order.

  // setupNetwork() will be called once configLoader is set
  // to ensure we have the correct multicast IPs and ports.
}

MainWindow::~MainWindow() { stopBroadcastReceiver(); }

void MainWindow::setXTSClients(XTSMarketDataClient *mdClient,
                               XTSInteractiveClient *iaClient) {
  m_xtsMarketDataClient = mdClient;
  m_xtsInteractiveClient = iaClient;

  // Propagate to WindowFactory
  if (m_windowFactory)
    m_windowFactory->setXTSClients(mdClient, iaClient);

  if (m_xtsMarketDataClient) {
    connect(m_xtsMarketDataClient, &XTSMarketDataClient::tickReceived, this,
            &MainWindow::onTickReceived);
  }

  if (m_scripBar && m_xtsMarketDataClient) {
    m_scripBar->setXTSClient(m_xtsMarketDataClient);
  }

  // Set XTS client for cached SnapQuote window
  WindowCacheManager::instance().setXTSClientForSnapQuote(
      m_xtsMarketDataClient);

  // Wire ConnectionStatusManager for live connection state tracking
  auto& connMgr = ConnectionStatusManager::instance();
  connMgr.wireXTSMarketDataClient(m_xtsMarketDataClient);
  connMgr.wireXTSInteractiveClient(m_xtsInteractiveClient);
}

void MainWindow::setTradingDataService(TradingDataService *service) {
  m_tradingDataService = service;

  // Propagate to WindowFactory
  if (m_windowFactory)
    m_windowFactory->setTradingDataService(service);
}

void MainWindow::setConfigLoader(ConfigLoader *loader) {
  m_configLoader = loader;

  // Update ATM default source from config
  if (m_configLoader) {
    QString mode = m_configLoader->getBasePriceMode();
    auto& atm = ATMWatchManager::getInstance();
    ATMWatchManager::BasePriceSource source =
        (mode == "future") ? ATMWatchManager::BasePriceSource::Future
                           : ATMWatchManager::BasePriceSource::Cash;

    atm.setDefaultBasePriceSource(source);

    // Add default watches using the configured source
    atm.addWatch("NIFTY", "27JAN2026", source);
    atm.addWatch("BANKNIFTY", "27JAN2026", source);
  }

  // ✅ DO NOT create IndicesView here!
  // IndicesView will be created in main.cpp continue button callback
  // AFTER mainWindow->show() completes rendering
  // This prevents the race condition where IndicesView appears during login

  // ✅ Initialize GreeksCalculationService
  GreeksCalculationService::instance().loadConfiguration();
  GreeksCalculationService::instance().setRepositoryManager(
      RepositoryManager::getInstance());
  qDebug() << "[MainWindow] GreeksCalculationService initialized";

  // Start UDP broadcast receivers AFTER main window is fully shown
  // Using QTimer to ensure window is rendered and responsive first
  setupNetwork();

  // ✅ Initialize XTSFeedBridge (XTS-only fallback for internet users)
  initializeXTSFeedBridge();

  // ✅ Initialize ConnectionStatusManager with config
  auto& connMgr = ConnectionStatusManager::instance();
  connMgr.wireUdpBroadcastService();

  // Set default primary source from config.ini [FEED] primary_data_provider
  QString provider = m_configLoader->getPrimaryDataProvider();
  PrimaryDataSource defaultSource = PrimaryDataSource::UDP_PRIMARY;
  if (provider == "xts") {
      defaultSource = PrimaryDataSource::XTS_PRIMARY;
  }
  connMgr.setDefaultPrimarySource(defaultSource);

  qDebug() << "[MainWindow] ConnectionStatusManager initialized — default source:"
           << (defaultSource == PrimaryDataSource::UDP_PRIMARY ? "UDP" : "XTS")
           << "(config: primary_data_provider=" << provider << ")";
}

void MainWindow::refreshScripBar() {
  if (m_scripBar)
    m_scripBar->refreshSymbols();
}

bool MainWindow::hasIndicesView() const { return m_indicesView != nullptr; }

void MainWindow::closeEvent(QCloseEvent *event) {
  QSettings s("TradingCompany", "TradingTerminal");
  s.setValue("mainwindow/state", saveState());

  if (m_infoDock)
    s.setValue("mainwindow/info_visible", m_infoDock->isVisible());
  if (m_statusBar)
    s.setValue("mainwindow/status_visible", m_statusBar->isVisible());
  if (m_indicesDock)
    s.setValue("mainwindow/indices_visible", m_indicesDock->isVisible());

  CustomMainWindow::closeEvent(event);
}

void MainWindow::onTickReceived(const XTS::Tick &tick) {
  // Convert XTS::Tick to UDP::MarketTick to funnel through the new architecture
  UDP::MarketTick udpTick;
  udpTick.exchangeSegment =
      static_cast<UDP::ExchangeSegment>(tick.exchangeSegment);
  udpTick.token = static_cast<uint32_t>(tick.exchangeInstrumentID);
  udpTick.ltp = tick.lastTradedPrice;
  udpTick.ltq = tick.lastTradedQuantity;
  udpTick.volume = tick.volume;
  udpTick.open = tick.open;
  udpTick.high = tick.high;
  udpTick.low = tick.low;
  udpTick.prevClose = tick.close;
  udpTick.atp = tick.averagePrice;
  udpTick.openInterest = tick.openInterest;

  // Simplistic depth conversion for compatibility
  if (tick.bidPrice > 0) {
    udpTick.bids[0].price = tick.bidPrice;
    udpTick.bids[0].quantity = tick.bidQuantity;
  }
  if (tick.askPrice > 0) {
    udpTick.asks[0].price = tick.askPrice;
    udpTick.asks[0].quantity = tick.askQuantity;
  }

  // Publish through new FeedHandler
  FeedHandler::instance().onUdpTickReceived(udpTick);
}

void MainWindow::onPriceSubscriptionRequest(QString requesterId, uint32_t token,
                                            uint16_t segment) {
  // Legacy routing: FeedHandler now manages this automatically via subscribe()
  // but we keep the slot for any other component that might use it.
  UdpBroadcastService::instance().subscribeToken(token, segment);
}

// UDP Broadcast Logic
void MainWindow::startBroadcastReceiver() {
  if (!m_configLoader) {
    qWarning() << "[MainWindow] startBroadcastReceiver failed: ConfigLoader is "
                  "missing!";
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

  if (config.bseFoIp.empty())
    config.bseFoIp = "239.1.2.5";
  if (config.bseFoPort == 0)
    config.bseFoPort = 26002;
  if (config.bseCmIp.empty())
    config.bseCmIp = "239.1.2.4"; // Standard BSE CM Multicast
  if (config.bseCmPort == 0)
    config.bseCmPort = 26001;

  // TODO: Add NSE CM methods to ConfigLoader if they don't exist
  // For now we start with what we have

  UdpBroadcastService::instance().start(config);

  if (m_statusBar) {
    m_statusBar->showMessage("Market Data Receivers: INITIALIZING...", 3000);
  }
}

void MainWindow::stopBroadcastReceiver() {
  UdpBroadcastService::instance().stop();
  if (m_statusBar)
    m_statusBar->showMessage("Market Data Receivers: STOPPED");
}

#include "views/PreferenceDialog.h"

// ... (existing includes)

void MainWindow::showPreferences() {
  PreferenceDialog dialog(this);
  dialog.exec();
}

void MainWindow::placeOrder(const XTS::OrderParams &params) {
  if (!m_xtsInteractiveClient || !m_xtsInteractiveClient->isLoggedIn()) {
    if (m_statusBar)
      m_statusBar->showMessage("Error: Interactive API not logged in");
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
  if (clientID.isEmpty())
    clientID = m_xtsInteractiveClient->getClientID();
  orderJson["clientID"] = clientID;

  qDebug() << "[MainWindow] Placing order:" << orderJson;

  // Capture order parameters for chart marker visualization
  XTS::OrderParams capturedParams = params;

  m_xtsInteractiveClient->placeOrder(orderJson, [this, capturedParams](bool success,
                                                       const QString &orderID,
                                                       const QString &message) {
    // CRITICAL: This callback runs from HTTP background thread!
    // Must use invokeMethod to safely update UI on main thread
    QMetaObject::invokeMethod(
        this,
        [this, success, orderID, message, capturedParams]() {
          if (success) {
            QString msg =
                QString("Order Placed Successfully. Order ID: %1").arg(orderID);
            if (m_statusBar)
              m_statusBar->showMessage(msg, 5000);
            QMessageBox::information(this, "Order Placed", msg);

            // Add order marker to all chart windows showing this symbol
            if (m_mdiArea) {
              QList<CustomMDISubWindow *> windows = m_mdiArea->windowList();
              for (CustomMDISubWindow *window : windows) {
                if (window->windowType() == "ChartWindow") {
#ifdef HAVE_TRADINGVIEW
                  TradingViewChartWidget *chart =
                      qobject_cast<TradingViewChartWidget *>(window->contentWidget());
                  if (chart && chart->isReady()) {
                    // Use current time for marker
                    qint64 currentTime = QDateTime::currentSecsSinceEpoch();
                    
                    // Determine price (use limit price for limit orders, or 0 for market)
                    double price = capturedParams.limitPrice > 0 
                                    ? capturedParams.limitPrice 
                                    : capturedParams.stopPrice;
                    
                    // Determine marker properties based on order side
                    QString text = capturedParams.orderSide == "BUY" ? "BUY" : "SELL";
                    QString color = capturedParams.orderSide == "BUY" ? "#26a69a" : "#ef5350";
                    QString shape = capturedParams.orderSide == "BUY" ? "arrow_up" : "arrow_down";
                    
                    if (price > 0) {
                      chart->addOrderMarker(currentTime, price, text, color, shape);
                      qDebug() << "[MainWindow] Added order marker to chart:"
                               << text << "@" << price;
                    }
                  }
#endif // HAVE_TRADINGVIEW
                } // if ChartWindow
              }
            }

            // Refresh orders via HTTP polling (since Interactive socket may not
            // be stable) Use a short delay to allow server to process the order
            QTimer::singleShot(5, this, [this]() {
              if (m_xtsInteractiveClient && m_tradingDataService) {
                // Refresh Orders
                m_xtsInteractiveClient->getOrders(
                    [this](bool ordersSuccess,
                           const QVector<XTS::Order> &orders, const QString &) {
                      QMetaObject::invokeMethod(
                          this,
                          [this, ordersSuccess, orders]() {
                            if (ordersSuccess && m_tradingDataService) {
                              m_tradingDataService->setOrders(orders);
                              qDebug()
                                  << "[MainWindow] Orders refreshed via HTTP:"
                                  << orders.size();
                            }
                          },
                          Qt::QueuedConnection);
                    });

                // Refresh Trades (for Trade Book)
                m_xtsInteractiveClient->getTrades(
                    [this](bool tradesSuccess,
                           const QVector<XTS::Trade> &trades, const QString &) {
                      QMetaObject::invokeMethod(
                          this,
                          [this, tradesSuccess, trades]() {
                            if (tradesSuccess && m_tradingDataService) {
                              m_tradingDataService->setTrades(trades);
                              qDebug()
                                  << "[MainWindow] Trades refreshed via HTTP:"
                                  << trades.size();
                            }
                          },
                          Qt::QueuedConnection);
                    });

                // Refresh Positions (for Net Position)
                m_xtsInteractiveClient->getPositions(
                    "NetWise", [this](bool posSuccess,
                                      const QVector<XTS::Position> &positions,
                                      const QString &) {
                      QMetaObject::invokeMethod(
                          this,
                          [this, posSuccess, positions]() {
                            if (posSuccess && m_tradingDataService) {
                              m_tradingDataService->setPositions(positions);
                              qDebug() << "[MainWindow] Positions refreshed "
                                          "via HTTP:"
                                       << positions.size();
                            }
                          },
                          Qt::QueuedConnection);
                    });
              }
            });
          } else {
            QString msg = QString("Order Failed: %1").arg(message);
            if (m_statusBar)
              m_statusBar->showMessage(msg, 5000);
            QMessageBox::critical(this, "Order Failed", msg);
          }
        },
        Qt::QueuedConnection);
  });
}

void MainWindow::modifyOrder(const XTS::ModifyOrderParams &params) {
  if (!m_xtsInteractiveClient || !m_xtsInteractiveClient->isLoggedIn()) {
    if (m_statusBar)
      m_statusBar->showMessage("Error: Interactive API not logged in");
    return;
  }

  qDebug() << "[MainWindow] Modifying order:" << params.appOrderID;

  m_xtsInteractiveClient->modifyOrder(params, [this,
                                               params](bool success,
                                                       const QString &orderID,
                                                       const QString &message) {
    QMetaObject::invokeMethod(
        this,
        [this, success, orderID, message, params]() {
          if (success) {
            QString msg = QString("Order Modified Successfully. Order ID: %1")
                              .arg(orderID);
            if (m_statusBar)
              m_statusBar->showMessage(msg, 5000);
            QMessageBox::information(this, "Order Modified", msg);

            // Refresh orders after modification
            QTimer::singleShot(5, this, [this]() {
              if (m_xtsInteractiveClient && m_tradingDataService) {
                m_xtsInteractiveClient->getOrders(
                    [this](bool ordersSuccess,
                           const QVector<XTS::Order> &orders, const QString &) {
                      QMetaObject::invokeMethod(
                          this,
                          [this, ordersSuccess, orders]() {
                            if (ordersSuccess && m_tradingDataService) {
                              m_tradingDataService->setOrders(orders);
                            }
                          },
                          Qt::QueuedConnection);
                    });
              }
            });
          } else {
            QString msg = QString("Modify Order Failed: %1").arg(message);
            if (m_statusBar)
              m_statusBar->showMessage(msg, 5000);
            QMessageBox::critical(this, "Modify Failed", msg);
          }
        },
        Qt::QueuedConnection);
  });
}

void MainWindow::cancelOrder(int64_t appOrderID) {
  if (!m_xtsInteractiveClient || !m_xtsInteractiveClient->isLoggedIn()) {
    if (m_statusBar)
      m_statusBar->showMessage("Error: Interactive API not logged in");
    return;
  }

  qDebug() << "[MainWindow] Cancelling order:" << appOrderID;

  m_xtsInteractiveClient->cancelOrder(appOrderID, [this, appOrderID](
                                                      bool success,
                                                      const QString &message) {
    QMetaObject::invokeMethod(
        this,
        [this, success, message, appOrderID]() {
          if (success) {
            QString msg = QString("Order Cancelled Successfully. Order ID: %1")
                              .arg(appOrderID);
            if (m_statusBar)
              m_statusBar->showMessage(msg, 5000);

            // Refresh orders after cancellation
            QTimer::singleShot(5, this, [this]() {
              if (m_xtsInteractiveClient && m_tradingDataService) {
                m_xtsInteractiveClient->getOrders(
                    [this](bool ordersSuccess,
                           const QVector<XTS::Order> &orders, const QString &) {
                      QMetaObject::invokeMethod(
                          this,
                          [this, ordersSuccess, orders]() {
                            if (ordersSuccess && m_tradingDataService) {
                              m_tradingDataService->setOrders(orders);
                            }
                          },
                          Qt::QueuedConnection);
                    });
              }
            });
          } else {
            QString msg = QString("Cancel Order Failed: %1").arg(message);
            if (m_statusBar)
              m_statusBar->showMessage(msg, 5000);
            QMessageBox::critical(this, "Cancel Failed", msg);
          }
        },
        Qt::QueuedConnection);
  });
}

// setupShortcuts() is defined in core/GlobalShortcuts.cpp

void MainWindow::onScripBarEscapePressed() {
  // Restore focus to the last active MDI window + its last focused widget
  QWidget* activeWin = WindowManager::instance().getActiveWindow();
  if (activeWin) {
    // Find the MDI sub-window that contains this content widget
    if (auto* parent = qobject_cast<CustomMDISubWindow*>(activeWin->parentWidget())) {
      parent->activateWindow();
      parent->raise();
    } else {
      activeWin->activateWindow();
      activeWin->raise();
    }
    WindowManager::instance().restoreFocusState(activeWin);
    qDebug() << "[MainWindow] ScripBar Esc → restored focus to last active window";
  } else {
    // Fallback: focus any MarketWatch (via WindowFactory)
    MarketWatchWindow *activeMW = m_windowFactory->getActiveMarketWatch();
    if (activeMW) {
      activeMW->setFocus();
      qDebug() << "[MainWindow] ScripBar Esc → fallback to MarketWatch";
    }
  }
}
