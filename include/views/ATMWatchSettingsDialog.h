#ifndef ATM_WATCH_SETTINGS_DIALOG_H
#define ATM_WATCH_SETTINGS_DIALOG_H

#include <QDialog>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QSettings>
#include <QLabel>
#include "services/ATMWatchManager.h"

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
    ~ATMWatchSettingsDialog() = default;
    
private slots:
    void onOkClicked();
    void onCancelClicked();
    void onResetClicked();
    void onStrikeRangeChanged(int value);
    
private:
    void setupUI();
    void loadSettings();
    void saveSettings();
    void applySettings();
    void updateStrikeRangeExample();
    
    // UI Components - Strike Selection
    QGroupBox* m_strikeSelectionGroup;
    QSpinBox* m_strikeRangeSpinBox;
    QLabel* m_strikeRangeExampleLabel;
    
    // UI Components - Update Settings
    QGroupBox* m_updateSettingsGroup;
    QCheckBox* m_autoRecalculateCheckbox;
    QSpinBox* m_updateIntervalSpinBox;
    QDoubleSpinBox* m_thresholdMultiplierSpinBox;
    QComboBox* m_basePriceSourceCombo;
    
    // UI Components - Greeks (Future - Phase 4)
    QGroupBox* m_greeksGroup;
    QCheckBox* m_enableGreeksCheckbox;
    QDoubleSpinBox* m_riskFreeRateSpinBox;
    QCheckBox* m_showGreeksColumnsCheckbox;
    
    // UI Components - Alerts
    QGroupBox* m_alertsGroup;
    QCheckBox* m_soundAlertsCheckbox;
    QCheckBox* m_visualAlertsCheckbox;
    QCheckBox* m_systemNotificationsCheckbox;
    
    // Buttons
    QPushButton* m_okButton;
    QPushButton* m_cancelButton;
    QPushButton* m_resetButton;
};

#endif // ATM_WATCH_SETTINGS_DIALOG_H
