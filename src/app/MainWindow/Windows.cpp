#include "app/MainWindow.h"
#include "app/ScripBar.h"
#include "core/WindowCacheManager.h"
#include "core/WindowConstants.h"
#include "core/widgets/CustomMDIArea.h"
#include "core/widgets/CustomMDISubWindow.h"
#include "data/PriceStoreGateway.h"
#include "repository/RepositoryManager.h"
#include "services/UdpBroadcastService.h"
#include "ui/ATMWatchWindow.h"
#include "ui/OptionChainWindow.h"
#include "views/BuyWindow.h"
#include "views/CustomizeDialog.h"
#include "views/MarketWatchWindow.h"
#include "views/OrderBookWindow.h"
#include "views/PositionWindow.h"
#include "views/SellWindow.h"
#include "views/SnapQuoteWindow.h"
#include "views/TradeBookWindow.h"
#include <QDateTime> // Added for performance logging
#include <QDebug>
#include <QElapsedTimer> // Added for performance logging
#include <QStatusBar>

// Helper to count windows
int MainWindow::countWindowsOfType(const QString &type) {
  int count = 0;
  if (m_mdiArea) {
    for (auto window : m_mdiArea->windowList()) {
      if (window->windowType() == type) {
        count++;
      }
    }
  }
  return count;
}

// Helper to close windows by type
void MainWindow::closeWindowsByType(const QString &type) {
  if (!m_mdiArea)
    return;

  QList<CustomMDISubWindow *> windows = m_mdiArea->windowList();
  for (auto window : windows) {
    if (window->windowType() == type) {
      window->close();
    }
  }
}

// Helper to connect signals
void MainWindow::connectWindowSignals(CustomMDISubWindow *window) {
  if (!window)
    return;

  // Connect MDI area signals
  connect(window, &CustomMDISubWindow::closeRequested, window, &QWidget::close);
  connect(window, &CustomMDISubWindow::minimizeRequested, this,
          [this, window]() { m_mdiArea->minimizeWindow(window); });
  connect(window, &CustomMDISubWindow::maximizeRequested, window,
          &CustomMDISubWindow::maximize);
  connect(window, &CustomMDISubWindow::windowActivated, this,
          [this, window]() { m_mdiArea->activateWindow(window); });

  // Connect Customize signal
  connect(window, &CustomMDISubWindow::customizeRequested, this,
          [this, window]() {
            QString windowType = window->windowType();
            QWidget *targetWidget = window->contentWidget();

            CustomizeDialog dialog(windowType, targetWidget, this);
            dialog.exec();
          });

  // Auto-connect BaseOrderWindow signals (Buy/Sell)
  if (window->contentWidget() && (window->windowType() == "BuyWindow" ||
                                  window->windowType() == "SellWindow")) {
    BaseOrderWindow *orderWin =
        qobject_cast<BaseOrderWindow *>(window->contentWidget());
    if (orderWin) {
      connect(orderWin, &BaseOrderWindow::orderSubmitted, this,
              &MainWindow::placeOrder);
    }
  }
}

