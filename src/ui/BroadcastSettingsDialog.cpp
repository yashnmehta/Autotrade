#include "ui/BroadcastSettingsDialog.h"
#include "utils/ConfigLoader.h"
#include "services/UdpBroadcastService.h"
#include "services/XTSFeedBridge.h"
#include "core/ExchangeSegment.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QPushButton>
#include <QTabWidget>
#include <QTimer>
#include <QFrame>
#include <QMessageBox>
#include <QDebug>

// ═══════════════════════════════════════════════════════════════════════
// Constructor
// ═══════════════════════════════════════════════════════════════════════

BroadcastSettingsDialog::BroadcastSettingsDialog(ConfigLoader* configLoader,
                                                   QWidget *parent)
    : QDialog(parent)
    , m_configLoader(configLoader)
{
    setWindowTitle("Connection & Broadcast Settings");
    setMinimumSize(560, 500);
    resize(600, 540);
    setModal(false);

    buildUI();
    applyStyleSheet();
    loadFromConfig();

    // Wire to ConnectionStatusManager for live updates
    auto& mgr = ConnectionStatusManager::instance();
    connect(&mgr, &ConnectionStatusManager::stateChanged,
            this, &BroadcastSettingsDialog::onStateChanged);

    // Stats refresh timer (1 second)
    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setInterval(1000);
    connect(m_refreshTimer, &QTimer::timeout, this, &BroadcastSettingsDialog::refreshStats);
    m_refreshTimer->start();

    // Trigger initial refresh
    refreshStats();
}

// ═══════════════════════════════════════════════════════════════════════
// Build UI
// ═══════════════════════════════════════════════════════════════════════

void BroadcastSettingsDialog::buildUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    // ── Title ────────────────────────────────────────────────────────
    QLabel* titleLabel = new QLabel("Connection & Broadcast Settings", this);
    titleLabel->setObjectName("dialogTitle");
    mainLayout->addWidget(titleLabel);

    // ── Tab Widget ───────────────────────────────────────────────────
    QTabWidget* tabs = new QTabWidget(this);
    tabs->addTab(createStatusPanel(),   "Status");
    tabs->addTab(createUdpPanel(),      "UDP Config");
    tabs->addTab(createFeedModePanel(), "Feed Mode");
    mainLayout->addWidget(tabs, 1);

    // ── Bottom buttons ───────────────────────────────────────────────
    QHBoxLayout* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();

    m_applyBtn = new QPushButton("Apply && Restart", this);
    m_applyBtn->setObjectName("applyBtn");
    connect(m_applyBtn, &QPushButton::clicked, this, &BroadcastSettingsDialog::onApply);
    btnLayout->addWidget(m_applyBtn);

    m_closeBtn = new QPushButton("Close", this);
    m_closeBtn->setObjectName("closeBtn");
    connect(m_closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    btnLayout->addWidget(m_closeBtn);

    mainLayout->addLayout(btnLayout);
}

