#include "ui/ConnectionBarWidget.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>
#include <QPushButton>
#include <QFrame>
#include <QToolTip>

// ═══════════════════════════════════════════════════════════════════════
// Constructor
// ═══════════════════════════════════════════════════════════════════════

ConnectionBarWidget::ConnectionBarWidget(QWidget *parent)
    : QWidget(parent)
{
    buildUI();
    applyStyleSheet();

    // Wire to ConnectionStatusManager
    auto& mgr = ConnectionStatusManager::instance();
    connect(&mgr, &ConnectionStatusManager::stateChanged,
            this, &ConnectionBarWidget::onStateChanged);
    connect(&mgr, &ConnectionStatusManager::statsUpdated,
            this, &ConnectionBarWidget::onStatsUpdated);
    connect(&mgr, &ConnectionStatusManager::feedModeChanged,
            this, &ConnectionBarWidget::onFeedModeChanged);

    // Initial state
    onFeedModeChanged(mgr.feedModeString());
    onStatsUpdated();
}

ConnectionBarWidget::~ConnectionBarWidget()
{
    // Disconnect from the singleton ConnectionStatusManager to prevent
    // use-after-free during static destruction.  The UdpBroadcastService
    // destructor calls stop() → emits statusChanged → ConnectionStatusManager
    // emits stateChanged → arrives here after this widget is already destroyed.
    disconnect(&ConnectionStatusManager::instance(), nullptr, this, nullptr);
}

// ═══════════════════════════════════════════════════════════════════════
// Build UI
// ═══════════════════════════════════════════════════════════════════════

void ConnectionBarWidget::buildUI()
{
    setFixedHeight(28);

    QHBoxLayout *root = new QHBoxLayout(this);
    root->setContentsMargins(8, 0, 8, 0);
    root->setSpacing(4);
    root->setAlignment(Qt::AlignVCenter);

    // ── XTS Section ──────────────────────────────────────────────────
    QWidget* xtsGroup = new QWidget(this);
    QHBoxLayout* xtsLay = new QHBoxLayout(xtsGroup);
    xtsLay->setContentsMargins(0, 0, 0, 0);
    xtsLay->setSpacing(6);

    xtsLay->addWidget(createIndicator(ConnectionId::XTS_MARKET_DATA, "MD"));
    xtsLay->addWidget(createIndicator(ConnectionId::XTS_INTERACTIVE, "IA"));
    root->addWidget(xtsGroup);

    // Vertical separator
    auto addSep = [&]() {
        QFrame *sep = new QFrame(this);
        sep->setFrameShape(QFrame::VLine);
        sep->setFrameShadow(QFrame::Sunken);
        sep->setFixedWidth(1);
        sep->setStyleSheet("QFrame { color: #cbd5e1; }");
        root->addWidget(sep);
    };
    addSep();

    // ── UDP Section ──────────────────────────────────────────────────
    QWidget* udpGroup = new QWidget(this);
    QHBoxLayout* udpLay = new QHBoxLayout(udpGroup);
    udpLay->setContentsMargins(0, 0, 0, 0);
    udpLay->setSpacing(6);

    udpLay->addWidget(createIndicator(ConnectionId::UDP_NSEFO, "NF"));
    udpLay->addWidget(createIndicator(ConnectionId::UDP_NSECM, "NC"));
    udpLay->addWidget(createIndicator(ConnectionId::UDP_BSEFO, "BF"));
    udpLay->addWidget(createIndicator(ConnectionId::UDP_BSECM, "BC"));

    // Summary label (e.g. "4/4  2.1k/s")
    m_udpSummaryLabel = new QLabel("—", this);
    m_udpSummaryLabel->setObjectName("udpSummary");
    udpLay->addWidget(m_udpSummaryLabel);

    root->addWidget(udpGroup);

    addSep();

    // ── Feed Mode Badge ──────────────────────────────────────────────
    m_feedModeLabel = new QLabel("Hybrid", this);
    m_feedModeLabel->setObjectName("feedModeBadge");
    m_feedModeLabel->setCursor(Qt::PointingHandCursor);
    m_feedModeLabel->setToolTip("Current data feed mode — click ⚙ to change");
    root->addWidget(m_feedModeLabel);

    addSep();

    // ── Settings Button ──────────────────────────────────────────────
    m_settingsButton = new QToolButton(this);
    m_settingsButton->setText("⚙");
    m_settingsButton->setObjectName("connSettingsBtn");
    m_settingsButton->setToolTip("Connection Settings");
    m_settingsButton->setCursor(Qt::PointingHandCursor);
    connect(m_settingsButton, &QToolButton::clicked,
            this, &ConnectionBarWidget::settingsRequested);
    root->addWidget(m_settingsButton);

    root->addStretch();
}

