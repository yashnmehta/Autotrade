/**
 * @file ChartsWindow.cpp
 * @brief Multi-chart TradingView window implementation.
 *
 * Adapted from reference with project-specific paths:
 *   - Charting library: resources/tradingview/charting_library/
 *   - HTML file: resources/html/chart.html
 *   - Light-themed header bar (per project palette)
 */

#include "views/ChartsWindow.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QCoreApplication>
#include <QTimer>
#include <QWebEngineProfile>
#include <QWebEngineSettings>
#include <QWebEnginePage>
#include <QDebug>
#include <QHBoxLayout>

// ─────────────────────────────────────────────────────────────────────────────
// DebugWebPage – logs JS console messages to Qt qDebug
// ─────────────────────────────────────────────────────────────────────────────
class DebugWebPage : public QWebEnginePage {
public:
    DebugWebPage(QWebEngineProfile *profile, QObject *parent = nullptr)
        : QWebEnginePage(profile, parent) {}
protected:
    void javaScriptConsoleMessage(JavaScriptConsoleMessageLevel level,
                                  const QString &message,
                                  int lineNumber,
                                  const QString &sourceID) override {
        Q_UNUSED(level)
        qDebug() << "[JS Console] Line" << lineNumber << ":" << message
                 << "from" << sourceID;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Construction / Destruction
// ─────────────────────────────────────────────────────────────────────────────
ChartsWindow::ChartsWindow(QWidget *parent)
    : QWidget(parent)
{
    initUI();
}

ChartsWindow::ChartsWindow(const WindowContext &context, QWidget *parent)
    : QWidget(parent)
{
    m_lastContext = context;
    initUI();
    if (m_lastContext.token != 0 && !m_lastContext.symbol.isEmpty())
        addChart(m_lastContext);
}

ChartsWindow::~ChartsWindow()
{
    for (ChartInstance *chart : m_charts) {
        if (chart->webView) {
            chart->webView->setPage(nullptr);
            delete chart->webView;
        }
        delete chart->dataFeed;
        delete chart;
    }
    m_charts.clear();
}

// ─────────────────────────────────────────────────────────────────────────────
// initUI
// ─────────────────────────────────────────────────────────────────────────────
void ChartsWindow::initUI()
{
    qDebug() << "[ChartsWindow] Setting up UI.";
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // ── Top bar: ScripBar + layout toolbar ────────────────────────────────
    QHBoxLayout *topBarLayout = new QHBoxLayout();
    topBarLayout->setContentsMargins(5, 5, 5, 5);
    topBarLayout->setSpacing(10);

    m_scripBar = new ScripBar(this);
    connect(m_scripBar, &ScripBar::addToWatchRequested,
            this, &ChartsWindow::onScripSelected);
    topBarLayout->addWidget(m_scripBar, 1);

    initLayoutToolbar();
    topBarLayout->addWidget(m_layoutToolbar);

    mainLayout->addLayout(topBarLayout);

    // ── Chart grid container ──────────────────────────────────────────────
    m_chartContainerWidget = new QWidget(this);
    m_gridLayout = new QGridLayout(m_chartContainerWidget);
    m_gridLayout->setContentsMargins(0, 0, 0, 0);
    m_gridLayout->setSpacing(2);
    mainLayout->addWidget(m_chartContainerWidget, 1);
}

// ─────────────────────────────────────────────────────────────────────────────
// Layout toolbar
// ─────────────────────────────────────────────────────────────────────────────
void ChartsWindow::initLayoutToolbar()
{
    m_layoutToolbar = new QWidget(this);
    QHBoxLayout *layout = new QHBoxLayout(m_layoutToolbar);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);

    m_syncTimelineCheck = new QCheckBox("Sync Timeline", m_layoutToolbar);
    m_syncTimelineCheck->setStyleSheet("color: #1e293b;");
    m_syncTimelineCheck->setChecked(false);
    layout->addWidget(m_syncTimelineCheck);

    QLabel *lbl = new QLabel("Layout:", m_layoutToolbar);
    lbl->setStyleSheet("color: #1e293b;");
    layout->addWidget(lbl);

    m_layoutCombo = new QComboBox(m_layoutToolbar);
    m_layoutCombo->addItem("Single",    static_cast<int>(TVChartLayout::Single));
    m_layoutCombo->addItem("2 Horiz",   static_cast<int>(TVChartLayout::TwoHoriz));
    m_layoutCombo->addItem("2 Vert",    static_cast<int>(TVChartLayout::TwoVert));
    m_layoutCombo->addItem("3 Left",    static_cast<int>(TVChartLayout::ThreeLeft));
    m_layoutCombo->addItem("3 Right",   static_cast<int>(TVChartLayout::ThreeRight));
    m_layoutCombo->addItem("3 Top",     static_cast<int>(TVChartLayout::ThreeTop));
    m_layoutCombo->addItem("3 Bottom",  static_cast<int>(TVChartLayout::ThreeBottom));
    m_layoutCombo->addItem("4 Grid",    static_cast<int>(TVChartLayout::FourGrid));

    connect(m_layoutCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index) {
        setChartLayout(static_cast<TVChartLayout>(
            m_layoutCombo->itemData(index).toInt()));
        if (m_scripBar) m_scripBar->focusInput();
    });