QWidget* BroadcastSettingsDialog::createStatusPanel()
{
    QWidget* panel = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    // XTS Status group
    QGroupBox* xtsGroup = new QGroupBox("XTS WebSocket Connections", panel);
    QVBoxLayout* xtsLayout = new QVBoxLayout(xtsGroup);

    // XTS MD — always connected
    auto createStatusRow = [this, panel](ConnectionId id, const QString& label) -> QWidget* {
        QWidget* row = new QWidget(panel);
        QHBoxLayout* rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(4, 2, 4, 2);
        rowLayout->setSpacing(8);

        ReceiverRow rr;
        rr.id = id;

        rr.statusDot = new QLabel("●", row);
        rr.statusDot->setFixedWidth(14);
        rr.statusDot->setAlignment(Qt::AlignCenter);
        rr.statusDot->setStyleSheet(
            QString("QLabel { color: %1; font-size: 12px; }")
                .arg(connectionStateColor(ConnectionState::Disconnected)));
        rowLayout->addWidget(rr.statusDot);

        QLabel* nameLabel = new QLabel(label, row);
        nameLabel->setMinimumWidth(140);
        rowLayout->addWidget(nameLabel);

        rr.statusText = new QLabel("Disconnected", row);
        rr.statusText->setObjectName("statusText");
        rr.statusText->setMinimumWidth(100);
        rowLayout->addWidget(rr.statusText);

        rr.ppsLabel = new QLabel("—", row);
        rr.ppsLabel->setObjectName("ppsLabel");
        rr.ppsLabel->setMinimumWidth(60);
        rr.ppsLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        rowLayout->addWidget(rr.ppsLabel);

        rowLayout->addStretch();

        m_rows[id] = rr;
        return row;
    };

    xtsLayout->addWidget(createStatusRow(ConnectionId::XTS_MARKET_DATA, "Market Data (always on)"));
    xtsLayout->addWidget(createStatusRow(ConnectionId::XTS_INTERACTIVE, "Interactive (orders)"));
    layout->addWidget(xtsGroup);

    // UDP Status group
    QGroupBox* udpGroup = new QGroupBox("UDP Broadcast Receivers", panel);
    QVBoxLayout* udpLayout = new QVBoxLayout(udpGroup);

    udpLayout->addWidget(createStatusRow(ConnectionId::UDP_NSEFO, "NSE F&O"));
    udpLayout->addWidget(createStatusRow(ConnectionId::UDP_NSECM, "NSE Cash"));
    udpLayout->addWidget(createStatusRow(ConnectionId::UDP_BSEFO, "BSE F&O"));
    udpLayout->addWidget(createStatusRow(ConnectionId::UDP_BSECM, "BSE Cash"));
    layout->addWidget(udpGroup);

    // XTS Feed Bridge stats
    QGroupBox* bridgeGroup = new QGroupBox("XTS Subscription Stats", panel);
    QVBoxLayout* bridgeLayout = new QVBoxLayout(bridgeGroup);
    QLabel* bridgeInfo = new QLabel("—", bridgeGroup);
    bridgeInfo->setObjectName("bridgeStatsLabel");
    bridgeLayout->addWidget(bridgeInfo);
    layout->addWidget(bridgeGroup);

    layout->addStretch();
    return panel;
}

// Helper for header labels (file-local)
static QLabel* createHeaderLabel(const QString& text, QWidget* parent) {
    QLabel* label = new QLabel(text, parent);
    label->setStyleSheet("QLabel { color: #64748b; font-size: 10px; font-weight: 600; }");
    return label;
}

QWidget* BroadcastSettingsDialog::createUdpPanel()
{
    QWidget* panel = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    QLabel* helpLabel = new QLabel(
        "Configure UDP multicast addresses for each exchange segment.\n"
        "Changes take effect when you click 'Apply & Restart' or use the per-row restart button.",
        panel);
    helpLabel->setObjectName("helpLabel");
    helpLabel->setWordWrap(true);
    layout->addWidget(helpLabel);

    // Column headers
    QWidget* headerRow = new QWidget(panel);
    QHBoxLayout* headerLay = new QHBoxLayout(headerRow);
    headerLay->setContentsMargins(4, 0, 4, 0);
    headerLay->setSpacing(8);
    headerLay->addWidget(new QLabel("", headerRow), 0); // dot spacer
    headerLay->addWidget(createHeaderLabel("Segment", headerRow), 2);
    headerLay->addWidget(createHeaderLabel("Multicast IP", headerRow), 3);
    headerLay->addWidget(createHeaderLabel("Port", headerRow), 1);
    headerLay->addWidget(createHeaderLabel("pps", headerRow), 1);
    headerLay->addWidget(new QLabel("", headerRow), 1); // button spacer
    layout->addWidget(headerRow);

    // Receiver rows
    layout->addWidget(createReceiverRow(ConnectionId::UDP_NSEFO, "NSE F&O"));
    layout->addWidget(createReceiverRow(ConnectionId::UDP_NSECM, "NSE Cash"));
    layout->addWidget(createReceiverRow(ConnectionId::UDP_BSEFO, "BSE F&O"));
    layout->addWidget(createReceiverRow(ConnectionId::UDP_BSECM, "BSE Cash"));

    layout->addStretch();
    return panel;
}

