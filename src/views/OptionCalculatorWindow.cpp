#include "views/OptionCalculatorWindow.h"
#include "ui_OptionCalculatorWindow.h"
#include "utils/WindowSettingsHelper.h"
#include "utils/PreferencesManager.h"
#include "repository/RepositoryManager.h"
#include "data/PriceStoreGateway.h"
#include <nsecm_price_store.h>
#include <QDate>
#include <QDoubleValidator>

OptionCalculatorWindow::OptionCalculatorWindow(QWidget *parent)
    : QWidget(parent), ui(new Ui::OptionCalculatorWindow)
{
  ui->setupUi(this);

  // ── assign convenience member pointers ────────────────────────────────
  m_spotPriceEdit           = ui->m_spotPriceEdit;
  m_strikePriceEdit         = ui->m_strikePriceEdit;
  m_volatilityEdit          = ui->m_volatilityEdit;
  m_interestRateEdit        = ui->m_interestRateEdit;
  m_dividendYieldEdit       = ui->m_dividendYieldEdit;
  m_daysUntilExpirationEdit = ui->m_daysUntilExpirationEdit;

  m_callTheoPriceLabel = ui->m_callTheoPriceLabel;
  m_callDeltaLabel     = ui->m_callDeltaLabel;
  m_callGammaLabel     = ui->m_callGammaLabel;
  m_callThetaLabel     = ui->m_callThetaLabel;
  m_callVegaLabel      = ui->m_callVegaLabel;
  m_callRhoLabel       = ui->m_callRhoLabel;

  m_putTheoPriceLabel = ui->m_putTheoPriceLabel;
  m_putDeltaLabel     = ui->m_putDeltaLabel;
  m_putGammaLabel     = ui->m_putGammaLabel;
  m_putThetaLabel     = ui->m_putThetaLabel;
  m_putVegaLabel      = ui->m_putVegaLabel;
  m_putRhoLabel       = ui->m_putRhoLabel;

  m_calcImpliedVolCheckBox = ui->m_calcImpliedVolCheckBox;
  m_actualMarketValueEdit  = ui->m_actualMarketValueEdit;

  m_callRadio         = ui->m_callRadio;
  m_putRadio          = ui->m_putRadio;
  m_binomialRadio     = ui->m_binomialRadio;
  m_blackScholesRadio = ui->m_blackScholesRadio;

  m_calculateBtn  = ui->m_calculateBtn;
  m_impliedVolBtn = ui->m_impliedVolBtn;
  m_cancelBtn     = ui->m_cancelBtn;

  m_errorLabel = ui->m_errorLabel;

  // ── validators ────────────────────────────────────────────────────────
  auto *priceV   = new QDoubleValidator(0.0,  1e6,    4, this);
  auto *percentV = new QDoubleValidator(0.0,  500.0,  4, this);
  auto *daysV    = new QDoubleValidator(0.0,  3650.0, 4, this);
  percentV->setNotation(QDoubleValidator::StandardNotation);

  m_spotPriceEdit->setValidator(priceV);
  m_strikePriceEdit->setValidator(priceV);
  m_volatilityEdit->setValidator(percentV);
  m_interestRateEdit->setValidator(percentV);
  m_dividendYieldEdit->setValidator(percentV);
  m_daysUntilExpirationEdit->setValidator(daysV);
  m_actualMarketValueEdit->setValidator(priceV);

  setupConnections();
  WindowSettingsHelper::loadAndApplyWindowSettings(this, "OptionCalculator");
  initializeFromDefaults();
}

OptionCalculatorWindow::~OptionCalculatorWindow()
{
  delete ui;
}

// ────────────────────────────────────────────────────────────────────────────
// NOTE: All UI layout is defined in resources/forms/OptionCalculatorWindow.ui
//       Edit visuals with Qt Designer, not here.
// ────────────────────────────────────────────────────────────────────────────

// (placeholder to preserve file structure — real UI comes from .ui file)
static void _ui_placeholder() {} // suppress unused-code warnings

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
    m_volatilityEdit->setStyleSheet(
        "QLineEdit { background-color:#f1f5f9; color:#64748b;"
        " border:1px solid #e2e8f0; border-radius:3px;"
        " padding:2px 6px; font-size:11px; }");
  } else {
    m_volatilityEdit->setStyleSheet(
        "QLineEdit { background:#ffffff; color:#1e293b;"
        " border:1px solid #cbd5e1; border-radius:3px;"
        " padding:2px 6px; font-size:11px; }"
        "QLineEdit:focus { border-color:#3b82f6; }");
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

void OptionCalculatorWindow::initializeFromDefaults() {
  auto &prefs = PreferencesManager::instance();

  // Pre-fill interest rate and dividend yield from preferences
  m_interestRateEdit->setText(QString::number(prefs.getInterestRate(),    'f', 2));
  m_dividendYieldEdit->setText(QString::number(prefs.getDividendYield(), 'f', 2));

  QString symbol = prefs.getDefaultDerivativeSymbol();
  if (symbol.isEmpty()) symbol = "NIFTY";

  auto *repo = RepositoryManager::getInstance();
  if (!repo) return;

  // ── Spot price ──────────────────────────────────────────────────────────
  QString src = prefs.getUnderlyingPriceSource(); // "cash" or "future"
  double spot = 0.0;

  if (src == "future") {
    QVector<ContractData> contracts = repo->getOptionChain("NSE", symbol);
    QDate today = QDate::currentDate();
    int minDays = INT_MAX;
    int64_t futToken = 0;
    for (const auto &c : contracts) {
      if (c.instrumentType == 1 && c.expiryDate_dt.isValid()) {
        int d = today.daysTo(c.expiryDate_dt);
        if (d >= 0 && d < minDays) { minDays = d; futToken = c.exchangeInstrumentID; }
      }
    }
    if (futToken != 0) {
      auto snap = MarketData::PriceStoreGateway::instance()
                      .getUnifiedSnapshot(2, futToken);
      spot = snap.ltp;
    }
  } else {
    int64_t tok = repo->getAssetTokenForSymbol(symbol);
    if (tok > 0)
      spot = nsecm::getGenericLtp(static_cast<uint32_t>(tok));
  }

  if (spot > 0.0)
    m_spotPriceEdit->setText(QString::number(spot, 'f', 2));

  // ── Nearest expiry → days remaining ─────────────────────────────────────
  QVector<ContractData> contracts2 = repo->getOptionChain("NSE", symbol);
  QDate today2 = QDate::currentDate();
  int minExpDays = INT_MAX;
  for (const auto &c : contracts2) {
    if (c.instrumentType == 2 && c.expiryDate_dt.isValid()) {
      int d = today2.daysTo(c.expiryDate_dt);
      if (d >= 0 && d < minExpDays) minExpDays = d;
    }
  }
  if (minExpDays != INT_MAX && minExpDays > 0)
    m_daysUntilExpirationEdit->setText(QString::number(minExpDays));
}
