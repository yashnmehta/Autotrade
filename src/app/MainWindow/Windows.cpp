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
#include "ui/GlobalSearchWidget.h"
#include "ui/OptionChainWindow.h"
#include "ui/StrategyManagerWindow.h"
#ifdef HAVE_TRADINGVIEW
#include "ui/TradingViewChartWidget.h"
#endif
#ifdef HAVE_QTCHARTS
#include "ui/IndicatorChartWidget.h"
#endif
#include "views/BuyWindow.h"
#include "views/CustomizeDialog.h"
#include "views/MarketMovementWindow.h"
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


CustomMDISubWindow *MainWindow::createGlobalSearchWindow() {
  // Check if search window already exists
  for (auto win : m_mdiArea->windowList()) {
    if (win->windowType() == "GlobalSearch") {
      win->activateWindow();
      return win;
    }
  }

  CustomMDISubWindow *window =
      new CustomMDISubWindow("Script Search", m_mdiArea);
  window->setWindowType("GlobalSearch");

  auto *searchWidget = new GlobalSearchWidget(window);

  // When a script is selected from search...
  connect(
      searchWidget, &GlobalSearchWidget::scripSelected, this,
      [this](const ContractData &contract) {
        qDebug() << "[MainWindow] Script selected from Search:"
                 << contract.displayName;

#ifdef HAVE_TRADINGVIEW
        // 1. Find active chart
        TradingViewChartWidget *activeChart = nullptr;
        CustomMDISubWindow *activeSub = m_mdiArea->activeWindow();
        if (activeSub && activeSub->windowType() == "ChartWindow") {
          activeChart = qobject_cast<TradingViewChartWidget *>(
              activeSub->contentWidget());
        }

        if (!activeChart) {
          // Fallback to first chart found
          for (auto win : m_mdiArea->windowList()) {
            if (win->windowType() == "ChartWindow") {
              activeChart =
                  qobject_cast<TradingViewChartWidget *>(win->contentWidget());
              break;
            }
          }
        }

        if (activeChart) {
          int segId = 2; // Default to NSEFO
          if (contract.exchangeInstrumentID >= 11000000) {
            segId = (contract.strikePrice > 0 || contract.instrumentType == 1)
                        ? 12
                        : 11;
          } else {
            segId = (contract.strikePrice > 0 || contract.instrumentType == 1)
                        ? 2
                        : 1;
          }

          qDebug() << "[MainWindow] Updating chart with token:"
                   << contract.exchangeInstrumentID << "segment:" << segId;
          activeChart->loadSymbol(contract.name, segId,
                                  contract.exchangeInstrumentID);
        } else
#endif
        {
          qDebug() << "[MainWindow] No active chart. Adding to Watchlist.";
          // Fallback: Add to MarketWatch
          InstrumentData data;
          data.exchangeInstrumentID = contract.exchangeInstrumentID;
          data.name = contract.displayName;
          data.symbol = contract.name;
          data.series = contract.series;
          data.instrumentType =
              contract.instrumentType == 1
                  ? "Futures"
                  : (contract.instrumentType == 2 ? "Options" : "Cash");
          data.expiryDate = contract.expiryDate;
          data.strikePrice = contract.strikePrice;
          data.optionType = contract.optionType;
          data.exchangeSegment =
              (contract.exchangeInstrumentID >= 11000000)
                  ? ((contract.strikePrice > 0 || contract.instrumentType == 1)
                         ? 12
                         : 11)
                  : ((contract.strikePrice > 0 || contract.instrumentType == 1)
                         ? 2
                         : 1);

          onAddToWatchRequested(data);
        }
      });

  window->setContentWidget(searchWidget);
  window->resize(800, 500);
  connectWindowSignals(window);
  m_mdiArea->addWindow(window);
  window->show();
  window->activateWindow();

  return window;
}

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

  // Auto-connect ATMWatch signals
  if (window->contentWidget() && window->windowType() == "ATMWatch") {
    ATMWatchWindow *atm =
        qobject_cast<ATMWatchWindow *>(window->contentWidget());
    if (atm) {
      connect(atm, &ATMWatchWindow::openOptionChainRequested, this,
              &MainWindow::createOptionChainWindowForSymbol);

      // Route Buy/Sell/SnapQuote requests through MainWindow for proper
      // focus management (ATMWatch is set as initiating window)
      connect(atm, &ATMWatchWindow::buyRequested, this,
              [this, atm](const WindowContext &ctx) {
                createBuyWindowWithContext(ctx, atm);
              });
      connect(atm, &ATMWatchWindow::sellRequested, this,
              [this, atm](const WindowContext &ctx) {
                createSellWindowWithContext(ctx, atm);
              });
      connect(atm, &ATMWatchWindow::snapQuoteRequested, this,
              [this, atm](const WindowContext &ctx) {
                createSnapQuoteWindowWithContext(ctx, atm);
              });
    }
  }

  // Auto-connect OptionChainWindow signals (Buy/Sell from F1/F2)
  if (window->contentWidget() && window->windowType() == "OptionChain") {
    OptionChainWindow *oc =
        qobject_cast<OptionChainWindow *>(window->contentWidget());
    if (oc) {
      connect(oc, &OptionChainWindow::buyRequested, this,
              [this, oc](const WindowContext &ctx) {
                createBuyWindowWithContext(ctx, oc);
              });
      connect(oc, &OptionChainWindow::sellRequested, this,
              [this, oc](const WindowContext &ctx) {
                createSellWindowWithContext(ctx, oc);
              });
    }
  }

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

