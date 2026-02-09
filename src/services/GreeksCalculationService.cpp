#include "services/GreeksCalculationService.h"
#include "bse_price_store.h"
#include "nsecm_price_store.h"
#include "nsefo_price_store.h"
#include "repository/ContractData.h"
#include "repository/Greeks.h"
#include "repository/IVCalculator.h"
#include "repository/RepositoryManager.h"

#include <QDate>
#include <QDebug>
#include <QSettings>
#include <QTime>
#include <cmath>

// ============================================================================
// SINGLETON INSTANCE
// ============================================================================

GreeksCalculationService &GreeksCalculationService::instance() {
  static GreeksCalculationService inst;
  return inst;
}

GreeksCalculationService::GreeksCalculationService(QObject *parent)
    : QObject(parent), m_timeTickTimer(new QTimer(this)),
      m_illiquidUpdateTimer(new QTimer(this)) {
  connect(m_timeTickTimer, &QTimer::timeout, this,
          &GreeksCalculationService::onTimeTick);
  connect(m_illiquidUpdateTimer, &QTimer::timeout, this,
          &GreeksCalculationService::processIlliquidUpdates);
  loadNSEHolidays();
}

GreeksCalculationService::~GreeksCalculationService() {
  if (m_timeTickTimer) {
    m_timeTickTimer->stop();
  }
}

// ============================================================================
// INITIALIZATION
// ============================================================================

void GreeksCalculationService::initialize(const GreeksConfig &config) {
  m_config = config;
  // qInfo() << "[GreeksDebug] Initializing Service. Enabled:" <<
  // m_config.enabled
  //        << " RiskFree:" << m_config.riskFreeRate;

  // Start time tick timer
  if (m_config.enabled && m_config.timeTickIntervalSec > 0) {
    m_timeTickTimer->start(m_config.timeTickIntervalSec * 1000);
  }

  // Start illiquid update timer
  if (m_config.enabled && m_config.illiquidUpdateIntervalSec > 0) {
    m_illiquidUpdateTimer->start(m_config.illiquidUpdateIntervalSec *
                                 1000); // in ms
  }

  /* qDebug() << "[GreeksCalculationService] Initialized with:"
           << "riskFreeRate=" << m_config.riskFreeRate
           << "autoCalculate=" << m_config.autoCalculate
           << "throttleMs=" << m_config.throttleMs; */

  emit configurationChanged();
}

void GreeksCalculationService::loadConfiguration() {
  QSettings settings("configs/config.ini", QSettings::IniFormat);
  settings.beginGroup("GREEKS_CALCULATION");

  m_config.enabled = settings.value("enabled", true).toBool();
  m_config.riskFreeRate = settings.value("risk_free_rate", 0.065).toDouble();
  m_config.dividendYield = settings.value("dividend_yield", 0.0).toDouble();
  m_config.autoCalculate = settings.value("auto_calculate", true).toBool();
  m_config.throttleMs = settings.value("throttle_ms", 1000).toInt();
  m_config.ivInitialGuess = settings.value("iv_initial_guess", 0.20).toDouble();
  m_config.ivTolerance = settings.value("iv_tolerance", 1e-6).toDouble();
  m_config.ivMaxIterations = settings.value("iv_max_iterations", 100).toInt();
  m_config.timeTickIntervalSec =
      settings.value("time_tick_interval", 60).toInt();
  m_config.illiquidUpdateIntervalSec =
      settings.value("illiquid_update_interval", 30).toInt();
  m_config.illiquidThresholdSec =
      settings.value("illiquid_threshold", 30).toInt();
  m_config.basePriceMode =
      settings.value("base_price_mode", "cash").toString().toLower();
  m_config.calculateOnEveryFeed =
      settings.value("calculate_on_every_feed", false).toBool();

  settings.endGroup();

  // qDebug() << "[GreeksCalculationService] Configuration loaded from
  // config.ini";

  // Apply configuration
  initialize(m_config);
}