QWidget* ConnectionBarWidget::createIndicator(ConnectionId id, const QString& shortLabel)
{
    QWidget* container = new QWidget(this);
    QHBoxLayout* lay = new QHBoxLayout(container);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(2);

    Indicator ind;

    // Colour dot
    ind.dot = new QLabel("●", container);
    ind.dot->setFixedSize(12, 12);
    ind.dot->setAlignment(Qt::AlignCenter);
    ind.dot->setStyleSheet(
        QString("QLabel { color: %1; font-size: 10px; }")
            .arg(connectionStateColor(ConnectionState::Disconnected)));
    lay->addWidget(ind.dot);

    // Short text
    ind.label = new QLabel(shortLabel, container);
    ind.label->setObjectName("connLabel");
    lay->addWidget(ind.label);

    m_indicators[id] = ind;
    return container;
}

// ═══════════════════════════════════════════════════════════════════════
// Style
// ═══════════════════════════════════════════════════════════════════════

void ConnectionBarWidget::applyStyleSheet()
{
    setStyleSheet(
        "ConnectionBarWidget {"
        "  background-color: #f8fafc;"
        "}"
        "QLabel#connLabel {"
        "  color: #475569;"
        "  font-size: 10px;"
        "  font-weight: 500;"
        "}"
        "QLabel#udpSummary {"
        "  color: #475569;"
        "  font-size: 10px;"
        "  padding-left: 4px;"
        "}"
        "QLabel#feedModeBadge {"
        "  color: #1e40af;"
        "  background: #dbeafe;"
        "  border-radius: 3px;"
        "  padding: 1px 6px;"
        "  font-size: 10px;"
        "  font-weight: 600;"
        "}"
        "QToolButton#connSettingsBtn {"
        "  border: none;"
        "  font-size: 14px;"
        "  padding: 0 4px;"
        "  color: #64748b;"
        "}"
        "QToolButton#connSettingsBtn:hover {"
        "  color: #2563eb;"
        "  background: #dbeafe;"
        "  border-radius: 3px;"
        "}"
    );
}

// ═══════════════════════════════════════════════════════════════════════
// Slots
// ═══════════════════════════════════════════════════════════════════════

void ConnectionBarWidget::onStateChanged(ConnectionId id, ConnectionState newState,
                                          const ConnectionInfo& info)
{
    updateIndicatorDot(id, newState);

    // Update tooltip with details
    auto it = m_indicators.find(id);
    if (it != m_indicators.end() && it->dot && it->label) {
        QString tip = QString("%1\nState: %2\nAddress: %3\nUptime: %4")
            .arg(info.displayName)
            .arg(connectionStateToString(newState))
            .arg(info.address.isEmpty() ? "—" : info.address)
            .arg(info.uptimeString());

        if (!info.errorMessage.isEmpty())
            tip += "\nError: " + info.errorMessage;

        it->dot->setToolTip(tip);
        it->label->setToolTip(tip);
    }
}

void ConnectionBarWidget::onStatsUpdated()
{
    // Guard: during shutdown, the summary label or indicators may be freed
    if (!m_udpSummaryLabel) return;

    auto& mgr = ConnectionStatusManager::instance();

    // Update UDP summary
    double pps = mgr.totalUdpPacketsPerSec();
    int connected = mgr.udpConnectedCount();
    int total = mgr.udpTotalCount();

    QString rateStr;
    if (pps >= 1000.0)
        rateStr = QString("%1k/s").arg(pps / 1000.0, 0, 'f', 1);
    else
        rateStr = QString("%1/s").arg(static_cast<int>(pps));

    if (total == 0) {
        m_udpSummaryLabel->setText("Off");
    } else {
        m_udpSummaryLabel->setText(QString("%1/%2 %3").arg(connected).arg(total).arg(rateStr));
    }

    // Update indicator dots from current state (in case we missed a signal)
    for (auto it = m_indicators.begin(); it != m_indicators.end(); ++it) {
        auto info = mgr.getConnectionInfo(it.key());
        updateIndicatorDot(it.key(), info.state);
    }
}

void ConnectionBarWidget::onFeedModeChanged(const QString& mode)
{
    if (m_feedModeLabel) {
        m_feedModeLabel->setText(mode);

        // Different badge colour based on mode
        if (mode.toLower().contains("xts")) {
            m_feedModeLabel->setStyleSheet(
                "QLabel#feedModeBadge {"
                "  color: #9333ea;"
                "  background: #f3e8ff;"
                "  border-radius: 3px;"
                "  padding: 1px 6px;"
                "  font-size: 10px;"
                "  font-weight: 600;"
                "}");
        } else {
            m_feedModeLabel->setStyleSheet(
                "QLabel#feedModeBadge {"
                "  color: #1e40af;"
                "  background: #dbeafe;"
                "  border-radius: 3px;"
                "  padding: 1px 6px;"
                "  font-size: 10px;"
                "  font-weight: 600;"
                "}");
        }
    }
}

void ConnectionBarWidget::updateIndicatorDot(ConnectionId id, ConnectionState state)
{
    auto it = m_indicators.find(id);
    if (it == m_indicators.end()) return;

    // Guard against use-after-free during static destruction:
    // the dot QLabel may have been destroyed by Qt's parent-child cleanup
    // before the signal fires during app exit.
    if (!it->dot) return;

    it->dot->setStyleSheet(
        QString("QLabel { color: %1; font-size: 10px; }")
            .arg(connectionStateColor(state)));
}
