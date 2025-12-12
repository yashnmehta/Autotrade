#include "ui/MainWindow.h"
#include "ui/CustomTitleBar.h"
#include "ui/CustomMDIArea.h"
#include "ui/CustomMDISubWindow.h"
#include "ui/ScripBar.h"
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
#include <QSettings>
#include <QCloseEvent>
#include <QDebug>

MainWindow::MainWindow(QWidget *parent)
    : CustomMainWindow(parent)
{
    setTitle("Trading Terminal");
    resize(1600, 900);
    setMinimumSize(800, 600);

    setupContent();
    createMenuBar();
    // Restore saved toolbar/dock state
    QSettings s("TradingCompany", "TradingTerminal");
    restoreState(s.value("mainwindow/state").toByteArray());
    // Restore visibility preferences
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
    // Use the base class central widget container (created by CustomMainWindow)
    QWidget *container = this->centralWidget();
    if (!container)
    {
        // Fallback: create a local container
        container = new QWidget(this);
        QVBoxLayout *fallbackLayout = new QVBoxLayout(container);
        fallbackLayout->setContentsMargins(0, 0, 0, 0);
        setCentralWidget(container);
    }

    QVBoxLayout *layout = qobject_cast<QVBoxLayout *>(container->layout());
    if (!layout)
    {
        layout = new QVBoxLayout(container);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);
    }

    // Create toolbar (dockable)
    createToolBar();
    addToolBar(m_toolBar);
    // On macOS enforce compact toolbar height
#ifdef Q_OS_MAC
    m_toolBar->setFixedHeight(25);
#endif

    // Create connection bar
    createConnectionBar();
    // Wrap connection widget inside a QToolBar so it becomes dockable/movable
    m_connectionToolBar = new QToolBar(tr("Connection"), this);
    m_connectionToolBar->setAllowedAreas(Qt::AllToolBarAreas);
    m_connectionToolBar->setMovable(true);
    m_connectionToolBar->setFloatable(true);
    // Use same theme as main toolbar but remove outer border for a flat look
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
    // Reparent the widget into the toolbar
    m_connectionBar->setParent(nullptr);
    m_connectionToolBar->addWidget(m_connectionBar);
    QMainWindow::addToolBar(Qt::TopToolBarArea, m_connectionToolBar);
#ifdef Q_OS_MAC
    m_connectionToolBar->setFixedHeight(25);
    // also shrink wrapped widget
    m_connectionBar->setFixedHeight(25);
#endif

    // Create scrip bar (below connection bar) and wrap in a QToolBar
    m_scripBar = new ScripBar(container);
    connect(m_scripBar, &ScripBar::addToWatchRequested, this, &MainWindow::onAddToWatchRequested);
    m_scripToolBar = new QToolBar(tr("Scrip Bar"), this);
    m_scripToolBar->setAllowedAreas(Qt::TopToolBarArea | Qt::BottomToolBarArea);
    m_scripToolBar->setMovable(true);
    m_scripToolBar->setFloatable(true);
    // match main toolbar theme but remove outer border for flat look
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
    QMainWindow::addToolBar(Qt::TopToolBarArea, m_scripToolBar);
#ifdef Q_OS_MAC
    m_scripToolBar->setFixedHeight(25);
    m_scripBar->setMaximumHeight(25);
#endif

    // Custom MDI Area (Pure C++, no QMdiArea)
    m_mdiArea = new CustomMDIArea(container);
    layout->addWidget(m_mdiArea);

    // Create status bar
    createStatusBar();
    // Use QMainWindow's status bar
    setStatusBar(m_statusBar);

    // Create info bar
    createInfoBar();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    QSettings s("TradingCompany", "TradingTerminal");
    s.setValue("mainwindow/state", saveState());
    if (m_infoDock)
        s.setValue("mainwindow/info_visible", m_infoDock->isVisible());
    if (m_statusBar)
        s.setValue("mainwindow/status_visible", m_statusBar->isVisible());
    // Let base class handle the rest
    CustomMainWindow::closeEvent(event);
}

