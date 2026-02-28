#include "views/OptionCalculatorWindow.h"
#include "utils/WindowSettingsHelper.h"
#include <QDoubleValidator>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>

OptionCalculatorWindow::OptionCalculatorWindow(QWidget *parent)
    : QWidget(parent) {
  setupUI();
  setupConnections();
  WindowSettingsHelper::loadAndApplyWindowSettings(this, "OptionCalculator");
}

OptionCalculatorWindow::~OptionCalculatorWindow() = default;

void OptionCalculatorWindow::setupUI() {
  // Main layout
  QVBoxLayout *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(10, 10, 10, 10);
  mainLayout->setSpacing(10);

  // ==========================================
  // Top Section (Inputs & Outputs)
  // ==========================================
  QHBoxLayout *topLayout = new QHBoxLayout();

  // --- Input Values Group ---
  QGroupBox *inputGroup = new QGroupBox("Input Values");
  QFormLayout *inputLayout = new QFormLayout(inputGroup);

  // Double validators
  QDoubleValidator *priceValidator =
      new QDoubleValidator(0.0, 1000000.0, 4, this);
  QDoubleValidator *percentValidator =
      new QDoubleValidator(0.0, 500.0, 4, this);
  percentValidator->setNotation(QDoubleValidator::StandardNotation);

  m_spotPriceEdit = new QLineEdit("0");
  m_spotPriceEdit->setValidator(priceValidator);

  m_strikePriceEdit = new QLineEdit("0");
  m_strikePriceEdit->setValidator(priceValidator);

  m_volatilityEdit = new QLineEdit("20");
  m_volatilityEdit->setValidator(percentValidator);

  m_interestRateEdit = new QLineEdit("6.5");
  m_interestRateEdit->setValidator(percentValidator);

  m_dividendYieldEdit = new QLineEdit("0");
  m_dividendYieldEdit->setValidator(percentValidator);

  m_daysUntilExpirationEdit = new QLineEdit("30");
  QDoubleValidator *daysValidator = new QDoubleValidator(0.0, 3650.0, 4, this);
  m_daysUntilExpirationEdit->setValidator(daysValidator);

  inputLayout->addRow("Spot Price:", m_spotPriceEdit);
  inputLayout->addRow("Strike Price:", m_strikePriceEdit);
  inputLayout->addRow("Volatility[%]:", m_volatilityEdit);
  inputLayout->addRow("Interest Rate[%]:", m_interestRateEdit);
  inputLayout->addRow("Dividend Yield :", m_dividendYieldEdit);
  inputLayout->addRow("Days Until Expiration:", m_daysUntilExpirationEdit);

  topLayout->addWidget(inputGroup);

  // --- Output Values Group ---
  QGroupBox *outputGroup = new QGroupBox("Output Values");
  QGridLayout *outputLayout = new QGridLayout(outputGroup);

  // Headers
  outputLayout->addWidget(new QLabel("Call"), 0, 1, Qt::AlignCenter);
  outputLayout->addWidget(new QLabel("Put"), 0, 2, Qt::AlignCenter);

  // Rows
  outputLayout->addWidget(new QLabel("Theoretical:"), 1, 0);
  m_callTheoPriceLabel = new QLabel("0");
  m_callTheoPriceLabel->setStyleSheet(
      "background-color: white; padding: 2px; border: 1px solid #ccc;");
  m_putTheoPriceLabel = new QLabel("0");
  m_putTheoPriceLabel->setStyleSheet(
      "background-color: white; padding: 2px; border: 1px solid #ccc;");
  outputLayout->addWidget(m_callTheoPriceLabel, 1, 1);
  outputLayout->addWidget(m_putTheoPriceLabel, 1, 2);

  outputLayout->addWidget(new QLabel("Delta:"), 2, 0);
  m_callDeltaLabel = new QLabel("0");
  m_callDeltaLabel->setStyleSheet(
      "background-color: white; padding: 2px; border: 1px solid #ccc;");
  m_putDeltaLabel = new QLabel("0");
  m_putDeltaLabel->setStyleSheet(
      "background-color: white; padding: 2px; border: 1px solid #ccc;");
  outputLayout->addWidget(m_callDeltaLabel, 2, 1);
  outputLayout->addWidget(m_putDeltaLabel, 2, 2);

  outputLayout->addWidget(new QLabel("Gamma:"), 3, 0);
  m_callGammaLabel = new QLabel("0");
  m_callGammaLabel->setStyleSheet(
      "background-color: white; padding: 2px; border: 1px solid #ccc;");
  m_putGammaLabel = new QLabel("0");
  m_putGammaLabel->setStyleSheet(
      "background-color: white; padding: 2px; border: 1px solid #ccc;");
  outputLayout->addWidget(m_callGammaLabel, 3, 1);
  outputLayout->addWidget(m_putGammaLabel, 3, 2);

  outputLayout->addWidget(new QLabel("Theta:"), 4, 0);
  m_callThetaLabel = new QLabel("0");
  m_callThetaLabel->setStyleSheet(
      "background-color: white; padding: 2px; border: 1px solid #ccc;");
  m_putThetaLabel = new QLabel("0");
  m_putThetaLabel->setStyleSheet(
      "background-color: white; padding: 2px; border: 1px solid #ccc;");
  outputLayout->addWidget(m_callThetaLabel, 4, 1);
  outputLayout->addWidget(m_putThetaLabel, 4, 2);

  outputLayout->addWidget(new QLabel("Vega:"), 5, 0);
  m_callVegaLabel = new QLabel("0");
  m_callVegaLabel->setStyleSheet(
      "background-color: white; padding: 2px; border: 1px solid #ccc;");
  m_putVegaLabel = new QLabel("0");
  m_putVegaLabel->setStyleSheet(
      "background-color: white; padding: 2px; border: 1px solid #ccc;");
  outputLayout->addWidget(m_callVegaLabel, 5, 1);
  outputLayout->addWidget(m_putVegaLabel, 5, 2);

  outputLayout->addWidget(new QLabel("Rho:"), 6, 0);
  m_callRhoLabel = new QLabel("0");
  m_callRhoLabel->setStyleSheet(
      "background-color: white; padding: 2px; border: 1px solid #ccc;");
  m_putRhoLabel = new QLabel("0");
  m_putRhoLabel->setStyleSheet(
      "background-color: white; padding: 2px; border: 1px solid #ccc;");
  outputLayout->addWidget(m_callRhoLabel, 6, 1);
  outputLayout->addWidget(m_putRhoLabel, 6, 2);

  topLayout->addWidget(outputGroup);

  mainLayout->addLayout(topLayout);

  // ==========================================
  // Middle Section (IV & Market Value)
  // ==========================================
  QHBoxLayout *ivLayout = new QHBoxLayout();

  m_calcImpliedVolCheckBox = new QCheckBox("Calculate Implied Volatility");
  ivLayout->addWidget(m_calcImpliedVolCheckBox);

  ivLayout->addStretch();

  ivLayout->addWidget(new QLabel("Actual Market Value:"));
  m_actualMarketValueEdit = new QLineEdit("0");
  m_actualMarketValueEdit->setValidator(priceValidator);
  m_actualMarketValueEdit->setFixedWidth(80);
  ivLayout->addWidget(m_actualMarketValueEdit);

  mainLayout->addLayout(ivLayout);

  // ==========================================
  // Bottom Section (Options & Buttons)
  // ==========================================
  QHBoxLayout *bottomLayout = new QHBoxLayout();

  // --- Call/Put Radio Group ---
  QGroupBox *callPutGroup = new QGroupBox("Call/Put");
  QVBoxLayout *callPutLayout = new QVBoxLayout(callPutGroup);
  m_callRadio = new QRadioButton("Call");
  m_putRadio = new QRadioButton("Put");
  m_callRadio->setChecked(true); // Default to Call
  callPutLayout->addWidget(m_callRadio);
  callPutLayout->addWidget(m_putRadio);
  bottomLayout->addWidget(callPutGroup);

  // --- Calculation Method Group ---
  QGroupBox *calcMethodGroup = new QGroupBox("Calculation Method");
  QVBoxLayout *calcMethodLayout = new QVBoxLayout(calcMethodGroup);
  m_binomialRadio = new QRadioButton("Cox-Ross-Rubinstein Binomial Method");
  m_blackScholesRadio = new QRadioButton("Black & Scholes Pricing Model");

  m_blackScholesRadio->setChecked(true);
  m_binomialRadio->setEnabled(false); // Disabled as not implemented in Greeks.h

  calcMethodLayout->addWidget(m_binomialRadio);
  calcMethodLayout->addWidget(m_blackScholesRadio);
  bottomLayout->addWidget(calcMethodGroup);

  // --- Action Buttons ---
  QVBoxLayout *btnLayout = new QVBoxLayout();
  m_calculateBtn = new QPushButton("Calculate");
  m_impliedVolBtn = new QPushButton("Implied Vol");
  m_cancelBtn = new QPushButton("Cancel");

  btnLayout->addWidget(m_calculateBtn);
  btnLayout->addWidget(m_impliedVolBtn);
  btnLayout->addWidget(m_cancelBtn);

  bottomLayout->addLayout(btnLayout);

  mainLayout->addLayout(bottomLayout);

  // ==========================================
  // Error Message
  // ==========================================
  m_errorLabel = new QLabel("");
  m_errorLabel->setStyleSheet("color: red;");
  mainLayout->addWidget(m_errorLabel);

  // Initial UI state
  onCalculationMethodChanged();
}

