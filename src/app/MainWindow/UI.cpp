#include "app/MainWindow.h"
#include "app/ScripBar.h"
#include "core/widgets/CustomMDIArea.h"
#include "core/widgets/CustomMDISubWindow.h" // Needed for window iteration
#include "core/widgets/InfoBar.h"
#include "repository/RepositoryManager.h"
#include "services/UdpBroadcastService.h"
#include "views/AllIndicesWindow.h"
#include "views/CustomizeDialog.h"
#include "views/IndicesView.h"
#include "views/PreferenceDialog.h"
#include <QDateTime>
#include <QDebug>
#include <QDockWidget>
#include <QElapsedTimer>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QMainWindow>
#include <QMenuBar>
#include <QMessageBox>
#include <QSettings>
#include <QShortcut>
#include <QStatusBar>
#include <QToolBar>
#include <QVBoxLayout>

#include "models/profiles/GenericProfileManager.h"
#include "models/profiles/GenericTableProfile.h"
#include "views/BaseBookWindow.h"
#include "views/MarketWatchWindow.h"
#include <QJsonDocument>

void MainWindow::setupContent() {
  // Get the central widget container from CustomMainWindow
  QWidget *container = centralWidget();
  if (!container) {
    // Central widget missing, create it
    container = new QWidget(this);
    setCentralWidget(container);
  }

  // Ensure layout exists
  QVBoxLayout *layout = qobject_cast<QVBoxLayout *>(container->layout());
  if (!layout) {
    // Create main layout if missing
    layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    container->setLayout(layout);
  }

  // Create menu bar (custom widget, NOT QMainWindow::menuBar())
  createMenuBar();
  setCustomMenuBar(m_menuBar);

  // Create custom toolbar area using nested QMainWindow
  // This maintains dockable/floatable functionality while controlling position
  QMainWindow *toolbarHost = new QMainWindow(container);
  toolbarHost->setWindowFlags(
      Qt::Widget); // Make it behave as a widget, not a window
  toolbarHost->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);

  // Create main toolbar
  createToolBar();
  m_toolBar->setMovable(true);
  m_toolBar->setFloatable(true);
  m_toolBar->setAllowedAreas(Qt::TopToolBarArea);
  toolbarHost->addToolBar(Qt::TopToolBarArea, m_toolBar);

  // Create connection bar toolbar
  createConnectionBar();
  m_connectionToolBar = new QToolBar(tr("Connection"), toolbarHost);
  m_connectionToolBar->setObjectName("ConnectionToolBar");
  // Styling is handled inside createConnectionBar via parent/child propagation
  // or shared logic but we can apply the container style here if needed.
  m_connectionToolBar->setStyleSheet(
      "QToolBar { background-color: #e1e1e1; border: none; border-bottom: 1px "
      "solid #d1d1d1; }");
  m_connectionToolBar->addWidget(m_connectionBar);
  m_connectionToolBar->setMovable(true);
  m_connectionToolBar->setFloatable(true);
  m_connectionToolBar->setAllowedAreas(Qt::TopToolBarArea);
  toolbarHost->addToolBarBreak(Qt::TopToolBarArea); // Force new line
  toolbarHost->addToolBar(Qt::TopToolBarArea, m_connectionToolBar);

  // Create scrip bar toolbar
  m_scripBar = new ScripBar(toolbarHost);
  connect(m_scripBar, &ScripBar::addToWatchRequested, this,
          &MainWindow::onAddToWatchRequested);
  connect(m_scripBar, &ScripBar::scripBarEscapePressed, this,
          &MainWindow::onScripBarEscapePressed);
  m_scripToolBar = new QToolBar(tr("Scrip Bar"), toolbarHost);
  m_scripToolBar->setObjectName("ScripToolBar");
  m_scripToolBar->setStyleSheet("QToolBar { "
                                "   background-color: #e5e5e5; " // Dark
                                "   border: none; "
                                "   spacing: 2px; "
                                "} ");
  m_scripToolBar->addWidget(m_scripBar);
  m_scripToolBar->setMovable(true);
  m_scripToolBar->setFloatable(true);
  m_scripToolBar->setAllowedAreas(Qt::TopToolBarArea);
  toolbarHost->addToolBarBreak(Qt::TopToolBarArea); // Force new line
  toolbarHost->addToolBar(Qt::TopToolBarArea, m_scripToolBar);

  // Create a dummy central widget for the toolbar host (required by
  // QMainWindow)
  QWidget *dummyWidget = new QWidget(toolbarHost);
  dummyWidget->setFixedHeight(0); // Make it invisible
  dummyWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  toolbarHost->setCentralWidget(dummyWidget);

  // Add the toolbar host to main layout (below menu bar)
  layout->addWidget(toolbarHost);

  // Custom MDI Area (main content area)
  m_mdiArea = new CustomMDIArea(container);
  // Dark MDI background
  m_mdiArea->setStyleSheet("CustomMDIArea { background-color: #a19d9d; }");
  // NOTE: restoreWindowRequested is wired to WorkspaceManager in MainWindow constructor
  layout->addWidget(m_mdiArea, 1); // Give it stretch factor so it expands

  // Create info bar (using DockWidget approach as requested)
  createInfoBar();

  // ✅ DO NOT create IndicesView here - it will be created after login
  // completes and main window is shown (see setConfigLoader in MainWindow.cpp)

  // Create status bar at the bottom
  createStatusBar();
  setCustomStatusBar(m_statusBar);

  // Adjust central widget layout
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);
}