CustomMDISubWindow *MainWindow::createMarketWatch() {
  // ⏱️ PERFORMANCE LOG: Start timing market watch creation
  static int counter = 1;
  static int mwCounter = 0;
  mwCounter++;
  QElapsedTimer mwTimer;
  mwTimer.start();
  qint64 startTime = QDateTime::currentMSecsSinceEpoch();

  qDebug() << "[PERF] [CREATE_MARKETWATCH] #" << mwCounter
           << "START - Counter:" << counter;

  CustomMDISubWindow *window = new CustomMDISubWindow(
      QString("Market Watch %1").arg(counter++), m_mdiArea);
  window->setWindowType("MarketWatch");
  qint64 t0 = mwTimer.elapsed();

  MarketWatchWindow *marketWatch = new MarketWatchWindow(window);
  qint64 t1 = mwTimer.elapsed();

  marketWatch->setupZeroCopyMode();
  qint64 t2 = mwTimer.elapsed();

  window->setContentWidget(marketWatch);
  window->resize(900, 400);
  qint64 t3 = mwTimer.elapsed();

  connectWindowSignals(window);
  qint64 t4 = mwTimer.elapsed();

  // OPTIMIZATION: Batch MDI operations to reduce layout calculations (saves
  // ~50ms)
  m_mdiArea->setUpdatesEnabled(false);
  m_mdiArea->addWindow(window);
  qint64 t5 = mwTimer.elapsed();

  window->setFocus();
  window->raise();
  window->activateWindow();
  m_mdiArea->setUpdatesEnabled(true); // Single repaint here
  qint64 t6 = mwTimer.elapsed();

  qint64 totalTime = mwTimer.elapsed();
  qDebug() << "[PERF] [CREATE_MARKETWATCH] #" << mwCounter << "COMPLETE";
  qDebug() << "  TOTAL TIME:" << totalTime << "ms";
  qDebug() << "  Breakdown:";
  qDebug() << "    - Create MDI SubWindow:" << t0 << "ms";
  qDebug() << "    - Create MarketWatchWindow (see constructor logs):"
           << (t1 - t0) << "ms";
  qDebug() << "    - Setup zero-copy mode:" << (t2 - t1) << "ms";
  qDebug() << "    - Set content widget + resize:" << (t3 - t2) << "ms";
  qDebug() << "    - Connect signals:" << (t4 - t3) << "ms";
  qDebug() << "    - Add to MDI area (batched):" << (t5 - t4) << "ms";
  qDebug() << "    - Focus/raise/activate (batched):" << (t6 - t5) << "ms";
  qDebug() << "    - Add to MDI area:" << (t5 - t4) << "ms";
  qDebug() << "    - Focus/raise/activate:" << (t6 - t5) << "ms";
  return window;
}

CustomMDISubWindow *MainWindow::createBuyWindow() {
  static int f1Counter = 0;
  f1Counter++;
  qDebug() << "[PERF] [F1_PRESS] #" << f1Counter
           << " START Time:" << QDateTime::currentMSecsSinceEpoch();

  // Try cache first for fast opening (~10ms instead of ~400ms)
  MarketWatchWindow *activeMarketWatch = getActiveMarketWatch();
  WindowContext context;
  if (activeMarketWatch && activeMarketWatch->hasValidSelection()) {
    context = activeMarketWatch->getSelectedContractContext();
  }
  CustomMDISubWindow *cached = WindowCacheManager::instance().showBuyWindow(
      context.isValid() ? &context : nullptr);
  if (cached) {
    return cached; // Cache handled it
  }

  // Fallback to normal creation if cache not available
  // Enforce single order window limit: Only one Buy OR Sell window at a time
  closeWindowsByType("BuyWindow");
  closeWindowsByType("SellWindow");

  CustomMDISubWindow *window = new CustomMDISubWindow("Buy Order", m_mdiArea);
  window->setWindowType("BuyWindow");

  // activeMarketWatch is already declared above
  BuyWindow *buyWindow = nullptr;

  if (activeMarketWatch && activeMarketWatch->hasValidSelection()) {
    WindowContext context = activeMarketWatch->getSelectedContractContext();
    if (context.isValid()) {
      buyWindow = new BuyWindow(context, window);
    } else {
      buyWindow = new BuyWindow(window);
    }
  } else {
    CustomMDISubWindow *activeSub =
        m_mdiArea ? m_mdiArea->activeWindow() : nullptr;
    if (activeSub) {
      // Check for Snap Quote
      SnapQuoteWindow *snap =
          qobject_cast<SnapQuoteWindow *>(activeSub->contentWidget());
      if (snap) {
        WindowContext ctx = snap->getContext();
        if (ctx.isValid()) {
          buyWindow = new BuyWindow(ctx, window);
        }
      }
      // Check for Position Window
      else if (activeSub->windowType() == "PositionWindow") {
        PositionWindow *pw =
            qobject_cast<PositionWindow *>(activeSub->contentWidget());
        if (pw) {
          WindowContext ctx = pw->getSelectedContext();
          if (ctx.isValid()) {
            buyWindow = new BuyWindow(ctx, window);
          }
        }
      }
      // Check for Option Chain Window
      else if (activeSub->windowType() == "OptionChain") {
        OptionChainWindow *oc =
            qobject_cast<OptionChainWindow *>(activeSub->contentWidget());
        if (oc) {
          WindowContext ctx = oc->getSelectedContext();
          if (ctx.isValid()) {
            buyWindow = new BuyWindow(ctx, window);
          }
        }
      }
    }
    if (!buyWindow)
      buyWindow = new BuyWindow(window);
  }

  window->setContentWidget(buyWindow);
  window->resize(1220, 200);
  connectWindowSignals(window);
  m_mdiArea->addWindow(window);

  // Restore last saved position (shared between Buy and Sell windows)
  QSettings settings("TradingCompany", "TradingTerminal");
  if (settings.contains("orderwindow/last_x") &&
      settings.contains("orderwindow/last_y")) {
    int x = settings.value("orderwindow/last_x").toInt();
    int y = settings.value("orderwindow/last_y").toInt();
    window->move(x, y);
  } else {
    // Default position: bottom-right
    QSize mdiSize = m_mdiArea->size();
    int x = mdiSize.width() - 1220 - 20;
    int y = mdiSize.height() - 200 - 20;
    window->move(x, y);
  }

  // Save position when window is moved (connection auto-disconnects when window
  // is destroyed) Save position when window is moved (connection
  // auto-disconnects when window is destroyed)
  connect(
      window, &CustomMDISubWindow::windowMoved, window,
      [](const QPoint &pos) {
        WindowCacheManager::instance().saveOrderWindowPosition(pos);
      },
      Qt::UniqueConnection);

  window->activateWindow();
  return window;
}