    layout->addWidget(m_layoutCombo);
}

// ─────────────────────────────────────────────────────────────────────────────
// setChartLayout
// ─────────────────────────────────────────────────────────────────────────────
void ChartsWindow::setChartLayout(TVChartLayout layout)
{
    if (m_layout == layout) return;
    m_layout = layout;
    rearrangeGridLayout();
}

// ─────────────────────────────────────────────────────────────────────────────
// buildChartContainer – header strip + placeholder
// ─────────────────────────────────────────────────────────────────────────────
void ChartsWindow::buildChartContainer(ChartInstance *chart)
{
    chart->container = new QWidget(m_chartContainerWidget);
    QVBoxLayout *vbox = new QVBoxLayout(chart->container);
    vbox->setContentsMargins(0, 0, 0, 0);
    vbox->setSpacing(0);

    // Header strip
    QWidget *header = new QWidget(chart->container);
    header->setFixedHeight(24);
    header->setStyleSheet(
        "background-color: #f8fafc; color: #1e293b; "
        "border-bottom: 1px solid #e2e8f0;");
    QHBoxLayout *hbox = new QHBoxLayout(header);
    hbox->setContentsMargins(10, 0, 10, 0);

    chart->titleLabel = new QLabel(
        chart->context.symbol.isEmpty() ? "New Chart" : chart->context.symbol,
        header);
    chart->titleLabel->setStyleSheet("font-weight: bold; font-size: 13px; color: #1e293b;");

    chart->ltpLabel = new QLabel("LTP: --", header);
    chart->ltpLabel->setStyleSheet("color: #2563eb; font-weight: bold;");

    chart->closeBtn = new QPushButton("X", header);
    chart->closeBtn->setFixedSize(16, 16);
    chart->closeBtn->setStyleSheet(
        "QPushButton { border: none; font-weight: bold; color: #64748b; } "
        "QPushButton:hover { color: #dc2626; }");

    connect(chart->closeBtn, &QPushButton::clicked,
            this, [this, chart]() { removeChart(chart); });

    hbox->addWidget(chart->titleLabel);
    hbox->addStretch(1);
    hbox->addWidget(chart->ltpLabel);
    hbox->addSpacing(10);
    hbox->addWidget(chart->closeBtn);

    vbox->addWidget(header);

    // Loading placeholder
    chart->placeholder = new QWidget(chart->container);
    chart->placeholder->setStyleSheet("background-color: #131722;");
    QVBoxLayout *pl = new QVBoxLayout(chart->placeholder);
    QLabel *loadLabel = new QLabel("Loading TradingView...", chart->placeholder);
    loadLabel->setStyleSheet("color: #d1d4dc;");
    loadLabel->setAlignment(Qt::AlignCenter);
    pl->addWidget(loadLabel);
    vbox->addWidget(chart->placeholder, 1);
}