void GreeksCalculationService::setRepositoryManager(
    RepositoryManager *repoManager) {
  m_repoManager = repoManager;
}

void GreeksCalculationService::setRiskFreeRate(double rate) {
  m_config.riskFreeRate = rate;
  // qDebug() << "[GreeksCalculationService] Risk-free rate updated to:" <<
  // rate;
  emit configurationChanged();
}

// ============================================================================
// MAIN CALCULATION
// ============================================================================

GreeksResult GreeksCalculationService::calculateForToken(uint32_t token,
                                                         int exchangeSegment) {
  GreeksResult result;
  result.token = token;
  result.exchangeSegment = exchangeSegment;
  result.calculationTimestamp = QDateTime::currentMSecsSinceEpoch();

  // Debug: Log specific tokens for debugging
  // 26000 (Index), 59182 (Future), 42546 (Option)
  bool shouldLog =
      (token == 26000 || token == 59182 || token == 42546 || token == 59460 ||
       token == 2885 ||
       token == 131523); // Add any specific tokens you want to debug

  if (shouldLog) {
    // qInfo() << "[GreeksDebug] calculateForToken START Token:" << token
    //         << "Seg:" << exchangeSegment << "Enabled:" << m_config.enabled;
  }

  // Step 1: Check if service is enabled
  if (!m_config.enabled) {
    // qDebug() << "[GreeksDebug] Service disabled for token:" << token;
    emit calculationFailed(token, exchangeSegment, "Service disabled");
    return result;
  }

  // Step 2: Check repository manager
  if (!m_repoManager) {
    // qDebug() << "[GreeksDebug] RepoManager null for token:" << token;
    emit calculationFailed(token, exchangeSegment,
                           "Repository manager not set");
    return result;
  }

  // Step 3: Get contract data
  const ContractData *contract =
      m_repoManager->getContractByToken(exchangeSegment, token);

  if (!contract) {
    qDebug() << "[GreeksDebug] Contract not found for token:" << token;
    emit calculationFailed(token, exchangeSegment, "Contract not found");
    return result;
  }

  // Step 4: Check if it's an option
  if (!isOption(contract->instrumentType)) {
    // if (shouldLog)
    //   qDebug() << "[GreeksDebug] Not an option for token:" << token
    //            << "type:" << contract->instrumentType;
    return result;
  }

  // Step 5: Get option prices from price store
  double optionPrice = 0.0;
  double bidPrice = 0.0;
  double askPrice = 0.0;
  int64_t lastTradeTime = 0;

  if (exchangeSegment == 2) { // NSEFO
    auto state = nsefo::g_nseFoPriceStore.getUnifiedSnapshot(token);
    if (state.token != 0) {
      optionPrice = state.ltp;
      bidPrice = state.bids[0].price;
      askPrice = state.asks[0].price;
      lastTradeTime = state.lastTradeTime;
    }
  } else if (exchangeSegment == 12) { // BSEFO
    auto state = bse::g_bseFoPriceStore.getUnifiedSnapshot(token);
    if (state.token != 0) {
      optionPrice = state.ltp;
      bidPrice = state.bids[0].price;
      askPrice = state.asks[0].price;
      lastTradeTime = state.lastTradeTime;
    }
  }

  // Debug Option Prices
  if (shouldLog) {
    // qInfo() << "[GreeksDebug] Market Data for Token:" << token
    //         << "LTP:" << optionPrice << "Bid:" << bidPrice << "Ask:" <<
    //         askPrice
    //         << "LastTradeTime:" << lastTradeTime;
  }

  if (optionPrice <= 0 && bidPrice <= 0 && askPrice <= 0) {
    if (shouldLog) {
      qDebug() << "[GreeksDebug] No valid market data (prices <= 0) for token:"
               << token;
    }
    emit calculationFailed(token, exchangeSegment, "No market data available");
    return result;
  }

  // Step 6: Get underlying price
  double underlyingPrice = 0.0;

  // Try future-based pricing if configured
  if (m_config.basePriceMode == "future") {
    uint32_t futureToken = m_repoManager->getNextExpiryFutureToken(
        contract->name, exchangeSegment);
    if (futureToken > 0) {
      underlyingPrice = getUnderlyingPrice(futureToken, exchangeSegment);
    }
  }

  // Fallback to cash/spot pricing
  if (underlyingPrice <= 0) {
    uint32_t underlyingToken = 0;

    if (contract->assetToken > 0) {
      // Stock options: use assetToken directly
      underlyingToken = static_cast<uint32_t>(contract->assetToken);
    } else if (!contract->name.isEmpty()) {
      // Index options (NIFTY, BANKNIFTY): assetToken is -1, use symbol lookup
      underlyingToken = m_repoManager->getAssetTokenForSymbol(contract->name);

      if (shouldLog) {
        // qDebug() << "[GreeksDebug] Resolved symbol" << contract->name
        //          << "to token:" << underlyingToken;
      }
    }

    // Fetch underlying price directly from cash market
    if (underlyingToken > 0 && exchangeSegment == 2) { // NSE FO
      underlyingPrice = nsecm::getGenericLtp(underlyingToken);

      if (shouldLog) {
        // qDebug() << "[GreeksDebug] Fetched underlying price for token:" <<
        // underlyingToken
        //          << "Price:" << underlyingPrice;
      }
    } else if (underlyingToken > 0 && exchangeSegment == 4) { // BSE FO
      auto spotState =
          bse::g_bseCmPriceStore.getUnifiedSnapshot(underlyingToken);
      underlyingPrice = spotState.ltp;

      if (shouldLog) {
        // qDebug() << "[GreeksDebug] Fetched BSE underlying price for token:"
        // << underlyingToken
        //          << "Price:" << underlyingPrice;
      }
    }
  }

  if (underlyingPrice <= 0) {
    if (shouldLog) {
      // qDebug() << "[GreeksDebug] underlyingPrice is ZERO for token:" << token
      //          << "AssetToken:" << contract->assetToken;
    }
    emit calculationFailed(token, exchangeSegment,
                           "Underlying price not available");
    return result;
  }

  if (shouldLog) {
    // qDebug() << "[GreeksDebug] Got underlying price:" << underlyingPrice
    //          << "for" << contract->name << "| Option LTP:" << optionPrice;
  }

  // qDebug() << "[GreeksDebug] Successfully got underlying price:" <<
  // underlyingPrice
  //          << "for symbol:" << contract->name;

  // Step 7: Register underlying mapping
  if (!m_cache.contains(token) && contract->assetToken > 0) {
    m_underlyingToOptions.insert(static_cast<uint32_t>(contract->assetToken),
                                 token);
  }

  // Step 8: Get strike and expiry from contract
  double strikePrice = contract->strikePrice;
  QString expiryDate = contract->expiryDate;
  bool isCall = (contract->optionType == "CE");

  // Step 9: Get time to expiry (pre-calculated or fallback)
  // Use pre-calculated timeToExpiry from master file for performance (if
  // available) Otherwise calculate dynamically for intraday precision
  double T = 0.0;
  if (contract->timeToExpiry > 0.0) {
    // Use pre-calculated value from master file (fast path)
    T = contract->timeToExpiry;
  } else if (contract->expiryDate_dt.isValid()) {
    // Fallback: Calculate dynamically with trading days and intraday component
    // (slow path)
    T = calculateTimeToExpiry(contract->expiryDate_dt);
  } else {
    T = calculateTimeToExpiry(expiryDate);
  }

  if (T <= 0) {
    if (shouldLog) {
      qWarning() << "[GreeksDebug] FAIL: Time to expiry <= 0 for token:"
                 << token << "Expiry:" << expiryDate << "T:" << T;
    }
    emit calculationFailed(token, exchangeSegment, "Option expired");
    return result;
  }

  // Store input values
  result.spotPrice = underlyingPrice;
  result.strikePrice = strikePrice;
  result.timeToExpiry = T;
  result.optionPrice = optionPrice;

  // Step 10: Calculate Implied Volatility
  if (shouldLog) {
    // qInfo() << "[GreeksDebug] INPUTS Token:" << token
    //         << "Spot:" << underlyingPrice << "Strike:" << strikePrice
    //         << "TTE:" << T << "OptPrice:" << optionPrice
    //         << "RiskFree:" << m_config.riskFreeRate;
  }

  // Use cached IV as initial guess for faster convergence
  double usedIV = 0.0;
  double ivInitialGuess = m_config.ivInitialGuess;

  // Check if we have cached IV for this token
  auto cachedIt = m_cache.find(token);
  if (cachedIt != m_cache.end() &&
      cachedIt.value().result.impliedVolatility > 0) {
    ivInitialGuess = cachedIt.value().result.impliedVolatility;
  }

  if (optionPrice > 0) {
    IVResult ivResult = IVCalculator::calculate(
        optionPrice, underlyingPrice, strikePrice, T, m_config.riskFreeRate,
        isCall, ivInitialGuess, m_config.ivTolerance, m_config.ivMaxIterations);
    usedIV = ivResult.impliedVolatility;

    result.impliedVolatility = usedIV;
    result.ivConverged = ivResult.converged;
    result.ivIterations = ivResult.iterations;
  }

  // Calculate Bid IV
  if (bidPrice > 0) {
    IVResult bidRes = IVCalculator::calculate(
        bidPrice, underlyingPrice, strikePrice, T, m_config.riskFreeRate,
        isCall, usedIV > 0 ? usedIV : m_config.ivInitialGuess);
    result.bidIV = bidRes.impliedVolatility;
  }

  // Calculate Ask IV
  if (askPrice > 0) {
    IVResult askRes = IVCalculator::calculate(
        askPrice, underlyingPrice, strikePrice, T, m_config.riskFreeRate,
        isCall, usedIV > 0 ? usedIV : m_config.ivInitialGuess);
    result.askIV = askRes.impliedVolatility;
  }

  if (!result.ivConverged) {
    if (shouldLog) {
      qDebug() << "[GreeksDebug] IV convergence failed for token:" << token
               << "after" << result.ivIterations << "iterations";
    }
  }

  // Step 11: Calculate Greeks using Black-Scholes
  if (result.impliedVolatility > 0) {
    // OptionGreeks greeks = GreeksCalculator::calculate(
    //     underlyingPrice, strikePrice, T, m_config.riskFreeRate, usedIV,
    //     isCall);
    // FIX: Pass dividend yield if supported, otherwise just use standard
    // signature Assuming standard signature based on previous code
    OptionGreeks greeks = GreeksCalculator::calculate(
        underlyingPrice, strikePrice, T, m_config.riskFreeRate, usedIV, isCall);

    result.delta = greeks.delta;
    result.gamma = greeks.gamma;
    result.vega = greeks.vega;
    result.theta = greeks.theta;
    result.rho = greeks.rho;
    result.theoreticalPrice = greeks.price;

    if (shouldLog) {
      // qInfo() << "[GreeksDebug] OUTPUTS Token:" << token
      //         << "IV:" << result.impliedVolatility << "Delta:" <<
      //         result.delta
      //         << "Gamma:" << result.gamma << "Vega:" << result.vega
      //         << "Theta:" << result.theta;
    }
  } else if (shouldLog) {
    qInfo() << "[GreeksDebug] OUTPUTS Token:" << token
            << "IV is ZERO or failed to converge";
  }

  // Step 12: Update cache
  CacheEntry entry;
  entry.result = result;
  entry.lastCalculationTime = result.calculationTimestamp;
  entry.lastPrice = optionPrice;
  entry.lastUnderlyingPrice = underlyingPrice;
  entry.lastTradeTimestamp = QDateTime::currentMSecsSinceEpoch();

  m_cache[token] = entry;

  // Step 13: Emit signal
  // qDebug() << "[GreeksService] Emitting greeksCalculated for token:" << token
  //          << "IV:" << result.impliedVolatility << "Delta:" << result.delta;
  emit greeksCalculated(token, exchangeSegment, result);

  return result;
}