void MainWindow::createMenuBar() {
  // Create a custom menu bar widget
  // Note: Do NOT set 'this' as parent if using setCustomMenuBar,
  // as it will be reparented to the header container.
  m_menuBar = new QMenuBar(nullptr);
  m_menuBar->setNativeMenuBar(false); // Force in-window menu bar
  m_menuBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);

  // LIGHT THEME STYLES (Matching light theme palette)
  m_menuBar->setStyleSheet("QMenuBar {"
                           "  background: #f8fafc;"
                           "  color: #1e293b;"
                           "  font-size: 12px;"
                           "  padding: 4px 6px;"
                           "  border-bottom: 1px solid #e2e8f0;"
                           "}"
                           "QMenuBar::item {"
                           "  padding: 2px 6px;"
                           "  background: transparent;"
                           "}"
                           "QMenuBar::item:selected {"
                           "  background: #dbeafe;"
                           "  color: #1e40af;"
                           "}"
                           "QMenu {"
                           "  background: #ffffff;"
                           "  color: #1e293b;"
                           "  border: 1px solid #e2e8f0;"
                           "}"
                           "QMenu::item {"
                           "  padding: 4px 20px 4px 6px;"
                           "}"
                           "QMenu::item:selected {"
                           "  background: #dbeafe;"
                           "  color: #1e40af;"
                           "}");

  m_menuBar->setFixedHeight(32);

  // File Menu
  QMenu *fileMenu = m_menuBar->addMenu("&File");

  fileMenu->addAction("&Save Workspace...", this,
                      &MainWindow::saveCurrentWorkspace);
  fileMenu->addAction("&Load Workspace...", this, &MainWindow::loadWorkspace);

  fileMenu->addSeparator();

  QAction *exitAction = fileMenu->addAction("E&xit");
  connect(exitAction, &QAction::triggered, this, &QWidget::close);

  // Edit Menu
  QMenu *editMenu = m_menuBar->addMenu("&Edit");
  // Preferences moved to Window menu as per user request

  // View Menu
  QMenu *viewMenu = m_menuBar->addMenu("&View");
  m_statusBarAction = viewMenu->addAction("&Status Bar");
  m_statusBarAction->setCheckable(true);
  m_statusBarAction->setChecked(true);
  connect(m_statusBarAction, &QAction::toggled, this, [this](bool visible) {
    if (m_statusBar)
      m_statusBar->setVisible(visible);
  });

  m_infoBarAction = viewMenu->addAction("&Info Bar");
  m_infoBarAction->setCheckable(true);
  m_infoBarAction->setChecked(true);
  connect(m_infoBarAction, &QAction::toggled, this, [this](bool visible) {
    if (m_infoDock)
      m_infoDock->setVisible(visible);
  });

  m_indicesViewAction = viewMenu->addAction("In&dices View");
  m_indicesViewAction->setCheckable(true);
  // Checked state will be updated in createIndicesView after preference load

  m_allIndicesAction = viewMenu->addAction("&All Indices...");
  m_allIndicesAction->setCheckable(false);
  connect(m_allIndicesAction, &QAction::triggered, this,
          &MainWindow::showAllIndices);

  viewMenu->addSeparator();
  QAction *resetLayoutAction = viewMenu->addAction("Reset &Layout");
  connect(resetLayoutAction, &QAction::triggered, this,
          &MainWindow::resetLayout);

  // Window Menu
  QMenu *windowMenu = m_menuBar->addMenu("&Window");

  QAction *wmMarketWatch = windowMenu->addAction("&MarketWatch\tF4");
  connect(wmMarketWatch, &QAction::triggered, this,
          &MainWindow::createMarketWatch);

  QAction *wmBuy = windowMenu->addAction("&Buy\tF1");
  connect(wmBuy, &QAction::triggered, this, &MainWindow::createBuyWindow);

  QAction *wmSell = windowMenu->addAction("&Sell\tF2");
  connect(wmSell, &QAction::triggered, this, &MainWindow::createSellWindow);

  QAction *wmSnap = windowMenu->addAction("Snap&Quote\tF5");
  connect(wmSnap, &QAction::triggered, this,
          &MainWindow::createSnapQuoteWindow);

  QAction *wmOptionChain = windowMenu->addAction("&Option Chain\tF6");
  connect(wmOptionChain, &QAction::triggered, this,
          &MainWindow::createOptionChainWindow);

  QAction *wmATMWatch = windowMenu->addAction("ATM &Watch");
  connect(wmATMWatch, &QAction::triggered, this,
          &MainWindow::createATMWatchWindow);

  QAction *wmOrderBook = windowMenu->addAction("&OrderBook\tF3");
  connect(wmOrderBook, &QAction::triggered, this,
          &MainWindow::createOrderBookWindow);

  QAction *wmTradeBook = windowMenu->addAction("&TradeBook\tF8");
  connect(wmTradeBook, &QAction::triggered, this,
          &MainWindow::createTradeBookWindow);

  QAction *wmPosition = windowMenu->addAction("Net &Position\tAlt+F6");
  connect(wmPosition, &QAction::triggered, this,
          &MainWindow::createPositionWindow);

  QAction *wmStrategy = windowMenu->addAction("Strategy &Manager\tAlt+S");
  connect(wmStrategy, &QAction::triggered, this,
          &MainWindow::createStrategyManagerWindow);