// ─────────────────────────────────────────────────────────────────────────────
// addChart
// ─────────────────────────────────────────────────────────────────────────────
void ChartsWindow::addChart(const WindowContext &ctx)
{
    // Capacity determined by current layout
    int capacity = 1;
    switch (m_layout) {
    case TVChartLayout::TwoHoriz:
    case TVChartLayout::TwoVert:     capacity = 2; break;
    case TVChartLayout::ThreeLeft:
    case TVChartLayout::ThreeRight:
    case TVChartLayout::ThreeTop:
    case TVChartLayout::ThreeBottom: capacity = 3; break;
    case TVChartLayout::FourGrid:    capacity = 4; break;
    default:                       capacity = 1; break;
    }

    // At capacity → replace last chart
    if (m_charts.size() >= capacity) {
        ChartInstance *last = m_charts.last();
        last->context = ctx;
        m_lastContext = ctx;
        if (last->titleLabel) last->titleLabel->setText(ctx.symbol);
        if (last->dataFeed)
            last->dataFeed->setCurrentToken(ctx.token, ctx.symbol, ctx.exchange);
        if (last->webEngineInitialized && last->webView) {
            QString js = QStringLiteral(
                "if (typeof setSymbol === 'function') { setSymbol('%1'); }")
                .arg(ctx.symbol);
            last->webView->page()->runJavaScript(js);
        }
        return;
    }

    ChartInstance *chart = new ChartInstance();
    chart->context = ctx;
    m_lastContext  = ctx;

    chart->dataFeed = new ChartDataFeed(this);
    chart->dataFeed->setMarketDataClient(m_marketDataClient);
    chart->dataFeed->setCurrentToken(ctx.token, ctx.symbol, ctx.exchange);

    // Cross-chart timeline sync
    connect(chart->dataFeed, &ChartDataFeed::visibleRangeChanged,
            this, [this, chart](double fromMs, double toMs) {
        if (!m_syncTimelineCheck || !m_syncTimelineCheck->isChecked()) return;
        const QString js = QStringLiteral(
            "if(typeof setVisibleRange==='function')setVisibleRange(%1,%2);")
            .arg(fromMs, 0, 'f', 0).arg(toMs, 0, 'f', 0);
        for (ChartInstance *other : m_charts) {
            if (other != chart && other->webEngineInitialized && other->webView)
                other->webView->page()->runJavaScript(js);
        }
    });

    // LTP display on live update
    connect(chart->dataFeed, &ChartDataFeed::realtimeUpdate,
            this, [chart](const QString &, const QJsonObject &bar) {
        if (!chart->ltpLabel) return;
        double c = bar["close"].toDouble();
        if (c > 0.0)
            chart->ltpLabel->setText(QStringLiteral("LTP: %1").arg(c, 0, 'f', 2));
    });

    // Forward order signals
    connect(chart->dataFeed, &ChartDataFeed::orderFromChartRequested,
            this, &ChartsWindow::orderFromChartRequested);

    buildChartContainer(chart);
    m_charts.append(chart);
    rearrangeGridLayout();

    // Defer WebEngine init to keep UI responsive
    QTimer::singleShot(50 + m_charts.size() * 100,
                       this, [this, chart]() { initWebEngineForChart(chart); });
}