// ============================================================================
// CACHE MANAGEMENT
// ============================================================================

// currently not used
std::optional<GreeksResult>
GreeksCalculationService::getCachedGreeks(uint32_t token) const {
  auto it = m_cache.find(token);
  if (it != m_cache.end()) {
    return it.value().result;
  }
  return std::nullopt;
}

void GreeksCalculationService::clearCache() {
  m_cache.clear();
  m_underlyingToOptions.clear();
}

void GreeksCalculationService::forceRecalculateAll() {
  QList<uint32_t> tokens = m_cache.keys();
  for (uint32_t token : tokens) {
    int segment = m_cache[token].result.exchangeSegment;
    calculateForToken(token, segment);
  }
}

// ============================================================================
// PRICE UPDATE HANDLERS
// ============================================================================

void GreeksCalculationService::onPriceUpdate(uint32_t token, double ltp,
                                             int exchangeSegment) {
  // qDebug() << "[GreeksService] onPriceUpdate called: Token:" << token
  //          << "LTP:" << ltp << "Seg:" << exchangeSegment;

  if (!m_config.enabled || !m_config.autoCalculate) {
    qDebug() << "[GreeksService] SKIPPED: enabled=" << m_config.enabled
             << "autoCalculate=" << m_config.autoCalculate;
    return;
  }

  // If calculateOnEveryFeed is enabled, bypass throttling and calculate
  // immediately
  if (m_config.calculateOnEveryFeed) {
    calculateForToken(token, exchangeSegment);
    return;
  }

  // THROTTLING LOGIC (when calculateOnEveryFeed = false)
  // Check if enough time has passed since last calculation
  int64_t now = QDateTime::currentMSecsSinceEpoch();
  auto it = m_cache.find(token);

  if (it != m_cache.end()) {
    int64_t timeSinceLastCalc = now - it.value().result.calculationTimestamp;

    // If throttle time not elapsed, skip calculation
    if (timeSinceLastCalc < m_config.throttleMs) {
      return;
    }
  }

  // qDebug() << "[GreeksService] CALLING calculateForToken for:" << token;
  calculateForToken(token, exchangeSegment);
}