QWidget* BroadcastSettingsDialog::createReceiverRow(ConnectionId id, const QString& label)
{
    QWidget* row = new QWidget(this);
    QHBoxLayout* rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(4, 4, 4, 4);
    rowLayout->setSpacing(8);

    // Check if we already have a ReceiverRow for this id (from status panel)
    // If not, create fresh widgets
    ReceiverRow rr;
    if (m_rows.contains(id)) {
        // Status panel already has dot+status for this id;
        // the UDP panel gets its own IP/port/restart controls
        rr = m_rows[id];
    } else {
        rr.id = id;
    }

    // Status dot (small indicator)
    QLabel* dot = new QLabel("●", row);
    dot->setFixedWidth(14);
    dot->setAlignment(Qt::AlignCenter);
    dot->setStyleSheet(
        QString("QLabel { color: %1; font-size: 10px; }")
            .arg(connectionStateColor(ConnectionState::Disconnected)));
    rowLayout->addWidget(dot, 0);

    // Label
    QLabel* nameLabel = new QLabel(label, row);
    rowLayout->addWidget(nameLabel, 2);

    // IP input
    rr.ipEdit = new QLineEdit(row);
    rr.ipEdit->setPlaceholderText("e.g. 233.1.2.5");
    rr.ipEdit->setMinimumWidth(120);
    rowLayout->addWidget(rr.ipEdit, 3);

    // Port input
    rr.portSpin = new QSpinBox(row);
    rr.portSpin->setRange(1024, 65535);
    rr.portSpin->setMinimumWidth(70);
    rowLayout->addWidget(rr.portSpin, 1);

    // PPS label
    QLabel* ppsLbl = new QLabel("—", row);
    ppsLbl->setObjectName("ppsLabel");
    ppsLbl->setMinimumWidth(50);
    ppsLbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    rowLayout->addWidget(ppsLbl, 1);

    // Restart button
    rr.restartBtn = new QPushButton("Restart", row);
    rr.restartBtn->setObjectName("restartBtn");
    rr.restartBtn->setFixedWidth(65);
    connect(rr.restartBtn, &QPushButton::clicked, this, [this, id]() {
        onRestartReceiver(id);
    });
    rowLayout->addWidget(rr.restartBtn, 1);

    // Store the dot and pps label so refreshStats can update them
    rr.statusDot = (rr.statusDot != nullptr) ? rr.statusDot : dot;
    rr.ppsLabel = ppsLbl;

    m_rows[id] = rr;
    return row;
}

QWidget* BroadcastSettingsDialog::createFeedModePanel()
{
    QWidget* panel = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(8, 12, 8, 8);
    layout->setSpacing(12);

    QLabel* helpLabel = new QLabel(
        "Select the primary source for live tick data.\n\n"
        "• Hybrid (UDP Primary) — Office / co-located environment.\n"
        "  UDP multicast provides ultra-low-latency ticks.\n"
        "  XTS WebSocket supplements with 1-min OHLC candles (1505).\n\n"
        "• XTS Only — Internet / home user, no UDP available.\n"
        "  All price data comes through XTS WebSocket.\n"
        "  Subject to 1000-token subscription cap.\n\n"
        "Note: XTS Market Data WebSocket is ALWAYS connected\n"
        "regardless of this setting (required for candle data).",
        panel);
    helpLabel->setObjectName("helpLabel");
    helpLabel->setWordWrap(true);
    layout->addWidget(helpLabel);

    // Feed mode combo
    QHBoxLayout* comboRow = new QHBoxLayout();
    QLabel* modeLabel = new QLabel("Primary Data Source:", panel);
    modeLabel->setStyleSheet("QLabel { font-weight: 600; }");
    comboRow->addWidget(modeLabel);

    m_feedModeCombo = new QComboBox(panel);
    m_feedModeCombo->addItem("Hybrid (UDP Primary)", "hybrid");
    m_feedModeCombo->addItem("XTS Only", "xts_only");
    m_feedModeCombo->setMinimumWidth(200);

    // Set current selection from ConnectionStatusManager
    auto& mgr = ConnectionStatusManager::instance();
    if (mgr.isUdpPrimary()) {
        m_feedModeCombo->setCurrentIndex(0);
    } else {
        m_feedModeCombo->setCurrentIndex(1);
    }

    connect(m_feedModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &BroadcastSettingsDialog::onFeedModeChanged);
    comboRow->addWidget(m_feedModeCombo);
    comboRow->addStretch();
    layout->addLayout(comboRow);

    // Migration status area
    QGroupBox* migrationGroup = new QGroupBox("Migration Status", panel);
    QVBoxLayout* migLayout = new QVBoxLayout(migrationGroup);
    QLabel* migStatus = new QLabel("No migration in progress", migrationGroup);
    migStatus->setObjectName("migrationStatus");
    migLayout->addWidget(migStatus);

    // Wire migration progress
    connect(&mgr, &ConnectionStatusManager::migrationProgress,
            migStatus, &QLabel::setText);

    layout->addWidget(migrationGroup);

    layout->addStretch();
    return panel;
}