CustomMDISubWindow *MainWindow::createSellWindow() {
  static int f2Counter = 0;
  f2Counter++;
  qDebug() << "[PERF] [F2_PRESS] #" << f2Counter
           << " START Time:" << QDateTime::currentMSecsSinceEpoch();

  // Try cache first for fast opening (~10ms instead of ~400ms)
  MarketWatchWindow *activeMarketWatch = getActiveMarketWatch();
  WindowContext context;
  if (activeMarketWatch && activeMarketWatch->hasValidSelection()) {
    context = activeMarketWatch->getSelectedContractContext();
  }
  CustomMDISubWindow *cached = WindowCacheManager::instance().showSellWindow(
      context.isValid() ? &context : nullptr);
  if (cached) {
    return cached; // Cache handled it
  }

  // Fallback to normal creation if cache not available
  // Enforce single order window limit: Only one Buy OR Sell window at a time
  closeWindowsByType("BuyWindow");
  closeWindowsByType("SellWindow");

  CustomMDISubWindow *window = new CustomMDISubWindow("Sell Order", m_mdiArea);
  window->setWindowType("SellWindow");

  // activeMarketWatch is already declared above
  SellWindow *sellWindow = nullptr;

  if (activeMarketWatch && activeMarketWatch->hasValidSelection()) {
    WindowContext context = activeMarketWatch->getSelectedContractContext();
    if (context.isValid()) {
      sellWindow = new SellWindow(context, window);
    } else {
      sellWindow = new SellWindow(window);
    }
  } else {
    CustomMDISubWindow *activeSub =
        m_mdiArea ? m_mdiArea->activeWindow() : nullptr;
    if (activeSub) {
      SnapQuoteWindow *snap =
          qobject_cast<SnapQuoteWindow *>(activeSub->contentWidget());
      if (snap) {
        WindowContext ctx = snap->getContext();
        if (ctx.isValid()) {
          sellWindow = new SellWindow(ctx, window);
        }
      }
      // Check for Position Window
      else if (activeSub->windowType() == "PositionWindow") {
        PositionWindow *pw =
            qobject_cast<PositionWindow *>(activeSub->contentWidget());
        if (pw) {
          WindowContext ctx = pw->getSelectedContext();
          if (ctx.isValid()) {
            sellWindow = new SellWindow(ctx, window);
          }
        }
      }
      // Check for Option Chain Window
      else if (activeSub->windowType() == "OptionChain") {
        OptionChainWindow *oc =
            qobject_cast<OptionChainWindow *>(activeSub->contentWidget());
        if (oc) {
          WindowContext ctx = oc->getSelectedContext();
          if (ctx.isValid()) {
            sellWindow = new SellWindow(ctx, window);
          }
        }
      }
    }
    if (!sellWindow)
      sellWindow = new SellWindow(window);
  }

  window->setContentWidget(sellWindow);
  window->resize(1220, 200);
  connectWindowSignals(window);
  m_mdiArea->addWindow(window);

  // Restore last saved position (shared between Buy and Sell windows)
  QSettings settings("TradingCompany", "TradingTerminal");
  if (settings.contains("orderwindow/last_x") &&
      settings.contains("orderwindow/last_y")) {
    int x = settings.value("orderwindow/last_x").toInt();
    int y = settings.value("orderwindow/last_y").toInt();
    window->move(x, y);
  } else {
    // Default position: bottom-right
    QSize mdiSize = m_mdiArea->size();
    int x = mdiSize.width() - 1220 - 20;
    int y = mdiSize.height() - 200 - 20;
    window->move(x, y);
  }

  // Save position when window is moved (connection auto-disconnects when window
  // is destroyed)
  connect(
      window, &CustomMDISubWindow::windowMoved, window,
      [](const QPoint &pos) {
        QSettings s("TradingCompany", "TradingTerminal");
        s.setValue("orderwindow/last_x", pos.x());
        s.setValue("orderwindow/last_y", pos.y());
      },
      Qt::UniqueConnection);

  window->activateWindow();
  return window;
}