void OptionCalculatorWindow::setupConnections() {
  connect(m_calculateBtn, &QPushButton::clicked, this,
          &OptionCalculatorWindow::onCalculateClicked);
  connect(m_impliedVolBtn, &QPushButton::clicked, this,
          &OptionCalculatorWindow::onCalculateIVClicked);
  connect(m_cancelBtn, &QPushButton::clicked, this,
          &OptionCalculatorWindow::onCancelClicked);

  connect(m_calcImpliedVolCheckBox, &QCheckBox::stateChanged, this,
          &OptionCalculatorWindow::onCalculationMethodChanged);
}

void OptionCalculatorWindow::onCalculationMethodChanged() {
  bool ivMode = m_calcImpliedVolCheckBox->isChecked();
  m_volatilityEdit->setReadOnly(ivMode);
  if (ivMode) {
    m_volatilityEdit->setStyleSheet("background-color: #f0f0f0;");
  } else {
    m_volatilityEdit->setStyleSheet("");
  }
}

bool OptionCalculatorWindow::validateInputs(double &spot, double &strike,
                                            double &vol, double &rate,
                                            double &divYield, double &days) {
  bool ok;

  spot = m_spotPriceEdit->text().toDouble(&ok);
  if (!ok || spot <= 0) {
    m_errorLabel->setText("Spot Price Can not be Zero/Blank");
    return false;
  }

  strike = m_strikePriceEdit->text().toDouble(&ok);
  if (!ok || strike <= 0) {
    m_errorLabel->setText("Strike Price Can not be Zero/Blank");
    return false;
  }

  vol = m_volatilityEdit->text().toDouble(&ok);
  if (!ok || vol <= 0) {
    if (!m_calcImpliedVolCheckBox
             ->isChecked()) { // Only error if we're not calculating it
      m_errorLabel->setText("Volatility must be greater than zero");
      return false;
    }
  }

  rate = m_interestRateEdit->text().toDouble(&ok);
  if (!ok)
    rate = 0;

  divYield = m_dividendYieldEdit->text().toDouble(&ok);
  if (!ok)
    divYield = 0;

  days = m_daysUntilExpirationEdit->text().toDouble(&ok);
  if (!ok || days <= 0) {
    m_errorLabel->setText("Days Until Expiration must be greater than zero");
    return false;
  }

  m_errorLabel->setText(""); // Clear errors
  return true;
}

