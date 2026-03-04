#include "views/PreferencesDerivativeTab.h"
#include "utils/PreferencesManager.h"
#include <QDebug>

PreferencesDerivativeTab::PreferencesDerivativeTab(QWidget *tabWidget,
                                                   PreferencesManager *prefsManager,
                                                   QObject *parent)
    : QObject(parent)
    , m_tabWidget(tabWidget)
    , m_prefsManager(prefsManager)
    , m_defaultExchangeCombo(nullptr)
    , m_defaultSymbolEdit(nullptr)
    , m_dividendSpin(nullptr)
    , m_interestRateSpin(nullptr)
    , m_enableFairValueCheck(nullptr)
    , m_underlyingPriceCombo(nullptr)
    , m_pricingModelCombo(nullptr)
    , m_enableDerivativeAlertsCheck(nullptr)
{
    findWidgets();
    populateComboBoxes();
}

void PreferencesDerivativeTab::findWidgets()
{
    if (!m_tabWidget) return;

    m_defaultExchangeCombo = m_tabWidget->findChild<QComboBox*>("comboBox_defaultExchange");
    m_defaultSymbolEdit = m_tabWidget->findChild<QLineEdit*>("lineEdit_defaultSymbol");
    m_dividendSpin = m_tabWidget->findChild<QDoubleSpinBox*>("doubleSpinBox_dividend");
    m_interestRateSpin = m_tabWidget->findChild<QDoubleSpinBox*>("doubleSpinBox_interestRate");
    m_enableFairValueCheck = m_tabWidget->findChild<QCheckBox*>("checkBox_enableFairValue");
    m_underlyingPriceCombo = m_tabWidget->findChild<QComboBox*>("comboBox_underlyingPrice");
    m_pricingModelCombo = m_tabWidget->findChild<QComboBox*>("comboBox_pricingModel");
    m_enableDerivativeAlertsCheck = m_tabWidget->findChild<QCheckBox*>("checkBox_enableDerivativeAlerts");

    // Configure spin box ranges
    if (m_dividendSpin) {
        m_dividendSpin->setRange(0.0, 100.0);
        m_dividendSpin->setDecimals(2);
        m_dividendSpin->setSuffix(" %");
        m_dividendSpin->setSingleStep(0.10);
    }
    if (m_interestRateSpin) {
        m_interestRateSpin->setRange(0.0, 50.0);
        m_interestRateSpin->setDecimals(2);
        m_interestRateSpin->setSuffix(" %");
        m_interestRateSpin->setSingleStep(0.25);
    }
}

void PreferencesDerivativeTab::populateComboBoxes()
{
    if (m_defaultExchangeCombo) {
        m_defaultExchangeCombo->clear();
        m_defaultExchangeCombo->addItems({"NSE", "BSE"});
    }

    if (m_underlyingPriceCombo) {
        m_underlyingPriceCombo->clear();
        m_underlyingPriceCombo->addItem("Cash / Spot Price", "cash");
        m_underlyingPriceCombo->addItem("Future Price", "future");
    }

    if (m_pricingModelCombo) {
        m_pricingModelCombo->clear();
        m_pricingModelCombo->addItems({"Black-Scholes", "Binomial"});
    }
}

void PreferencesDerivativeTab::loadPreferences()
{
    if (!m_prefsManager) return;

    // Default Exchange
    if (m_defaultExchangeCombo) {
        QString exchange = m_prefsManager->getDefaultDerivativeExchange();
        int idx = m_defaultExchangeCombo->findText(exchange);
        if (idx >= 0) m_defaultExchangeCombo->setCurrentIndex(idx);
    }

    // Default Symbol
    if (m_defaultSymbolEdit) {
        m_defaultSymbolEdit->setText(m_prefsManager->getDefaultDerivativeSymbol());
    }

    // Dividend
    if (m_dividendSpin) {
        m_dividendSpin->setValue(m_prefsManager->getDividendYield());
    }

    // Interest Rate
    if (m_interestRateSpin) {
        m_interestRateSpin->setValue(m_prefsManager->getInterestRate());
    }

    // Fair Value
    if (m_enableFairValueCheck) {
        m_enableFairValueCheck->setChecked(m_prefsManager->isFairValueEnabled());
    }

    // Underlying Price Source
    if (m_underlyingPriceCombo) {
        QString source = m_prefsManager->getUnderlyingPriceSource();
        int idx = m_underlyingPriceCombo->findData(source);
        if (idx >= 0) m_underlyingPriceCombo->setCurrentIndex(idx);
    }

    // Pricing Model
    if (m_pricingModelCombo) {
        QString model = m_prefsManager->getPricingModel();
        int idx = m_pricingModelCombo->findText(model);
        if (idx >= 0) m_pricingModelCombo->setCurrentIndex(idx);
    }

    // Derivative Alerts
    if (m_enableDerivativeAlertsCheck) {
        m_enableDerivativeAlertsCheck->setChecked(m_prefsManager->isDerivativeAlertsEnabled());
    }
}

void PreferencesDerivativeTab::savePreferences()
{
    if (!m_prefsManager) return;

    if (m_defaultExchangeCombo)
        m_prefsManager->setDefaultDerivativeExchange(m_defaultExchangeCombo->currentText());

    if (m_defaultSymbolEdit)
        m_prefsManager->setDefaultDerivativeSymbol(m_defaultSymbolEdit->text().trimmed());

    if (m_dividendSpin)
        m_prefsManager->setDividendYield(m_dividendSpin->value());

    if (m_interestRateSpin)
        m_prefsManager->setInterestRate(m_interestRateSpin->value());

    if (m_enableFairValueCheck)
        m_prefsManager->setFairValueEnabled(m_enableFairValueCheck->isChecked());

    if (m_underlyingPriceCombo)
        m_prefsManager->setUnderlyingPriceSource(m_underlyingPriceCombo->currentData().toString());

    if (m_pricingModelCombo)
        m_prefsManager->setPricingModel(m_pricingModelCombo->currentText());

    if (m_enableDerivativeAlertsCheck)
        m_prefsManager->setDerivativeAlertsEnabled(m_enableDerivativeAlertsCheck->isChecked());

    qWarning() << "[DerivativeTab] Preferences saved - underlying source:"
               << (m_underlyingPriceCombo ? m_underlyingPriceCombo->currentData().toString() : "N/A");
}

void PreferencesDerivativeTab::restoreDefaults()
{
    if (m_defaultExchangeCombo) m_defaultExchangeCombo->setCurrentIndex(0); // NSE
    if (m_defaultSymbolEdit) m_defaultSymbolEdit->setText("NIFTY");
    if (m_dividendSpin) m_dividendSpin->setValue(0.0);
    if (m_interestRateSpin) m_interestRateSpin->setValue(6.50);
    if (m_enableFairValueCheck) m_enableFairValueCheck->setChecked(false);
    if (m_underlyingPriceCombo) m_underlyingPriceCombo->setCurrentIndex(0); // Cash
    if (m_pricingModelCombo) m_pricingModelCombo->setCurrentIndex(0);       // Black-Scholes
    if (m_enableDerivativeAlertsCheck) m_enableDerivativeAlertsCheck->setChecked(false);
}
