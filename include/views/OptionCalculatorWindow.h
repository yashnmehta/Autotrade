#ifndef OPTION_CALCULATOR_WINDOW_H
#define OPTION_CALCULATOR_WINDOW_H

#include "quant/Greeks.h"
#include "quant/IVCalculator.h"
#include <QCheckBox>
#include <QCloseEvent>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QWidget>

/**
 * @brief Option Calculator Window
 *
 * Provides UI to calculate Call and Put prices and Greeks based on the
 * Black-Scholes model. Also supports calculating Implied Volatility given a
 * market price.
 */
class OptionCalculatorWindow : public QWidget {
  Q_OBJECT

public:
  explicit OptionCalculatorWindow(QWidget *parent = nullptr);
  ~OptionCalculatorWindow() override;

protected:
  void closeEvent(QCloseEvent *event) override;

private slots:
  void onCalculateClicked();
  void onCalculateIVClicked();
  void onCancelClicked();
  void onCalculationMethodChanged();

private:
  void setupUI();
  void setupConnections();

  // UI Elements
  // Input Group
  QLineEdit *m_spotPriceEdit;
  QLineEdit *m_strikePriceEdit;
  QLineEdit *m_volatilityEdit;
  QLineEdit *m_interestRateEdit;
  QLineEdit *m_dividendYieldEdit;
  QLineEdit *m_daysUntilExpirationEdit;

  // Output Group (Call / Put)
  QLabel *m_callTheoPriceLabel;
  QLabel *m_callDeltaLabel;
  QLabel *m_callGammaLabel;
  QLabel *m_callThetaLabel;
  QLabel *m_callVegaLabel;
  QLabel *m_callRhoLabel;

  QLabel *m_putTheoPriceLabel;
  QLabel *m_putDeltaLabel;
  QLabel *m_putGammaLabel;
  QLabel *m_putThetaLabel;
  QLabel *m_putVegaLabel;
  QLabel *m_putRhoLabel;

  // IV Section
  QCheckBox *m_calcImpliedVolCheckBox;
  QLineEdit *m_actualMarketValueEdit;

  // Radio Groups
  QRadioButton *m_callRadio;
  QRadioButton *m_putRadio;

  QRadioButton *m_binomialRadio;
  QRadioButton *m_blackScholesRadio;

  // Action Buttons
  QPushButton *m_calculateBtn;
  QPushButton *m_impliedVolBtn;
  QPushButton *m_cancelBtn;

  // Validation Message
  QLabel *m_errorLabel;

  // Helpers
  bool validateInputs(double &spot, double &strike, double &vol, double &rate,
                      double &divYield, double &days);
  double applyDividendYield(double spot, double divYield, double timeToExpiry);
};

#endif // OPTION_CALCULATOR_WINDOW_H
