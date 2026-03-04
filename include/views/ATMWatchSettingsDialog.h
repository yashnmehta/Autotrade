#ifndef ATM_WATCH_SETTINGS_DIALOG_H
#define ATM_WATCH_SETTINGS_DIALOG_H

#include <QDialog>
#include "services/ATMWatchManager.h"

class QSpinBox;
class QDoubleSpinBox;
class QCheckBox;
class QComboBox;
class QPushButton;
class QGroupBox;
class QLabel;

namespace Ui { class ATMWatchSettingsDialog; }

/**
 * @brief Settings dialog for ATM Watch configuration
 * 
 * Allows users to configure:
 * - Strike range (ATM+1 to ATM+10)
 * - Timer interval (backup calculation frequency)
 * - Threshold multiplier (recalculation sensitivity)
 * - Base price source (Cash/Future)
 * - Alert preferences
 * 
 * Note: Column visibility is managed via GenericProfileDialog from the
 * context menu, not from this settings dialog.
 */
class ATMWatchSettingsDialog : public QDialog {
    Q_OBJECT
    
public:
    explicit ATMWatchSettingsDialog(QWidget* parent = nullptr);
    ~ATMWatchSettingsDialog();
    
private slots:
    void onOkClicked();
    void onCancelClicked();
    void onResetClicked();
    void onStrikeRangeChanged(int value);
    
private:
    void loadSettings();
    void saveSettings();
    void applySettings();
    void updateStrikeRangeExample();
    
    Ui::ATMWatchSettingsDialog *ui;

    // Convenience pointers (owned by ui)
    QGroupBox* m_strikeSelectionGroup;
    QSpinBox* m_strikeRangeSpinBox;
    QLabel* m_strikeRangeExampleLabel;
    
    QGroupBox* m_updateSettingsGroup;
    QCheckBox* m_autoRecalculateCheckbox;
    QSpinBox* m_updateIntervalSpinBox;
    QDoubleSpinBox* m_thresholdMultiplierSpinBox;
    QComboBox* m_basePriceSourceCombo;
    
    QGroupBox* m_greeksGroup;
    QCheckBox* m_enableGreeksCheckbox;
    QDoubleSpinBox* m_riskFreeRateSpinBox;
    QCheckBox* m_showGreeksColumnsCheckbox;
    
    QGroupBox* m_alertsGroup;
    QCheckBox* m_soundAlertsCheckbox;
    QCheckBox* m_visualAlertsCheckbox;
    QCheckBox* m_systemNotificationsCheckbox;
    
    QPushButton* m_okButton;
    QPushButton* m_cancelButton;
    QPushButton* m_resetButton;
};

#endif // ATM_WATCH_SETTINGS_DIALOG_H
