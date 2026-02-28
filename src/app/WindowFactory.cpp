/**
 * @file WindowFactory.cpp
 * @brief Factory for creating and managing MDI child windows
 *
 * Extracted from MainWindow::Windows.cpp during Phase 3.2 refactor.
 * All createXxxWindow() methods live here; MainWindow delegates to this class.
 */

#include "app/WindowFactory.h"
#include "app/MainWindow.h"
#include "app/ScripBar.h"
#include "core/WindowCacheManager.h"
#include "core/WindowConstants.h"
#include "core/widgets/CustomMDIArea.h"
#include "core/widgets/CustomMDISubWindow.h"
#include "data/PriceStoreGateway.h"
#include "repository/RepositoryManager.h"
#include "services/UdpBroadcastService.h"
#include "strategy/manager/StrategyManagerWindow.h"
#include "ui/GlobalSearchWidget.h"
#include "views/ATMWatchWindow.h"
#include "views/BuyWindow.h"
#include "views/CustomizeDialog.h"
#include "views/MarketMovementWindow.h"
#include "views/MarketWatchWindow.h"
#include "views/OptionCalculatorWindow.h"
#include "views/OptionChainWindow.h"
#include "views/OrderBookWindow.h"
#include "views/PositionWindow.h"
#include "views/SellWindow.h"
#include "views/SnapQuoteWindow.h"
#include "views/TradeBookWindow.h"
#ifdef HAVE_TRADINGVIEW
#include "ui/TradingViewChartWidget.h"
#endif
#ifdef HAVE_QTCHARTS
#include "ui/IndicatorChartWidget.h"
#endif

#include <QDateTime>
#include <QDebug>
#include <QElapsedTimer>
#include <QMessageBox>
#include <QSettings>
#include <QStatusBar>
#include <QTimer>

// ═══════════════════════════════════════════════════════════════════════
// Local helper: apply saved geometry or fall back to default size
// ═══════════════════════════════════════════════════════════════════════

/**
 * @brief Restore saved geometry for a window type, or fall back to default.
 *
 * The MDI subwindow geometry is saved as individual x/y/width/height keys
 * under WindowState/<type>/ on close.  This helper reads those keys and
 * applies them.  If no saved geometry exists, the caller-supplied default
 * size is used and the window will be cascade-positioned by addWindow().
 *
 * IMPORTANT: Must be called BEFORE m_mdiArea->addWindow() so that the
 * geometryRestored flag is set and addWindow() won't override the position.
 */
static void applyRestoredGeometryOrDefault(CustomMDISubWindow *window,
                                           const QString &windowType,
                                           int defaultW, int defaultH) {
  QSettings settings("TradingCompany", "TradingTerminal");
  QString prefix = QString("WindowState/%1/").arg(windowType);

  bool hasX = settings.contains(prefix + "x");
  bool hasY = settings.contains(prefix + "y");
  bool hasW = settings.contains(prefix + "width");
  bool hasH = settings.contains(prefix + "height");

  if (hasX && hasY && hasW && hasH) {
    int x = settings.value(prefix + "x").toInt();
    int y = settings.value(prefix + "y").toInt();
    int w = settings.value(prefix + "width").toInt();
    int h = settings.value(prefix + "height").toInt();

    // Sanity-check: width/height must be positive
    if (w > 0 && h > 0) {
      window->setGeometry(x, y, w, h);
      window->setGeometryRestored(true);  // Tell addWindow() not to cascade
      qDebug() << "[WindowFactory] Restored saved geometry for" << windowType
               << "-> (" << x << "," << y << "," << w << "x" << h << ")";
      return;
    }
  }

  // Also try the legacy QByteArray key for backward compatibility
  QByteArray legacyGeom = settings.value(
      QString("WindowState/%1/geometry").arg(windowType)).toByteArray();
  if (!legacyGeom.isEmpty()) {
    window->restoreGeometry(legacyGeom);
    window->setGeometryRestored(true);
    qDebug() << "[WindowFactory] Restored legacy geometry for" << windowType;
    // Migrate: save as individual keys and remove legacy key
    QRect geom = window->geometry();
    settings.setValue(prefix + "x", geom.x());
    settings.setValue(prefix + "y", geom.y());
    settings.setValue(prefix + "width", geom.width());
    settings.setValue(prefix + "height", geom.height());
    settings.remove(QString("WindowState/%1/geometry").arg(windowType));
    return;
  }

  // No saved geometry — use default size; let addWindow() cascade the position
  window->resize(defaultW, defaultH);
  qDebug() << "[WindowFactory] No saved geometry for" << windowType
           << "— using default" << defaultW << "x" << defaultH;
}