// ═══════════════════════════════════════════════════════════════════════
// Load Config
// ═══════════════════════════════════════════════════════════════════════

void BroadcastSettingsDialog::loadFromConfig()
{
    if (!m_configLoader) return;

    // Populate IP/port fields from config
    auto setRow = [this](ConnectionId id, const QString& ip, int port) {
        auto it = m_rows.find(id);
        if (it != m_rows.end()) {
            if (it->ipEdit)   it->ipEdit->setText(ip);
            if (it->portSpin) it->portSpin->setValue(port);
        }
    };

    setRow(ConnectionId::UDP_NSEFO,
           m_configLoader->getNSEFOMulticastIP(),
           m_configLoader->getNSEFOPort());
    setRow(ConnectionId::UDP_NSECM,
           m_configLoader->getNSECMMulticastIP(),
           m_configLoader->getNSECMPort());
    setRow(ConnectionId::UDP_BSEFO,
           m_configLoader->getBSEFOMulticastIP(),
           m_configLoader->getBSEFOPort());
    setRow(ConnectionId::UDP_BSECM,
           m_configLoader->getBSECMMulticastIP(),
           m_configLoader->getBSECMPort());
}

// ═══════════════════════════════════════════════════════════════════════
// Stylesheet
// ═══════════════════════════════════════════════════════════════════════

void BroadcastSettingsDialog::applyStyleSheet()
{
    setStyleSheet(
        // Dialog background
        "BroadcastSettingsDialog {"
        "  background-color: #ffffff;"
        "}"
        // Title
        "QLabel#dialogTitle {"
        "  color: #1e293b;"
        "  font-size: 16px;"
        "  font-weight: 700;"
        "  padding-bottom: 4px;"
        "}"
        // Group boxes
        "QGroupBox {"
        "  background-color: #f8fafc;"
        "  border: 1px solid #e2e8f0;"
        "  border-radius: 6px;"
        "  margin-top: 12px;"
        "  padding: 12px 8px 8px 8px;"
        "  font-weight: 600;"
        "  color: #1e293b;"
        "}"
        "QGroupBox::title {"
        "  subcontrol-origin: margin;"
        "  left: 12px;"
        "  padding: 0 4px;"
        "}"
        // Labels
        "QLabel {"
        "  color: #1e293b;"
        "  font-size: 12px;"
        "}"
        "QLabel#helpLabel {"
        "  color: #475569;"
        "  font-size: 11px;"
        "}"
        "QLabel#statusText {"
        "  color: #475569;"
        "  font-size: 11px;"
        "}"
        "QLabel#ppsLabel {"
        "  color: #64748b;"
        "  font-size: 10px;"
        "}"
        "QLabel#migrationStatus {"
        "  color: #475569;"
        "  font-size: 11px;"
        "  padding: 4px;"
        "}"
        // Inputs
        "QLineEdit {"
        "  background: #ffffff;"
        "  border: 1px solid #e2e8f0;"
        "  border-radius: 4px;"
        "  padding: 4px 6px;"
        "  color: #1e293b;"
        "  font-size: 12px;"
        "}"
        "QLineEdit:focus {"
        "  border-color: #3b82f6;"
        "}"
        "QSpinBox {"
        "  background: #ffffff;"
        "  border: 1px solid #e2e8f0;"
        "  border-radius: 4px;"
        "  padding: 4px 6px;"
        "  color: #1e293b;"
        "  font-size: 12px;"
        "}"
        "QSpinBox:focus {"
        "  border-color: #3b82f6;"
        "}"
        // Combo box
        "QComboBox {"
        "  background: #ffffff;"
        "  border: 1px solid #e2e8f0;"
        "  border-radius: 4px;"
        "  padding: 4px 8px;"
        "  color: #1e293b;"
        "  font-size: 12px;"
        "  min-height: 24px;"
        "}"
        "QComboBox:focus {"
        "  border-color: #3b82f6;"
        "}"
        "QComboBox QAbstractItemView {"
        "  background: #ffffff;"
        "  border: 1px solid #e2e8f0;"
        "  selection-background-color: #dbeafe;"
        "  selection-color: #1e40af;"
        "}"
        // Tab widget
        "QTabWidget::pane {"
        "  border: 1px solid #e2e8f0;"
        "  border-radius: 4px;"
        "  background: #ffffff;"
        "}"
        "QTabBar::tab {"
        "  background: #f1f5f9;"
        "  border: 1px solid #e2e8f0;"
        "  border-bottom: none;"
        "  padding: 6px 16px;"
        "  color: #475569;"
        "  font-size: 12px;"
        "  border-top-left-radius: 4px;"
        "  border-top-right-radius: 4px;"
        "}"
        "QTabBar::tab:selected {"
        "  background: #ffffff;"
        "  color: #2563eb;"
        "  font-weight: 600;"
        "}"
        "QTabBar::tab:hover {"
        "  background: #e2e8f0;"
        "}"
        // Buttons
        "QPushButton#applyBtn {"
        "  background-color: #2563eb;"
        "  color: #ffffff;"
        "  border: none;"
        "  border-radius: 4px;"
        "  padding: 6px 16px;"
        "  font-size: 12px;"
        "  font-weight: 600;"
        "  min-width: 100px;"
        "}"
        "QPushButton#applyBtn:hover {"
        "  background-color: #1d4ed8;"
        "}"
        "QPushButton#applyBtn:pressed {"
        "  background-color: #1e40af;"
        "}"
        "QPushButton#closeBtn {"
        "  background-color: #f1f5f9;"
        "  color: #475569;"
        "  border: 1px solid #e2e8f0;"
        "  border-radius: 4px;"
        "  padding: 6px 16px;"
        "  font-size: 12px;"
        "  min-width: 70px;"
        "}"
        "QPushButton#closeBtn:hover {"
        "  background-color: #e2e8f0;"
        "}"
        "QPushButton#restartBtn {"
        "  background-color: #f1f5f9;"
        "  color: #475569;"
        "  border: 1px solid #e2e8f0;"
        "  border-radius: 3px;"
        "  padding: 3px 8px;"
        "  font-size: 10px;"
        "}"
        "QPushButton#restartBtn:hover {"
        "  background-color: #dbeafe;"
        "  color: #2563eb;"
        "  border-color: #3b82f6;"
        "}"
    );
}