void GreeksCalculationService::onUnderlyingPriceUpdate(uint32_t underlyingToken,
                                                       double ltp,
                                                       int exchangeSegment) {
  if (!m_config.enabled || !m_config.autoCalculate)
    return;

  // Find all options linked to this underlying
  QList<uint32_t> optionTokens = m_underlyingToOptions.values(underlyingToken);
  int64_t now = QDateTime::currentMSecsSinceEpoch();

  // If calculateOnEveryFeed is enabled, force immediate calculation for ALL
  // options
  if (m_config.calculateOnEveryFeed) {
    for (uint32_t token : optionTokens) {
      auto it = m_cache.find(token);
      if (it != m_cache.end()) {
        calculateForToken(token, it.value().result.exchangeSegment);
      }
    }
    return;
  }

  // HYBRID THROTTLING LOGIC (when calculateOnEveryFeed = false)
  // 1. If Liquid (Traded recently): Update immediately
  // 2. If Illiquid: Skip (will be caught by processIlliquidUpdates timer)
  for (uint32_t token : optionTokens) {
    // We need the exchange segment to calculate
    auto it = m_cache.find(token);
    if (it != m_cache.end()) {

      int64_t lastTradeTime = it.value().lastTradeTimestamp;
      int64_t timeSinceTrade = now - lastTradeTime;

      if (timeSinceTrade < (m_config.illiquidThresholdSec * 1000)) {
        // Liquid: Update immediately using Cached IV + New Spot
        calculateForToken(token, it.value().result.exchangeSegment);
      }
      // else: Illiquid, do nothing. Background timer will handle it.
    }
  }
}

