#ifndef BROADCASTSETTINGSDIALOG_H
#define BROADCASTSETTINGSDIALOG_H

#include <QDialog>
#include <QMap>
#include "services/ConnectionStatusManager.h"

class QLineEdit;
class QSpinBox;
class QComboBox;
class QGroupBox;
class QLabel;
class QPushButton;
class QTabWidget;
class QCheckBox;

/**
 * @brief Runtime configuration dialog for UDP receivers & feed mode.
 *
 * Allows the user to:
 *   1. View live status of all 6 connections
 *   2. Change UDP multicast IP/port for each receiver
 *   3. Restart individual receivers with new settings
 *   4. Switch feed mode (Hybrid ↔ XTS Only)
 *   5. See live packet statistics
 *
 * Built entirely in C++ (no .ui file) since it's a dynamic settings dialog.
 */
class BroadcastSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit BroadcastSettingsDialog(class ConfigLoader* configLoader,
                                     QWidget *parent = nullptr);

private slots:
    void onApply();
    void onRestartReceiver(ConnectionId id);
    void onFeedModeChanged(int index);
    void refreshStats();
    void onStateChanged(ConnectionId id, ConnectionState newState,
                        const ConnectionInfo& info);

private:
    void buildUI();
    void loadFromConfig();
    void applyStyleSheet();

    QWidget* createStatusPanel();
    QWidget* createUdpPanel();
    QWidget* createFeedModePanel();

    /** @brief Create a row: [●status] [IP input] [Port input] [Restart btn] */
    QWidget* createReceiverRow(ConnectionId id, const QString& label);

    // ── Refs ─────────────────────────────────────────────────────────
    class ConfigLoader* m_configLoader;

    // ── UDP config inputs ────────────────────────────────────────────
    struct ReceiverRow {
        QLabel*      statusDot = nullptr;
        QLabel*      statusText = nullptr;
        QLineEdit*   ipEdit = nullptr;
        QSpinBox*    portSpin = nullptr;
        QPushButton* restartBtn = nullptr;
        QLabel*      ppsLabel = nullptr;
        ConnectionId id;
    };
    QMap<ConnectionId, ReceiverRow> m_rows;

    // ── Feed mode ────────────────────────────────────────────────────
    QComboBox* m_feedModeCombo = nullptr;
    QCheckBox* m_saveAsDefaultCheck = nullptr;

    // ── Buttons ──────────────────────────────────────────────────────
    QPushButton* m_applyBtn  = nullptr;
    QPushButton* m_closeBtn  = nullptr;

    // ── Stats refresh ────────────────────────────────────────────────
    QTimer* m_refreshTimer = nullptr;
};

#endif // BROADCASTSETTINGSDIALOG_H