double OptionCalculatorWindow::applyDividendYield(double spot, double divYield,
                                                  double timeToExpiry) {
  if (divYield > 0 && timeToExpiry > 0) {
    // Continuous dividend yield adjustment: S * e^(-q*T)
    // Divide divYield by 100 since it is entered as a percentage
    double q = divYield / 100.0;
    return spot * std::exp(-q * timeToExpiry);
  }
  return spot;
}

void OptionCalculatorWindow::onCalculateClicked() {
  double spot, strike, volPercentage, ratePercentage, divYieldPercentage, days;

  if (!validateInputs(spot, strike, volPercentage, ratePercentage,
                      divYieldPercentage, days)) {
    return;
  }

  // Convert to formats expected by calculators
  double timeToExpiry = days / 365.0;
  double r = ratePercentage / 100.0;
  double sigma = volPercentage / 100.0;

  // Apply dividend yield to spot price
  double adjustedSpot =
      applyDividendYield(spot, divYieldPercentage, timeToExpiry);

  // Calculate Call
  OptionGreeks callGreeks = GreeksCalculator::calculate(
      adjustedSpot, strike, timeToExpiry, r, sigma, true);

  m_callTheoPriceLabel->setText(QString::number(callGreeks.price, 'f', 4));
  m_callDeltaLabel->setText(QString::number(callGreeks.delta, 'f', 4));
  m_callGammaLabel->setText(QString::number(callGreeks.gamma, 'f', 4));
  m_callThetaLabel->setText(QString::number(callGreeks.theta, 'f', 4));
  m_callVegaLabel->setText(QString::number(callGreeks.vega, 'f', 4));
  m_callRhoLabel->setText(QString::number(callGreeks.rho, 'f', 4));

  // Calculate Put
  OptionGreeks putGreeks = GreeksCalculator::calculate(
      adjustedSpot, strike, timeToExpiry, r, sigma, false);

  m_putTheoPriceLabel->setText(QString::number(putGreeks.price, 'f', 4));
  m_putDeltaLabel->setText(QString::number(putGreeks.delta, 'f', 4));
  m_putGammaLabel->setText(QString::number(putGreeks.gamma, 'f', 4));
  m_putThetaLabel->setText(QString::number(putGreeks.theta, 'f', 4));
  m_putVegaLabel->setText(QString::number(putGreeks.vega, 'f', 4));
  m_putRhoLabel->setText(QString::number(putGreeks.rho, 'f', 4));
}