// ═══════════════════════════════════════════════════════════════════════
// Slots
// ═══════════════════════════════════════════════════════════════════════

void BroadcastSettingsDialog::onApply()
{
    qDebug() << "[BroadcastSettingsDialog] Applying settings...";

    // 1. Read values from UDP config rows
    auto applyRow = [this](ConnectionId id, ExchangeSegment seg) {
        auto it = m_rows.find(id);
        if (it == m_rows.end() || !it->ipEdit || !it->portSpin) return;

        QString ip = it->ipEdit->text().trimmed();
        int port = it->portSpin->value();

        if (ip.isEmpty() || port == 0) return;

        auto& udp = UdpBroadcastService::instance();
        bool wasActive = udp.isReceiverActive(seg);

        if (wasActive) {
            qDebug() << "[BroadcastSettingsDialog] Restarting" << connectionIdToLabel(id)
                     << "with" << ip << ":" << port;
            udp.restartReceiver(seg, ip.toStdString(), port);
        }
    };

    applyRow(ConnectionId::UDP_NSEFO, ExchangeSegment::NSEFO);
    applyRow(ConnectionId::UDP_NSECM, ExchangeSegment::NSECM);
    applyRow(ConnectionId::UDP_BSEFO, ExchangeSegment::BSEFO);
    applyRow(ConnectionId::UDP_BSECM, ExchangeSegment::BSECM);

    qDebug() << "[BroadcastSettingsDialog] Settings applied";
}

void BroadcastSettingsDialog::onRestartReceiver(ConnectionId id)
{
    auto it = m_rows.find(id);
    if (it == m_rows.end() || !it->ipEdit || !it->portSpin) return;

    QString ip = it->ipEdit->text().trimmed();
    int port = it->portSpin->value();

    if (ip.isEmpty()) {
        QMessageBox::warning(this, "Invalid Config", "IP address is empty");
        return;
    }

    // Map ConnectionId to ExchangeSegment
    ExchangeSegment seg;
    switch (id) {
        case ConnectionId::UDP_NSEFO: seg = ExchangeSegment::NSEFO; break;
        case ConnectionId::UDP_NSECM: seg = ExchangeSegment::NSECM; break;
        case ConnectionId::UDP_BSEFO: seg = ExchangeSegment::BSEFO; break;
        case ConnectionId::UDP_BSECM: seg = ExchangeSegment::BSECM; break;
        default: return;
    }

    qDebug() << "[BroadcastSettingsDialog] Restarting receiver"
             << connectionIdToLabel(id) << "→" << ip << ":" << port;

    auto& udp = UdpBroadcastService::instance();
    udp.restartReceiver(seg, ip.toStdString(), port);
}