void GreeksCalculationService::processIlliquidUpdates() {
  if (!m_config.enabled || !m_config.autoCalculate)
    return;

  int64_t now = QDateTime::currentMSecsSinceEpoch();
  int cutoff = m_config.illiquidThresholdSec * 1000;

  // Iterate all cached tokens
  auto it = m_cache.begin();
  while (it != m_cache.end()) {
    int64_t lastTradeTime = it.value().lastTradeTimestamp;

    // Process only if ILLIQUID (> 30s since last option trade)
    if ((now - lastTradeTime) > cutoff) {

      // Check if underlying moved significantly since last calc to avoid
      // useless work (Optional optimization: reusing shouldRecalculate logic
      // implicitly via calculateForToken)

      // Force calculation (using Cached IV + Current Spot)
      calculateForToken(it.key(), it.value().result.exchangeSegment);
    }
    ++it;
  }
}

void GreeksCalculationService::onTimeTick() {
  // Time-based recalculation for theta decay
  // Only recalculate if underlying or option price has changed significantly
  forceRecalculateAll();
}

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

double GreeksCalculationService::getUnderlyingPrice(uint32_t optionToken,
                                                    int exchangeSegment) {
  if (!m_repoManager) {
    return 0.0;
  }

  const ContractData *contract =
      m_repoManager->getContractByToken(exchangeSegment, optionToken);

  if (!contract) {
    return 0.0;
  }

  int64_t underlyingToken = contract->assetToken;

  qDebug() << "[GreeksDebug] getUnderlyingPrice() called for option token:"
           << optionToken << "Segment:" << exchangeSegment
           << "AssetToken:" << underlyingToken << "Symbol:" << contract->name;

  // Look up by symbol for index options (e.g., NIFTY)
  if (underlyingToken <= 0 && !contract->name.isEmpty()) {
    underlyingToken = m_repoManager->getAssetTokenForSymbol(contract->name);
    // qDebug() << "[GreeksDebug] Resolved symbol" << contract->name << "to
    // token:" << underlyingToken;
  }

  if (underlyingToken <= 0) {
    qDebug() << "[GreeksDebug] FAILED: Could not resolve underlying token for"
             << contract->name;
    return 0.0;
  }

  // qDebug() << "[GreeksDebug] Will fetch price for underlying token:" <<
  // underlyingToken;

  // Debug specific underlying tokens
  bool shouldLog = (underlyingToken == 26000 || underlyingToken == 59182);

  if (exchangeSegment == 2) { // NSEFO
    // Try futures price first
    auto futureState = nsefo::g_nseFoPriceStore.getUnifiedSnapshot(
        static_cast<uint32_t>(underlyingToken));

    if (shouldLog) {
      // qDebug() << "[GreeksDebug] getUnderlyingPrice Token:" <<
      // underlyingToken
      //          << "FutureLTP:" << futureState.ltp;
    }

    if (futureState.token != 0 && futureState.ltp > 0) {
      return futureState.ltp;
    }

    // Fallback to spot price (handles equities and indices)
    double price = nsecm::getGenericLtp(static_cast<uint32_t>(underlyingToken));

    if (shouldLog) {
      // qDebug() << "[GreeksDebug] getUnderlyingPrice Token:" <<
      // underlyingToken
      //          << "SpotLTP:" << price;
    }

    if (price > 0) {
      return price;
    }
  } else if (exchangeSegment == 12) { // BSEFO
    // Try futures first
    auto futureState = bse::g_bseFoPriceStore.getUnifiedSnapshot(
        static_cast<uint32_t>(underlyingToken));
    if (futureState.token != 0 && futureState.ltp > 0) {
      return futureState.ltp;
    }

    // Fallback to cash market
    auto cashState = bse::g_bseCmPriceStore.getUnifiedSnapshot(
        static_cast<uint32_t>(underlyingToken));
    if (cashState.token != 0 && cashState.ltp > 0) {
      return cashState.ltp;
    }
  }

  return 0.0;
}

