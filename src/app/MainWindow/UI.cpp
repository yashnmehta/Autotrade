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


#include "models/GenericProfileManager.h"
#include "models/GenericTableProfile.h"
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
  connect(m_mdiArea, &CustomMDIArea::restoreWindowRequested, this,
          &MainWindow::onRestoreWindowRequested);
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

  // LIGHT THEME STYLES (Matching user's recent grey/light prefs)
  m_menuBar->setStyleSheet("QMenuBar {"
                           "  background: #e1e1e1;"
                           "  color: #1a1a1a;"
                           "  font-size: 12px;"
                           "  padding: 4px 6px;"
                           "  border-bottom: 1px solid #cccccc;"
                           "}"
                           "QMenuBar::item {"
                           "  padding: 2px 6px;"
                           "  background: transparent;"
                           "}"
                           "QMenuBar::item:selected {"
                           "  background: #3e3e42;"
                           "  color: #ffffff;"
                           "}"
                           "QMenu {"
                           "  background: #ffffff;"
                           "  color: #1a1a1a;"
                           "  border: 1px solid #cccccc;"
                           "}"
                           "QMenu::item {"
                           "  padding: 4px 20px 4px 6px;"
                           "}"
                           "QMenu::item:selected {"
                           "  background: #094771;"
                           "  color: #ffffff;"
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
  m_indicesViewAction->setChecked(true);
  connect(m_indicesViewAction, &QAction::toggled, this, [this](bool visible) {
    if (m_indicesDock)
      m_indicesDock->setVisible(visible);
  });

  m_allIndicesAction = viewMenu->addAction("&All Indices...");
  m_allIndicesAction->setCheckable(false);
  connect(m_allIndicesAction, &QAction::triggered, this, [this]() {
    if (m_allIndicesWindow) {
      m_allIndicesWindow->show();
      m_allIndicesWindow->raise();
      m_allIndicesWindow->activateWindow();
    }
  });

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

  // DARK STYLE
  m_toolBar->setStyleSheet("QWidget { "
                           "   background-color: #e1e1e1; "
                           "} "
                           "QToolBar { "
                           "   background-color: #e1e1e1; "
                           "   border: none; "
                           "   color: #1e1e1e; "
                           "} "
                           "QToolButton { "
                           "   color: #1e1e1e; "
                           "   border: none; "
                           "   padding: 2px; "
                           "} "
                           "QToolButton:hover { "
                           "   background-color: #505050; "
                           "} "
                           "QToolButton:pressed { "
                           "   background-color: #094771; "
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
  // Dark style
  m_connectionBar->setStyleSheet("QWidget { "
                                 "   background-color: #e1e1e1; "
                                 "} "
                                 "QLabel { "
                                 "   color: #1e1e1e; "
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
  // Dark style
  m_statusBar->setStyleSheet("QStatusBar { "
                             "   background-color: #1e1e1e; "
                             "   color: #d4d4d8; "
                             "   border-top: 1px solid #3f3f46; "
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
      "QWidget { background-color: #e1e1e1; border-top: 1px solid #3f3f46; }"
      "QLabel { color: #1a1a1a; font-size: 11px; }");

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

void MainWindow::saveCurrentWorkspace() {
  bool ok;
  QString name = QInputDialog::getText(
      this, "Save Workspace", "Workspace name:", QLineEdit::Normal, "", &ok);
  if (!ok || name.isEmpty() || !m_mdiArea)
    return;

  // 1. Save Window Layout (Geometry, Type, etc.)
  m_mdiArea->saveWorkspace(name);

  // 2. Save Specific Content (Scrips, Profiles) for OPEN windows
  QSettings settings("TradingCompany", "TradingTerminal");
  settings.beginGroup("workspaces/" + name);

  QList<CustomMDISubWindow *> windows = m_mdiArea->windowList();
  for (int i = 0; i < windows.count(); ++i) {
    CustomMDISubWindow *window = windows[i];
    if (!window->contentWidget())
      continue;

    settings.beginGroup(QString("window_%1").arg(i));

    if (window->windowType() == "MarketWatch") {
      MarketWatchWindow *mw =
          qobject_cast<MarketWatchWindow *>(window->contentWidget());
      if (mw)
        mw->saveState(settings);
    } else {
      // Check for BaseBookWindow (OrderBook, TradeBook, PositionWindow)
      BaseBookWindow *book =
          qobject_cast<BaseBookWindow *>(window->contentWidget());
      if (book)
        book->saveState(settings);
    }

    settings.endGroup();
  }

  // 3. Save Default Profiles for specific window types (even if not open)
  // This ensures that when we load this workspace, these window types will use
  // the saved profile
  settings.beginGroup("global_profiles");
  QStringList types = {"OrderBook", "TradeBook", "PositionWindow"};
  GenericProfileManager pm("profiles");
  for (const auto &type : types) {
    GenericTableProfile p;
    if (pm.loadProfile(type, pm.getDefaultProfileName(type), p)) {
      settings.setValue(
          type, QJsonDocument(p.toJson()).toJson(QJsonDocument::Compact));
    }
  }
  settings.endGroup();

  settings.endGroup();
}

void MainWindow::loadWorkspace() {
  if (!m_mdiArea)
    return;
  QStringList workspaces = m_mdiArea->availableWorkspaces();
  if (workspaces.isEmpty())
    return;
  bool ok;
  QString name = QInputDialog::getItem(
      this, "Load Workspace", "Select workspace:", workspaces, 0, false, &ok);
  if (!ok || name.isEmpty())
    return;

  loadWorkspaceByName(name);
}

bool MainWindow::loadWorkspaceByName(const QString &name) {
  if (!m_mdiArea || name.isEmpty())
    return false;

  // Check if workspace exists
  if (!m_mdiArea->availableWorkspaces().contains(name)) {
    return false;
  }

  // 1. Restore Global Profiles (so newly created windows pick them up)
  QSettings settings("TradingCompany", "TradingTerminal");
  settings.beginGroup("workspaces/" + name + "/global_profiles");

  GenericProfileManager pm("profiles");
  QStringList types = settings.childKeys(); // "OrderBook", etc.
  for (const auto &type : types) {
    QByteArray data = settings.value(type).toByteArray();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isObject()) {
      GenericTableProfile p;
      p.fromJson(doc.object());
      // Save as default so new windows use it
      pm.saveProfile(type, p);
      pm.saveDefaultProfile(type, p.name());
    }
  }
  settings.endGroup();

  // 2. Load Layout (this triggers onRestoreWindowRequested for each window)
  m_mdiArea->loadWorkspace(name);
  return true;
}

void MainWindow::manageWorkspaces() {
  // Basic implementation or dialog
}

void MainWindow::createIndicesView() {
  // ⏱️ PERFORMANCE LOG: Track IndicesView creation time
  QElapsedTimer indicesTimer;
  indicesTimer.start();
  qint64 startTime = QDateTime::currentMSecsSinceEpoch();

  qDebug() << "========================================";
  qDebug() << "[PERF] [INDICES_CREATE] START";
  qDebug() << "  Timestamp:" << startTime;
  qDebug() << "========================================";

  // ✅ DEFERRED CREATION: This is now called ONLY after login completes
  // Create standalone tool window (subwindow to main app)
  // Parented to this so it closes with app, Qt::Tool makes it a lightweight
  // floating window
  qint64 t0 = indicesTimer.elapsed();
  m_indicesView = new IndicesView(this);
  qint64 t1 = indicesTimer.elapsed();

  m_indicesView->setWindowTitle("Indices");
  // ✅ FIX: Removed WindowStaysOnTopHint - now only stays with parent app, not
  // over other apps
  m_indicesView->setWindowFlags(Qt::Tool | Qt::WindowCloseButtonHint);

  // Set initial size
  m_indicesView->resize(400, 120);
  qint64 t2 = indicesTimer.elapsed();

  // ✅ Thread-Safe: Use QueuedConnection since UDP callbacks are on background
  // threads
  connect(&UdpBroadcastService::instance(),
          &UdpBroadcastService::udpIndexReceived, m_indicesView,
          &IndicesView::onIndexReceived, Qt::QueuedConnection);
  qint64 t3 = indicesTimer.elapsed();

  // ✅ Initialize with repository data to pre-populate index names
  if (RepositoryManager::getInstance()) {
    m_indicesView->initialize(RepositoryManager::getInstance());
  }

  // Create All Indices Window
  m_allIndicesWindow = new AllIndicesWindow(this);
  if (RepositoryManager::getInstance()) {
    m_allIndicesWindow->initialize(RepositoryManager::getInstance());
    // Load current selection
    QSettings settings("TradingCompany", "TradingTerminal");
    QStringList selected = settings.value("indices/selected").toStringList();
    m_allIndicesWindow->setSelectedIndices(selected);
  }

  // Connect selection changes from All Indices to Indices View
  connect(m_allIndicesWindow, &AllIndicesWindow::selectionChanged, this,
          [this](const QStringList &selected) {
            if (m_indicesView) {
              m_indicesView->reloadSelectedIndices(selected);
            }
          });

  // Restore position if saved
  // For now, position relative to main window
  // m_indicesView->move(this->x() + this->width() - 410, this->y() + 50);

  // Check user preference for visibility
  bool indicesVisible = QSettings("TradingCompany", "TradingTerminal")
                            .value("mainwindow/indices_visible", true)
                            .toBool();
  qint64 t4 = indicesTimer.elapsed();

  // Update action state
  if (m_indicesViewAction) {
    m_indicesViewAction->setChecked(indicesVisible);

    // Connect action to view visibility
    connect(m_indicesViewAction, &QAction::toggled, this, [this](bool visible) {
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

    // ✅ Connect hideRequested signal to update action state when user closes
    // window NOTE: When user closes with X button, we only uncheck the menu
    // item but DON'T save the preference as false - window should reopen on
    // next launch
    connect(m_indicesView, &IndicesView::hideRequested, this, [this]() {
      if (m_indicesViewAction) {
        // Temporarily disconnect to avoid triggering save
        m_indicesViewAction->blockSignals(true);
        m_indicesViewAction->setChecked(false);
        m_indicesViewAction->blockSignals(false);
      }
      // DON'T save preference here - keep it as true for next launch
    });
  }
  qint64 t5 = indicesTimer.elapsed();

  // Show if user preference was enabled
  if (indicesVisible) {
    m_indicesView->show();
    m_indicesView->raise();
  }
  qint64 t6 = indicesTimer.elapsed();

  qint64 totalTime = indicesTimer.elapsed();
  qDebug() << "========================================";
  qDebug() << "[PERF] [INDICES_CREATE] COMPLETE";
  qDebug() << "  TOTAL TIME:" << totalTime << "ms";
  qDebug() << "  Breakdown:";
  qDebug() << "    - Create IndicesView widget:" << (t1 - t0) << "ms";
  qDebug() << "    - Set window properties:" << (t2 - t1) << "ms";
  qDebug() << "    - Connect UDP signal:" << (t3 - t2) << "ms";
  qDebug() << "    - Load visibility preference:" << (t4 - t3) << "ms";
  qDebug() << "    - Setup action connections:" << (t5 - t4) << "ms";
  qDebug() << "    - Show window (if enabled):" << (t6 - t5) << "ms";
  qDebug() << "========================================";
}