#ifdef HAVE_TRADINGVIEW
  QAction *wmChart = windowMenu->addAction("Chart (&TradingView)\tF7");
  connect(wmChart, &QAction::triggered, this,
          &MainWindow::createChartWindow);
#endif

#ifdef HAVE_QTCHARTS
  QAction *wmIndicatorChart = windowMenu->addAction("&Indicator Chart\tF8");
  connect(wmIndicatorChart, &QAction::triggered, this,
          &MainWindow::createIndicatorChartWindow);
#endif

  windowMenu->addSeparator();
  windowMenu->addAction("&Cascade", this,
                        [this]() { m_mdiArea->cascadeWindows(); });
  windowMenu->addAction("&Tile", this, [this]() { m_mdiArea->tileWindows(); });

  windowMenu->addSeparator();
  QAction *preferencesAction = windowMenu->addAction("&Preferences");
  preferencesAction->setShortcut(QKeySequence("Ctrl+R"));
  connect(preferencesAction, &QAction::triggered, this,
          &MainWindow::showPreferences);

  // Data Menu
  QMenu *dataMenu = m_menuBar->addMenu("&Data");
  connect(dataMenu->addAction("Start &NSE Broadcast Receiver"),
          &QAction::triggered, this, &MainWindow::startBroadcastReceiver);
  connect(dataMenu->addAction("St&op NSE Broadcast Receiver"),
          &QAction::triggered, this, &MainWindow::stopBroadcastReceiver);

  // Help Menu
  QMenu *helpMenu = m_menuBar->addMenu("&Help");
  helpMenu->addAction("&About");
}

