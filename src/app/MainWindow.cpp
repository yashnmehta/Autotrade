#include "app/MainWindow.h"
#include "market_data_callback.h"
#include "core/widgets/CustomTitleBar.h"
#include "core/widgets/CustomMDIArea.h"
#include "core/widgets/CustomMDISubWindow.h"
#include "app/ScripBar.h"
#include "core/widgets/InfoBar.h"
#include "views/MarketWatchWindow.h"
#include "views/BuyWindow.h"
#include "views/SellWindow.h"
#include "views/SnapQuoteWindow.h"
#include "views/OptionChainWindow.h"
#include "views/PositionWindow.h"
#include "views/TradeBookWindow.h"
#include "views/OrderBookWindow.h"
#include "views/Preference.h"
#include "views/CustomizeDialog.h"
#include "repository/Greeks.h"
#include "api/XTSMarketDataClient.h"
#include "api/XTSInteractiveClient.h"
#include "services/PriceCache.h"
#include "services/FeedHandler.h"  // Phase 1: Direct callback-based tick distribution
#include "utils/LatencyTracker.h"  // Latency tracking
#include <thread>
#include <chrono>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QTableWidget>
#include <QHeaderView>
#include <QToolBar>
#include <QStatusBar>
#include <QDockWidget>
#include <QInputDialog>
#include <QMessageBox>
#include <QDialog>
#include <QListWidget>
#include <QShortcut>
#include <QComboBox>
#include <QSettings>
#include <QCloseEvent>
#include <QDebug>

MainWindow::MainWindow(QWidget *parent)
    : CustomMainWindow(parent)
    , m_xtsMarketDataClient(nullptr)
    , m_xtsInteractiveClient(nullptr)
    , m_tickDrainTimer(nullptr)
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
    qDebug() << "";
    qDebug() << "[MainWindow] ⚠️ UDP Broadcast NOT auto-started";
    qDebug() << "[MainWindow] ⚠️ To receive UDP data: Data Menu → Start NSE Broadcast Receiver";
    qDebug() << "[MainWindow] ⚠️ Without UDP, relying on XTS WebSocket (higher latency)";
    qDebug() << "";

    // Restore visibility preferences
    QSettings s("TradingCompany", "TradingTerminal");
    if (m_infoDock)
        m_infoDock->setVisible(s.value("mainwindow/info_visible", true).toBool());
    if (m_statusBar)
        m_statusBar->setVisible(s.value("mainwindow/status_visible", true).toBool());
}

MainWindow::~MainWindow()
{
    // Stop UDP receiver if running
    stopBroadcastReceiver();
}

void MainWindow::setXTSClients(XTSMarketDataClient *mdClient, XTSInteractiveClient *iaClient)
{
    m_xtsMarketDataClient = mdClient;
    m_xtsInteractiveClient = iaClient;
    
    // Connect to market data tick updates
    if (m_xtsMarketDataClient) {
        connect(m_xtsMarketDataClient, &XTSMarketDataClient::tickReceived, 
                this, &MainWindow::onTickReceived);
        qDebug() << "[MainWindow] Connected to XTS tickReceived signal";
    }
    
    // Pass to ScripBar if it exists
    if (m_scripBar && m_xtsMarketDataClient) {
        m_scripBar->setXTSClient(m_xtsMarketDataClient);
        qDebug() << "[MainWindow] XTS clients set and passed to ScripBar";
    }
}

void MainWindow::refreshScripBar()
{
    if (m_scripBar) {
        m_scripBar->refreshSymbols();
        qDebug() << "[MainWindow] ScripBar symbols refreshed after masters loaded";
    }
}

void MainWindow::setupContent()
{
    // Get the central widget container from CustomMainWindow
    // This contains the main vertical layout where we add our content
    QWidget *container = centralWidget();
    if (!container)
    {
        qDebug() << "ERROR: No central widget from CustomMainWindow!";
        return;
    }

    QVBoxLayout *layout = qobject_cast<QVBoxLayout *>(container->layout());
    if (!layout)
    {
        qDebug() << "ERROR: No layout on central widget!";
        return;
    }

    // Create menu bar (custom widget, NOT QMainWindow::menuBar())
    createMenuBar();

    // Create custom toolbar area using nested QMainWindow
    // This maintains dockable/floatable functionality while controlling position
    QMainWindow *toolbarHost = new QMainWindow(container);
    toolbarHost->setWindowFlags(Qt::Widget); // Make it behave as a widget, not a window
    toolbarHost->setStyleSheet(
        "QMainWindow { background-color: #aaaaaa;  }"
        "QMainWindow::separator { background: #3e3e42; width: 1px; height: 1px; }");
    
    // Set dynamic sizing policy - expands horizontally, fits content vertically
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
    m_connectionToolBar->setStyleSheet(
        "QToolBar { "
        "   background-color: #f5f5f5; "
        "   border: none; "
        "   spacing: 2px; "
        "} "
        "QToolButton { "
        "   color: #111111; "
        "   border: none; "
        "   padding: 2px; "
        "} "
        "QToolButton:hover { "
        "   background-color: #3e3e42; "
        "} "
        "QToolButton:pressed { "
        "   background-color: #094771; "
        "}");
    m_connectionToolBar->addWidget(m_connectionBar);
    m_connectionToolBar->setMovable(true);
    m_connectionToolBar->setFloatable(true);
    m_connectionToolBar->setAllowedAreas(Qt::TopToolBarArea);
    toolbarHost->addToolBarBreak(Qt::TopToolBarArea);  // Force new line
    toolbarHost->addToolBar(Qt::TopToolBarArea, m_connectionToolBar);

    // Create scrip bar toolbar
    m_scripBar = new ScripBar(toolbarHost);
    connect(m_scripBar, &ScripBar::addToWatchRequested, this, &MainWindow::onAddToWatchRequested);
    m_scripToolBar = new QToolBar(tr("Scrip Bar"), toolbarHost);
    m_scripToolBar->setStyleSheet(
        "QToolBar { "
        "   background-color: #f5f5f5; "
        "   border: none; "
        "   spacing: 2px; "
        "} "
        "QToolButton { "
        "   background-color: transparent; "
        "   color: #ffffff; "
        "   border: none; "
        "   padding: 4px; "
        "} "
        "QToolButton:hover { "
        "   background-color: #3e3e42; "
        "} "
        "QToolButton:pressed { "
        "   background-color: #094771; "
        "}");
    m_scripToolBar->addWidget(m_scripBar);
    m_scripToolBar->setMovable(true);
    m_scripToolBar->setFloatable(true);
    m_scripToolBar->setAllowedAreas(Qt::TopToolBarArea);
    toolbarHost->addToolBarBreak(Qt::TopToolBarArea);  // Force new line
    toolbarHost->addToolBar(Qt::TopToolBarArea, m_scripToolBar);
    
    // Create a dummy central widget for the toolbar host (required by QMainWindow)
    QWidget *dummyWidget = new QWidget(toolbarHost);
    dummyWidget->setFixedHeight(0); // Make it invisible
    dummyWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    toolbarHost->setCentralWidget(dummyWidget);
    
    // Add the toolbar host to main layout (below menu bar)
    // No stretch factor - takes only the space it needs
    layout->addWidget(toolbarHost);

    // Custom MDI Area (main content area)
    m_mdiArea = new CustomMDIArea(container);
    m_mdiArea->setStyleSheet("CustomMDIArea { background-color: #f5f5f5; }");
    layout->addWidget(m_mdiArea, 1); // Give it stretch factor so it expands

    // Create info bar (inline widget) and add above status bar
    createInfoBar();
    layout->addWidget(m_infoBar);

    // Create status bar at the bottom
    createStatusBar();
    layout->addWidget(m_statusBar);

    // Adjust central widget layout
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Force initial layout update
    container->updateGeometry();
    container->adjustSize();
}

