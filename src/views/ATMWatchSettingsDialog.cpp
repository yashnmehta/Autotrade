#include "views/ATMWatchSettingsDialog.h"
#include "ui_ATMWatchSettingsDialog.h"
#include <QLabel>
#include <QMessageBox>
#include <QDebug>
#include <QSettings>

ATMWatchSettingsDialog::ATMWatchSettingsDialog(QWidget* parent)
    : QDialog(parent), ui(new Ui::ATMWatchSettingsDialog)
{
    ui->setupUi(this);

    // Assign convenience pointers from ui
    m_strikeSelectionGroup      = ui->m_strikeSelectionGroup;
    m_strikeRangeSpinBox        = ui->m_strikeRangeSpinBox;
    m_strikeRangeExampleLabel   = ui->m_strikeRangeExampleLabel;
    m_updateSettingsGroup       = ui->m_updateSettingsGroup;
    m_autoRecalculateCheckbox   = ui->m_autoRecalculateCheckbox;
    m_updateIntervalSpinBox     = ui->m_updateIntervalSpinBox;
    m_thresholdMultiplierSpinBox= ui->m_thresholdMultiplierSpinBox;
    m_basePriceSourceCombo      = ui->m_basePriceSourceCombo;
    m_greeksGroup               = ui->m_greeksGroup;
    m_enableGreeksCheckbox      = ui->m_enableGreeksCheckbox;
    m_riskFreeRateSpinBox       = ui->m_riskFreeRateSpinBox;
    m_showGreeksColumnsCheckbox = ui->m_showGreeksColumnsCheckbox;
    m_alertsGroup               = ui->m_alertsGroup;
    m_soundAlertsCheckbox       = ui->m_soundAlertsCheckbox;
    m_visualAlertsCheckbox      = ui->m_visualAlertsCheckbox;
    m_systemNotificationsCheckbox = ui->m_systemNotificationsCheckbox;
    m_okButton                  = ui->m_okButton;
    m_cancelButton              = ui->m_cancelButton;
    m_resetButton               = ui->m_resetButton;

    // Wire signals
    connect(m_strikeRangeSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &ATMWatchSettingsDialog::onStrikeRangeChanged);
    connect(m_okButton, &QPushButton::clicked, this, &ATMWatchSettingsDialog::onOkClicked);
    connect(m_cancelButton, &QPushButton::clicked, this, &ATMWatchSettingsDialog::onCancelClicked);
    connect(m_resetButton, &QPushButton::clicked, this, &ATMWatchSettingsDialog::onResetClicked);

    loadSettings();
}

ATMWatchSettingsDialog::~ATMWatchSettingsDialog() { delete ui; }

void ATMWatchSettingsDialog::loadSettings() {
    QSettings settings("configs/config.ini", QSettings::IniFormat);
    settings.beginGroup("ATM_WATCH");
    
    // Strike range
    int rangeCount = settings.value("strike_range_count", 0).toInt();
    m_strikeRangeSpinBox->setValue(rangeCount);
    
    // Update settings
    bool autoRecalc = settings.value("auto_recalculate", true).toBool();
    m_autoRecalculateCheckbox->setChecked(autoRecalc);
    
    int interval = settings.value("update_interval", 60).toInt();
    m_updateIntervalSpinBox->setValue(interval);
    
    double threshold = settings.value("threshold_multiplier", 0.5).toDouble();
    m_thresholdMultiplierSpinBox->setValue(threshold);
    
    QString priceSource = settings.value("base_price_source", "Cash").toString();
    m_basePriceSourceCombo->setCurrentIndex(priceSource == "Future" ? 1 : 0);
    
    // Greeks (Phase 4)
    bool enableGreeks = settings.value("enable_greeks", false).toBool();
    m_enableGreeksCheckbox->setChecked(enableGreeks);
    
    double riskFreeRate = settings.value("risk_free_rate", 6.5).toDouble();
    m_riskFreeRateSpinBox->setValue(riskFreeRate);
    
    bool showGreeks = settings.value("show_greeks_columns", true).toBool();
    m_showGreeksColumnsCheckbox->setChecked(showGreeks);
    
    // Alerts
    bool soundAlerts = settings.value("sound_alerts", false).toBool();
    m_soundAlertsCheckbox->setChecked(soundAlerts);
    
    bool visualAlerts = settings.value("visual_alerts", true).toBool();
    m_visualAlertsCheckbox->setChecked(visualAlerts);
    
    bool systemNotif = settings.value("system_notifications", false).toBool();
    m_systemNotificationsCheckbox->setChecked(systemNotif);
    
    settings.endGroup();
    
    updateStrikeRangeExample();
}