bool GreeksCalculationService::isOption(int instrumentType) {
  return instrumentType == 2; // 2 = Option in NSE/BSE
}

bool GreeksCalculationService::shouldRecalculate(
    uint32_t token, double currentPrice, double currentUnderlyingPrice) {
  return true;
}

double
GreeksCalculationService::calculateTimeToExpiry(const QString &expiryDate) {
  // Parse expiry date (format: "27JAN2026" or "27-JAN-2026")
  QDate expiry;

  // Try different formats
  expiry = QDate::fromString(expiryDate, "ddMMMyyyy");
  if (!expiry.isValid()) {
    expiry = QDate::fromString(expiryDate, "dd-MMM-yyyy");
  }
  if (!expiry.isValid()) {
    expiry = QDate::fromString(expiryDate, "yyyy-MM-dd");
  }

  if (!expiry.isValid()) {
    qWarning() << "[GreeksCalculationService] Failed to parse expiry date:"
               << expiryDate;
    return 0.0;
  }

  return calculateTimeToExpiry(expiry);
}

double GreeksCalculationService::calculateTimeToExpiry(const QDate &expiry) {
  QDate today = QDate::currentDate();

  if (expiry < today) {
    return 0.0; // Expired
  }

  // Calculate trading days
  int tradingDays = calculateTradingDays(today, expiry);

  // Add intraday component
  QTime now = QTime::currentTime();
  QTime marketClose(15, 30, 0); // NSE closes at 3:30 PM

  double intraDayFraction = 0.0;
  if (now < marketClose) {
    // Trading day not over
    int secondsRemaining = now.secsTo(marketClose);
    // int tradingSeconds = 6.5 * 60 * 60; // 6.5 hours trading day
    intraDayFraction = static_cast<double>(secondsRemaining) / (24 * 60 * 60);

    // For Greeks, T is typically (Days + TimeFrac)/252
    if (tradingDays > 0) {
      tradingDays--; // Don't double count today
    }
  }

  // Convert to years (252 trading days per year in India)
  double T = (tradingDays + intraDayFraction) / 252.0;

  // Minimum time to avoid division by zero
  return std::max(T, 0.0001);
}