// ─────────────────────────────────────────────────────────────────────────────
// removeChart
// ─────────────────────────────────────────────────────────────────────────────
void ChartsWindow::removeChart(ChartInstance *chart)
{
    if (m_charts.size() <= 1) return;

    int idx = m_charts.indexOf(chart);
    if (idx < 0) return;

    m_charts.removeAt(idx);
    m_gridLayout->removeWidget(chart->container);

    if (chart->webView) {
        chart->webView->setPage(nullptr);
        chart->webView->deleteLater();
    }
    chart->dataFeed->deleteLater();
    chart->container->deleteLater();
    delete chart;

    int size = m_charts.size();

    // Auto-rescale layout to avoid blank spaces
    if (size == 1 && m_layout != TVChartLayout::Single) {
        m_layoutCombo->setCurrentIndex(0);
    } else if (size == 2 &&
               m_layout != TVChartLayout::TwoHoriz &&
               m_layout != TVChartLayout::TwoVert) {
        m_layoutCombo->setCurrentIndex(1);
    } else if (size == 3 &&
               m_layout != TVChartLayout::ThreeLeft &&
               m_layout != TVChartLayout::ThreeRight &&
               m_layout != TVChartLayout::ThreeTop &&
               m_layout != TVChartLayout::ThreeBottom) {
        m_layoutCombo->setCurrentIndex(6);
    } else {
        rearrangeGridLayout();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// rearrangeGridLayout – 8 layout modes
// ─────────────────────────────────────────────────────────────────────────────
void ChartsWindow::rearrangeGridLayout()
{
    // Clear existing grid items without deleting widgets
    while (QLayoutItem *item = m_gridLayout->takeAt(0))
        delete item;

    int count = m_charts.size();
    if (count == 0) return;

    auto widgetAt = [&](int i) -> QWidget * {
        return (i < count) ? m_charts[i]->container : nullptr;
    };

    switch (m_layout) {
    case TVChartLayout::Single:
        if (auto *w = widgetAt(0)) m_gridLayout->addWidget(w, 0, 0, 1, 2);
        m_gridLayout->setColumnStretch(0, 1);
        m_gridLayout->setColumnStretch(1, 0);
        m_gridLayout->setRowStretch(0, 1);
        m_gridLayout->setRowStretch(1, 0);
        break;

    case TVChartLayout::TwoHoriz:
        if (auto *w = widgetAt(0)) m_gridLayout->addWidget(w, 0, 0, 1, 1);
        if (auto *w = widgetAt(1)) m_gridLayout->addWidget(w, 0, 1, 1, 1);
        m_gridLayout->setColumnStretch(0, 1);
        m_gridLayout->setColumnStretch(1, 1);
        m_gridLayout->setRowStretch(0, 1);
        m_gridLayout->setRowStretch(1, 0);
        break;

    case TVChartLayout::TwoVert:
        if (auto *w = widgetAt(0)) m_gridLayout->addWidget(w, 0, 0, 1, 2);
        if (auto *w = widgetAt(1)) m_gridLayout->addWidget(w, 1, 0, 1, 2);
        m_gridLayout->setColumnStretch(0, 1);
        m_gridLayout->setColumnStretch(1, 0);
        m_gridLayout->setRowStretch(0, 1);
        m_gridLayout->setRowStretch(1, 1);
        break;

    case TVChartLayout::ThreeLeft:
        if (auto *w = widgetAt(0)) m_gridLayout->addWidget(w, 0, 0, 2, 1);
        if (auto *w = widgetAt(1)) m_gridLayout->addWidget(w, 0, 1, 1, 1);
        if (auto *w = widgetAt(2)) m_gridLayout->addWidget(w, 1, 1, 1, 1);
        m_gridLayout->setColumnStretch(0, 1);
        m_gridLayout->setColumnStretch(1, 1);
        m_gridLayout->setRowStretch(0, 1);
        m_gridLayout->setRowStretch(1, 1);
        break;

    case TVChartLayout::ThreeRight:
        if (auto *w = widgetAt(0)) m_gridLayout->addWidget(w, 0, 1, 2, 1);
        if (auto *w = widgetAt(1)) m_gridLayout->addWidget(w, 0, 0, 1, 1);
        if (auto *w = widgetAt(2)) m_gridLayout->addWidget(w, 1, 0, 1, 1);
        m_gridLayout->setColumnStretch(0, 1);
        m_gridLayout->setColumnStretch(1, 1);
        m_gridLayout->setRowStretch(0, 1);
        m_gridLayout->setRowStretch(1, 1);
        break;

    case TVChartLayout::ThreeTop:
        if (auto *w = widgetAt(0)) m_gridLayout->addWidget(w, 0, 0, 1, 2);
        if (auto *w = widgetAt(1)) m_gridLayout->addWidget(w, 1, 0, 1, 1);
        if (auto *w = widgetAt(2)) m_gridLayout->addWidget(w, 1, 1, 1, 1);
        m_gridLayout->setColumnStretch(0, 1);
        m_gridLayout->setColumnStretch(1, 1);
        m_gridLayout->setRowStretch(0, 1);
        m_gridLayout->setRowStretch(1, 1);
        break;

    case TVChartLayout::ThreeBottom:
        if (auto *w = widgetAt(0)) m_gridLayout->addWidget(w, 0, 0, 1, 1);
        if (auto *w = widgetAt(1)) m_gridLayout->addWidget(w, 0, 1, 1, 1);
        if (auto *w = widgetAt(2)) m_gridLayout->addWidget(w, 1, 0, 1, 2);
        m_gridLayout->setColumnStretch(0, 1);
        m_gridLayout->setColumnStretch(1, 1);
        m_gridLayout->setRowStretch(0, 1);
        m_gridLayout->setRowStretch(1, 1);
        break;

    case TVChartLayout::FourGrid:
    default:
        for (int i = 0; i < count; ++i) {
            if (auto *w = widgetAt(i))
                m_gridLayout->addWidget(w, i / 2, i % 2, 1, 1);
        }
        m_gridLayout->setColumnStretch(0, 1);
        m_gridLayout->setColumnStretch(1, 1);
        m_gridLayout->setRowStretch(0, 1);
        m_gridLayout->setRowStretch(1, 1);
        break;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// initWebEngineForChart – heavy init deferred via QTimer
// ─────────────────────────────────────────────────────────────────────────────
void ChartsWindow::initWebEngineForChart(ChartInstance *chart)
{
    if (chart->webEngineInitialized) return;

    chart->webView = new QWebEngineView(chart->container);

    QWebEngineProfile *profile = QWebEngineProfile::defaultProfile();
    profile->setHttpCacheType(QWebEngineProfile::DiskHttpCache);
    profile->setPersistentCookiesPolicy(QWebEngineProfile::NoPersistentCookies);

    chart->webView->setPage(new DebugWebPage(profile, chart->webView));

    QWebEngineSettings *settings = chart->webView->page()->settings();
    settings->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls, true);
    settings->setAttribute(QWebEngineSettings::LocalContentCanAccessFileUrls, true);
    settings->setAttribute(QWebEngineSettings::JavascriptEnabled, true);

    // Register ChartDataFeed as the QWebChannel backend
    chart->webChannel = new QWebChannel(chart->webView->page());
    chart->webChannel->registerObject(QStringLiteral("backend"), chart->dataFeed);
    chart->webView->page()->setWebChannel(chart->webChannel);

    // ── Resolve paths ────────────────────────────────────────────────────
    // Our charting library lives at: <appDir>/resources/tradingview/charting_library/
    // chart.html lives at:           <appDir>/resources/html/chart.html
    QString appDir = QCoreApplication::applicationDirPath();

    // HTML file — try build_ninja output dir first, then relative paths
    QString htmlPath = QDir(appDir).filePath("resources/html/chart.html");
    if (!QFile::exists(htmlPath))
        htmlPath = QDir(appDir).filePath("../../resources/html/chart.html");
    if (!QFile::exists(htmlPath))
        htmlPath = QDir(appDir).filePath("../resources/html/chart.html");

    // Charting library JS path
    QString libDir = QDir(appDir).filePath("resources/tradingview/charting_library/");
    if (!QDir(libDir).exists())
        libDir = QDir(appDir).filePath("../../resources/tradingview/charting_library/");
    if (!QDir(libDir).exists())
        libDir = QDir(appDir).filePath("../resources/tradingview/charting_library/");

    QString jsLibPath = "file:///" + QFileInfo(libDir).absoluteFilePath() + "/";

    // ── Load HTML with template substitution ─────────────────────────────
    QFile file(htmlPath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QString htmlContent = file.readAll();
        htmlContent.replace("{{LIBRARY_PATH}}", jsLibPath);

        QString initialSymbol =
            (!chart->context.symbol.isEmpty())
                ? chart->context.symbol : "RELIANCE";
        htmlContent.replace("{{INITIAL_SYMBOL}}", initialSymbol);

        chart->webView->setHtml(
            htmlContent,
            QUrl::fromLocalFile(QFileInfo(htmlPath).absoluteFilePath()));
    } else {
        qWarning() << "[ChartsWindow] Failed to load chart.html from" << htmlPath;
    }

    // Swap placeholder → webView
    QVBoxLayout *vl = qobject_cast<QVBoxLayout *>(chart->container->layout());
    if (vl && chart->placeholder) {
        int idx = vl->indexOf(chart->placeholder);
        if (idx >= 0) {
            vl->removeWidget(chart->placeholder);
            chart->placeholder->deleteLater();
            chart->placeholder = nullptr;
            vl->insertWidget(idx, chart->webView, 1);
        }
    }

    // Install event filter for ESC key handling
    chart->webView->installEventFilter(this);
    if (chart->webView->focusProxy())
        chart->webView->focusProxy()->installEventFilter(this);

    chart->webEngineInitialized = true;
    qDebug() << "[ChartsWindow] WebEngine initialized for:" << chart->context.symbol;
}

// ─────────────────────────────────────────────────────────────────────────────
// eventFilter – handle ESC to close a chart panel
// ─────────────────────────────────────────────────────────────────────────────
bool ChartsWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->key() == Qt::Key_Escape) {
            for (ChartInstance *chart : m_charts) {
                if (watched == chart->webView ||
                    watched == chart->webView->focusProxy() ||
                    watched == chart->container) {
                    qDebug() << "[ChartsWindow] ESC → closing chart:"
                             << chart->context.symbol;
                    removeChart(chart);
                    return true;
                }
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

// ─────────────────────────────────────────────────────────────────────────────
// loadFromContext – called when MDI opens chart for a specific instrument
// ─────────────────────────────────────────────────────────────────────────────
void ChartsWindow::loadFromContext(const WindowContext &context)
{
    m_lastContext = context;
    if (m_lastContext.token == 0 || m_lastContext.symbol.isEmpty()) return;

    if (!m_charts.isEmpty()) {
        ChartInstance *first = m_charts.first();
        first->context = m_lastContext;
        if (first->titleLabel)
            first->titleLabel->setText(m_lastContext.symbol);
        if (first->dataFeed)
            first->dataFeed->setCurrentToken(
                m_lastContext.token, m_lastContext.symbol, m_lastContext.exchange);
        if (first->webEngineInitialized && first->webView) {
            QString js = QStringLiteral(
                "if (typeof setSymbol === 'function') { setSymbol('%1'); }")
                .arg(m_lastContext.symbol);
            first->webView->page()->runJavaScript(js);
        }
    } else {
        addChart(m_lastContext);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// setMarketDataClient
// ─────────────────────────────────────────────────────────────────────────────
void ChartsWindow::setMarketDataClient(XTSMarketDataClient *client)
{
    m_marketDataClient = client;

    // ScripBar needs the XTS client for instrument search
    if (m_scripBar)
        m_scripBar->setXTSClient(client);

    for (ChartInstance *chart : m_charts) {
        if (chart->dataFeed)
            chart->dataFeed->setMarketDataClient(client);

        if (client && chart->webEngineInitialized && chart->webView) {
            QTimer::singleShot(200, this, [chart]() {
                chart->webView->page()->runJavaScript(
                    "try{if(window.tvWidget){var c=window.tvWidget.activeChart();"
                    "if(c)c.resetData();}}catch(e){}");
            });
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// onScripSelected – user picked a symbol via ScripBar
// ─────────────────────────────────────────────────────────────────────────────
void ChartsWindow::onScripSelected(const InstrumentData &data)
{
    qDebug() << "[ChartsWindow] Scrip selected:" << data.symbol;

    WindowContext ctx;
    ctx.exchange = QString::number(data.exchangeSegment);
    ctx.token    = data.exchangeInstrumentID;
    ctx.symbol   = data.symbol;
    ctx.series   = data.series;

    if (m_charts.size() < 4) {
        addChart(ctx);
    } else {
        // Full → replace last
        ChartInstance *last = m_charts.last();
        last->context = ctx;
        if (last->titleLabel)
            last->titleLabel->setText(ctx.symbol);
        if (last->dataFeed)
            last->dataFeed->setCurrentToken(ctx.token, ctx.symbol, ctx.exchange);
        if (last->webEngineInitialized && last->webView) {
            QString js = QStringLiteral(
                "if (typeof setSymbol === 'function') { setSymbol('%1'); }")
                .arg(ctx.symbol);
            last->webView->page()->runJavaScript(js);
        }
    }
}