CustomMDISubWindow *MainWindow::createSnapQuoteWindow() {
  qDebug() << "[PERF] [CTRL+Q_PRESS] START Time:"
           << QDateTime::currentMSecsSinceEpoch();

  // ⭐ TRY CACHE FIRST (~10-20ms if hit!)
  MarketWatchWindow *activeMarketWatch = getActiveMarketWatch();
  WindowContext context;

  if (activeMarketWatch && activeMarketWatch->hasValidSelection()) {
    context = activeMarketWatch->getSelectedContractContext();
  } else {
    // Check active OptionChain/Position window for context
    CustomMDISubWindow *activeSub =
        m_mdiArea ? m_mdiArea->activeWindow() : nullptr;
    if (activeSub && activeSub->windowType() == "OptionChain") {
      OptionChainWindow *oc =
          qobject_cast<OptionChainWindow *>(activeSub->contentWidget());
      if (oc)
        context = oc->getSelectedContext();
    }
  }

  // ⭐ CHECK CACHE
  CustomMDISubWindow *cached =
      WindowCacheManager::instance().showSnapQuoteWindow(
          context.isValid() ? &context : nullptr);
  if (cached) {
    qDebug() << "[PERF] ⚡ Cache HIT! Time:"
             << QDateTime::currentMSecsSinceEpoch() << "(~10-20ms)";
    return cached; // INSTANT! ⚡
  }

  qDebug() << "[PERF] Cache MISS - Creating new window (~370-1500ms)";

  // Fallback: Create new window (first time only)
  // Count existing VISIBLE SnapQuote windows (exclude off-screen cached ones)
  int count = 0;
  QList<CustomMDISubWindow *> allWindows = m_mdiArea->windowList();
  for (auto win : allWindows) {
    if (win->windowType() == "SnapQuote" &&
        win->geometry().x() >= WindowConstants::VISIBLE_THRESHOLD_X) {
      qDebug() << "[MainWindow] Visible SnapQuote:" << win->title();
      count++;
    }
  }

  qDebug() << "[MainWindow] Visible Snap Quote windows:" << count;

  if (count >= WindowCacheManager::CACHED_SNAPQUOTE_COUNT) {
    if (m_statusBar)
      m_statusBar->showMessage(
          QString("Maximum %1 Snap Quote windows allowed")
              .arg(WindowCacheManager::CACHED_SNAPQUOTE_COUNT),
          3000);
    return nullptr;
  }

  // Determine a unique index (1, 2, or 3)
  QSet<int> used;
  for (auto win : allWindows) {
    if (win->windowType() == "SnapQuote") {
      QString t = win->title();
      // Expecting "Snap Quote #"
      if (t.startsWith("Snap Quote ")) {
        used.insert(t.mid(11).toInt());
      } else if (t == "Snap Quote") {
        used.insert(0); // Old title format
      }
    }
  }
  int idx = 1;
  while (used.contains(idx))
    idx++;

  QString title = QString("Snap Quote %1").arg(idx);
  qDebug() << "[MainWindow] Creating new Snap Quote window with title:"
           << title;

  CustomMDISubWindow *window = new CustomMDISubWindow(title, m_mdiArea);
  window->setWindowType("SnapQuote");

  // Reuse activeMarketWatch from cache check section above
  SnapQuoteWindow *snapWindow = nullptr;

  if (activeMarketWatch && activeMarketWatch->hasValidSelection()) {
    WindowContext context = activeMarketWatch->getSelectedContractContext();
    if (context.isValid()) {
      snapWindow = new SnapQuoteWindow(context, window);
    }
  } else {
    CustomMDISubWindow *activeSub =
        m_mdiArea ? m_mdiArea->activeWindow() : nullptr;
    if (activeSub && activeSub->windowType() == "OptionChain") {
      OptionChainWindow *oc =
          qobject_cast<OptionChainWindow *>(activeSub->contentWidget());
      if (oc) {
        WindowContext ctx = oc->getSelectedContext();
        if (ctx.isValid()) {
          snapWindow = new SnapQuoteWindow(ctx, window);
        }
      }
    }
  }
  if (!snapWindow)
    snapWindow = new SnapQuoteWindow(window);

  if (m_xtsMarketDataClient) {
    snapWindow->setXTSClient(m_xtsMarketDataClient);
    if (snapWindow->getContext().isValid()) {
      snapWindow->fetchQuote();
    }
  }

  // Connect to UDP broadcast service for real-time tick updates
  connect(&UdpBroadcastService::instance(),
          &UdpBroadcastService::udpTickReceived, snapWindow,
          &SnapQuoteWindow::onTickUpdate);

  window->setContentWidget(snapWindow);
  window->resize(860, 300);
  connectWindowSignals(window);

  // ⭐ Mark cached window for reset when closed
  connect(window, &CustomMDISubWindow::closeRequested, this, [this, window]() {
    // NOTE: This is legacy code path - cached windows handle this differently
    // Pass 0 as a placeholder (this window is not part of the cache pool)
    WindowCacheManager::instance().markSnapQuoteWindowClosed(0);
    qDebug() << "[MainWindow] SnapQuote window closed (legacy path), marked "
                "for reset";
  });

  m_mdiArea->addWindow(window);
  window->activateWindow();

  qDebug() << "[PERF] SnapQuote window created. Time:"
           << QDateTime::currentMSecsSinceEpoch();
  return window;
}