// ═══════════════════════════════════════════════════════════════════════
// Construction & dependency injection
// ═══════════════════════════════════════════════════════════════════════

WindowFactory::WindowFactory(MainWindow *mainWindow, CustomMDIArea *mdiArea,
                             QObject *parent)
    : QObject(parent), m_mainWindow(mainWindow), m_mdiArea(mdiArea) {}

void WindowFactory::setXTSClients(XTSMarketDataClient *mdClient,
                                  XTSInteractiveClient *iaClient) {
  m_xtsMarketDataClient = mdClient;
  m_xtsInteractiveClient = iaClient;
}

void WindowFactory::setTradingDataService(TradingDataService *service) {
  m_tradingDataService = service;
}

// ═══════════════════════════════════════════════════════════════════════
// Helpers
// ═══════════════════════════════════════════════════════════════════════

int WindowFactory::countWindowsOfType(const QString &type) {
  int count = 0;
  if (m_mdiArea) {
    for (auto window : m_mdiArea->windowList()) {
      if (window->windowType() == type)
        count++;
    }
  }
  return count;
}

void WindowFactory::closeWindowsByType(const QString &type) {
  if (!m_mdiArea)
    return;
  QList<CustomMDISubWindow *> windows = m_mdiArea->windowList();
  for (auto window : windows) {
    if (window->windowType() == type)
      window->close();
  }
}

MarketWatchWindow *WindowFactory::getActiveMarketWatch() const {
  CustomMDISubWindow *activeSubWindow =
      m_mdiArea ? m_mdiArea->activeWindow() : nullptr;
  if (activeSubWindow && activeSubWindow->windowType() == "MarketWatch") {
    return qobject_cast<MarketWatchWindow *>(activeSubWindow->contentWidget());
  }
  // Fallback: first MarketWatch
  if (m_mdiArea) {
    for (auto win : m_mdiArea->windowList()) {
      if (win->windowType() == "MarketWatch") {
        return qobject_cast<MarketWatchWindow *>(win->contentWidget());
      }
    }
  }
  return nullptr;
}

void WindowFactory::connectWindowSignals(CustomMDISubWindow *window) {
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
            CustomizeDialog dialog(windowType, targetWidget, m_mainWindow);
            dialog.exec();
          });

  // Auto-connect ATMWatch signals
  if (window->contentWidget() && window->windowType() == "ATMWatch") {
    ATMWatchWindow *atm =
        qobject_cast<ATMWatchWindow *>(window->contentWidget());
    if (atm) {
      connect(atm, &ATMWatchWindow::openOptionChainRequested, this,
              &WindowFactory::createOptionChainWindowForSymbol);
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
      connect(orderWin, &BaseOrderWindow::orderSubmitted, m_mainWindow,
              &MainWindow::placeOrder);
    }
  }
}