void BroadcastSettingsDialog::onFeedModeChanged(int index)
{
    if (!m_feedModeCombo) return;

    QString modeKey = m_feedModeCombo->currentData().toString();
    PrimaryDataSource newSource = (modeKey == "xts_only")
                                  ? PrimaryDataSource::XTS_PRIMARY
                                  : PrimaryDataSource::UDP_PRIMARY;

    auto& mgr = ConnectionStatusManager::instance();
    PrimaryDataSource currentSource = mgr.primarySource();

    if (newSource == currentSource) return;

    // Confirm if switching at runtime (involves subscription migration)
    QString fromLabel = (currentSource == PrimaryDataSource::UDP_PRIMARY) ? "UDP" : "XTS";
    QString toLabel   = (newSource == PrimaryDataSource::UDP_PRIMARY) ? "UDP" : "XTS";

    int result = QMessageBox::question(this, "Switch Data Source",
        QString("Switch primary data source from %1 to %2?\n\n"
                "This will migrate all active subscriptions.\n"
                "XTS Market Data socket will remain connected for candle data.")
            .arg(fromLabel).arg(toLabel),
        QMessageBox::Yes | QMessageBox::No);

    if (result != QMessageBox::Yes) {
        // Revert combo selection
        m_feedModeCombo->blockSignals(true);
        m_feedModeCombo->setCurrentIndex(currentSource == PrimaryDataSource::UDP_PRIMARY ? 0 : 1);
        m_feedModeCombo->blockSignals(false);
        return;
    }

    // Perform the switch (includes subscription migration)
    mgr.switchPrimarySource(newSource, false);

    qDebug() << "[BroadcastSettingsDialog] Switched primary source to" << toLabel;
}

void BroadcastSettingsDialog::refreshStats()
{
    auto& mgr = ConnectionStatusManager::instance();

    // Update each status row
    for (auto it = m_rows.begin(); it != m_rows.end(); ++it) {
        ConnectionId id = it.key();
        ReceiverRow& row = it.value();
        ConnectionInfo info = mgr.getConnectionInfo(id);

        // Update status dot
        if (row.statusDot) {
            row.statusDot->setStyleSheet(
                QString("QLabel { color: %1; font-size: 12px; }")
                    .arg(connectionStateColor(info.state)));
        }

        // Update status text
        if (row.statusText) {
            QString statusStr = connectionStateToString(info.state);
            if (info.isConnected() && !info.uptimeString().isEmpty()) {
                statusStr += " (" + info.uptimeString() + ")";
            }
            row.statusText->setText(statusStr);
        }

        // Update packets/sec
        if (row.ppsLabel) {
            if (info.packetsPerSec > 0) {
                if (info.packetsPerSec >= 1000.0) {
                    row.ppsLabel->setText(QString("%1k/s").arg(info.packetsPerSec / 1000.0, 0, 'f', 1));
                } else {
                    row.ppsLabel->setText(QString("%1/s").arg(static_cast<int>(info.packetsPerSec)));
                }
            } else {
                row.ppsLabel->setText("—");
            }
        }
    }

    // Update XTS Feed Bridge stats
    QLabel* bridgeLabel = findChild<QLabel*>("bridgeStatsLabel");
    if (bridgeLabel) {
        auto stats = XTSFeedBridge::instance().getStats();
        bridgeLabel->setText(
            QString("Active: %1 / %2  |  Pending: %3  |  REST calls: %4  |  Evictions: %5")
                .arg(stats.totalSubscribed)
                .arg(stats.totalCapacity)
                .arg(stats.totalPending)
                .arg(stats.restCallsMade)
                .arg(stats.totalEvicted));
    }
}

void BroadcastSettingsDialog::onStateChanged(ConnectionId id, ConnectionState newState,
                                              const ConnectionInfo& info)
{
    // Immediately update the relevant row
    auto it = m_rows.find(id);
    if (it == m_rows.end()) return;

    ReceiverRow& row = it.value();

    if (row.statusDot) {
        row.statusDot->setStyleSheet(
            QString("QLabel { color: %1; font-size: 12px; }")
                .arg(connectionStateColor(newState)));
    }

    if (row.statusText) {
        row.statusText->setText(connectionStateToString(newState));
    }
}