CustomMDISubWindow *MainWindow::createOptionChainWindow() {
  CustomMDISubWindow *window =
      new CustomMDISubWindow("Option Chain", m_mdiArea);
  window->setWindowType("OptionChain");

  OptionChainWindow *optionWindow = new OptionChainWindow(window);
  // Logic to set symbol/expiry from context is simplified here

  window->setContentWidget(optionWindow);
  window->resize(1600, 800);
  connectWindowSignals(window);
  m_mdiArea->addWindow(window);
  window->activateWindow();
  return window;
}

CustomMDISubWindow *MainWindow::createATMWatchWindow() {
  CustomMDISubWindow *window = new CustomMDISubWindow("ATM Watch", m_mdiArea);
  window->setWindowType("ATMWatch");

  ATMWatchWindow *atmWindow = new ATMWatchWindow(window);

  window->setContentWidget(atmWindow);
  window->resize(1200, 600);
  connectWindowSignals(window);
  m_mdiArea->addWindow(window);
  window->activateWindow();
  return window;
}

CustomMDISubWindow *MainWindow::createOrderBookWindow() {
  if (countWindowsOfType("OrderBook") >= 5)
    return nullptr;
  CustomMDISubWindow *window = new CustomMDISubWindow("Order Book", m_mdiArea);
  window->setWindowType("OrderBook");
  OrderBookWindow *ob = new OrderBookWindow(m_tradingDataService, window);

  // Connect order modification signal - route to appropriate Buy/Sell window
  connect(ob, &OrderBookWindow::modifyOrderRequested, this,
          [this](const XTS::Order &order) {
            if (order.orderSide.toUpper() == "BUY") {
              openBuyWindowForModification(order);
            } else {
              openSellWindowForModification(order);
            }
          });

  // Connect Batch Modification signal
  connect(ob, &OrderBookWindow::batchModifyRequested, this,
          [this](const QVector<XTS::Order> &orders) {
            if (orders.isEmpty())
              return;
            if (orders.first().orderSide.toUpper() == "BUY") {
              openBatchBuyWindowForModification(orders);
            } else {
              openBatchSellWindowForModification(orders);
            }
          });

  // Connect order cancellation signal
  connect(ob, &OrderBookWindow::cancelOrderRequested, this,
          &MainWindow::cancelOrder);

  window->setContentWidget(ob);
  window->resize(1400, 600);
  connectWindowSignals(window);
  m_mdiArea->addWindow(window);
  window->activateWindow();
  return window;
}