void OptionCalculatorWindow::onCalculateIVClicked() {
  if (!m_calcImpliedVolCheckBox->isChecked()) {
    m_calcImpliedVolCheckBox->setChecked(true);
  }

  double spot, strike, volPercentage, ratePercentage, divYieldPercentage, days;

  // Basic validation, bypass Volatility check for IV calculation
  bool ok;
  spot = m_spotPriceEdit->text().toDouble(&ok);
  if (!ok || spot <= 0) {
    m_errorLabel->setText("Spot Price Can not be Zero/Blank");
    return;
  }
  strike = m_strikePriceEdit->text().toDouble(&ok);
  if (!ok || strike <= 0) {
    m_errorLabel->setText("Strike Price Can not be Zero/Blank");
    return;
  }
  ratePercentage = m_interestRateEdit->text().toDouble(&ok);
  if (!ok)
    ratePercentage = 0;
  divYieldPercentage = m_dividendYieldEdit->text().toDouble(&ok);
  if (!ok)
    divYieldPercentage = 0;
  days = m_daysUntilExpirationEdit->text().toDouble(&ok);
  if (!ok || days <= 0) {
    m_errorLabel->setText("Days Until Expiration must be greater than zero");
    return;
  }

  double marketValue = m_actualMarketValueEdit->text().toDouble(&ok);
  if (!ok || marketValue < 0) {
    m_errorLabel->setText("Invalid Actual Market Value");
    return;
  }

  // Conver to formats expected by calculators
  double timeToExpiry = days / 365.0;
  double r = ratePercentage / 100.0;
  bool isCall = m_callRadio->isChecked();

  // Apply dividend yield to spot price
  double adjustedSpot =
      applyDividendYield(spot, divYieldPercentage, timeToExpiry);

  // Check if IV is calculable
  if (!IVCalculator::isCalculable(marketValue, adjustedSpot, strike,
                                  timeToExpiry, r, isCall)) {
    m_errorLabel->setText(
        "IV Calculation failed. Market price must be >= intrinsic value.");
    return;
  }

  m_errorLabel->setText(""); // clear errors

  IVResult ivResult = IVCalculator::calculate(marketValue, adjustedSpot, strike,
                                              timeToExpiry, r, isCall);

  if (ivResult.converged) {
    m_volatilityEdit->setText(
        QString::number(ivResult.impliedVolatility * 100.0, 'f', 4));

    // Update greeks using the new IV
    double sigma = ivResult.impliedVolatility;

    // Calculate Call
    OptionGreeks callGreeks = GreeksCalculator::calculate(
        adjustedSpot, strike, timeToExpiry, r, sigma, true);
    m_callTheoPriceLabel->setText(QString::number(callGreeks.price, 'f', 4));
    m_callDeltaLabel->setText(QString::number(callGreeks.delta, 'f', 4));
    m_callGammaLabel->setText(QString::number(callGreeks.gamma, 'f', 4));
    m_callThetaLabel->setText(QString::number(callGreeks.theta, 'f', 4));
    m_callVegaLabel->setText(QString::number(callGreeks.vega, 'f', 4));
    m_callRhoLabel->setText(QString::number(callGreeks.rho, 'f', 4));

    // Calculate Put
    OptionGreeks putGreeks = GreeksCalculator::calculate(
        adjustedSpot, strike, timeToExpiry, r, sigma, false);
    m_putTheoPriceLabel->setText(QString::number(putGreeks.price, 'f', 4));
    m_putDeltaLabel->setText(QString::number(putGreeks.delta, 'f', 4));
    m_putGammaLabel->setText(QString::number(putGreeks.gamma, 'f', 4));
    m_putThetaLabel->setText(QString::number(putGreeks.theta, 'f', 4));
    m_putVegaLabel->setText(QString::number(putGreeks.vega, 'f', 4));
    m_putRhoLabel->setText(QString::number(putGreeks.rho, 'f', 4));

  } else {
    // Fallback to brent
    IVResult brentResult = IVCalculator::calculateBrent(
        marketValue, adjustedSpot, strike, timeToExpiry, r, isCall);
    if (brentResult.converged) {
      m_volatilityEdit->setText(
          QString::number(brentResult.impliedVolatility * 100.0, 'f', 4));
    } else {
      m_errorLabel->setText("IV Calculation failed to converge.");
    }
  }
}

void OptionCalculatorWindow::onCancelClicked() {
  // Attempt to close the parent window if it's an MDI subwindow, otherwise
  // close self
  QWidget *parentWin = parentWidget();
  while (parentWin != nullptr) {
    if (parentWin->inherits("QMdiSubWindow") ||
        parentWin->inherits("CustomMDISubWindow")) {
      parentWin->close();
      return;
    }
    parentWin = parentWin->parentWidget();
  }

  // Fallback if not inside an MDI window
  close();
}

void OptionCalculatorWindow::closeEvent(QCloseEvent *event) {
  WindowSettingsHelper::saveWindowSettings(this, "OptionCalculator");
  QWidget::closeEvent(event);
}
