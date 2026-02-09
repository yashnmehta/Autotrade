#include "ui/ATMWatchSettingsDialog.h"
#include <QLabel>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QDebug>

ATMWatchSettingsDialog::ATMWatchSettingsDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("ATM Watch Settings");
    setModal(true);
    setMinimumWidth(500);
    
    setupUI();
    loadSettings();
    
    // Update example when strike range changes
    connect(m_strikeRangeSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &ATMWatchSettingsDialog::onStrikeRangeChanged);
}

void ATMWatchSettingsDialog::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(15);
    
    // ========== Strike Selection Group ==========
    m_strikeSelectionGroup = new QGroupBox("Strike Selection", this);
    QFormLayout* strikeLayout = new QFormLayout();
    
    m_strikeRangeSpinBox = new QSpinBox(this);
    m_strikeRangeSpinBox->setRange(0, 10);
    m_strikeRangeSpinBox->setSuffix(" strikes around ATM");
    m_strikeRangeSpinBox->setToolTip(
        "Number of strikes to display on each side of ATM\n"
        "0 = ATM only (1 strike)\n"
        "5 = ATM ± 5 (11 strikes total)"
    );
    strikeLayout->addRow("Strike Range:", m_strikeRangeSpinBox);
    
    m_strikeRangeExampleLabel = new QLabel(this);
    m_strikeRangeExampleLabel->setStyleSheet("QLabel { color: gray; font-size: 9pt; }");
    m_strikeRangeExampleLabel->setWordWrap(true);
    strikeLayout->addRow("", m_strikeRangeExampleLabel);
    
    m_strikeSelectionGroup->setLayout(strikeLayout);
    mainLayout->addWidget(m_strikeSelectionGroup);
    
    // ========== Update Settings Group ==========
    m_updateSettingsGroup = new QGroupBox("Update Settings", this);
    QFormLayout* updateLayout = new QFormLayout();
    
    m_autoRecalculateCheckbox = new QCheckBox("Auto-calculate on price changes", this);
    m_autoRecalculateCheckbox->setToolTip("Enable event-driven ATM calculation");
    updateLayout->addRow(m_autoRecalculateCheckbox);
    
    m_updateIntervalSpinBox = new QSpinBox(this);
    m_updateIntervalSpinBox->setRange(10, 300);
    m_updateIntervalSpinBox->setSuffix(" seconds (backup)");
    m_updateIntervalSpinBox->setToolTip(
        "Backup timer interval for ATM calculation\n"
        "Only used if auto-calculate is disabled or events are missed"
    );
    updateLayout->addRow("Timer Interval:", m_updateIntervalSpinBox);
    
    m_thresholdMultiplierSpinBox = new QDoubleSpinBox(this);
    m_thresholdMultiplierSpinBox->setRange(0.25, 1.0);
    m_thresholdMultiplierSpinBox->setSingleStep(0.05);
    m_thresholdMultiplierSpinBox->setDecimals(2);
    m_thresholdMultiplierSpinBox->setToolTip(
        "Price movement threshold for recalculation\n"
        "0.25 = High sensitivity (quarter strike interval)\n"
        "0.50 = Default (half strike interval)\n"
        "1.00 = Conservative (full strike interval)"
    );
    updateLayout->addRow("Threshold Multiplier:", m_thresholdMultiplierSpinBox);
    
    QLabel* thresholdNote = new QLabel(
        "Recalculates when price moves by multiplier × strike interval", this);
    thresholdNote->setStyleSheet("QLabel { color: gray; font-size: 9pt; }");
    thresholdNote->setWordWrap(true);
    updateLayout->addRow("", thresholdNote);
    
    m_basePriceSourceCombo = new QComboBox(this);
    m_basePriceSourceCombo->addItem("Cash (Spot Price)");
    m_basePriceSourceCombo->addItem("Future (Front Month)");
    m_basePriceSourceCombo->setToolTip(
        "Source for underlying price calculation\n"
        "Cash: Uses equity/index cash market price\n"
        "Future: Uses front month futures price"
    );
    updateLayout->addRow("Base Price Source:", m_basePriceSourceCombo);
    
    m_updateSettingsGroup->setLayout(updateLayout);
    mainLayout->addWidget(m_updateSettingsGroup);
    
    // ========== Greeks Calculation Group (Future - Phase 4) ==========
    m_greeksGroup = new QGroupBox("Greeks Calculation", this);
    QFormLayout* greeksLayout = new QFormLayout();
    
    m_enableGreeksCheckbox = new QCheckBox("Enable Greeks calculation", this);
    m_enableGreeksCheckbox->setEnabled(false); // Disabled until Phase 4
    m_enableGreeksCheckbox->setToolTip("Greeks calculation (IV, Delta, Gamma, etc.) - Coming in Phase 4");
    greeksLayout->addRow(m_enableGreeksCheckbox);
    
    m_riskFreeRateSpinBox = new QDoubleSpinBox(this);
    m_riskFreeRateSpinBox->setRange(0.0, 10.0);
    m_riskFreeRateSpinBox->setSingleStep(0.1);
    m_riskFreeRateSpinBox->setDecimals(2);
    m_riskFreeRateSpinBox->setSuffix(" % (RBI Repo Rate)");
    m_riskFreeRateSpinBox->setEnabled(false); // Disabled until Phase 4
    greeksLayout->addRow("Risk-Free Rate:", m_riskFreeRateSpinBox);
    
    m_showGreeksColumnsCheckbox = new QCheckBox("Show Greeks columns (IV, Delta, Gamma, etc.)", this);
    m_showGreeksColumnsCheckbox->setEnabled(false); // Disabled until Phase 4
    greeksLayout->addRow(m_showGreeksColumnsCheckbox);
    
    QLabel* greeksNote = new QLabel("Greeks features will be available in Phase 4", this);
    greeksNote->setStyleSheet("QLabel { color: gray; font-size: 9pt; font-style: italic; }");
    greeksLayout->addRow("", greeksNote);
    
    m_greeksGroup->setLayout(greeksLayout);
    mainLayout->addWidget(m_greeksGroup);
    
    // ========== Alerts Group ==========
    m_alertsGroup = new QGroupBox("Alerts", this);
    QVBoxLayout* alertsLayout = new QVBoxLayout();
    
    m_soundAlertsCheckbox = new QCheckBox("Sound alerts on ATM change", this);
    m_soundAlertsCheckbox->setToolTip("Play sound when ATM strike changes");
    alertsLayout->addWidget(m_soundAlertsCheckbox);
    
    m_visualAlertsCheckbox = new QCheckBox("Visual flash on strike change", this);
    m_visualAlertsCheckbox->setToolTip("Highlight row when ATM strike changes");
    alertsLayout->addWidget(m_visualAlertsCheckbox);
    
    m_systemNotificationsCheckbox = new QCheckBox("System notifications", this);
    m_systemNotificationsCheckbox->setToolTip("Show system notifications for ATM changes");
    alertsLayout->addWidget(m_systemNotificationsCheckbox);
    
    m_alertsGroup->setLayout(alertsLayout);
    mainLayout->addWidget(m_alertsGroup);
    
    // ========== Buttons ==========
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    
    m_resetButton = new QPushButton("Reset to Defaults", this);
    connect(m_resetButton, &QPushButton::clicked, this, &ATMWatchSettingsDialog::onResetClicked);
    buttonLayout->addWidget(m_resetButton);
    
    buttonLayout->addSpacing(20);
    
    m_okButton = new QPushButton("OK", this);
    m_okButton->setDefault(true);
    connect(m_okButton, &QPushButton::clicked, this, &ATMWatchSettingsDialog::onOkClicked);
    buttonLayout->addWidget(m_okButton);
    
    m_cancelButton = new QPushButton("Cancel", this);
    connect(m_cancelButton, &QPushButton::clicked, this, &ATMWatchSettingsDialog::onCancelClicked);
    buttonLayout->addWidget(m_cancelButton);
    
    mainLayout->addLayout(buttonLayout);
}

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
