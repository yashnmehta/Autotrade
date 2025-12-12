#include "ui/MainWindow.h"
#include "ui/CustomTitleBar.h"
#include "ui/CustomMDIArea.h"
#include "ui/CustomMDISubWindow.h"
#include "ui/ScripBar.h"
#include "ui/InfoBar.h"
#include "data/Greeks.h"
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
#include <QSettings>
#include <QCloseEvent>
#include <QDebug>

MainWindow::MainWindow(QWidget *parent)
    : CustomMainWindow(parent)
{
    setTitle("Trading Terminal");
    resize(1600, 900);
    setMinimumSize(800, 600);

    // Setup content FIRST (creates layout and widgets)
    setupContent();

    // Restore visibility preferences
    QSettings s("TradingCompany", "TradingTerminal");
    if (m_infoDock)
        m_infoDock->setVisible(s.value("mainwindow/info_visible", true).toBool());
    if (m_statusBar)
        m_statusBar->setVisible(s.value("mainwindow/status_visible", true).toBool());
}

MainWindow::~MainWindow()
{
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

    // Create toolbar
    createToolBar();
    layout->addWidget(m_toolBar);
#ifdef Q_OS_MAC
    m_toolBar->setFixedHeight(25);
#endif

    // Create connection bar wrapped in a toolbar-like widget
    createConnectionBar();
    m_connectionToolBar = new QToolBar(tr("Connection"), container);
    m_connectionToolBar->setStyleSheet(
        "QToolBar { "
        "   background-color: #2d2d30; "
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
    m_connectionBar->setParent(nullptr);
    m_connectionToolBar->addWidget(m_connectionBar);
    layout->addWidget(m_connectionToolBar);
#ifdef Q_OS_MAC
    m_connectionToolBar->setFixedHeight(25);
    m_connectionBar->setFixedHeight(25);
#endif

    // Create scrip bar wrapped in a toolbar
    m_scripBar = new ScripBar(container);
    connect(m_scripBar, &ScripBar::addToWatchRequested, this, &MainWindow::onAddToWatchRequested);
    m_scripToolBar = new QToolBar(tr("Scrip Bar"), container);
    m_scripToolBar->setStyleSheet(
        "QToolBar { "
        "   background-color: #2d2d30; "
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
    m_scripBar->setParent(nullptr);
    m_scripToolBar->addWidget(m_scripBar);
    layout->addWidget(m_scripToolBar);
#ifdef Q_OS_MAC
    m_scripToolBar->setFixedHeight(25);
    m_scripBar->setMaximumHeight(25);
#endif

    // Custom MDI Area (main content area)
    m_mdiArea = new CustomMDIArea(container);
    layout->addWidget(m_mdiArea, 1); // Give it stretch factor so it expands

    // Create status bar at the bottom
    createStatusBar();
    layout->addWidget(m_statusBar);

    // Create info bar (as a dock widget on the side)
    createInfoBar();
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
    menuBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    menuBar->setStyleSheet(
        "QMenuBar { "
        "   background-color: #2d2d30; "
        "   color: #ffffff; "
        "   border-bottom: 1px solid #3e3e42; "
        "   padding: 0px; "
        "} "
        "QMenuBar::item { "
        "   padding: 4px 8px; "
        "   background-color: transparent; "
        "} "
        "QMenuBar::item:selected { "
        "   background-color: #3e3e42; "
        "} "
        "QMenu { "
        "   background-color: #252526; "
        "   color: #ffffff; "
        "   border: 1px solid #3e3e42; "
        "} "
        "QMenu::item { "
        "   padding: 4px 20px; "
        "} "
        "QMenu::item:selected { "
        "   background-color: #094771; "
        "}");

    // CRITICAL: Set fixed height to prevent menu bar from being too tall
    menuBar->setFixedHeight(25);

    // Platform-specific attachment: macOS uses the native system menu bar,
    // other platforms embed the menu bar inside the window (top of central widget).
#ifdef Q_OS_MAC
    menuBar->setNativeMenuBar(true);
    qDebug() << "Using native macOS menu bar";
#else
    menuBar->setNativeMenuBar(false);
    // Force menu bar to take full width and be part of vertical layout
    menuBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    menuBar->setFixedHeight(25);

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
    editMenu->addAction("&Preferences");

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

    // Window Menu
    QMenu *windowMenu = menuBar->addMenu("&Window");
    QAction *marketWatchAction = windowMenu->addAction("New &Market Watch", this, &MainWindow::createMarketWatch, QKeySequence("Ctrl+M"));
    QAction *buyWindowAction = windowMenu->addAction("New &Buy Window", this, &MainWindow::createBuyWindow, QKeySequence("Ctrl+B"));
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
    QShortcut *nextShortcut = new QShortcut(QKeySequence("Meta+Tab"), this);
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
    QShortcut *prevShortcut = new QShortcut(QKeySequence("Meta+Shift+Tab"), this);
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
    m_toolBar->setStyleSheet(
        "QToolBar { "
        "   background-color: #2d2d30; "
        "   border: 1px solid #3e3e42; "
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

    // Add toolbar actions
    QAction *newMarketWatch = m_toolBar->addAction("Market Watch");
    newMarketWatch->setIcon(QIcon(":/icons/market_watch.png")); // Placeholder
    connect(newMarketWatch, &QAction::triggered, this, &MainWindow::createMarketWatch);

    QAction *newBuyOrder = m_toolBar->addAction("Buy Order");
    newBuyOrder->setIcon(QIcon(":/icons/buy_order.png")); // Placeholder
    connect(newBuyOrder, &QAction::triggered, this, &MainWindow::createBuyWindow);

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
        "   background-color: #252526; "
        "   border-bottom: 1px solid #3e3e42; "
        "} "
        "QLabel { "
        "   color: #cccccc; "
        "   padding: 2px 6px; "
        "   font-size: 10px; "
        "}");

    QHBoxLayout *layout = new QHBoxLayout(m_connectionBar);
    layout->setContentsMargins(8, 0, 8, 0);
    layout->setSpacing(8);
    layout->setAlignment(Qt::AlignVCenter);

    QLabel *statusLabel = new QLabel("Connection Status:", m_connectionBar);
    layout->addWidget(statusLabel);

    QLabel *statusValue = new QLabel("Disconnected", m_connectionBar);
    statusValue->setStyleSheet("color: #ff6b6b; font-weight: bold; font-size: 10px;");
    layout->addWidget(statusValue);

    layout->addStretch();

    QLabel *serverLabel = new QLabel("Server: XTS API", m_connectionBar);
    layout->addWidget(serverLabel);

    QLabel *userLabel = new QLabel("User: Demo", m_connectionBar);
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
    // Demo: populate InfoBar with sample stats
    QMap<QString, QString> stats;
    stats["NSE Eq:Open"] = "0";
    stats["NSE Eq Orders"] = "0";
    stats["NSE Der Orders"] = "0";
    stats["BSE Eq Orders"] = "0";
    m_infoBar->setSegmentStats(stats);
    m_infoBar->setTotalCounts(0, 0, 0);
    m_infoBar->setUserId("User: demouser");
}
// InfoBar menu handled by InfoBar; no MainWindow callback needed

void MainWindow::createMarketWatch()
{
    static int counter = 1;

    CustomMDISubWindow *window = new CustomMDISubWindow(QString("Market Watch %1").arg(counter++), m_mdiArea);
    window->setWindowType("MarketWatch");

    // Create table for market watch
    QTableWidget *table = new QTableWidget(5, 7, window);
    table->setHorizontalHeaderLabels({"Symbol", "LTP", "Change", "Open", "High", "Low", "Volume"});
    table->horizontalHeader()->setStretchLastSection(true);
    table->setAlternatingRowColors(true);

    // Dark theme for table (removed border:none to not interfere with parent)
    table->setStyleSheet(
        "QTableWidget { "
        "   background-color: #1e1e1e; "
        "   color: #cccccc; "
        "   gridline-color: #3e3e42; "
        "} "
        "QHeaderView::section { "
        "   background-color: #2d2d30; "
        "   color: #ffffff; "
        "   padding: 4px; "
        "   border: 1px solid #3e3e42; "
        "}");

    // Sample data
    QStringList symbols = {"RELIANCE", "INFY", "TCS", "HDFCBANK", "NIFTY"};
    for (int i = 0; i < 5; ++i)
    {
        table->setItem(i, 0, new QTableWidgetItem(symbols[i]));
        table->setItem(i, 1, new QTableWidgetItem("0.00"));
    }

    window->setContentWidget(table);

    // Connect signals
    connect(window, &CustomMDISubWindow::closeRequested, window, &QWidget::close);
    connect(window, &CustomMDISubWindow::minimizeRequested, [this, window]()
            { m_mdiArea->minimizeWindow(window); });
    connect(window, &CustomMDISubWindow::maximizeRequested, [window]()
            { window->maximize(); });
    connect(window, &CustomMDISubWindow::windowActivated, [this, window]()
            { m_mdiArea->activateWindow(window); });

    // Add to MDI area
    m_mdiArea->addWindow(window);

    qDebug() << "Market Watch created";
}

void MainWindow::createBuyWindow()
{
    CustomMDISubWindow *window = new CustomMDISubWindow("Buy Order", m_mdiArea);
    window->setWindowType("BuyWindow");

    // Create buy window content
    QWidget *content = new QWidget(window);
    QVBoxLayout *layout = new QVBoxLayout(content);

    QLabel *label = new QLabel("Buy Order Window", content);
    label->setAlignment(Qt::AlignCenter);
    QFont font = label->font();
    font.setPointSize(16);
    label->setFont(font);

    layout->addWidget(label);

    // Test Greeks button
    QPushButton *btn = new QPushButton("Calculate Greeks", content);
    layout->addWidget(btn);

    connect(btn, &QPushButton::clicked, []()
            {
        qDebug() << "Calculating Greeks for NIFTY 18000 CE...";
        
        double S = 18100.0;
        double K = 18000.0;
        double T = 7.0 / 365.0;
        double r = 0.05;
        double sigma = 0.15;
        
        OptionGreeks g = GreeksCalculator::calculate(S, K, T, r, sigma, true);
        
        qDebug() << "Price:" << g.price;
        qDebug() << "Delta:" << g.delta;
        qDebug() << "Gamma:" << g.gamma;
        qDebug() << "Theta:" << g.theta;
        qDebug() << "Vega:" << g.vega; });

    layout->addStretch();

    window->setContentWidget(content);
    window->resize(400, 300);

    // Connect signals
    connect(window, &CustomMDISubWindow::closeRequested, window, &QWidget::close);
    connect(window, &CustomMDISubWindow::minimizeRequested, [this, window]()
            { m_mdiArea->minimizeWindow(window); });
    connect(window, &CustomMDISubWindow::maximizeRequested, [window]()
            { window->maximize(); });
    connect(window, &CustomMDISubWindow::windowActivated, [this, window]()
            { m_mdiArea->activateWindow(window); });

    // Add to MDI area
    m_mdiArea->addWindow(window);

    qDebug() << "Buy Window created";
}

void MainWindow::onAddToWatchRequested(const QString &exchange, const QString &segment,
                                       const QString &instrument, const QString &symbol,
                                       const QString &expiry, const QString &strike,
                                       const QString &optionType)
{
    qDebug() << "Adding to watch:" << symbol << "from" << exchange << segment << instrument;

    // Find existing market watch window or create new one
    // For now, just create a new market watch and add the symbol
    createMarketWatch();

    // TODO: Add the symbol to the market watch table
    // This would require updating the table with the selected scrip data
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