void MainWindow::createMenuBar()
{
    QMenuBar *menuBar = new QMenuBar(this);
    menuBar->setStyleSheet(
        "QMenuBar { "
        "   background-color: #2d2d30; "
        "   color: #ffffff; "
        "   border-bottom: 1px solid #3e3e42; "
        "} "
        "QMenuBar::item:selected { "
        "   background-color: #3e3e42; "
        "} "
        "QMenu { "
        "   background-color: #252526; "
        "   color: #ffffff; "
        "   border: 1px solid #3e3e42; "
        "} "
        "QMenu::item:selected { "
        "   background-color: #094771; "
        "}");

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
    m_statusBarAction->setChecked(m_statusBar && m_statusBar->isVisible());
    connect(m_statusBarAction, &QAction::toggled, this, [this](bool visible)
            { if (m_statusBar) m_statusBar->setVisible(visible); });

    m_infoBarAction = viewMenu->addAction("&Info Bar");
    m_infoBarAction->setCheckable(true);
    m_infoBarAction->setChecked(m_infoDock && m_infoDock->isVisible());
    connect(m_infoBarAction, &QAction::toggled, this, [this](bool visible)
            { if (m_infoDock) m_infoDock->setVisible(visible); });
    viewMenu->addSeparator();
    viewMenu->addAction("&Fullscreen");
    QAction *resetLayoutAction = viewMenu->addAction("Reset &Layout");
    connect(resetLayoutAction, &QAction::triggered, this, &MainWindow::resetLayout);

    // Window Menu
    QMenu *windowMenu = menuBar->addMenu("&Window");
    QAction *marketWatchAction = windowMenu->addAction("New &Market Watch");
    QAction *buyWindowAction = windowMenu->addAction("New &Buy Window");
    windowMenu->addSeparator();
    windowMenu->addAction("&Cascade");
    windowMenu->addAction("&Tile");

    connect(marketWatchAction, &QAction::triggered, this, &MainWindow::createMarketWatch);
    connect(buyWindowAction, &QAction::triggered, this, &MainWindow::createBuyWindow);

    // Tools Menu
    QMenu *toolsMenu = menuBar->addMenu("&Tools");
    toolsMenu->addAction("&Options Calculator");
    toolsMenu->addAction("&Strategy Builder");

    // Help Menu
    QMenu *helpMenu = menuBar->addMenu("&Help");
    helpMenu->addAction("&About");
    helpMenu->addAction("&Documentation");

    // Add menu bar to title bar (custom integration)
    titleBar()->layout()->addWidget(menuBar);
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

    m_infoBarWidget = new QWidget(this);
#ifdef Q_OS_MAC
    m_infoBarWidget->setFixedHeight(15);
#else
    m_infoBarWidget->setFixedHeight(25);
#endif
    m_infoBarWidget->setStyleSheet(
        "QWidget { "
        "   background-color: #1e1e1e; "
        "   border-top: 1px solid #3e3e42; "
        "} "
        "QLabel { "
        "   color: #888888; "
        "   font-size: 10px; "
        "   padding: 2px 4px; "
        "}");

    QHBoxLayout *layout = new QHBoxLayout(m_infoBarWidget);
    layout->setContentsMargins(10, 0, 10, 0);
    layout->setSpacing(10);

    QLabel *versionLabel = new QLabel("Trading Terminal v1.0.0", m_infoBarWidget);
    layout->addWidget(versionLabel);

    // Center info text (elidable if too long)
    m_infoTextLabel = new QLabel("Ready", m_infoBarWidget);
    m_infoTextLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_infoTextLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_infoTextLabel);

    // Right-hand compact widgets
    QLabel *separator = new QLabel("|", m_infoBarWidget);
    separator->setStyleSheet("color: #666666; padding: 0 6px;");
    layout->addWidget(separator);

    m_connIconLabel = new QLabel(m_infoBarWidget);
    m_connIconLabel->setFixedSize(12, 12);
    // draw a default gray circle
    QPixmap dot(12, 12);
    dot.fill(Qt::transparent);
    QPainter p(&dot);
    p.setRenderHint(QPainter::Antialiasing);
    p.setBrush(QBrush(QColor("#888888")));
    p.setPen(Qt::NoPen);
    p.drawEllipse(0, 0, 12, 12);
    p.end();
    m_connIconLabel->setPixmap(dot);
    m_connIconLabel->setToolTip("Not connected");
    layout->addWidget(m_connIconLabel);

    m_lastUpdateLabel = new QLabel("Last Update: --", m_infoBarWidget);
    m_lastUpdateLabel->setStyleSheet("color: #888888;");
    layout->addWidget(m_lastUpdateLabel);

    m_infoDock->setWidget(m_infoBarWidget);
    // Context menu for quick actions (hide)
    m_infoBarWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_infoBarWidget, &QWidget::customContextMenuRequested, this, &MainWindow::showInfoBarContextMenu);
    addDockWidget(Qt::BottomDockWidgetArea, m_infoDock);
}

void MainWindow::showInfoBarContextMenu(const QPoint &pos)
{
    if (!m_infoBarWidget || !m_infoDock)
        return;
    QMenu menu(m_infoBarWidget);
    QAction *hideAction = menu.addAction("Hide Info Bar");
    connect(hideAction, &QAction::triggered, this, [this]()
            {
        if (m_infoDock)
        {
            m_infoDock->hide();
            if (m_infoBarAction)
                m_infoBarAction->setChecked(false);
            // persist
            QSettings s("TradingCompany", "TradingTerminal");
            s.setValue("mainwindow/info_visible", false);
        } });
    menu.exec(m_infoBarWidget->mapToGlobal(pos));
}

void MainWindow::createMarketWatch()
{
    static int counter = 1;

    CustomMDISubWindow *window = new CustomMDISubWindow(QString("Market Watch %1").arg(counter++), m_mdiArea);

    // Create table for market watch
    QTableWidget *table = new QTableWidget(5, 7, window);
    table->setHorizontalHeaderLabels({"Symbol", "LTP", "Change", "Open", "High", "Low", "Volume"});
    table->horizontalHeader()->setStretchLastSection(true);
    table->setAlternatingRowColors(true);

    // Dark theme for table
    table->setStyleSheet(
        "QTableWidget { "
        "   background-color: #1e1e1e; "
        "   color: #cccccc; "
        "   gridline-color: #3e3e42; "
        "   border: none; "
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

    // Add to MDI area
    m_mdiArea->addWindow(window);

    qDebug() << "Market Watch created";
}

void MainWindow::createBuyWindow()
{
    CustomMDISubWindow *window = new CustomMDISubWindow("Buy Order", m_mdiArea);

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