void MainWindow::setupShortcuts()
{
    qDebug() << "[MainWindow] Setting up keyboard shortcuts...";
    
    // F1: Open Buy Window
    QShortcut *buyShortcut = new QShortcut(QKeySequence(Qt::Key_F1), this);
    connect(buyShortcut, &QShortcut::activated, this, &MainWindow::createBuyWindow);
    qDebug() << "  F1 -> Buy Window";
    
    // F2: Open Sell Window
    QShortcut *sellShortcut = new QShortcut(QKeySequence(Qt::Key_F2), this);
    connect(sellShortcut, &QShortcut::activated, this, &MainWindow::createSellWindow);
    qDebug() << "  F2 -> Sell Window";
    
    // F3: Open Order Book
    QShortcut *orderBookShortcut = new QShortcut(QKeySequence(Qt::Key_F3), this);
    connect(orderBookShortcut, &QShortcut::activated, this, &MainWindow::createOrderBookWindow);
    qDebug() << "  F3 -> Order Book";
    
    // F4: New Market Watch
    QShortcut *marketWatchShortcut = new QShortcut(QKeySequence(Qt::Key_F4), this);
    connect(marketWatchShortcut, &QShortcut::activated, this, &MainWindow::createMarketWatch);
    qDebug() << "  F4 -> Market Watch";
    
    // F5: Open SnapQuote Window
    QShortcut *snapQuoteShortcut = new QShortcut(QKeySequence(Qt::Key_F5), this);
    connect(snapQuoteShortcut, &QShortcut::activated, this, &MainWindow::createSnapQuoteWindow);
    qDebug() << "  F5 -> SnapQuote Window";
    
    // F8: Open Tradebook
    QShortcut *tradebookShortcut = new QShortcut(QKeySequence(Qt::Key_F8), this);
    connect(tradebookShortcut, &QShortcut::activated, this, &MainWindow::createTradeBookWindow);
    qDebug() << "  F8 -> TradeBook Window";
    
    // Alt+F6: Open Net Position Window
    QShortcut *positionShortcut = new QShortcut(QKeySequence(Qt::ALT | Qt::Key_F6), this);
    connect(positionShortcut, &QShortcut::activated, this, &MainWindow::createPositionWindow);
    qDebug() << "  Alt+F6 -> Net Position Window";
    
    // Alt+S: Focus on Exchange combobox (cbExch) in ScripBar
    QShortcut *focusExchangeShortcut = new QShortcut(QKeySequence(Qt::ALT | Qt::Key_S), this);
    connect(focusExchangeShortcut, &QShortcut::activated, this, [this]() {
        if (m_scripBar) {
            // Find the exchange combobox by object name
            QComboBox *exchangeCombo = m_scripBar->findChild<QComboBox*>("cbExchange");
            if (exchangeCombo) {
                exchangeCombo->setFocus();
                exchangeCombo->showPopup(); // Auto-open dropdown for quick selection
                qDebug() << "[MainWindow] Alt+S: Focused on exchange combobox";
            } else {
                qWarning() << "[MainWindow] Alt+S: Exchange combobox not found";
            }
        }
    });
    qDebug() << "  Alt+S -> Focus Exchange Combobox";

    // Ctrl+R: Open Preferences
    QShortcut *preferencesShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_R), this);
    connect(preferencesShortcut, &QShortcut::activated, this, &MainWindow::showPreferences);
    qDebug() << "  Ctrl+R -> Preferences Window";
    
    qDebug() << "[MainWindow] Keyboard shortcuts setup complete";
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    // Save user preferences
    QSettings s("TradingCompany", "TradingTerminal");
    if (m_infoDock)
        s.setValue("mainwindow/info_visible", m_infoDock->isVisible());
    if (m_statusBar)
        s.setValue("mainwindow/status_visible", m_statusBar->isVisible());
    // Let base class handle the rest
    CustomMainWindow::closeEvent(event);
}

void MainWindow::createMenuBar()
{
    // Create a custom menu bar widget (NOT using QMainWindow::menuBar())
    // This avoids conflicts with our custom title bar and layout
    // CRITICAL: Create with nullptr parent first to avoid auto-positioning
    QMenuBar *menuBar = new QMenuBar(nullptr);

    // CRITICAL for ALL platforms: Force native=false BEFORE adding to layout
    // This prevents macOS/Linux from positioning it at window/desktop level
    menuBar->setNativeMenuBar(false);

    // Force menu bar to take full width and be part of vertical layout
    menuBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);

    menuBar->setStyleSheet(
        "QMenuBar {"
        "  background: #f7f7f7;"
        "  color: #222222;"
        "  font-size: 11px;"
        "  padding: 2px 0;"
        "  border-bottom: 1px solid #dcdcdc;"
        "  min-height: 20px;"
        "}"
        "QMenuBar::item {"
        "  font-size: 10px;"

        "  padding: 2px 4px;"
        "  margin: 0 2px;"
        "  border-radius: 3px;"
        "}"
        "QMenuBar::item:selected {"
        "  font-size: 10px;"

        "  background: #2a6fb3;"
        "  color: #ffffff;"
        "}"
        "QMenu {"
        "  background: #ffffff;"
        "  color: #222222;"
        "  border: 1px solid #dcdcdc;"
        "  padding: 3px 0;"
        "}"
        "QMenu::item {"
        "  font-size: 10px;"
        "  padding: 3px 6px;"
        "}"
        "QMenu::separator {"
        "  height: 1px;"
        "  background: #eeeeee;"
        "  margin: 6px 0;"
        "}"
        "QMenu::item:selected {"
        "  background: #e6f0fb;"
        "  color: #0b57a4;"
        "}");

    // CRITICAL: Set fixed height to prevent menu bar from being too tall
    menuBar->setMaximumHeight(30);

    // Platform-specific attachment: macOS uses the native system menu bar,
    // other platforms embed the menu bar inside the window (top of central widget).
#ifdef Q_OS_MAC
    menuBar->setNativeMenuBar(true);
    qDebug() << "Using native macOS menu bar";
#else
    menuBar->setNativeMenuBar(false);
    // Force menu bar to take full width and natural height
    menuBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);

    QVBoxLayout *layout = qobject_cast<QVBoxLayout *>(centralWidget()->layout());
    if (layout)
    {
        // Reparent to central widget and add to layout
        menuBar->setParent(centralWidget());
        layout->insertWidget(0, menuBar); // Insert at position 0 (top of content)
        qDebug() << "Menu bar added to layout at position 0";
    }
    else
    {
        qDebug() << "ERROR: Could not get layout to add menu bar!";
    }
#endif

    // File Menu
    QMenu *fileMenu = menuBar->addMenu("&File");
    QAction *exitAction = fileMenu->addAction("E&xit");
    connect(exitAction, &QAction::triggered, this, &QWidget::close);

    // Edit Menu
    QMenu *editMenu = menuBar->addMenu("&Edit");
    QAction *preferencesAction = editMenu->addAction("&Preferences\tCtrl+R");
    connect(preferencesAction, &QAction::triggered, this, &MainWindow::showPreferences);

    // View Menu
    QMenu *viewMenu = menuBar->addMenu("&View");
    viewMenu->addAction("&Toolbar");
    m_statusBarAction = viewMenu->addAction("&Status Bar");
    m_statusBarAction->setCheckable(true);
    m_statusBarAction->setChecked(true); // Will be updated after statusBar is created
    connect(m_statusBarAction, &QAction::toggled, this, [this](bool visible)
            { if (m_statusBar) m_statusBar->setVisible(visible); });

    m_infoBarAction = viewMenu->addAction("&Info Bar");
    m_infoBarAction->setCheckable(true);
    m_infoBarAction->setChecked(true); // Will be updated after infoBar is created
    connect(m_infoBarAction, &QAction::toggled, this, [this](bool visible)
            { if (m_infoDock) m_infoDock->setVisible(visible); });
    viewMenu->addSeparator();
    viewMenu->addAction("&Fullscreen");
    QAction *resetLayoutAction = viewMenu->addAction("Reset &Layout");
    connect(resetLayoutAction, &QAction::triggered, this, &MainWindow::resetLayout);

    // Window Menu - Create windows and manage MDI layout
    QMenu *windowMenu = menuBar->addMenu("&Window");
    
    // Create Window Actions
    QAction *wmMarketWatch = windowMenu->addAction("&MarketWatch\tF4");
    connect(wmMarketWatch, &QAction::triggered, this, &MainWindow::createMarketWatch);
    
    QAction *wmBuy = windowMenu->addAction("&Buy\tF1");
    connect(wmBuy, &QAction::triggered, this, &MainWindow::createBuyWindow);
    
    QAction *wmSell = windowMenu->addAction("&Sell\tF2");
    connect(wmSell, &QAction::triggered, this, &MainWindow::createSellWindow);
    
    QAction *wmSnap = windowMenu->addAction("Snap&Quote\tF5");
    connect(wmSnap, &QAction::triggered, this, &MainWindow::createSnapQuoteWindow);
    
    QAction *wmOptionChain = windowMenu->addAction("&Option Chain\tF6");
    connect(wmOptionChain, &QAction::triggered, this, &MainWindow::createOptionChainWindow);
    
    QAction *wmOrderBook = windowMenu->addAction("&OrderBook\tF3");
    connect(wmOrderBook, &QAction::triggered, this, &MainWindow::createOrderBookWindow);
    
    QAction *wmTradeBook = windowMenu->addAction("&TradeBook\tF8");
    connect(wmTradeBook, &QAction::triggered, this, &MainWindow::createTradeBookWindow);
    
    QAction *wmPosition = windowMenu->addAction("Net &Position\tAlt+F6");
    connect(wmPosition, &QAction::triggered, this, &MainWindow::createPositionWindow);
    
    windowMenu->addSeparator();

    // Window Arrangement
    QAction *cascadeAction = windowMenu->addAction("&Cascade", this, [this]()
                                                   { m_mdiArea->cascadeWindows(); });

    QAction *tileAction = windowMenu->addAction("&Tile", this, [this]()
                                                { m_mdiArea->tileWindows(); });

    QAction *tileHorzAction = windowMenu->addAction("Tile &Horizontally", this, [this]()
                                                    { m_mdiArea->tileHorizontally(); });

    QAction *tileVertAction = windowMenu->addAction("Tile &Vertically", this, [this]()
                                                    { m_mdiArea->tileVertically(); });

    windowMenu->addSeparator();

    // Window Navigation
    QAction *nextWindowAction = windowMenu->addAction("&Next Window", this, [this]()
                                                      {
        QList<CustomMDISubWindow*> windows = m_mdiArea->windowList();
        if (windows.count() > 1) {
            CustomMDISubWindow *current = m_mdiArea->activeWindow();
            int index = windows.indexOf(current);
            int nextIndex = (index + 1) % windows.count();
            if (nextIndex < 0) nextIndex = 0;
            m_mdiArea->activateWindow(windows[nextIndex]);
        } });
    // QShortcut for cross-platform window switching
