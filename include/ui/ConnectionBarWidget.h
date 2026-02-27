#ifndef CONNECTIONBARWIDGET_H
#define CONNECTIONBARWIDGET_H

#include <QWidget>
#include <QMap>
#include "services/ConnectionStatusManager.h"

class QLabel;
class QHBoxLayout;
class QPushButton;
class QToolButton;

/**
 * @brief Compact connection status bar displayed in the toolbar area.
 *
 * Shows real-time status of all 6 connections (2 XTS + 4 UDP) with
 * colour-coded dot indicators, packet rate, feed mode badge, and
 * a settings button that opens BroadcastSettingsDialog.
 *
 * Layout:
 *   [●XTS MD ●XTS IA] | [UDP: 4/4 (2.1k/s)] | [Hybrid ▾] | [⚙]
 *
 * Observes ConnectionStatusManager signals exclusively — no polling.
 */
class ConnectionBarWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ConnectionBarWidget(QWidget *parent = nullptr);

signals:
    /** @brief User clicked the feed-mode toggle / dropdown */
    void feedModeToggleRequested();

    /** @brief User clicked the settings gear icon */
    void settingsRequested();

private slots:
    void onStateChanged(ConnectionId id, ConnectionState newState,
                        const ConnectionInfo& info);
    void onStatsUpdated();
    void onFeedModeChanged(const QString& mode);

private:
    void buildUI();
    void applyStyleSheet();

    /** @brief Create a single dot+label pair for one connection */
    QWidget* createIndicator(ConnectionId id, const QString& shortLabel);

    /** @brief Update a single indicator dot colour */
    void updateIndicatorDot(ConnectionId id, ConnectionState state);

    // ── Widgets ──────────────────────────────────────────────────────
    struct Indicator {
        QLabel* dot   = nullptr;
        QLabel* label = nullptr;
    };
    QMap<ConnectionId, Indicator> m_indicators;

    QLabel*      m_udpSummaryLabel  = nullptr;
    QLabel*      m_feedModeLabel    = nullptr;
    QToolButton* m_settingsButton   = nullptr;
};

#endif // CONNECTIONBARWIDGET_H