CustomMDISubWindow *MainWindow::createTradeBookWindow() {
  if (countWindowsOfType("TradeBook") >= 5)
    return nullptr;
  CustomMDISubWindow *window = new CustomMDISubWindow("Trade Book", m_mdiArea);
  window->setWindowType("TradeBook");
  TradeBookWindow *tb = new TradeBookWindow(m_tradingDataService, window);
  window->setContentWidget(tb);
  window->resize(1400, 600);
  connectWindowSignals(window);
  m_mdiArea->addWindow(window);
  window->activateWindow();
  return window;
}

CustomMDISubWindow *MainWindow::createPositionWindow() {
  if (countWindowsOfType("PositionWindow") >= 5)
    return nullptr;
  CustomMDISubWindow *window =
      new CustomMDISubWindow("Integrated Net Position", m_mdiArea);
  window->setWindowType("PositionWindow");
  PositionWindow *pw = new PositionWindow(m_tradingDataService, window);
  window->setContentWidget(pw);
  window->resize(1000, 500);
  connectWindowSignals(window);
  m_mdiArea->addWindow(window);
  window->activateWindow();
  return window;
}

void MainWindow::onAddToWatchRequested(const InstrumentData &instrument) {
  // Find active market watch window
  CustomMDISubWindow *activeWindow = m_mdiArea->activeWindow();
  MarketWatchWindow *marketWatch = nullptr;

  if (activeWindow && activeWindow->windowType() == "MarketWatch") {
    marketWatch =
        qobject_cast<MarketWatchWindow *>(activeWindow->contentWidget());
  } else {
    for (auto win : m_mdiArea->windowList()) {
      if (win->windowType() == "MarketWatch") {
        marketWatch = qobject_cast<MarketWatchWindow *>(win->contentWidget());
        if (marketWatch) {
          m_mdiArea->activateWindow(win);
          break;
        }
      }
    }
  }

  if (!marketWatch) {
    createMarketWatch();
    activeWindow = m_mdiArea->activeWindow();
    if (activeWindow) {
      marketWatch =
          qobject_cast<MarketWatchWindow *>(activeWindow->contentWidget());
      if (marketWatch)
        marketWatch->setupZeroCopyMode();
    }
  }

  if (marketWatch) {
    // Simple add scrip for now (snippet had complex mapping, simplifying for
    // compilation safety)
    marketWatch->addScrip(
        instrument.symbol,
        RepositoryManager::getExchangeSegmentName(instrument.exchangeSegment),
        (int)instrument.exchangeInstrumentID);

    // Apply cached price if available from Distributed PriceStore
    auto unifiedState =
        MarketData::PriceStoreGateway::instance().getUnifiedState(
            instrument.exchangeSegment, instrument.exchangeInstrumentID);
    if (unifiedState) {
      double lastTradedPrice = unifiedState->ltp;
      double closePrice = unifiedState->close;
      if (closePrice <= 0) {
        const ContractData *contract =
            RepositoryManager::getInstance()->getContractByToken(
                RepositoryManager::getExchangeSegmentName(
                    instrument.exchangeSegment),
                instrument.exchangeInstrumentID);
        if (contract)
          closePrice = contract->prevClose;
      }

      if (closePrice > 0) {
        double change = lastTradedPrice - closePrice;
        double pct = (change / closePrice) * 100;
        marketWatch->updatePrice(instrument.exchangeInstrumentID,
                                 lastTradedPrice, change, pct);
      } else {
        marketWatch->updatePrice(instrument.exchangeInstrumentID,
                                 lastTradedPrice, 0, 0);
      }
    }
  }
}