#ifdef Q_OS_MACOS
    // Cmd+` is standard macOS shortcut for cycling windows
    QShortcut *nextShortcut = new QShortcut(QKeySequence("Meta+`"), this);
#else
    QShortcut *nextShortcut = new QShortcut(QKeySequence("Ctrl+Tab"), this);
#endif
    nextShortcut->setContext(Qt::ApplicationShortcut);
    connect(nextShortcut, &QShortcut::activated, this, [this]()
            {
        QList<CustomMDISubWindow*> windows = m_mdiArea->windowList();
        if (windows.count() > 1) {
            CustomMDISubWindow *current = m_mdiArea->activeWindow();
            int index = windows.indexOf(current);
            int nextIndex = (index + 1) % windows.count();
            if (nextIndex < 0) nextIndex = 0;
            m_mdiArea->activateWindow(windows[nextIndex]);
        } });

    QAction *prevWindowAction = windowMenu->addAction("&Previous Window", this, [this]()
                                                      {
        QList<CustomMDISubWindow*> windows = m_mdiArea->windowList();
        if (windows.count() > 1) {
            CustomMDISubWindow *current = m_mdiArea->activeWindow();
            int index = windows.indexOf(current);
            int prevIndex = (index - 1 + windows.count()) % windows.count();
            if (prevIndex < 0) prevIndex = 0;
            m_mdiArea->activateWindow(windows[prevIndex]);
        } });
#ifdef Q_OS_MACOS
    // Cmd+Shift+` is standard macOS shortcut for cycling windows backwards
    QShortcut *prevShortcut = new QShortcut(QKeySequence("Meta+Shift+`"), this);
#else
    QShortcut *prevShortcut = new QShortcut(QKeySequence("Ctrl+Shift+Tab"), this);
#endif
    prevShortcut->setContext(Qt::ApplicationShortcut);
    connect(prevShortcut, &QShortcut::activated, this, [this]()
            {
        QList<CustomMDISubWindow*> windows = m_mdiArea->windowList();
        if (windows.count() > 1) {
            CustomMDISubWindow *current = m_mdiArea->activeWindow();
            int index = windows.indexOf(current);
            int prevIndex = (index - 1 + windows.count()) % windows.count();
            if (prevIndex < 0) prevIndex = 0;
            m_mdiArea->activateWindow(windows[prevIndex]);
        } });

    QAction *closeWindowAction = windowMenu->addAction("&Close Window", this, [this]()
                                                       {
        CustomMDISubWindow *active = m_mdiArea->activeWindow();
        if (active) active->close(); }, QKeySequence("Ctrl+W"));

    windowMenu->addSeparator();

    // Maximize/Minimize
    QAction *maximizeAction = windowMenu->addAction("Ma&ximize Window", this, [this]()
                                                    {
        CustomMDISubWindow *active = m_mdiArea->activeWindow();
        if (active) active->maximize(); }, QKeySequence("F11"));

    windowMenu->addSeparator();

    // Workspace Management
    QAction *saveWorkspaceAction = windowMenu->addAction("&Save Workspace...", this, &MainWindow::saveCurrentWorkspace, QKeySequence("Ctrl+Shift+S"));
    QAction *loadWorkspaceAction = windowMenu->addAction("&Load Workspace...", this, &MainWindow::loadWorkspace, QKeySequence("Ctrl+Shift+O"));
    QAction *manageWorkspacesAction = windowMenu->addAction("&Manage Workspaces...", this, &MainWindow::manageWorkspaces);

    // Tools Menu
    QMenu *toolsMenu = menuBar->addMenu("&Tools");
    toolsMenu->addAction("&Options Calculator");
    toolsMenu->addAction("&Strategy Builder");

    // Data Menu
    QMenu *dataMenu = menuBar->addMenu("&Data");
    QAction *startBroadcastAction = dataMenu->addAction("Start &NSE Broadcast Receiver");
    connect(startBroadcastAction, &QAction::triggered, this, &MainWindow::startBroadcastReceiver);
    
    QAction *stopBroadcastAction = dataMenu->addAction("St&op NSE Broadcast Receiver");
    connect(stopBroadcastAction, &QAction::triggered, this, &MainWindow::stopBroadcastReceiver);

    // Help Menu
    QMenu *helpMenu = menuBar->addMenu("&Help");
    helpMenu->addAction("&About");
    helpMenu->addAction("&Documentation");

    // Note: menu bar is added to the central widget layout above so it appears
    // as a full-width bar at the top of the content area. We intentionally do
    // not insert the menu into the custom title bar to avoid cramped/side-by-side
    // placement with the window controls on some Linux desktop environments.
}

void MainWindow::resetLayout()
{
    QSettings s("TradingCompany", "TradingTerminal");
    s.remove("mainwindow/state");
    // Restore to default (empty) state
    restoreState(QByteArray());
}

void MainWindow::createToolBar()
{
    m_toolBar = new QToolBar("Main Toolbar", this);
    m_toolBar->setAllowedAreas(Qt::AllToolBarAreas);
    m_toolBar->setMovable(true);
    m_toolBar->setFloatable(true);
    m_toolBar->setIconSize(QSize(16, 16));
    m_toolBar->setFixedHeight( 30 );
    m_toolBar->setStyleSheet(
        "QToolBar { "
        "   background-color: #f1f1f1; "
        "} "
        "QToolButton { "
        "   background-color: transparent; "
        "   color: #242424; "
        "   border: none; "
        "   padding: 2px; "
        "} "
        "QToolButton:hover { "
        "   background-color: #3e3e42; "
        "} "
        "QToolButton:pressed { "
        "   background-color: #094771; "
        "}");

    // Add toolbar actions
    QAction *newMarketWatch = m_toolBar->addAction("Market Watch");
    newMarketWatch->setIcon(QIcon(":/icons/market_watch.png")); // Placeholder
    connect(newMarketWatch, &QAction::triggered, this, &MainWindow::createMarketWatch);

    QAction *newBuyOrder = m_toolBar->addAction("Buy Order");
    newBuyOrder->setIcon(QIcon(":/icons/buy_order.png")); // Placeholder
    connect(newBuyOrder, &QAction::triggered, this, &MainWindow::createBuyWindow);

    QAction *newSellOrder = m_toolBar->addAction("Sell Order");
    newSellOrder->setIcon(QIcon(":/icons/sell_order.png")); // Placeholder
    connect(newSellOrder, &QAction::triggered, this, &MainWindow::createSellWindow);

    QAction *newSnapQuote = m_toolBar->addAction("Snap Quote");
    newSnapQuote->setIcon(QIcon(":/icons/snap_quote.png")); // Placeholder
    connect(newSnapQuote, &QAction::triggered, this, &MainWindow::createSnapQuoteWindow);

    m_toolBar->addSeparator();

    QAction *connectAction = m_toolBar->addAction("Connect");
    connectAction->setIcon(QIcon(":/icons/connect.png")); // Placeholder

    QAction *disconnectAction = m_toolBar->addAction("Disconnect");
    disconnectAction->setIcon(QIcon(":/icons/disconnect.png")); // Placeholder
}