void MainWindow::createToolBar() {
  m_toolBar = new QToolBar("Main Toolbar", this);
  m_toolBar->setObjectName("MainToolBar"); // Object name for state saving
  m_toolBar->setAllowedAreas(Qt::AllToolBarAreas);
  m_toolBar->setMovable(true);
  m_toolBar->setFloatable(true);
  m_toolBar->setIconSize(QSize(16, 16));
  m_toolBar->setFixedHeight(32);

  // Light theme toolbar style
  m_toolBar->setStyleSheet("QWidget { "
                           "   background-color: #f8fafc; "
                           "} "
                           "QToolBar { "
                           "   background-color: #f8fafc; "
                           "   border: none; "
                           "   color: #1e293b; "
                           "} "
                           "QToolButton { "
                           "   color: #1e293b; "
                           "   border: none; "
                           "   padding: 2px; "
                           "} "
                           "QToolButton:hover { "
                           "   background-color: #e2e8f0; "
                           "} "
                           "QToolButton:pressed { "
                           "   background-color: #dbeafe; "
                           "   color: #1e40af; "
                           "}");

  // Add toolbar actions with text (icons can be added if available in qrc)
  m_toolBar->addAction("Market Watch", this, SLOT(createMarketWatch()));
  m_toolBar->addAction("Buy Order", this, SLOT(createBuyWindow()));
  m_toolBar->addAction("Sell Order", this, SLOT(createSellWindow()));
  m_toolBar->addAction("Snap Quote", this, SLOT(createSnapQuoteWindow()));

  m_toolBar->addSeparator();

  // Connection actions can be added here
}

void MainWindow::createConnectionBar() {
  m_connectionBar = new QWidget(this);
  m_connectionBar->setFixedHeight(28);
  // Light theme style
  m_connectionBar->setStyleSheet("QWidget { "
                                 "   background-color: #f8fafc; "
                                 "} "
                                 "QLabel { "
                                 "   color: #475569; "
                                 "   padding: 2px 6px; "
                                 "   font-size: 10px; "
                                 "}");

  QHBoxLayout *layout = new QHBoxLayout(m_connectionBar);
  layout->setContentsMargins(8, 0, 8, 0);
  layout->setSpacing(3);
  layout->setAlignment(Qt::AlignVCenter);

  QLabel *statusLabel = new QLabel("Connection Status:", m_connectionBar);
  layout->addWidget(statusLabel);

  QLabel *statusValue = new QLabel("Disconnected", m_connectionBar);
  statusValue->setStyleSheet(
      "color: #ff6b6b; font-weight: bold; font-size: 10px;");
  layout->addWidget(statusValue);

  layout->addStretch();
}

void MainWindow::createStatusBar() {
  m_statusBar = new QStatusBar(this);
  // Light theme style
  m_statusBar->setStyleSheet("QStatusBar { "
                             "   background-color: #f8fafc; "
                             "   color: #475569; "
                             "   border-top: 1px solid #e2e8f0; "
                             "} "
                             "QStatusBar::item { "
                             "   border: none; "
                             "}");

  m_statusBar->showMessage("Ready");
  m_statusBar->addPermanentWidget(new QLabel("Market: Closed"));
}

void MainWindow::createInfoBar() {
  m_infoDock = new QDockWidget("Info", this);
  m_infoDock->setAllowedAreas(Qt::BottomDockWidgetArea);
  m_infoDock->setFeatures(QDockWidget::DockWidgetClosable);
  m_infoDock->setTitleBarWidget(new QWidget()); // Hides title bar

  m_infoBar = new InfoBar(this);
  m_infoBar->setFixedHeight(50);

  // Apply styling to InfoBar widget itself (Container frame)
  m_infoBar->setStyleSheet(
      "QWidget { background-color: #f8fafc; border-top: 1px solid #e2e8f0; }"
      "QLabel { color: #334155; font-size: 11px; }");

  m_infoDock->setWidget(m_infoBar);

  connect(m_infoBar, &InfoBar::hideRequested, this, [this]() {
    if (m_infoDock) {
      m_infoDock->hide();
      if (m_infoBarAction)
        m_infoBarAction->setChecked(false);
      QSettings s("TradingCompany", "TradingTerminal");
      s.setValue("mainwindow/info_visible", false);
    }
  });

  addDockWidget(Qt::BottomDockWidgetArea, m_infoDock);

  connect(m_infoDock, &QDockWidget::visibilityChanged, this,
          [this](bool visible) {
            if (m_infoBarAction)
              m_infoBarAction->setChecked(visible);
            QSettings s("TradingCompany", "TradingTerminal");
            s.setValue("mainwindow/info_visible", visible);
          });
}

