#ifndef PREFERENCESDERIVATIVETAB_H
#define PREFERENCESDERIVATIVETAB_H

#include <QObject>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QLineEdit>

class PreferencesManager;

/**
 * @brief Handles the Derivatives tab in Preferences dialog
 * 
 * Manages:
 * - Underlying price source (Cash/Spot or Future)
 * - Default exchange and symbol for derivatives
 * - Dividend yield and interest rate
 * - Pricing model selection
 * - Fair value calculation toggle
 * - Derivative alerts toggle
 */
class PreferencesDerivativeTab : public QObject
{
    Q_OBJECT

public:
    explicit PreferencesDerivativeTab(QWidget *tabWidget,
                                     PreferencesManager *prefsManager,
                                     QObject *parent = nullptr);
    ~PreferencesDerivativeTab() = default;

    void loadPreferences();
    void savePreferences();
    void restoreDefaults();

private:
    void findWidgets();
    void populateComboBoxes();

    QWidget *m_tabWidget;
    PreferencesManager *m_prefsManager;

    // Widgets from .ui
    QComboBox *m_defaultExchangeCombo;
    QLineEdit *m_defaultSymbolEdit;
    QDoubleSpinBox *m_dividendSpin;
    QDoubleSpinBox *m_interestRateSpin;
    QCheckBox *m_enableFairValueCheck;
    QComboBox *m_underlyingPriceCombo;
    QComboBox *m_pricingModelCombo;
    QCheckBox *m_enableDerivativeAlertsCheck;
};

#endif // PREFERENCESDERIVATIVETAB_H