void ATMWatchSettingsDialog::saveSettings() {
    QSettings settings("configs/config.ini", QSettings::IniFormat);
    settings.beginGroup("ATM_WATCH");
    
    // Strike range
    settings.setValue("strike_range_count", m_strikeRangeSpinBox->value());
    
    // Update settings
    settings.setValue("auto_recalculate", m_autoRecalculateCheckbox->isChecked());
    settings.setValue("update_interval", m_updateIntervalSpinBox->value());
    settings.setValue("threshold_multiplier", m_thresholdMultiplierSpinBox->value());
    settings.setValue("base_price_source", 
                     m_basePriceSourceCombo->currentIndex() == 1 ? "Future" : "Cash");
    
    // Greeks (Phase 4)
    settings.setValue("enable_greeks", m_enableGreeksCheckbox->isChecked());
    settings.setValue("risk_free_rate", m_riskFreeRateSpinBox->value());
    settings.setValue("show_greeks_columns", m_showGreeksColumnsCheckbox->isChecked());
    
    // Alerts
    settings.setValue("sound_alerts", m_soundAlertsCheckbox->isChecked());
    settings.setValue("visual_alerts", m_visualAlertsCheckbox->isChecked());
    settings.setValue("system_notifications", m_systemNotificationsCheckbox->isChecked());
    
    settings.endGroup();
    
    settings.sync();
}

void ATMWatchSettingsDialog::applySettings() {
    // Apply to ATMWatchManager
    ATMWatchManager& manager = ATMWatchManager::getInstance();
    
    // Strike range
    int rangeCount = m_strikeRangeSpinBox->value();
    manager.setStrikeRangeCount(rangeCount);
    
    // Threshold multiplier
    double multiplier = m_thresholdMultiplierSpinBox->value();
    manager.setThresholdMultiplier(multiplier);
    
    // Base price source
    auto source = m_basePriceSourceCombo->currentIndex() == 1 
                  ? ATMWatchManager::BasePriceSource::Future 
                  : ATMWatchManager::BasePriceSource::Cash;
    manager.setDefaultBasePriceSource(source);
    
    qInfo() << "[ATMWatch] Settings applied: range=" << rangeCount 
            << ", threshold=" << multiplier
            << ", source=" << (source == ATMWatchManager::BasePriceSource::Cash ? "Cash" : "Future");
}

void ATMWatchSettingsDialog::updateStrikeRangeExample() {
    int range = m_strikeRangeSpinBox->value();
    
    QString example;
    if (range == 0) {
        example = "Example: NIFTY ATM=24000\n"
                 "Will show: 24000 CE, 24000 PE (1 strike only)";
    } else {
        int totalStrikes = range * 2 + 1;
        int lowerStrike = 24000 - (range * 100); // Assuming 100 point intervals
        int upperStrike = 24000 + (range * 100);
        
        example = QString("Example: NIFTY ATM=24000\n"
                         "Will show: %1 strikes from %2 to %3\n"
                         "(23500, 23600, ..., 24000, ..., 24400, 24500)")
                 .arg(totalStrikes)
                 .arg(lowerStrike)
                 .arg(upperStrike);
    }
    
    m_strikeRangeExampleLabel->setText(example);
}

void ATMWatchSettingsDialog::onStrikeRangeChanged(int value) {
    Q_UNUSED(value);
    updateStrikeRangeExample();
}

void ATMWatchSettingsDialog::onOkClicked() {
    saveSettings();
    applySettings();
    
    QMessageBox::information(this, "Settings Saved", 
        "ATM Watch settings have been saved and applied.\n\n"
        "Note: Strike range changes will take effect on next ATM calculation.");
    
    accept();
}

void ATMWatchSettingsDialog::onCancelClicked() {
    reject();
}

void ATMWatchSettingsDialog::onResetClicked() {
    auto reply = QMessageBox::question(this, "Reset Settings", 
        "Reset all ATM Watch settings to default values?",
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    
    if (reply == QMessageBox::Yes) {
        // Reset to defaults
        m_strikeRangeSpinBox->setValue(0);
        m_autoRecalculateCheckbox->setChecked(true);
        m_updateIntervalSpinBox->setValue(60);
        m_thresholdMultiplierSpinBox->setValue(0.5);
        m_basePriceSourceCombo->setCurrentIndex(0);
        m_enableGreeksCheckbox->setChecked(false);
        m_riskFreeRateSpinBox->setValue(6.5);
        m_showGreeksColumnsCheckbox->setChecked(true);
        m_soundAlertsCheckbox->setChecked(false);
        m_visualAlertsCheckbox->setChecked(true);
        m_systemNotificationsCheckbox->setChecked(false);
        
        updateStrikeRangeExample();
        
        QMessageBox::information(this, "Settings Reset", 
            "All settings have been reset to default values.\n"
            "Click OK to apply these settings.");
    }
}