WindowContext MainWindow::getBestWindowContext() const {
  WindowContext context;
  CustomMDISubWindow *activeSub =
      m_mdiArea ? m_mdiArea->activeWindow() : nullptr;

  // 1. Check if ACTIVE window is a Provider
  if (activeSub) {
    QString type = activeSub->windowType();
    if (type == "ATMWatch") {
      ATMWatchWindow *atm =
          qobject_cast<ATMWatchWindow *>(activeSub->contentWidget());
      if (atm)
        context = atm->getCurrentContext();
    } else if (type == "OptionChain") {
      OptionChainWindow *oc =
          qobject_cast<OptionChainWindow *>(activeSub->contentWidget());
      if (oc)
        context = oc->getSelectedContext();
    } else if (type == "MarketWatch") {
      MarketWatchWindow *mw =
          qobject_cast<MarketWatchWindow *>(activeSub->contentWidget());
      if (mw)
        context = mw->getSelectedContractContext();
    } else if (type == "PositionWindow") {
      PositionWindow *pw =
          qobject_cast<PositionWindow *>(activeSub->contentWidget());
      if (pw)
        context = pw->getSelectedContext();
    } else if (type == "SnapQuote") {
      SnapQuoteWindow *sq =
          qobject_cast<SnapQuoteWindow *>(activeSub->contentWidget());
      if (sq)
        context = sq->getContext();
    } else if (type == "BuyWindow" || type == "SellWindow") {
      // ⚡ CRITICAL FIX: If an order window is active, take context from it!
      // This allows F1 -> F2 (Buy -> Sell) transition to work perfectly.
      BaseOrderWindow *orderWin =
          qobject_cast<BaseOrderWindow *>(activeSub->contentWidget());
      if (orderWin)
        context = orderWin->getContext();
    }
  }

  // 2. If no context from active window, search for ATM Watch anywhere
  if (!context.isValid() && m_mdiArea) {
    for (auto win : m_mdiArea->windowList()) {
      if (win->windowType() == "ATMWatch") {
        ATMWatchWindow *atm =
            qobject_cast<ATMWatchWindow *>(win->contentWidget());
        if (atm) {
          context = atm->getCurrentContext();
          if (context.isValid())
            break;
        }
      }
    }
  }

  // 3. Fallback to Option Chain anywhere
  if (!context.isValid() && m_mdiArea) {
    for (auto win : m_mdiArea->windowList()) {
      if (win->windowType() == "OptionChain") {
        OptionChainWindow *oc =
            qobject_cast<OptionChainWindow *>(win->contentWidget());
        if (oc) {
          context = oc->getSelectedContext();
          if (context.isValid())
            break;
        }
      }
    }
  }

  // 4. Final Fallback: Active or First Market Watch
  if (!context.isValid()) {
    MarketWatchWindow *mw = getActiveMarketWatch();
    if (mw)
      context = mw->getSelectedContractContext();
  }

  return context;
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

  // Connect window activation to restore focus with a delay
  connect(window, &CustomMDISubWindow::windowActivated, marketWatch,
          [marketWatch]() {
            qDebug() << "[MainWindow] Market Watch window activated, "
                        "scheduling focus restore";
            // Use a timer to ensure the window is fully activated and ready
            QTimer::singleShot(100, marketWatch, [marketWatch]() {
              if (marketWatch->getModel() &&
                  marketWatch->getModel()->rowCount() > 0) {
                marketWatch->restoreFocusedRow();
              }
            });
          });

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

#ifdef HAVE_TRADINGVIEW
CustomMDISubWindow *MainWindow::createChartWindow() {
  static int counter = 1;

  CustomMDISubWindow *window =
      new CustomMDISubWindow(QString("Chart %1").arg(counter++), m_mdiArea);
  window->setWindowType("ChartWindow");

  TradingViewChartWidget *chartWidget = new TradingViewChartWidget(window);
  chartWidget->setXTSMarketDataClient(m_xtsMarketDataClient);
  chartWidget->setRepositoryManager(RepositoryManager::getInstance());
  window->setContentWidget(chartWidget);
  window->resize(1200, 700);

  connectWindowSignals(window);

  // Connect chart order placement to MainWindow::placeOrder
  connect(chartWidget, &TradingViewChartWidget::orderRequestedFromChart, this,
          &MainWindow::placeOrder);

  m_mdiArea->setUpdatesEnabled(false);
  m_mdiArea->addWindow(window);
  window->setFocus();
  window->raise();
  window->activateWindow();
  m_mdiArea->setUpdatesEnabled(true);

  // Load a default symbol if we have context
  WindowContext context = getBestWindowContext();
  if (context.isValid()) {
    // Convert segment string to int (E=1, D=2, F=2, etc.)
    int segmentInt = 1; // Default to NSE CM
    if (context.segment == "F" || context.segment == "2") {
      segmentInt = 2; // NSE FO
    }
    // Pass token from context
    chartWidget->loadSymbol(context.symbol, segmentInt, context.token,
                            "5"); // 5-minute default
  }

  qDebug() << "[MainWindow] Created Chart Window";
  return window;
}
#else
CustomMDISubWindow *MainWindow::createChartWindow() {
  qWarning() << "[MainWindow] TradingView not available. Cannot create Chart.";
  return nullptr;
}
#endif

CustomMDISubWindow *MainWindow::createIndicatorChartWindow() {
#ifdef HAVE_QTCHARTS
  static int counter = 1;

  CustomMDISubWindow *window = new CustomMDISubWindow(
      QString("Indicators %1").arg(counter++), m_mdiArea);
  window->setWindowType("IndicatorChart");

  IndicatorChartWidget *chartWidget = new IndicatorChartWidget(window);
  window->setContentWidget(chartWidget);
  window->resize(1400, 900);

  connectWindowSignals(window);

  // Inject dependencies
  chartWidget->setXTSMarketDataClient(m_xtsMarketDataClient);
  chartWidget->setRepositoryManager(RepositoryManager::getInstance());

  // Connect symbol search signal
  connect(
      chartWidget, &IndicatorChartWidget::symbolChangeRequested, this,
      [this, chartWidget](const QString &symbol) {
        qDebug() << "[MainWindow] Symbol search requested:" << symbol;

        RepositoryManager *repo = RepositoryManager::getInstance();
        if (!repo) {
          qWarning() << "[MainWindow] Repository manager not available";
          return;
        }

        // Detect and handle "EQ" suffix for forced cash segment
        bool forceCash = false;
        QString searchQuery = symbol;
        if (searchQuery.endsWith(" EQ", Qt::CaseInsensitive)) {
          forceCash = true;
          searchQuery = searchQuery.left(searchQuery.length() - 3).trimmed();
          qDebug() << "[MainWindow] Forced cash segment for:" << searchQuery;
        }

        // Search for the symbol using global search
        QVector<ContractData> results =
            repo->searchScripsGlobal(searchQuery, "", "", "", 20);

        qDebug() << "[MainWindow] Search found" << results.size() << "results";

        if (results.isEmpty()) {
          qWarning() << "[MainWindow] No results found for:" << searchQuery;
          return;
        }

        // Filter and prioritize results
        ContractData bestMatch;
        bool foundMatch = false;

        for (const auto &contract : results) {
          // Skip indices (type 10) unless explicitly searched
          if (contract.instrumentType == 10)
            continue;

          // Determine segment for this contract
          int segment = (contract.exchangeInstrumentID >= 11000000) ? 11 : 1;
          if (contract.strikePrice > 0 || contract.instrumentType == 1) {
            segment = (contract.exchangeInstrumentID >= 11000000) ? 12 : 2;
          }

          if (forceCash) {
            // Strictly prefer segment 1 (NSE CM) or 11 (BSE CM)
            if (segment == 1 || segment == 11) {
              bestMatch = contract;
              foundMatch = true;
              break; // Exact match found
            }
            continue;
          }

          // General search priority:
          // 1. NSE CM Stock (instrumentType 0, segment 1)
          // 2. Any Stock (instrumentType 0)
          // 3. First available tradable instrument
          if (!foundMatch) {
            bestMatch = contract;
            foundMatch = true;
          }

          if (contract.instrumentType == 0 && segment == 1) {
            bestMatch = contract;
            break; // Found primary stock, stop
          }
        }

        if (!foundMatch) {
          qWarning() << "[MainWindow] No suitable tradable instruments found";
          return;
        }

        // Re-determine final segment for the best match
        int segmentInt = (bestMatch.exchangeInstrumentID >= 11000000) ? 11 : 1;
        if (bestMatch.strikePrice > 0 || bestMatch.instrumentType == 1) {
          segmentInt = (bestMatch.exchangeInstrumentID >= 11000000) ? 12 : 2;
        }

        qDebug() << "[MainWindow] Final Match:" << bestMatch.name
                 << "Token:" << bestMatch.exchangeInstrumentID
                 << "Segment:" << segmentInt;

        // Load the symbol (this will trigger OHLC fetch)
        chartWidget->loadSymbol(bestMatch.name, segmentInt,
                                bestMatch.exchangeInstrumentID);
      });

  m_mdiArea->setUpdatesEnabled(false);
  m_mdiArea->addWindow(window);
  window->setFocus();
  window->raise();
  window->activateWindow();
  m_mdiArea->setUpdatesEnabled(true);

  // Load a default symbol if we have context
  WindowContext context = getBestWindowContext();
  if (context.isValid()) {
    // Don't load indices by default (they don't have OHLC data)
    // Indices have tokens between 26000-36000 typically
    bool isIndex = (context.token >= 26000 && context.token <= 36000);

    if (!isIndex) {
      // Convert segment string to int (E=1, D=2, F=2, etc.)
      int segmentInt = 1; // Default to NSE CM
      if (context.segment == "F" || context.segment == "2") {
        segmentInt = 2; // NSE FO
      }

      chartWidget->loadSymbol(context.symbol, segmentInt, context.token);

      qDebug() << "[MainWindow] Loaded symbol into indicator chart:"
               << context.symbol << "segment:" << segmentInt;
    } else {
      qDebug() << "[MainWindow] Skipping index" << context.symbol
               << "(indices don't have OHLC candle data)";
      qDebug() << "[MainWindow] Use Search to load a stock: RELIANCE, TCS, "
                  "INFY, HDFCBANK";
    }
  }

  qDebug() << "[MainWindow] Created Indicator Chart Window";

  return window;
#else
  qWarning() << "[MainWindow] Qt Charts not available. Cannot create "
                "Indicator Chart.";
  return nullptr;
#endif
}

CustomMDISubWindow *MainWindow::createBuyWindow() {
  static int f1Counter = 0;
  f1Counter++;
  qDebug() << "[PERF] [F1_PRESS] #" << f1Counter
           << " START Time:" << QDateTime::currentMSecsSinceEpoch();

  // Try cache first for fast opening (~10ms instead of ~400ms)
  MarketWatchWindow *activeMarketWatch = getActiveMarketWatch();

  // Store the focused row before opening Buy window
  if (activeMarketWatch) {
    activeMarketWatch->storeFocusedRow();
  }

  // Get currently active content widget for focus restoration on close
  QWidget *currentlyActive = nullptr;
  if (m_mdiArea && m_mdiArea->activeWindow()) {
    currentlyActive = m_mdiArea->activeWindow()->contentWidget();
  }

  WindowContext context = getBestWindowContext();

  // Pass currently active widget as initiating window for focus restoration
  QWidget *initiating = currentlyActive ? currentlyActive : activeMarketWatch;
  qWarning() << "[MainWindow][FOCUS-DEBUG] createBuyWindow: initiating ="
             << (initiating ? initiating->metaObject()->className()
                            : "nullptr");
  CustomMDISubWindow *cached = WindowCacheManager::instance().showBuyWindow(
      context.isValid() ? &context : nullptr, initiating);
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

  if (context.isValid()) {
    buyWindow = new BuyWindow(context, window);
  } else {
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

  // Store the focused row before opening Sell window
  if (activeMarketWatch) {
    activeMarketWatch->storeFocusedRow();
  }

  // Get currently active content widget for focus restoration on close
  QWidget *currentlyActive = nullptr;
  if (m_mdiArea && m_mdiArea->activeWindow()) {
    currentlyActive = m_mdiArea->activeWindow()->contentWidget();
  }

  WindowContext context = getBestWindowContext();

  // Pass currently active widget as initiating window for focus restoration
  QWidget *initiating = currentlyActive ? currentlyActive : activeMarketWatch;
  CustomMDISubWindow *cached = WindowCacheManager::instance().showSellWindow(
      context.isValid() ? &context : nullptr, initiating);
  if (cached) {
    return cached; // Cache handled it
  }

  // Fallback to normal creation if cache not available
  // Enforce single order window limit: Only one Buy OR Sell window at a time
  closeWindowsByType("BuyWindow");
  closeWindowsByType("SellWindow");

  CustomMDISubWindow *window = new CustomMDISubWindow("Sell Order", m_mdiArea);
  window->setWindowType("SellWindow");

  SellWindow *sellWindow = nullptr;

  if (context.isValid()) {
    sellWindow = new SellWindow(context, window);
  } else {
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
        WindowCacheManager::instance().saveOrderWindowPosition(pos);
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

  // Store the focused row before opening Snap Quote window
  if (activeMarketWatch) {
    activeMarketWatch->storeFocusedRow();
  }

  // Get currently active content widget for focus restoration on close
  QWidget *currentlyActive = nullptr;
  if (m_mdiArea && m_mdiArea->activeWindow()) {
    currentlyActive = m_mdiArea->activeWindow()->contentWidget();
  }

  WindowContext context = getBestWindowContext();

  // ⭐ CHECK CACHE — pass currently active widget for focus restoration, not
  // just MW
  QWidget *initiating = currentlyActive ? currentlyActive : activeMarketWatch;
  CustomMDISubWindow *cached =
      WindowCacheManager::instance().showSnapQuoteWindow(
          context.isValid() ? &context : nullptr, initiating);
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
    } else if (activeSub && activeSub->windowType() == "ATMWatch") {
      ATMWatchWindow *atm =
          qobject_cast<ATMWatchWindow *>(activeSub->contentWidget());
      if (atm) {
        WindowContext ctx = atm->getCurrentContext();
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
  // Set initiating window for focus restoration (legacy non-cached path)
  if (currentlyActive) {
    window->setInitiatingWindow(currentlyActive);
  } else if (activeMarketWatch) {
    window->setInitiatingWindow(activeMarketWatch);
  }
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
  // Get currently active content widget for focus restoration on close
  QWidget *currentlyActive = nullptr;
  if (m_mdiArea && m_mdiArea->activeWindow()) {
    currentlyActive = m_mdiArea->activeWindow()->contentWidget();
  }

  CustomMDISubWindow *window =
      new CustomMDISubWindow("Option Chain", m_mdiArea);
  window->setWindowType("OptionChain");

  OptionChainWindow *optionWindow = new OptionChainWindow(window);
  // Logic to set symbol/expiry from context is simplified here

  window->setContentWidget(optionWindow);
  if (currentlyActive) {
    window->setInitiatingWindow(currentlyActive);
  }
  window->resize(1600, 800);
  connectWindowSignals(window);
  m_mdiArea->addWindow(window);
  window->activateWindow();
  return window;
}

CustomMDISubWindow *
MainWindow::createOptionChainWindowForSymbol(const QString &symbol,
                                             const QString &expiry) {
  CustomMDISubWindow *window = createOptionChainWindow();
  if (!window)
    return nullptr;
  OptionChainWindow *oc =
      qobject_cast<OptionChainWindow *>(window->contentWidget());
  if (oc) {
    oc->setSymbol(symbol, expiry);
  }
  return window;
}

CustomMDISubWindow *MainWindow::createATMWatchWindow() {
  // Get currently active content widget for focus restoration on close
  QWidget *currentlyActive = nullptr;
  if (m_mdiArea && m_mdiArea->activeWindow()) {
    currentlyActive = m_mdiArea->activeWindow()->contentWidget();
  }

  CustomMDISubWindow *window = new CustomMDISubWindow("ATM Watch", m_mdiArea);
  window->setWindowType("ATMWatch");

  ATMWatchWindow *atmWindow = new ATMWatchWindow(window);

  window->setContentWidget(atmWindow);
  if (currentlyActive) {
    window->setInitiatingWindow(currentlyActive);
  }
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

CustomMDISubWindow *MainWindow::createStrategyManagerWindow() {
  CustomMDISubWindow *window =
      new CustomMDISubWindow("Strategy Manager", m_mdiArea);
  window->setWindowType("StrategyManager");

  StrategyManagerWindow *strategyWindow = new StrategyManagerWindow(window);

  window->setContentWidget(strategyWindow);
  window->resize(1000, 600);
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

    // Apply cached price if available from Distributed PriceStore (thread-safe)
    auto unifiedState =
        MarketData::PriceStoreGateway::instance().getUnifiedSnapshot(
            instrument.exchangeSegment, instrument.exchangeInstrumentID);
    if (unifiedState.token != 0) {
      double lastTradedPrice = unifiedState.ltp;
      double closePrice = unifiedState.close;
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

// ─── Context-aware window creation with explicit initiating window ─────────

void MainWindow::createBuyWindowWithContext(const WindowContext &context,
                                            QWidget *initiatingWindow) {
  // Use WindowCacheManager with the explicit initiating window
  CustomMDISubWindow *cached = WindowCacheManager::instance().showBuyWindow(
      context.isValid() ? &context : nullptr, initiatingWindow);
  if (cached) {
    return; // Cache handled it
  }

  // Fallback: create through normal path
  // The context will be picked up from getBestWindowContext() inside
  createBuyWindow();
}

void MainWindow::createSellWindowWithContext(const WindowContext &context,
                                             QWidget *initiatingWindow) {
  CustomMDISubWindow *cached = WindowCacheManager::instance().showSellWindow(
      context.isValid() ? &context : nullptr, initiatingWindow);
  if (cached) {
    return;
  }

  createSellWindow();
}

void MainWindow::createSnapQuoteWindowWithContext(const WindowContext &context,
                                                  QWidget *initiatingWindow) {
  CustomMDISubWindow *cached =
      WindowCacheManager::instance().showSnapQuoteWindow(
          context.isValid() ? &context : nullptr, initiatingWindow);
  if (cached) {
    return;
  }

  // Fallback to normal creation — createSnapQuoteWindow will use
  // getBestWindowContext
  createSnapQuoteWindow();
}

// ─── Widget-aware window creation (called from CustomMDISubWindow F1/F2
// fallback) ───

void MainWindow::createBuyWindowFromWidget(QWidget *initiatingWidget) {
  qWarning()
      << "[MainWindow][FOCUS-DEBUG] createBuyWindowFromWidget: initiating ="
      << (initiatingWidget ? initiatingWidget->metaObject()->className()
                           : "nullptr")
      << (initiatingWidget ? initiatingWidget->objectName() : "");

  WindowContext context = getBestWindowContext();
  CustomMDISubWindow *cached = WindowCacheManager::instance().showBuyWindow(
      context.isValid() ? &context : nullptr, initiatingWidget);
  if (cached) {
    return;
  }
  // Fallback to generic path (no initiating window)
  createBuyWindow();
}

void MainWindow::createSellWindowFromWidget(QWidget *initiatingWidget) {
  qWarning()
      << "[MainWindow][FOCUS-DEBUG] createSellWindowFromWidget: initiating ="
      << (initiatingWidget ? initiatingWidget->metaObject()->className()
                           : "nullptr")
      << (initiatingWidget ? initiatingWidget->objectName() : "");

  WindowContext context = getBestWindowContext();
  CustomMDISubWindow *cached = WindowCacheManager::instance().showSellWindow(
      context.isValid() ? &context : nullptr, initiatingWidget);
  if (cached) {
    return;
  }
  createSellWindow();
}
CustomMDISubWindow *MainWindow::createMarketMovementWindow() {
  WindowContext context = getBestWindowContext();

  // Check if valid option instrument is selected
  if (!context.isValid()) {
    QMessageBox::warning(
        this, "Market Movement",
        "Please select an option instrument from Market Watch first.");
    return nullptr;
  }

  // Verify it's an option (must have strike price and option type)
  if (context.instrumentType != "OPTSTK" &&
      context.instrumentType != "OPTIDX") {
    QMessageBox::warning(
        this, "Market Movement",
        "This window is only for option instruments (OPTSTK).\n"
        "Please select an option from Market Watch.");
    return nullptr;
  }

  // Create unique title with instrument details
  QString title =
      QString("Market Movement - %1 %2 %3 %4")
          .arg(context.symbol)
          .arg(context.expiry)
          .arg(context.strikePrice > 0 ? QString::number(context.strikePrice)
                                       : "")
          .arg(context.optionType);

  CustomMDISubWindow *window = new CustomMDISubWindow(title, m_mdiArea);
  window->setWindowType("MarketMovement");

  MarketMovementWindow *marketMovement =
      new MarketMovementWindow(context, window);

  window->setContentWidget(marketMovement);
  window->resize(1000, 600);

  connectWindowSignals(window);
  m_mdiArea->addWindow(window);
  window->show();

  qDebug() << "[MainWindow] Market Movement window created for"
           << context.symbol;

  return window;
}