void MainWindow::focusScripBar() {
  if (m_scripBar)
    m_scripBar->focusInput();
}

// Implement slot methods here
void MainWindow::resetLayout() {
  QSettings s("TradingCompany", "TradingTerminal");
  s.remove("mainwindow/state");
  // Also remove from mdi area
  // m_mdiArea->closeAllSubWindows(); // This might be drastic but it resets
  // Restore logic if implemented
}

// saveCurrentWorkspace(), loadWorkspace(), loadWorkspaceByName(), manageWorkspaces()
// have been extracted to WorkspaceManager (src/app/WorkspaceManager.cpp).
// MainWindow::Windows.cpp provides thin delegator slots.

void MainWindow::createIndicesView() {
  if (m_indicesView)
    return;

  qDebug() << "[MainWindow] Creating IndicesView (staggered initialization)...";

  // Phase 1: Create the widget (fast)
  m_indicesView = new IndicesView(this);
  m_indicesView->setWindowTitle("Indices");
  m_indicesView->setWindowFlags(Qt::Tool | Qt::WindowCloseButtonHint);
  m_indicesView->resize(400, 120);

  // Phase 2: Deferred initialization to keep startup smooth
  QTimer::singleShot(10, this, [this]() {
    if (!m_indicesView)
      return;

    // Connect UDP signal
    connect(&UdpBroadcastService::instance(),
            &UdpBroadcastService::udpIndexReceived, m_indicesView,
            &IndicesView::onIndexReceived, Qt::QueuedConnection);

    // Initialize data from repository
    if (RepositoryManager::getInstance()) {
      m_indicesView->initialize(RepositoryManager::getInstance());
    }

    // Connect action to view visibility
    if (m_indicesViewAction) {
      connect(m_indicesViewAction, &QAction::toggled, this,
              [this](bool visible) {
                if (!m_indicesView)
                  return;
                if (visible) {
                  m_indicesView->show();
                  m_indicesView->raise();
                  m_indicesView->activateWindow();
                } else {
                  m_indicesView->hide();
                }
                // Save preference
                QSettings s("TradingCompany", "TradingTerminal");
                s.setValue("mainwindow/indices_visible", visible);
              });

      // ✅ Connect hideRequested signal
      connect(m_indicesView, &IndicesView::hideRequested, this, [this]() {
        if (m_indicesViewAction) {
          m_indicesViewAction->blockSignals(true);
          m_indicesViewAction->setChecked(false);
          m_indicesViewAction->blockSignals(false);
        }
      });
    }

    // Show if preference says so
    bool indicesVisible = QSettings("TradingCompany", "TradingTerminal")
                              .value("mainwindow/indices_visible", true)
                              .toBool();
    if (indicesVisible) {
      m_indicesView->show();
      m_indicesView->raise();
      if (m_indicesViewAction)
        m_indicesViewAction->setChecked(true);
    }

    qDebug() << "[MainWindow] IndicesView background initialization complete";
  });
}

// Helper to ensure AllIndicesWindow is created only when needed
void MainWindow::showAllIndices() {
  if (!m_allIndicesWindow) {
    qDebug() << "[MainWindow] Creating AllIndicesWindow on-demand...";
    m_allIndicesWindow = new AllIndicesWindow(this);
    if (RepositoryManager::getInstance()) {
      m_allIndicesWindow->initialize(RepositoryManager::getInstance());

      // Sync selection
      QSettings settings("TradingCompany", "TradingTerminal");
      QStringList selected = settings.value("indices/selected").toStringList();
      m_allIndicesWindow->setSelectedIndices(selected);
    }

    // Connect selection changes
    connect(m_allIndicesWindow, &AllIndicesWindow::selectionChanged, this,
            [this](const QStringList &selected) {
              if (m_indicesView) {
                m_indicesView->reloadSelectedIndices(selected);
              }
            });
  }

  m_allIndicesWindow->show();
  m_allIndicesWindow->raise();
  m_allIndicesWindow->activateWindow();
}