void MainWindow::onRestoreWindowRequested(
    const QString &type, const QString &title, const QRect &geometry,
    bool isMinimized, bool isMaximized, bool isPinned,
    const QString &workspaceName, int index) {
  qDebug() << "[MainWindow] Restoring window:" << type << title
           << "Index:" << index;

  CustomMDISubWindow *window = nullptr;

  // Create the window based on type
  if (type == "MarketWatch") {
    window = createMarketWatch();
  } else if (type == "BuyWindow") {
    window = createBuyWindow();
  } else if (type == "SellWindow") {
    window = createSellWindow();
  } else if (type == "SnapQuote" || type.startsWith("SnapQuote")) {
    window = createSnapQuoteWindow();
  } else if (type == "OptionChain") {
    window = createOptionChainWindow();
  } else if (type == "OrderBook") {
    window = createOrderBookWindow();
  } else if (type == "TradeBook") {
    window = createTradeBookWindow();
  } else if (type == "PositionWindow") {
    window = createPositionWindow();
  } else {
    qWarning() << "[MainWindow] Unknown window type for restore:" << type;
    return;
  }

  if (!window) {
    qWarning() << "[MainWindow] Failed to find restored window for:" << type;
    return;
  }

  // Apply saved geometry
  if (!isMaximized && !isMinimized) {
    window->setGeometry(geometry);
  }

  // Apply state
  if (isMaximized) {
    window->maximize();
  } else if (isMinimized) {
    m_mdiArea->minimizeWindow(window);
  }

  window->setPinned(isPinned);

  // ⚡ CRITICAL: Only set title if it matches the expected pattern for the
  // window type This prevents cached windows from getting incorrect titles
  // during workspace restore
  bool shouldSetTitle = false;
  if (!title.isEmpty() && window->title() != title) {
    if ((type == "SnapQuote" || type.startsWith("SnapQuote")) &&
        title.startsWith("Snap Quote")) {
      shouldSetTitle = true;
    } else if (type == "BuyWindow" && title.contains("Buy")) {
      shouldSetTitle = true;
    } else if (type == "SellWindow" && title.contains("Sell")) {
      shouldSetTitle = true;
    } else if (type != "SnapQuote" && !type.startsWith("SnapQuote") &&
               type != "BuyWindow" && type != "SellWindow") {
      // For non-cached window types, always apply the saved title
      shouldSetTitle = true;
    }

    if (shouldSetTitle) {
      window->setTitle(title);
    } else {
      qDebug() << "[MainWindow] Skipping mismatched title:" << title
               << "for window type:" << type;
    }
  }

  // Restore detailed state (Script lists, column profiles, etc.)
  if (!workspaceName.isEmpty() && index >= 0 && window->contentWidget()) {
    QSettings settings("TradingCompany", "TradingTerminal");
    settings.beginGroup(
        QString("workspaces/%1/window_%2").arg(workspaceName).arg(index));

    if (type == "MarketWatch") {
      MarketWatchWindow *mw =
          qobject_cast<MarketWatchWindow *>(window->contentWidget());
      if (mw) {
        mw->setupZeroCopyMode();
        mw->restoreState(settings);
      }
    } else {
      // Try BaseBookWindow for order/trade/position windows
      BaseBookWindow *book =
          qobject_cast<BaseBookWindow *>(window->contentWidget());
      if (book)
        book->restoreState(settings);
    }

    settings.endGroup();
  }
}