int GreeksCalculationService::calculateTradingDays(const QDate &start,
                                                   const QDate &end) {
  int count = 0;
  QDate current = start;

  while (current <= end) {
    if (isNSETradingDay(current)) {
      count++;
    }
    current = current.addDays(1);
  }

  return count;
}

bool GreeksCalculationService::isNSETradingDay(const QDate &date) {
  // Weekend check
  int dayOfWeek = date.dayOfWeek();
  if (dayOfWeek == Qt::Saturday || dayOfWeek == Qt::Sunday) {
    return false;
  }

  // Holiday check
  return !m_nseHolidays.contains(date);
}

void GreeksCalculationService::loadNSEHolidays() {
  // NSE Holidays 2026 (update annually)
  // These are approximate - should be loaded from a config file
  m_nseHolidays = {
      QDate(2026, 1, 26),  // Republic Day
      QDate(2026, 3, 14),  // Holi
      QDate(2026, 3, 30),  // Good Friday
      QDate(2026, 4, 2),   // Ram Navami
      QDate(2026, 4, 14),  // Dr. Ambedkar Jayanti
      QDate(2026, 5, 1),   // Maharashtra Day
      QDate(2026, 8, 15),  // Independence Day
      QDate(2026, 8, 19),  // Janmashtami
      QDate(2026, 10, 2),  // Gandhi Jayanti
      QDate(2026, 10, 24), // Dussehra
      QDate(2026, 11, 12), // Diwali
      QDate(2026, 11, 13), // Diwali (Laxmi Pujan)
      QDate(2026, 11, 14), // Diwali (Balipratipada)
      QDate(2026, 12, 25), // Christmas
  };
}