void MainWindow::createConnectionBar()
{
    m_connectionBar = new QWidget(this);
    // Slightly more compact connection bar to fit toolbar height
    m_connectionBar->setFixedHeight(28);
    m_connectionBar->setStyleSheet(
        "QWidget { "
        "   background-color: #f1f1f1; "
        "} "
        "QLabel { "

        "   color: #242424; "
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
    statusValue->setStyleSheet("color: #ff6b6b; font-weight: bold; font-size: 10px;");
    layout->addWidget(statusValue);

    layout->addStretch();

    QLabel *serverLabel = new QLabel("Server: XTS API", m_connectionBar);
    layout->addWidget(serverLabel);

    QLabel *userLabel = new QLabel("User: -", m_connectionBar);
    layout->addWidget(userLabel);
}

void MainWindow::createStatusBar()
{
    m_statusBar = new QStatusBar(this);
    m_statusBar->setStyleSheet(
        "QStatusBar { "
        "   background-color: #2d2d30; "
        "   color: #cccccc; "
        "   border-top: 1px solid #3e3e42; "
        "} "
        "QStatusBar::item { "
        "   border: none; "
        "}");

    m_statusBar->showMessage("Ready");
    m_statusBar->addPermanentWidget(new QLabel("Market: Closed"));
    m_statusBar->addPermanentWidget(new QLabel("Last Update: --"));
}

void MainWindow::createInfoBar()
{
    // Info bar is implemented as a bottom QDockWidget so it's not part of central widget
    // It is not movable/floatable and can be shown/hidden via the View menu.
    m_infoDock = new QDockWidget(this);
    m_infoDock->setAllowedAreas(Qt::BottomDockWidgetArea);
    // Allow only close (to hide) but not float or move to other areas
    m_infoDock->setFeatures(QDockWidget::DockWidgetClosable);
    // Remove the title bar for a compact look
    m_infoDock->setTitleBarWidget(new QWidget());

#ifdef Q_OS_MAC
    m_infoBar = new InfoBar(this);
    m_infoBar->setFixedHeight(50);
#else
    m_infoBar = new InfoBar(this);
    m_infoBar->setFixedHeight(50);
#endif

    // InfoBar is responsible for its internal layout

    m_infoDock->setWidget(m_infoBar);
    connect(m_infoBar, &InfoBar::hideRequested, this, [this]()
            {
        if (m_infoDock)
        {
            m_infoDock->hide();
            if (m_infoBarAction)
                m_infoBarAction->setChecked(false);
            QSettings s("TradingCompany", "TradingTerminal");
            s.setValue("mainwindow/info_visible", false);
        } });
    connect(m_infoBar, &InfoBar::detailsRequested, this, [this]()
            {
        // stub: show info details dialog
        qDebug() << "InfoBar details requested"; });
    addDockWidget(Qt::BottomDockWidgetArea, m_infoDock);
    // Keep View Menu in sync when user manually toggles dock visibility
    connect(m_infoDock, &QDockWidget::visibilityChanged, this, [this](bool visible)
            {
        if (m_infoBarAction)
            m_infoBarAction->setChecked(visible);
        QSettings s("TradingCompany", "TradingTerminal");
        s.setValue("mainwindow/info_visible", visible); });
}
// InfoBar menu handled by InfoBar; no MainWindow callback needed

void MainWindow::createMarketWatch()
{
    static int counter = 1;

    CustomMDISubWindow *window = new CustomMDISubWindow(QString("Market Watch %1").arg(counter++), m_mdiArea);
    window->setWindowType("MarketWatch");

    // Create new MarketWatchWindow with token support
    MarketWatchWindow *marketWatch = new MarketWatchWindow(window);
    
    // Connect Buy/Sell signals
    connect(marketWatch, &MarketWatchWindow::buyRequested,
            this, [this](const QString &symbol, int token) {
        qDebug() << "Buy requested for" << symbol << "token:" << token;
        // TODO: Open buy window with pre-filled data
    });
    
    connect(marketWatch, &MarketWatchWindow::sellRequested,
            this, [this](const QString &symbol, int token) {
        qDebug() << "Sell requested for" << symbol << "token:" << token;
        // TODO: Open sell window with pre-filled data
    });

    window->setContentWidget(marketWatch);
    window->resize(900, 400);

    // Connect window signals using consolidated helper
    connectWindowSignals(window);

    // Add to MDI area
    m_mdiArea->addWindow(window);
    
    // Set focus to newly created window
    window->setFocus();
    window->raise();
    window->activateWindow();

    qDebug() << "Market Watch created with token support";
}

void MainWindow::createBuyWindow()
{
    CustomMDISubWindow *window = new CustomMDISubWindow("Buy Order", m_mdiArea);
    window->setWindowType("BuyWindow");

    // Check if there's an active Market Watch with valid selection
 
    //it should be getActiveSubwindow insted of getActiveMarketWatch as buy window can be initiated 
    //from snapquote/sell window/ netposition window also and its type should be subwindow not 
    //marketwatch but if we assign type to be sub window then how will we differentiate between 
    //buy/sell/netposition/snapquote window  and also how we will get context from those windows 

    MarketWatchWindow *activeMarketWatch = getActiveMarketWatch();
    // MarketWatchWindow *activeMarketWatch = getActiveMarketWatch();



    BuyWindow *buyWindow = nullptr;
    
    if (activeMarketWatch && activeMarketWatch->hasValidSelection()) {
        // Get context from selected scrip
        WindowContext context = activeMarketWatch->getSelectedContractContext();
        if (context.isValid()) {
            // Create BuyWindow with context (auto-fills contract details)
            buyWindow = new BuyWindow(context, window);
            qDebug() << "[MainWindow] Buy Window created with context:" << context.toString();
        } else {
            // Fallback to empty window
            buyWindow = new BuyWindow(window);
        }
    } else {
        // If active window is SnapQuote, try to use its context
        CustomMDISubWindow *activeSub = m_mdiArea ? m_mdiArea->activeWindow() : nullptr;
        if (activeSub) {
            SnapQuoteWindow *snap = qobject_cast<SnapQuoteWindow*>(activeSub->contentWidget());
            if (snap) {
                WindowContext ctx = snap->getContext();
                if (ctx.isValid()) {
                    buyWindow = new BuyWindow(ctx, window);
                    qDebug() << "[MainWindow] Buy Window created with SnapQuote context:" << ctx.toString();
                }
            }
        }

        // No selection or context - create empty BuyWindow
        if (!buyWindow) buyWindow = new BuyWindow(window);
    }
    
    window->setContentWidget(buyWindow);
    window->setMinimumWidth(1200);
    window->resize(1200, 200);
    window->setMinimumSize(1200, 250);
    window->setMaximumHeight(230);

    // Connect order submitted signal
    connect(buyWindow, &BuyWindow::orderSubmitted, [](const QString &exchange, int token, 
                                                        const QString &symbol, int qty, 
                                                        double price, const QString &orderType) {
        qDebug() << "[MainWindow] Buy Order Submitted:" 
                 << "Exchange:" << exchange 
                 << "Token:" << token
                 << "Symbol:" << symbol
                 << "Qty:" << qty 
                 << "Price:" << price
                 << "Type:" << orderType;
        // TODO: Connect to order management system
    });

    // Connect request to switch to SellWindow (F2)
    connect(buyWindow, &BuyWindow::requestSellWithContext, this,
            [this](const WindowContext &context) {
                // Create a sell window for the same contract
                CustomMDISubWindow *sellSub = new CustomMDISubWindow("Sell Order", m_mdiArea);
                sellSub->setWindowType("SellWindow");
                SellWindow *swin = new SellWindow(context, sellSub);
                sellSub->setContentWidget(swin);
                sellSub->resize(600, 250);
                
                // Connect MDI signals using helper
                connectWindowSignals(sellSub);
                
                m_mdiArea->addWindow(sellSub);
                qDebug() << "[MainWindow] Switched Buy->Sell for token:" << context.token;
            });

    // Connect MDI signals
    connectWindowSignals(window);

    // Add to MDI area
    m_mdiArea->addWindow(window);
    
    // Set focus to newly created window
    window->setFocus();
    window->raise();
    window->activateWindow();

    qDebug() << "[MainWindow] Buy Window created";
}

void MainWindow::createSellWindow()
{
    CustomMDISubWindow *window = new CustomMDISubWindow("Sell Order", m_mdiArea);
    window->setWindowType("SellWindow");

    // Try to get context from active MarketWatch
    MarketWatchWindow *activeMarketWatch = getActiveMarketWatch();
    SellWindow *sellWindow = nullptr;
    
    if (activeMarketWatch && activeMarketWatch->hasValidSelection()) {
        WindowContext context = activeMarketWatch->getSelectedContractContext();
        if (context.isValid()) {
            sellWindow = new SellWindow(context, window);
            qDebug() << "[MainWindow] Sell window created with context:" << context.symbol;
        }
    }
    
    // Fallback to empty window if no context
    if (!sellWindow) {
        sellWindow = new SellWindow(window);
        qDebug() << "[MainWindow] Sell window created without context";
    }
    
    window->setContentWidget(sellWindow);
    window->resize(600, 250);

    // Connect order submitted signal
    connect(sellWindow, &SellWindow::orderSubmitted, [](const QString &exchange, int token, 
                                                         const QString &symbol, int qty, 
                                                         double price, const QString &orderType) {
        qDebug() << "[MainWindow] Sell Order Submitted:" 
                 << "Exchange:" << exchange 
                 << "Token:" << token
                 << "Symbol:" << symbol
                 << "Qty:" << qty 
                 << "Price:" << price
                 << "Type:" << orderType;
        // TODO: Connect to order management system
    });

    // Connect request to switch to BuyWindow (F2)
    connect(sellWindow, &SellWindow::requestBuyWithContext, this,
            [this](const WindowContext &context) {
                // Create a buy window for the same contract
                CustomMDISubWindow *buySub = new CustomMDISubWindow("Buy Order", m_mdiArea);
                buySub->setWindowType("BuyWindow");
                BuyWindow *bwin = new BuyWindow(context, buySub);
                buySub->setContentWidget(bwin);
                buySub->resize(1200, 250);
                connectWindowSignals(buySub);
                
                // Connect order signal
                connect(bwin, &BuyWindow::orderSubmitted, [](const QString &ex, int tok, const QString &sym, int q, double p, const QString &ot) {
                    qDebug() << "[MainWindow] Buy Order Submitted:" << ex << tok << sym << q << "@" << p << ot;
                });
                // Connect F2 switch back to Sell
                connect(bwin, &BuyWindow::requestSellWithContext, this, [this](const WindowContext &ctx) {
                    CustomMDISubWindow *sellSub = new CustomMDISubWindow("Sell Order", m_mdiArea);
                    sellSub->setWindowType("SellWindow");
                    SellWindow *swin = new SellWindow(ctx, sellSub);
                    sellSub->setContentWidget(swin);
                    sellSub->resize(1200, 250);
                    
                    connectWindowSignals(sellSub);
                    
                    m_mdiArea->addWindow(sellSub);
                });
                // Add to MDI area
                m_mdiArea->addWindow(buySub);
            });

    // Connect MDI signals
    connectWindowSignals(window);

    // Add to MDI area
    m_mdiArea->addWindow(window);
    
    // Set focus to newly created window
    window->setFocus();
    window->raise();
    window->activateWindow();

    qDebug() << "[MainWindow] Sell Window created";
}

void MainWindow::createSnapQuoteWindow()
{
    CustomMDISubWindow *window = new CustomMDISubWindow("Snap Quote", m_mdiArea);
    window->setWindowType("SnapQuote");

    // Try to get context from active MarketWatch
    MarketWatchWindow *activeMarketWatch = getActiveMarketWatch();
    SnapQuoteWindow *snapWindow = nullptr;
    
    if (activeMarketWatch && activeMarketWatch->hasValidSelection()) {
        WindowContext context = activeMarketWatch->getSelectedContractContext();
        if (context.isValid()) {
            snapWindow = new SnapQuoteWindow(context, window);
            qDebug() << "[MainWindow] SnapQuote window created with context:" << context.symbol;
        }
    }
    
    // Fallback to empty window if no context
    if (!snapWindow) {
        snapWindow = new SnapQuoteWindow(window);
        qDebug() << "[MainWindow] SnapQuote window created without context";
    }
    
    // Set XTS client for market data access
    if (m_xtsMarketDataClient) {
        snapWindow->setXTSClient(m_xtsMarketDataClient);
        // Fetch current quote if context is valid
        if (snapWindow->getContext().isValid()) {
            snapWindow->fetchQuote();
        }
    }
    
    window->setContentWidget(snapWindow);
    window->resize(860, 300);
    
    // Note: Statistics and depth data are now populated by fetchQuote() API call
    // No need for dummy data here as API provides real-time values

    // Connect MDI signals
    connectWindowSignals(window);

    // Add to MDI area
    m_mdiArea->addWindow(window);
    
    // Set focus to newly created window
    window->setFocus();
    window->raise();
    window->activateWindow();

    qDebug() << "[MainWindow] Snap Quote Window created";
}

void MainWindow::createOptionChainWindow()
{
    CustomMDISubWindow *window = new CustomMDISubWindow("Option Chain", m_mdiArea);
    window->setWindowType("OptionChain");
    
    // Try to get context from active MarketWatch
    MarketWatchWindow *activeMarketWatch = getActiveMarketWatch();
    OptionChainWindow *optionWindow = new OptionChainWindow(window);
    
    if (activeMarketWatch && activeMarketWatch->hasValidSelection()) {
        WindowContext context = activeMarketWatch->getSelectedContractContext();
        if (context.isValid()) {
            // Set symbol and expiry from context
            optionWindow->setSymbol(context.symbol, context.expiry);
            qDebug() << "[MainWindow] Option Chain created with context:" << context.symbol << context.expiry;
        }
    } else {
        qDebug() << "[MainWindow] Option Chain created with default symbol (NIFTY)";
    }
    
    // Connect signals
    connect(optionWindow, &OptionChainWindow::tradeRequested, 
            [this](const QString &symbol, const QString &expiry, double strike, const QString &optionType) {
        qDebug() << "[MainWindow] Trade requested:" << symbol << expiry << strike << optionType;
        // TODO: Open Buy/Sell window with pre-filled option details
    });
    
    connect(optionWindow, &OptionChainWindow::calculatorRequested,
            [this](const QString &symbol, const QString &expiry, double strike, const QString &optionType) {
        qDebug() << "[MainWindow] Calculator requested:" << symbol << expiry;
        // TODO: Open calculator window
    });
    
    connect(optionWindow, &OptionChainWindow::refreshRequested, [optionWindow]() {
        qDebug() << "[MainWindow] Option Chain refresh requested";
        // TODO: Fetch fresh option chain data from API
    });
    
    window->setContentWidget(optionWindow);
    window->resize(1600, 800);
    
    // Connect MDI signals
    connectWindowSignals(window);
    
    // Add to MDI area
    m_mdiArea->addWindow(window);
    
    // Set focus
    window->setFocus();
    window->raise();
    window->activateWindow();
    
    qDebug() << "[MainWindow] Option Chain Window created";
}

void MainWindow::createPositionWindow()
{
    // Create window with Position widget
    CustomMDISubWindow *window = new CustomMDISubWindow("Integrated Net Position", m_mdiArea);
    window->setWindowType("PositionWindow");

    PositionWindow *posWindow = new PositionWindow(window);
    window->setContentWidget(posWindow);
    window->resize(1000, 500);

    // Connect MDI signals
    connectWindowSignals(window);

    // Add to MDI area
    m_mdiArea->addWindow(window);
    
    // Set focus to newly created window
    window->setFocus();
    window->raise();
    window->activateWindow();

    qDebug() << "[MainWindow] Position Window created";
}

void MainWindow::createTradeBookWindow()
{
    // Create window with TradeBook widget
    CustomMDISubWindow *window = new CustomMDISubWindow("Trade Book", m_mdiArea);
    window->setWindowType("TradeBook");

    TradeBookWindow *tradeBook = new TradeBookWindow(window);
    window->setContentWidget(tradeBook);
    window->resize(1400, 600);

    // Connect MDI signals
    connectWindowSignals(window);

    // Add to MDI area
    m_mdiArea->addWindow(window);
    
    // Set focus to newly created window
    window->setFocus();
    window->raise();
    window->activateWindow();

    qDebug() << "[MainWindow] Trade Book Window created";
}

void MainWindow::createOrderBookWindow()
{
    // Create window with OrderBook widget
    CustomMDISubWindow *window = new CustomMDISubWindow("Order Book", m_mdiArea);
    window->setWindowType("OrderBook");

    OrderBookWindow *orderBook = new OrderBookWindow(window);
    window->setContentWidget(orderBook);
    window->resize(1400, 600);

    // Connect MDI signals
    connectWindowSignals(window);

    // Add to MDI area
    m_mdiArea->addWindow(window);
    
    // Set focus to newly created window
    window->setFocus();
    window->raise();
    window->activateWindow();

    qDebug() << "[MainWindow] Order Book Window created";
}

void MainWindow::onAddToWatchRequested(const InstrumentData &instrument)
{
    qDebug() << "Adding to watch:" << instrument.symbol << "token:" << instrument.exchangeInstrumentID;

    // Find active market watch window
    CustomMDISubWindow *activeWindow = m_mdiArea->activeWindow();
    MarketWatchWindow *marketWatch = nullptr;
    
    if (activeWindow && activeWindow->windowType() == "MarketWatch") {
        // Use active market watch
        marketWatch = qobject_cast<MarketWatchWindow*>(activeWindow->contentWidget());
    } else {
        // Find first market watch window
        QList<CustomMDISubWindow*> windows = m_mdiArea->windowList();
        for (CustomMDISubWindow *win : windows) {
            if (win->windowType() == "MarketWatch") {
                marketWatch = qobject_cast<MarketWatchWindow*>(win->contentWidget());
                if (marketWatch) {
                    m_mdiArea->activateWindow(win);
                    break;
                }
            }
        }
    }
    
    // If no market watch found, create a new one
    if (!marketWatch) {
        qDebug() << "No market watch found, creating new one";
        createMarketWatch();
        
        // Get the newly created market watch
        activeWindow = m_mdiArea->activeWindow();
        if (activeWindow) {
            marketWatch = qobject_cast<MarketWatchWindow*>(activeWindow->contentWidget());
        }
    }
    
    if (marketWatch) {
        // Map exchangeSegment code to exchange string
        QString exchangeStr;
        switch(instrument.exchangeSegment) {
            case 1:  exchangeStr = "NSECM"; break;
            case 2:  exchangeStr = "NSEFO"; break;
            case 13: exchangeStr = "NSECD"; break;
            case 11: exchangeStr = "BSECM"; break;
            case 12: exchangeStr = "BSEFO"; break;
            case 61: exchangeStr = "BSECD"; break;
            case 51: exchangeStr = "MCXFO"; break;
            default: exchangeStr = "NSEFO"; break;
        }
        
        // Build complete ScripData with all fields from InstrumentData
        ScripData scripData;
        scripData.symbol = instrument.symbol;
        scripData.scripName = instrument.name;
        scripData.instrumentName = instrument.name;
        scripData.instrumentType = instrument.instrumentType;  // FUTIDX, FUTSTK, OPTSTK, EQUITY, etc.
        scripData.exchange = exchangeStr;
        scripData.token = (int)instrument.exchangeInstrumentID;
        scripData.code = (int)instrument.exchangeInstrumentID;
        
        // Series (from instrument series field)
        if (!instrument.series.isEmpty()) {
            scripData.seriesExpiry = instrument.series;
        }
        
        // Expiry date
        if (!instrument.expiryDate.isEmpty() && instrument.expiryDate != "N/A") {
            scripData.seriesExpiry = instrument.expiryDate;
        }
        
        // Strike price
        if (instrument.strikePrice > 0) {
            scripData.strikePrice = instrument.strikePrice;
        }
        
        // Option type (CE/PE)
        if (!instrument.optionType.isEmpty() && instrument.optionType != "N/A") {
            scripData.optionType = instrument.optionType;
        }
        
        // BSE scrip code (if BSE)
        if (!instrument.scripCode.isEmpty()) {
            scripData.isinCode = instrument.scripCode;  // Store BSE code in isinCode field
        }
        
        // Initialize price fields to zero (will be updated by tick data and getQuote)
        scripData.ltp = 0.0;
        scripData.change = 0.0;
        scripData.changePercent = 0.0;
        scripData.isBlankRow = false;
        
        // Use addScripFromContract to pass all fields
        bool added = marketWatch->addScripFromContract(scripData);
        if (added) {
            qDebug() << "Successfully added" << instrument.symbol << "to market watch with token" << instrument.exchangeInstrumentID;
            
            // Check if we have a cached price BEFORE subscribing
            auto cachedPrice = PriceCache::instance().getPrice(instrument.exchangeInstrumentID);
            if (cachedPrice.has_value()) {
                qDebug() << "📦 Applying cached price for token" << instrument.exchangeInstrumentID;
                
                // Calculate price change
                double change = cachedPrice->lastTradedPrice - cachedPrice->close;
                double changePercent = (cachedPrice->close != 0) ? (change / cachedPrice->close) * 100.0 : 0.0;
                
                // Update the market watch with cached price immediately
                marketWatch->updatePrice(instrument.exchangeInstrumentID,
                                       cachedPrice->lastTradedPrice,
                                       change,
                                       changePercent);
                
                qDebug() << "✅ Applied cached price - LTP:" << cachedPrice->lastTradedPrice 
                        << "Change:" << change << "(" << changePercent << "%)";
            } else {
                qDebug() << "ℹ️ No cached price for token" << instrument.exchangeInstrumentID << "- will wait for UDP broadcast";
            }
            
            // ============================================================
            // XTS SUBSCRIPTION DISABLED - Testing UDP Broadcast Only
            // ============================================================
            // Commenting out XTS subscription to measure pure UDP latency
            // Uncomment below code to enable XTS WebSocket subscription
            // ============================================================
            /*
            if (m_xtsMarketDataClient) {
                int64_t instrumentID = instrument.exchangeInstrumentID;
                
                // Subscribe API returns initial snapshot (if not already subscribed) + enables live updates
                // If token is already subscribed (e.g., in another market watch), we get success but no snapshot
                // In that case, live updates via WebSocket will still work for this window
                QVector<int64_t> ids;
                ids.append(instrumentID);
                m_xtsMarketDataClient->subscribe(ids, instrument.exchangeSegment, [instrumentID](bool ok, const QString &info){
                    if (ok) {
                        qDebug() << "[MainWindow] Subscribed instrument" << instrumentID << "- will receive live updates";
                    } else {
                        qWarning() << "[MainWindow] Failed to subscribe instrument" << instrumentID << ":" << info;
                    }
                });
            }
            */
            qDebug() << "⚠️ [XTS DISABLED] Not subscribing to XTS - relying on UDP broadcast only";
            qDebug() << "⚠️ Make sure to start UDP receiver: Data → Start NSE Broadcast Receiver";
            // ============================================================
        } else {
            qDebug() << "Failed to add" << instrument.symbol << "to market watch (duplicate or invalid)";
        }
    } else {
        qWarning() << "Failed to get MarketWatchWindow instance";
    }
}

void MainWindow::saveCurrentWorkspace()
{
    bool ok;
    QString name = QInputDialog::getText(this, "Save Workspace",
                                         "Workspace name:", QLineEdit::Normal,
                                         "", &ok);

    if (ok && !name.isEmpty())
    {
        m_mdiArea->saveWorkspace(name);
        QMessageBox::information(this, "Workspace Saved",
                                 QString("Workspace '%1' has been saved.").arg(name));
    }
}

void MainWindow::loadWorkspace()
{
    QStringList workspaces = m_mdiArea->availableWorkspaces();

    if (workspaces.isEmpty())
    {
        QMessageBox::information(this, "No Workspaces",
                                 "No saved workspaces found.");
        return;
    }

    bool ok;
    QString name = QInputDialog::getItem(this, "Load Workspace",
                                         "Select workspace:", workspaces,
                                         0, false, &ok);

    if (ok && !name.isEmpty())
    {
        m_mdiArea->loadWorkspace(name);
        QMessageBox::information(this, "Workspace Loaded",
                                 QString("Workspace '%1' has been loaded.").arg(name));
    }
}

void MainWindow::manageWorkspaces()
{
    QStringList workspaces = m_mdiArea->availableWorkspaces();

    if (workspaces.isEmpty())
    {
        QMessageBox::information(this, "No Workspaces",
                                 "No saved workspaces found.");
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle("Manage Workspaces");
    dialog.resize(400, 300);

    QVBoxLayout *layout = new QVBoxLayout(&dialog);

    QListWidget *listWidget = new QListWidget(&dialog);
    listWidget->addItems(workspaces);
    layout->addWidget(listWidget);

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    QPushButton *loadBtn = new QPushButton("Load", &dialog);
    QPushButton *deleteBtn = new QPushButton("Delete", &dialog);
    QPushButton *closeBtn = new QPushButton("Close", &dialog);

    buttonLayout->addWidget(loadBtn);
    buttonLayout->addWidget(deleteBtn);
    buttonLayout->addStretch();
    buttonLayout->addWidget(closeBtn);
    layout->addLayout(buttonLayout);

    connect(loadBtn, &QPushButton::clicked, [&]()
            {
        QListWidgetItem *item = listWidget->currentItem();
        if (item) {
            m_mdiArea->loadWorkspace(item->text());
            QMessageBox::information(&dialog, "Workspace Loaded",
                                   QString("Workspace '%1' has been loaded.").arg(item->text()));
            dialog.accept();
        } });

    connect(deleteBtn, &QPushButton::clicked, [&]()
            {
        QListWidgetItem *item = listWidget->currentItem();
        if (item) {
            QMessageBox::StandardButton reply = QMessageBox::question(
                &dialog, "Delete Workspace",
                QString("Are you sure you want to delete workspace '%1'?").arg(item->text()),
                QMessageBox::Yes | QMessageBox::No);
            
            if (reply == QMessageBox::Yes) {
                m_mdiArea->deleteWorkspace(item->text());
                delete listWidget->takeItem(listWidget->row(item));
            }
        } });

    connect(closeBtn, &QPushButton::clicked, &dialog, &QDialog::accept);

    dialog.exec();
}

void MainWindow::showPreferences()
{
    PreferenceDialog dialog(this);
    
    if (dialog.exec() == QDialog::Accepted) {
        qDebug() << "[MainWindow] Preferences saved and accepted";
        // Preferences are automatically saved by the dialog
        // Optionally: Apply any immediate UI changes here
    } else {
        qDebug() << "[MainWindow] Preferences dialog cancelled";
    }
}



// i think there is a bug here when where snapquote or any other subwindow is active other than marketwatch
// it will return that as active marketwatch which is wrong
// kindly fix it

//it should be getActiveSubwindow insted of getActiveMarketWatch as buy window can be initiated 
//from snapquote/sell window/ netposition window also and its type should be subwindow not 
//marketwatch but if we assign type to be sub window then how will we differentiate between 
//buy/sell/netposition/snapquote window  and also how we will get context from those windows


MarketWatchWindow* MainWindow::getActiveMarketWatch() const                    
// {
//     // Get the currently active MDI sub-window
//     CustomMDISubWindow *activeSubWindow = m_mdiArea ? m_mdiArea->activeWindow() : nullptr;
//     if (!activeSubWindow) {
//         return nullptr;
//     }
    
//     // Try to cast the content widget to MarketWatchWindow
//     QWidget *widget = activeSubWindow->contentWidget();
//     return qobject_cast<MarketWatchWindow*>(widget);
// }

{
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
    
    // No MarketWatch found
    return nullptr;
}




void MainWindow::onTickReceived(const XTS::Tick &tick)
{
    // ========================================================================
    // PHASE 2 COMPLETE: Direct callback architecture - NO POLLING!
    // ========================================================================
    // Performance: <1μs latency (vs 50-100ms with old timer polling)
    // CPU Usage: 99.75% reduction (direct callbacks vs polling all tokens)
    // ========================================================================
    
    // Step 1: Update PriceCache (for queries and new windows loading cached prices)
    PriceCache::instance().updatePrice((int)tick.exchangeInstrumentID, tick);
    
    // Step 2: Distribute tick to all subscribers via FeedHandler
    // MarketWatch windows now subscribe directly and receive instant callbacks
    FeedHandler::instance().onTickReceived(tick);
    
    // ✅ Manual window iteration REMOVED - MarketWatch uses direct callbacks now
    // ✅ No timer polling - Real-time push updates
    // ✅ Automatic subscription management - Add scrip = auto-subscribe
    // ✅ Automatic cleanup - Remove scrip/close window = auto-unsubscribe
}

// ============================================================================
// UDP BROADCAST RECEIVER - PHASE 1: Test UDP Reception
// ============================================================================

void MainWindow::startBroadcastReceiver() {
    qDebug() << "[UDP] ========================================";
    qDebug() << "[UDP] Starting NSE broadcast receiver...";
    qDebug() << "[UDP] ========================================";
    
    // TODO: Read from config.ini
    std::string multicastIP = "233.1.2.5";  // NSE F&O multicast IP
    int port = 34331;                            // NSE F&O port
    
    qDebug() << "[UDP] Multicast IP:" << QString::fromStdString(multicastIP);
    qDebug() << "[UDP] Port:" << port;
    
    // Create receiver
    m_udpReceiver = std::make_unique<MulticastReceiver>(multicastIP, port);
    
    if (!m_udpReceiver->isValid()) {
        qWarning() << "[UDP] ❌ Failed to initialize receiver!";
        qWarning() << "[UDP] Check:";
        qWarning() << "[UDP]   1. Network interface supports multicast";
        qWarning() << "[UDP]   2. Firewall not blocking port" << port;
        qWarning() << "[UDP]   3. NSE broadcast is active (market hours)";
        return;
    }
    
    qDebug() << "[UDP] ✅ Receiver initialized successfully";
    
    // Register callback for 7200/7208 messages (Touchline data)
    qDebug() << "[UDP] Registering callback for message 7200/7208 (Touchline)...";
    MarketDataCallbackRegistry::instance().registerTouchlineCallback(
        [this](const TouchlineData& data) {
            // 7200 (MBO/MBP) and 7208 (MBP only) both dispatch to TouchlineData
            m_msg7200Count++;
            
            // Convert NSE TouchlineData to XTS::Tick format
            // IMPORTANT: Only set fields present in 7200 message
            // Do NOT set bid/ask/depth fields - they come from separate 7208 depth callback
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
            
            // Copy latency tracking fields from UDP parse
            tick.refNo = data.refNo;
            tick.timestampUdpRecv = data.timestampRecv;
            tick.timestampParsed = data.timestampParsed;
            tick.timestampQueued = LatencyTracker::now();  // Mark queue time
            
            // NOTE: Do NOT set these fields - they come from 7208 depth callback
            // Setting them to 0 would erase existing depth data in MarketWatch
            // MarketWatch will keep old values until depth update arrives
            // tick.totalBuyQuantity = 0;  // ❌ WRONG - erases depth data
            // tick.totalSellQuantity = 0; // ❌ WRONG - erases depth data
            // tick.bidPrice = 0.0;        // ❌ WRONG - erases depth data
            // tick.bidQuantity = 0;       // ❌ WRONG - erases depth data
            // tick.askPrice = 0.0;        // ❌ WRONG - erases depth data
            // tick.askQuantity = 0;       // ❌ WRONG - erases depth data
            
            // ⚡ ULTRA-LOW LATENCY: Direct callback to FeedHandler (bypasses queue!)
            // Uses QMetaObject::invokeMethod with Qt::AutoConnection:
            // - If called from UI thread → Direct call (0ns overhead)
            // - If called from UDP thread → Queued (but necessary for thread safety)
            tick.timestampDequeued = LatencyTracker::now();
            
            // Debug logging for token 49543
            if (data.token == 49543) {
                std::cout << "[MAINWINDOW-TOUCHLINE] Token: 49543 | RefNo: " << tick.refNo
                          << " | LTP: " << tick.lastTradedPrice
                          << " | timestampDequeued: " << tick.timestampDequeued << " µs"
                          << " | Queue delay: " << (tick.timestampDequeued - tick.timestampParsed) << " µs" << std::endl;
            }
            
            QMetaObject::invokeMethod(this, [this, tick]() {
                FeedHandler::instance().onTickReceived(tick);
            }, Qt::AutoConnection);  // Auto-detects thread context
        }
    );
    
    // Register callback for market depth data (from 7200/7208)
    qDebug() << "[UDP] Registering callback for Market Depth...";
    MarketDataCallbackRegistry::instance().registerMarketDepthCallback(
        [this](const MarketDepthData& data) {
            // Count market depth messages
            m_depthCount++;            
            // Create tick with ONLY depth fields (do NOT touch LTP/OHLC!)
            // IMPORTANT: We only set fields present in depth message
            // LTP/OHLC/volume are updated by touchline callback

            XTS::Tick tick;
            tick.exchangeSegment = 2; // NSEFO
            tick.exchangeInstrumentID = data.token;
            
            // Set depth fields from 7208 message
            if (!data.bids.empty()) {
                tick.bidPrice = data.bids[0].price;
                tick.bidQuantity = data.bids[0].quantity;
            }
            if (!data.asks.empty()) {
                tick.askPrice = data.asks[0].price;
                tick.askQuantity = data.asks[0].quantity;
            }
            tick.totalBuyQuantity = (int)data.totalBuyQty;
            tick.totalSellQuantity = (int)data.totalSellQty;
            
            // Copy latency tracking fields
            tick.refNo = data.refNo;
            tick.timestampUdpRecv = data.timestampRecv;
            tick.timestampParsed = data.timestampParsed;
            tick.timestampQueued = LatencyTracker::now();
            tick.timestampDequeued = LatencyTracker::now();
            
            // NOTE: Do NOT set LTP/OHLC/volume here - they come from touchline callback
            // MarketWatch will merge this depth update with existing price data
            
            // ⚡ ULTRA-LOW LATENCY: Direct callback (bypasses queue!)
            QMetaObject::invokeMethod(this, [this, tick]() {
                FeedHandler::instance().onTickReceived(tick);
            }, Qt::AutoConnection);
        }
    );
    
    // Register callback for ticker data (7202 - OI and fill updates)
    qDebug() << "[UDP] Registering callback for Ticker (OI - 7202)...";
    MarketDataCallbackRegistry::instance().registerTickerCallback(
        [this](const TickerData& data) {
            m_msg7202Count++;
            
            // Create tick with ONLY volume field (if available)
            // IMPORTANT: Only update volume from ticker, don't touch other fields
            if (data.fillVolume > 0) {
                XTS::Tick tick;
                tick.exchangeSegment = 2; // NSEFO
                tick.exchangeInstrumentID = data.token;
                tick.volume = data.fillVolume;
                
                // Copy latency tracking fields
                tick.refNo = data.refNo;
                tick.timestampUdpRecv = data.timestampRecv;
                tick.timestampParsed = data.timestampParsed;
                tick.timestampQueued = LatencyTracker::now();
                tick.timestampDequeued = LatencyTracker::now();
                
                // Debug logging for token 49543
                if (data.token == 49543) {
                    std::cout << "[MAINWINDOW-TICKER] Token: 49543 | RefNo: " << tick.refNo
                              << " | OI: " << data.openInterest
                              << " | timestampDequeued: " << tick.timestampDequeued << " µs"
                              << " | Queue delay: " << (tick.timestampDequeued - tick.timestampParsed) << " µs" << std::endl;
                }
                
                // NOTE: Do NOT set LTP/OHLC/bid/ask here - they come from other callbacks
                // MarketWatch will merge this volume update with existing data
                
                // ⚡ ULTRA-LOW LATENCY: Direct callback (bypasses queue!)
                QMetaObject::invokeMethod(this, [this, tick]() {
                    FeedHandler::instance().onTickReceived(tick);
                }, Qt::AutoConnection);
            }
            
            // Log OI for monitoring (XTS::Tick doesn't have OI field yet)
            // qDebug() << "[UDP 7202] Token:" << data.token << "OI:" << data.openInterest;
        }
    );
    
    // Register callback for 7201 messages (Market Watch Round Robin)
    qDebug() << "[UDP] Registering callback for message 7201 (Market Watch)...";
    MarketDataCallbackRegistry::instance().registerMarketWatchCallback(
        [this](const MarketWatchData& data) {
            m_msg7201Count++;
            
            // Print every 7201 message we receive (they're rare!)
            qDebug() << "[UDP 7201] *** MARKET WATCH MESSAGE RECEIVED ***";
            qDebug() << "[UDP 7201] Token:" << data.token;
            qDebug() << "[UDP 7201] Open Interest:" << data.openInterest;
            
            // Print all 3 market levels (Normal, Stop Loss, Auction)
            for (size_t i = 0; i < data.levels.size() && i < 3; i++) {
                QString marketType;
                if (i == 0) marketType = "Normal Market";
                else if (i == 1) marketType = "Stop Loss";
                else if (i == 2) marketType = "Auction";
                
                qDebug() << "[UDP 7201] " << marketType << ":";
                qDebug() << "[UDP 7201]   Buy - Price:" << data.levels[i].buyPrice 
                         << "Volume:" << data.levels[i].buyVolume;
                qDebug() << "[UDP 7201]   Sell - Price:" << data.levels[i].sellPrice 
                         << "Volume:" << data.levels[i].sellVolume;
            }
            qDebug() << "[UDP 7201] ========================================";
        }
    );
    
    // Start receiving in background thread (non-blocking)
    qDebug() << "[UDP] Starting receiver thread...";
    
    // Use std::thread to run in background - keeps UI responsive
    std::thread udpThread([this]() {
        m_udpReceiver->start();  // This blocks in the thread, not the UI
    });
    
    // Detach thread so it runs independently
    udpThread.detach();
    
    qDebug() << "[UDP] ✅ Receiver thread started (running in background)!";
    qDebug() << "[UDP] Waiting for broadcast data...";
    qDebug() << "[UDP] (If no data appears, check market hours: 9:15 AM - 3:30 PM)";
    qDebug() << "[UDP] ========================================";
    qDebug() << "";
    qDebug() << "[UDP] Message counters active - will show summary on exit";
    qDebug() << "[UDP] Tracking: 7200/7208 (Touchline), 7201 (MarketWatch), 7202 (Ticker)";
    
    // Update status bar
    if (m_statusBar) {
        m_statusBar->showMessage("NSE Broadcast Receiver: ACTIVE");
    }
}

void MainWindow::stopBroadcastReceiver() {
    if (m_udpReceiver) {
        qDebug() << "[UDP] Stopping receiver...";
        m_udpReceiver->stop();
        
        // Give thread time to stop gracefully
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Print statistics
        const UDPStats& stats = m_udpReceiver->getStats();
        qDebug() << "[UDP] ========================================";
        qDebug() << "[UDP] UDP PACKET STATISTICS:";
        qDebug() << "[UDP]   Total packets:" << stats.totalPackets;
        qDebug() << "[UDP]   Total bytes:" << stats.totalBytes;
        qDebug() << "[UDP]   Compressed packets:" << stats.compressedPackets;
        qDebug() << "[UDP]   Decompressed packets:" << stats.decompressedPackets;
        qDebug() << "[UDP]   Decompression failures:" << stats.decompressionFailures;
        qDebug() << "[UDP] ========================================";
        qDebug() << "[UDP] MESSAGE TYPE STATISTICS:";
        qDebug() << "[UDP]   7200/7208 (Touchline) messages:" << m_msg7200Count.load();
        qDebug() << "[UDP]   7201 (Market Watch) messages:" << m_msg7201Count.load();
        qDebug() << "[UDP]   7202 (Ticker/OI) messages:" << m_msg7202Count.load();
        qDebug() << "[UDP]   Market Depth callbacks:" << m_depthCount.load();
        qDebug() << "[UDP] ========================================";
        
        if (m_msg7201Count.load() == 0) {
            qDebug() << "[UDP] ⚠️  NO 7201 messages received!";
            qDebug() << "[UDP] This is normal - NSE rarely broadcasts 7201 (Market Watch).";
            qDebug() << "[UDP] They prefer 7200/7208 which provide better market depth.";
        } else {
            qDebug() << "[UDP] ✅ Received" << m_msg7201Count.load() << "7201 messages!";
        }
        
        m_udpReceiver.reset();
        qDebug() << "[UDP] ✅ Receiver stopped";
        
        // Update status bar
        if (m_statusBar) {
            m_statusBar->showMessage("NSE Broadcast Receiver: STOPPED");
        }
    }
}

void MainWindow::drainTickQueue()
{
    // Drain all pending ticks from UDP thread
    // Runs on UI thread at 1ms interval (1000 Hz) for real-time performance
    
    int count = 0;
    const int MAX_BATCH = 1000;  // Prevent UI freeze if queue backs up
    
    // Track latency for first tick in batch (detailed)
    static bool printNextDetailed = false;
    static int ticksProcessed = 0;
    
    while (count < MAX_BATCH) {
        auto tick = m_udpTickQueue.dequeue();
        if (!tick.has_value()) {
            break;  // Queue empty - all caught up!
        }
        
        // Mark dequeue timestamp
        tick->timestampDequeued = LatencyTracker::now();
        
        // Print detailed latency breakdown for every 100th tick
        ticksProcessed++;
        if (ticksProcessed % 100 == 1 && tick->refNo > 0) {
            printNextDetailed = true;
        }
        
        // Process tick on UI thread (thread-safe)
        FeedHandler::instance().onTickReceived(*tick);
        count++;
        
        // If this tick was marked for detailed tracking, FeedHandler/Model/View will log it
    }
    
    // Monitor queue depth for performance tuning
    static int totalDrained = 0;
    static int maxBatchSize = 0;
    
    totalDrained += count;
    if (count > maxBatchSize) {
        maxBatchSize = count;
        if (maxBatchSize > 100) {
            qDebug() << "[Drain] New max batch size:" << maxBatchSize 
                     << "(consider tuning timer interval)";
        }
    }
    
    // Log every 1000 ticks to track throughput
    if (totalDrained >= 1000) {
        qDebug() << "[Drain] Processed" << totalDrained << "total ticks"
                 << "- Max batch:" << maxBatchSize;
        totalDrained = 0;
        maxBatchSize = 0;
        
        // Print aggregate statistics
        LatencyTracker::printAggregateStats();
    }
    
    // Warn if we hit max batch (queue might be backing up)
    if (count >= MAX_BATCH) {
        qWarning() << "[Drain] Hit max batch size!" << MAX_BATCH 
                   << "- Queue depth exceeds drain rate!";
    }
}

void MainWindow::connectWindowSignals(CustomMDISubWindow *window)
{
    if (!window) return;

    // Connect MDI area signals
    connect(window, &CustomMDISubWindow::closeRequested, window, &QWidget::close);
    connect(window, &CustomMDISubWindow::minimizeRequested, this, [this, window]() {
        m_mdiArea->minimizeWindow(window);
    });
    connect(window, &CustomMDISubWindow::maximizeRequested, window, &CustomMDISubWindow::maximize);
    connect(window, &CustomMDISubWindow::windowActivated, this, [this, window]() {
        m_mdiArea->activateWindow(window);
    });

    // Connect Customize signal
    connect(window, &CustomMDISubWindow::customizeRequested, this, [this, window]() {
        QString windowType = window->windowType();
        QWidget *targetWidget = window->contentWidget();
        
        CustomizeDialog dialog(windowType, targetWidget, this);
        dialog.exec();
    });
}