WindowContext WindowFactory::getBestWindowContext() const {
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

// ═══════════════════════════════════════════════════════════════════════
// Window creation methods
// ═══════════════════════════════════════════════════════════════════════

CustomMDISubWindow *WindowFactory::createGlobalSearchWindow() {
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

  connect(
      searchWidget, &GlobalSearchWidget::scripSelected, this,
      [this](const ContractData &contract) {
        qDebug() << "[WindowFactory] Script selected from Search:"
                 << contract.displayName;

#ifdef HAVE_TRADINGVIEW
        TradingViewChartWidget *activeChart = nullptr;
        CustomMDISubWindow *activeSub = m_mdiArea->activeWindow();
        if (activeSub && activeSub->windowType() == "ChartWindow") {
          activeChart = qobject_cast<TradingViewChartWidget *>(
              activeSub->contentWidget());
        }
        if (!activeChart) {
          for (auto win : m_mdiArea->windowList()) {
            if (win->windowType() == "ChartWindow") {
              activeChart =
                  qobject_cast<TradingViewChartWidget *>(win->contentWidget());
              break;
            }
          }
        }
        if (activeChart) {
          int segId = 2;
          if (contract.exchangeInstrumentID >= 11000000) {
            segId = (contract.strikePrice > 0 || contract.instrumentType == 1)
                        ? 12
                        : 11;
          } else {
            segId = (contract.strikePrice > 0 || contract.instrumentType == 1)
                        ? 2
                        : 1;
          }
          qDebug() << "[WindowFactory] Updating chart with token:"
                   << contract.exchangeInstrumentID << "segment:" << segId;
          activeChart->loadSymbol(contract.name, segId,
                                  contract.exchangeInstrumentID);
        } else
#endif
        {
          qDebug() << "[WindowFactory] No active chart. Adding to Watchlist.";
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
  applyRestoredGeometryOrDefault(window, "GlobalSearch", 800, 500);
  connectWindowSignals(window);
  m_mdiArea->addWindow(window);
  window->show();
  window->activateWindow();
  return window;
}

CustomMDISubWindow *WindowFactory::createMarketWatch() {
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
  applyRestoredGeometryOrDefault(window, "MarketWatch", 900, 400);
  qint64 t3 = mwTimer.elapsed();

  connectWindowSignals(window);
  qint64 t4 = mwTimer.elapsed();

  // Connect window activation to restore focus with a delay
  connect(window, &CustomMDISubWindow::windowActivated, marketWatch,
          [marketWatch]() {
            qDebug() << "[WindowFactory] Market Watch window activated, "
                        "scheduling focus restore";
            QTimer::singleShot(100, marketWatch, [marketWatch]() {
              if (marketWatch->getModel() &&
                  marketWatch->getModel()->rowCount() > 0) {
                marketWatch->restoreFocusedRow();
              }
            });
          });

  m_mdiArea->setUpdatesEnabled(false);
  m_mdiArea->addWindow(window);
  qint64 t5 = mwTimer.elapsed();

  window->setFocus();
  window->raise();
  window->activateWindow();
  m_mdiArea->setUpdatesEnabled(true);
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
CustomMDISubWindow *WindowFactory::createChartWindow() {
  static int counter = 1;

  CustomMDISubWindow *window =
      new CustomMDISubWindow(QString("Chart %1").arg(counter++), m_mdiArea);
  window->setWindowType("ChartWindow");

  TradingViewChartWidget *chartWidget = new TradingViewChartWidget(window);
  chartWidget->setXTSMarketDataClient(m_xtsMarketDataClient);
  chartWidget->setRepositoryManager(RepositoryManager::getInstance());
  window->setContentWidget(chartWidget);
  applyRestoredGeometryOrDefault(window, "ChartWindow", 1200, 700);

  connectWindowSignals(window);

  connect(chartWidget, &TradingViewChartWidget::orderRequestedFromChart,
          m_mainWindow, &MainWindow::placeOrder);

  m_mdiArea->setUpdatesEnabled(false);
  m_mdiArea->addWindow(window);
  window->setFocus();
  window->raise();
  window->activateWindow();
  m_mdiArea->setUpdatesEnabled(true);

  WindowContext context = getBestWindowContext();
  if (context.isValid()) {
    int segmentInt = 1;
    if (context.segment == "F" || context.segment == "2") {
      segmentInt = 2;
    }
    chartWidget->loadSymbol(context.symbol, segmentInt, context.token, "5");
  }

  qDebug() << "[WindowFactory] Created Chart Window";
  return window;
}
#else
CustomMDISubWindow *WindowFactory::createChartWindow() {
  qWarning()
      << "[WindowFactory] TradingView not available. Cannot create Chart.";
  return nullptr;
}
#endif

CustomMDISubWindow *WindowFactory::createIndicatorChartWindow() {
#ifdef HAVE_QTCHARTS
  static int counter = 1;

  CustomMDISubWindow *window = new CustomMDISubWindow(
      QString("Indicators %1").arg(counter++), m_mdiArea);
  window->setWindowType("IndicatorChart");

  IndicatorChartWidget *chartWidget = new IndicatorChartWidget(window);
  window->setContentWidget(chartWidget);
  applyRestoredGeometryOrDefault(window, "IndicatorChart", 1400, 900);

  connectWindowSignals(window);

  chartWidget->setXTSMarketDataClient(m_xtsMarketDataClient);
  chartWidget->setRepositoryManager(RepositoryManager::getInstance());

  connect(
      chartWidget, &IndicatorChartWidget::symbolChangeRequested, this,
      [this, chartWidget](const QString &symbol) {
        qDebug() << "[WindowFactory] Symbol search requested:" << symbol;

        RepositoryManager *repo = RepositoryManager::getInstance();
        if (!repo) {
          qWarning() << "[WindowFactory] Repository manager not available";
          return;
        }

        bool forceCash = false;
        QString searchQuery = symbol;
        if (searchQuery.endsWith(" EQ", Qt::CaseInsensitive)) {
          forceCash = true;
          searchQuery = searchQuery.left(searchQuery.length() - 3).trimmed();
          qDebug() << "[WindowFactory] Forced cash segment for:" << searchQuery;
        }

        QVector<ContractData> results =
            repo->searchScripsGlobal(searchQuery, "", "", "", 20);

        qDebug() << "[WindowFactory] Search found" << results.size()
                 << "results";

        if (results.isEmpty()) {
          qWarning() << "[WindowFactory] No results found for:" << searchQuery;
          return;
        }

        ContractData bestMatch;
        bool foundMatch = false;

        for (const auto &contract : results) {
          if (contract.instrumentType == 10)
            continue;

          int segment = (contract.exchangeInstrumentID >= 11000000) ? 11 : 1;
          if (contract.strikePrice > 0 || contract.instrumentType == 1) {
            segment = (contract.exchangeInstrumentID >= 11000000) ? 12 : 2;
          }

          if (forceCash) {
            if (segment == 1 || segment == 11) {
              bestMatch = contract;
              foundMatch = true;
              break;
            }
            continue;
          }

          if (!foundMatch) {
            bestMatch = contract;
            foundMatch = true;
          }

          if (contract.instrumentType == 0 && segment == 1) {
            bestMatch = contract;
            break;
          }
        }

        if (!foundMatch) {
          qWarning()
              << "[WindowFactory] No suitable tradable instruments found";
          return;
        }

        int segmentInt = (bestMatch.exchangeInstrumentID >= 11000000) ? 11 : 1;
        if (bestMatch.strikePrice > 0 || bestMatch.instrumentType == 1) {
          segmentInt = (bestMatch.exchangeInstrumentID >= 11000000) ? 12 : 2;
        }

        qDebug() << "[WindowFactory] Final Match:" << bestMatch.name
                 << "Token:" << bestMatch.exchangeInstrumentID
                 << "Segment:" << segmentInt;

        chartWidget->loadSymbol(bestMatch.name, segmentInt,
                                bestMatch.exchangeInstrumentID);
      });

  m_mdiArea->setUpdatesEnabled(false);
  m_mdiArea->addWindow(window);
  window->setFocus();
  window->raise();
  window->activateWindow();
  m_mdiArea->setUpdatesEnabled(true);

  WindowContext context = getBestWindowContext();
  if (context.isValid()) {
    bool isIndex = (context.token >= 26000 && context.token <= 36000);
    if (!isIndex) {
      int segmentInt = 1;
      if (context.segment == "F" || context.segment == "2") {
        segmentInt = 2;
      }
      chartWidget->loadSymbol(context.symbol, segmentInt, context.token);
      qDebug() << "[WindowFactory] Loaded symbol into indicator chart:"
               << context.symbol << "segment:" << segmentInt;
    } else {
      qDebug() << "[WindowFactory] Skipping index" << context.symbol
               << "(indices don't have OHLC candle data)";
    }
  }

  qDebug() << "[WindowFactory] Created Indicator Chart Window";
  return window;
#else
  qWarning() << "[WindowFactory] Qt Charts not available. Cannot create "
                "Indicator Chart.";
  return nullptr;
#endif
}

CustomMDISubWindow *WindowFactory::createBuyWindow() {
  static int f1Counter = 0;
  f1Counter++;
  qDebug() << "[PERF] [F1_PRESS] #" << f1Counter
           << " START Time:" << QDateTime::currentMSecsSinceEpoch();

  MarketWatchWindow *activeMarketWatch = getActiveMarketWatch();
  if (activeMarketWatch) {
    activeMarketWatch->storeFocusedRow();
  }

  QWidget *currentlyActive = nullptr;
  if (m_mdiArea && m_mdiArea->activeWindow()) {
    currentlyActive = m_mdiArea->activeWindow()->contentWidget();
  }

  WindowContext context = getBestWindowContext();

  QWidget *initiating = currentlyActive ? currentlyActive : activeMarketWatch;
  qWarning() << "[WindowFactory][FOCUS-DEBUG] createBuyWindow: initiating ="
             << (initiating ? initiating->metaObject()->className()
                            : "nullptr");
  CustomMDISubWindow *cached = WindowCacheManager::instance().showBuyWindow(
      context.isValid() ? &context : nullptr, initiating);
  if (cached) {
    return cached;
  }

  closeWindowsByType("BuyWindow");
  closeWindowsByType("SellWindow");

  CustomMDISubWindow *window = new CustomMDISubWindow("Buy Order", m_mdiArea);
  window->setWindowType("BuyWindow");

  BuyWindow *buyWindow = nullptr;
  if (context.isValid()) {
    buyWindow = new BuyWindow(context, window);
  } else {
    buyWindow = new BuyWindow(window);
  }

  window->setContentWidget(buyWindow);
  window->resize(975, 155);
  connectWindowSignals(window);
  m_mdiArea->addWindow(window);

  QSettings settings("TradingCompany", "TradingTerminal");
  if (settings.contains("orderwindow/last_x") &&
      settings.contains("orderwindow/last_y")) {
    int x = settings.value("orderwindow/last_x").toInt();
    int y = settings.value("orderwindow/last_y").toInt();
    window->move(x, y);
  } else {
    QSize mdiSize = m_mdiArea->size();
    int x = mdiSize.width() - 1220 - 20;
    int y = mdiSize.height() - 200 - 20;
    window->move(x, y);
  }

  connect(
      window, &CustomMDISubWindow::windowMoved, window,
      [](const QPoint &pos) {
        WindowCacheManager::instance().saveOrderWindowPosition(pos);
      },
      Qt::UniqueConnection);

  window->activateWindow();
  return window;
}

CustomMDISubWindow *WindowFactory::createSellWindow() {
  static int f2Counter = 0;
  f2Counter++;
  qDebug() << "[PERF] [F2_PRESS] #" << f2Counter
           << " START Time:" << QDateTime::currentMSecsSinceEpoch();

  MarketWatchWindow *activeMarketWatch = getActiveMarketWatch();
  if (activeMarketWatch) {
    activeMarketWatch->storeFocusedRow();
  }

  QWidget *currentlyActive = nullptr;
  if (m_mdiArea && m_mdiArea->activeWindow()) {
    currentlyActive = m_mdiArea->activeWindow()->contentWidget();
  }

  WindowContext context = getBestWindowContext();

  QWidget *initiating = currentlyActive ? currentlyActive : activeMarketWatch;
  CustomMDISubWindow *cached = WindowCacheManager::instance().showSellWindow(
      context.isValid() ? &context : nullptr, initiating);
  if (cached) {
    return cached;
  }

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
  window->resize(975, 155);
  connectWindowSignals(window);
  m_mdiArea->addWindow(window);

  QSettings settings("TradingCompany", "TradingTerminal");
  if (settings.contains("orderwindow/last_x") &&
      settings.contains("orderwindow/last_y")) {
    int x = settings.value("orderwindow/last_x").toInt();
    int y = settings.value("orderwindow/last_y").toInt();
    window->move(x, y);
  } else {
    QSize mdiSize = m_mdiArea->size();
    int x = mdiSize.width() - 1220 - 20;
    int y = mdiSize.height() - 200 - 20;
    window->move(x, y);
  }

  connect(
      window, &CustomMDISubWindow::windowMoved, window,
      [](const QPoint &pos) {
        WindowCacheManager::instance().saveOrderWindowPosition(pos);
      },
      Qt::UniqueConnection);

  window->activateWindow();
  return window;
}

CustomMDISubWindow *WindowFactory::createSnapQuoteWindow() {
  qDebug() << "[PERF] [CTRL+Q_PRESS] START Time:"
           << QDateTime::currentMSecsSinceEpoch();

  MarketWatchWindow *activeMarketWatch = getActiveMarketWatch();
  if (activeMarketWatch) {
    activeMarketWatch->storeFocusedRow();
  }

  QWidget *currentlyActive = nullptr;
  if (m_mdiArea && m_mdiArea->activeWindow()) {
    currentlyActive = m_mdiArea->activeWindow()->contentWidget();
  }

  WindowContext context = getBestWindowContext();

  QWidget *initiating = currentlyActive ? currentlyActive : activeMarketWatch;
  CustomMDISubWindow *cached =
      WindowCacheManager::instance().showSnapQuoteWindow(
          context.isValid() ? &context : nullptr, initiating);
  if (cached) {
    qDebug() << "[PERF] ⚡ Cache HIT! Time:"
             << QDateTime::currentMSecsSinceEpoch() << "(~10-20ms)";
    return cached;
  }

  qDebug() << "[PERF] Cache MISS - Creating new window (~370-1500ms)";

  int count = 0;
  QList<CustomMDISubWindow *> allWindows = m_mdiArea->windowList();
  for (auto win : allWindows) {
    if (win->windowType() == "SnapQuote" &&
        win->geometry().x() >= WindowConstants::VISIBLE_THRESHOLD_X) {
      qDebug() << "[WindowFactory] Visible SnapQuote:" << win->title();
      count++;
    }
  }

  qDebug() << "[WindowFactory] Visible Snap Quote windows:" << count;

  if (count >= WindowCacheManager::CACHED_SNAPQUOTE_COUNT) {
    QStatusBar *sb = m_mainWindow->statusBar();
    if (sb)
      sb->showMessage(
          QString("Maximum %1 Snap Quote windows allowed")
              .arg(WindowCacheManager::CACHED_SNAPQUOTE_COUNT),
          3000);
    return nullptr;
  }

  QSet<int> used;
  for (auto win : allWindows) {
    if (win->windowType() == "SnapQuote") {
      QString t = win->title();
      if (t.startsWith("Snap Quote ")) {
        used.insert(t.mid(11).toInt());
      } else if (t == "Snap Quote") {
        used.insert(0);
      }
    }
  }
  int idx = 1;
  while (used.contains(idx))
    idx++;

  QString title = QString("Snap Quote %1").arg(idx);
  qDebug() << "[WindowFactory] Creating new Snap Quote window with title:"
           << title;

  CustomMDISubWindow *window = new CustomMDISubWindow(title, m_mdiArea);
  window->setWindowType("SnapQuote");

  SnapQuoteWindow *snapWindow = nullptr;

  if (activeMarketWatch && activeMarketWatch->hasValidSelection()) {
    WindowContext mwContext = activeMarketWatch->getSelectedContractContext();
    if (mwContext.isValid()) {
      snapWindow = new SnapQuoteWindow(mwContext, window);
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

  connect(&UdpBroadcastService::instance(),
          &UdpBroadcastService::udpTickReceived, snapWindow,
          &SnapQuoteWindow::onTickUpdate);

  window->setContentWidget(snapWindow);
  applyRestoredGeometryOrDefault(window, "SnapQuote", 780, 250);
  if (currentlyActive) {
    window->setInitiatingWindow(currentlyActive);
  } else if (activeMarketWatch) {
    window->setInitiatingWindow(activeMarketWatch);
  }
  connectWindowSignals(window);

  connect(window, &CustomMDISubWindow::closeRequested, this, [this, window]() {
    WindowCacheManager::instance().markSnapQuoteWindowClosed(0);
    qDebug() << "[WindowFactory] SnapQuote window closed (legacy path), marked "
                "for reset";
  });

  m_mdiArea->addWindow(window);
  window->activateWindow();

  qDebug() << "[PERF] SnapQuote window created. Time:"
           << QDateTime::currentMSecsSinceEpoch();
  return window;
}

CustomMDISubWindow *WindowFactory::createOptionChainWindow() {
  QWidget *currentlyActive = nullptr;
  if (m_mdiArea && m_mdiArea->activeWindow()) {
    currentlyActive = m_mdiArea->activeWindow()->contentWidget();
  }

  CustomMDISubWindow *window =
      new CustomMDISubWindow("Option Chain", m_mdiArea);
  window->setWindowType("OptionChain");

  OptionChainWindow *optionWindow = new OptionChainWindow(window);

  window->setContentWidget(optionWindow);
  if (currentlyActive) {
    window->setInitiatingWindow(currentlyActive);
  }
  applyRestoredGeometryOrDefault(window, "OptionChain", 1600, 800);
  connectWindowSignals(window);
  m_mdiArea->addWindow(window);
  window->activateWindow();
  return window;
}

CustomMDISubWindow *
WindowFactory::createOptionChainWindowForSymbol(const QString &symbol,
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

CustomMDISubWindow *WindowFactory::createOptionCalculatorWindow() {
  QWidget *currentlyActive = nullptr;
  if (m_mdiArea && m_mdiArea->activeWindow()) {
    currentlyActive = m_mdiArea->activeWindow()->contentWidget();
  }

  CustomMDISubWindow *window =
      new CustomMDISubWindow("Option Calculator", m_mdiArea);
  window->setWindowType("OptionCalculator");

  OptionCalculatorWindow *calcWindow = new OptionCalculatorWindow(window);
  window->setContentWidget(calcWindow);
  if (currentlyActive) {
    window->setInitiatingWindow(currentlyActive);
  }
  applyRestoredGeometryOrDefault(window, "OptionCalculator", 500, 600);
  connectWindowSignals(window);
  m_mdiArea->addWindow(window);
  window->activateWindow();
  return window;
}

CustomMDISubWindow *WindowFactory::createATMWatchWindow() {
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
  applyRestoredGeometryOrDefault(window, "ATMWatch", 1200, 600);
  connectWindowSignals(window);
  m_mdiArea->addWindow(window);
  window->activateWindow();
  return window;
}

CustomMDISubWindow *WindowFactory::createOrderBookWindow() {
  if (countWindowsOfType("OrderBook") >= 5)
    return nullptr;
  CustomMDISubWindow *window = new CustomMDISubWindow("Order Book", m_mdiArea);
  window->setWindowType("OrderBook");
  OrderBookWindow *ob = new OrderBookWindow(m_tradingDataService, window);

  connect(ob, &OrderBookWindow::modifyOrderRequested, this,
          [this](const XTS::Order &order) {
            if (order.orderSide.toUpper() == "BUY") {
              openBuyWindowForModification(order);
            } else {
              openSellWindowForModification(order);
            }
          });

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

  connect(ob, &OrderBookWindow::cancelOrderRequested, m_mainWindow,
          &MainWindow::cancelOrder);

  window->setContentWidget(ob);
  applyRestoredGeometryOrDefault(window, "OrderBook", 1400, 600);
  connectWindowSignals(window);
  m_mdiArea->addWindow(window);
  window->activateWindow();
  return window;
}

CustomMDISubWindow *WindowFactory::createTradeBookWindow() {
  if (countWindowsOfType("TradeBook") >= 5)
    return nullptr;
  CustomMDISubWindow *window = new CustomMDISubWindow("Trade Book", m_mdiArea);
  window->setWindowType("TradeBook");
  TradeBookWindow *tb = new TradeBookWindow(m_tradingDataService, window);
  window->setContentWidget(tb);
  applyRestoredGeometryOrDefault(window, "TradeBook", 1400, 600);
  connectWindowSignals(window);
  m_mdiArea->addWindow(window);
  window->activateWindow();
  return window;
}

CustomMDISubWindow *WindowFactory::createPositionWindow() {
  if (countWindowsOfType("PositionWindow") >= 5)
    return nullptr;
  CustomMDISubWindow *window =
      new CustomMDISubWindow("Integrated Net Position", m_mdiArea);
  window->setWindowType("PositionWindow");
  PositionWindow *pw = new PositionWindow(m_tradingDataService, window);
  window->setContentWidget(pw);
  applyRestoredGeometryOrDefault(window, "PositionWindow", 1000, 500);
  connectWindowSignals(window);
  m_mdiArea->addWindow(window);
  window->activateWindow();
  return window;
}

CustomMDISubWindow *WindowFactory::createStrategyManagerWindow() {
  CustomMDISubWindow *window =
      new CustomMDISubWindow("Strategy Manager", m_mdiArea);
  window->setWindowType("StrategyManager");

  StrategyManagerWindow *strategyWindow = new StrategyManagerWindow(window);

  window->setContentWidget(strategyWindow);
  applyRestoredGeometryOrDefault(window, "StrategyManager", 1000, 600);
  connectWindowSignals(window);
  m_mdiArea->addWindow(window);
  window->activateWindow();
  return window;
}

CustomMDISubWindow *WindowFactory::createMarketMovementWindow() {
  WindowContext context = getBestWindowContext();

  if (!context.isValid()) {
    QMessageBox::warning(
        m_mainWindow, "Market Movement",
        "Please select an option instrument from Market Watch first.");
    return nullptr;
  }

  if (context.instrumentType != "OPTSTK" &&
      context.instrumentType != "OPTIDX") {
    QMessageBox::warning(
        m_mainWindow, "Market Movement",
        "This window is only for option instruments (OPTSTK).\n"
        "Please select an option from Market Watch.");
    return nullptr;
  }

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
  applyRestoredGeometryOrDefault(window, "MarketMovement", 1000, 600);

  connectWindowSignals(window);
  m_mdiArea->addWindow(window);
  window->show();

  qDebug() << "[WindowFactory] Market Movement window created for"
           << context.symbol;

  return window;
}

// ═══════════════════════════════════════════════════════════════════════
// Order modification windows
// ═══════════════════════════════════════════════════════════════════════

void WindowFactory::openBuyWindowForModification(const XTS::Order &order) {
  CustomMDISubWindow *window =
      new CustomMDISubWindow("Modify Buy Order", m_mdiArea);
  window->setWindowType("BuyWindow");

  BuyWindow *buyWindow = new BuyWindow(window);
  buyWindow->loadFromOrder(order);

  connect(buyWindow, &BaseOrderWindow::orderModificationSubmitted, m_mainWindow,
          &MainWindow::modifyOrder);

  window->setContentWidget(buyWindow);
  window->resize(975, 180);
  connectWindowSignals(window);
  m_mdiArea->addWindow(window);
  window->activateWindow();
}

void WindowFactory::openSellWindowForModification(const XTS::Order &order) {
  CustomMDISubWindow *window =
      new CustomMDISubWindow("Modify Sell Order", m_mdiArea);
  window->setWindowType("SellWindow");

  SellWindow *sellWindow = new SellWindow(window);
  sellWindow->loadFromOrder(order);

  connect(sellWindow, &BaseOrderWindow::orderModificationSubmitted,
          m_mainWindow, &MainWindow::modifyOrder);

  window->setContentWidget(sellWindow);
  window->resize(975, 180);
  connectWindowSignals(window);
  m_mdiArea->addWindow(window);
  window->activateWindow();
}

void WindowFactory::openBatchBuyWindowForModification(
    const QVector<XTS::Order> &orders) {
  CustomMDISubWindow *window =
      new CustomMDISubWindow("Batch Modify Buy", m_mdiArea);
  window->setWindowType("BuyWindow");

  BuyWindow *buyWindow = new BuyWindow(window);
  buyWindow->loadFromOrders(orders);

  connect(buyWindow, &BaseOrderWindow::orderModificationSubmitted, m_mainWindow,
          &MainWindow::modifyOrder);

  window->setContentWidget(buyWindow);
  window->resize(975, 180);
  connectWindowSignals(window);
  m_mdiArea->addWindow(window);
  window->activateWindow();
}

void WindowFactory::openBatchSellWindowForModification(
    const QVector<XTS::Order> &orders) {
  CustomMDISubWindow *window =
      new CustomMDISubWindow("Batch Modify Sell", m_mdiArea);
  window->setWindowType("SellWindow");

  SellWindow *sellWindow = new SellWindow(window);
  sellWindow->loadFromOrders(orders);

  connect(sellWindow, &BaseOrderWindow::orderModificationSubmitted,
          m_mainWindow, &MainWindow::modifyOrder);

  window->setContentWidget(sellWindow);
  window->resize(975, 180);
  connectWindowSignals(window);
  m_mdiArea->addWindow(window);
  window->activateWindow();
}

// ═══════════════════════════════════════════════════════════════════════
// Context-aware & Widget-aware creation
// ═══════════════════════════════════════════════════════════════════════

void WindowFactory::createBuyWindowWithContext(const WindowContext &context,
                                               QWidget *initiatingWindow) {
  CustomMDISubWindow *cached = WindowCacheManager::instance().showBuyWindow(
      context.isValid() ? &context : nullptr, initiatingWindow);
  if (cached)
    return;
  createBuyWindow();
}

void WindowFactory::createSellWindowWithContext(const WindowContext &context,
                                                QWidget *initiatingWindow) {
  CustomMDISubWindow *cached = WindowCacheManager::instance().showSellWindow(
      context.isValid() ? &context : nullptr, initiatingWindow);
  if (cached)
    return;
  createSellWindow();
}

void WindowFactory::createSnapQuoteWindowWithContext(
    const WindowContext &context, QWidget *initiatingWindow) {
  CustomMDISubWindow *cached =
      WindowCacheManager::instance().showSnapQuoteWindow(
          context.isValid() ? &context : nullptr, initiatingWindow);
  if (cached)
    return;
  createSnapQuoteWindow();
}

void WindowFactory::createBuyWindowFromWidget(QWidget *initiatingWidget) {
  qWarning()
      << "[WindowFactory][FOCUS-DEBUG] createBuyWindowFromWidget: initiating ="
      << (initiatingWidget ? initiatingWidget->metaObject()->className()
                           : "nullptr")
      << (initiatingWidget ? initiatingWidget->objectName() : "");

  WindowContext context = getBestWindowContext();
  CustomMDISubWindow *cached = WindowCacheManager::instance().showBuyWindow(
      context.isValid() ? &context : nullptr, initiatingWidget);
  if (cached)
    return;
  createBuyWindow();
}

void WindowFactory::createSellWindowFromWidget(QWidget *initiatingWidget) {
  qWarning()
      << "[WindowFactory][FOCUS-DEBUG] createSellWindowFromWidget: initiating ="
      << (initiatingWidget ? initiatingWidget->metaObject()->className()
                           : "nullptr")
      << (initiatingWidget ? initiatingWidget->objectName() : "");

  WindowContext context = getBestWindowContext();
  CustomMDISubWindow *cached = WindowCacheManager::instance().showSellWindow(
      context.isValid() ? &context : nullptr, initiatingWidget);
  if (cached)
    return;
  createSellWindow();
}

// ═══════════════════════════════════════════════════════════════════════
// Add-to-watch
// ═══════════════════════════════════════════════════════════════════════

void WindowFactory::onAddToWatchRequested(const InstrumentData &instrument) {
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
    marketWatch->addScrip(
        instrument.symbol,
        RepositoryManager::getExchangeSegmentName(instrument.exchangeSegment),
        (int)instrument.exchangeInstrumentID);

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
