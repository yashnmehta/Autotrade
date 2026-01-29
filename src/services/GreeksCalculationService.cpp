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
  qInfo() << "[GreeksDebug] Initializing Service. Enabled:" << m_config.enabled
          << " RiskFree:" << m_config.riskFreeRate;

  // Start time tick timer
  if (m_config.enabled && m_config.timeTickIntervalSec > 0) {
    m_timeTickTimer->start(m_config.timeTickIntervalSec * 1000);
  }

  // Start illiquid update timer
  if (m_config.enabled && m_config.illiquidUpdateIntervalSec > 0) {
    m_illiquidUpdateTimer->start(m_config.illiquidUpdateIntervalSec * 1000); // in ms
  }

  qDebug() << "[GreeksCalculationService] Initialized with:"
           << "riskFreeRate=" << m_config.riskFreeRate
           << "autoCalculate=" << m_config.autoCalculate
           << "throttleMs=" << m_config.throttleMs;

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

  settings.endGroup();

  qDebug() << "[GreeksCalculationService] Configuration loaded from config.ini";

  // Apply configuration
  initialize(m_config);
}

void GreeksCalculationService::setRepositoryManager(
    RepositoryManager *repoManager) {
  m_repoManager = repoManager;
}

void GreeksCalculationService::setRiskFreeRate(double rate) {
  m_config.riskFreeRate = rate;
  qDebug() << "[GreeksCalculationService] Risk-free rate updated to:" << rate;
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

  // Debug: Log specific tokens for debugging (NSEFO: 58705, 58706, 49339 | NSECM: 26000)
  bool shouldLog = false;
  if (exchangeSegment == 2 && (token == 58705 || token == 58706 || token == 49339)) {
    shouldLog = true;
    qInfo() << "[GreeksDebug] calculateForToken Token:" << token
            << "Seg:" << exchangeSegment << "Enabled:" << m_config.enabled;
  } else if (exchangeSegment == 1 && token == 26000) {
    shouldLog = true;
    qInfo() << "[GreeksDebug] calculateForToken Token:" << token
            << "Seg:" << exchangeSegment << "Enabled:" << m_config.enabled;
  }

  // Step 1: Check if service is enabled
  if (!m_config.enabled) {
    qDebug() << "[GreeksDebug] Service disabled for token:" << token;
    emit calculationFailed(token, exchangeSegment, "Service disabled");
    return result;
  }

  // Step 2: Check repository manager
  if (!m_repoManager) {
    qDebug() << "[GreeksDebug] RepoManager null for token:" << token;
    emit calculationFailed(token, exchangeSegment, "Repository manager not set");
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
    qDebug() << "[GreeksDebug] Not an option, token:" << token << "type:" << contract->instrumentType;
    return result;
  }

  // Step 5: Get option prices from price store
  double optionPrice = 0.0;
  double bidPrice = 0.0;
  double askPrice = 0.0;

  if (exchangeSegment == 2) { // NSEFO
    const auto *state = nsefo::g_nseFoPriceStore.getUnifiedState(token);
    if (state) {
      optionPrice = state->ltp;
      bidPrice = state->bids[0].price;
      askPrice = state->asks[0].price;
    }
  } else if (exchangeSegment == 12) { // BSEFO
    const auto *state = bse::g_bseFoPriceStore.getUnifiedState(token);
    if (state) {
      optionPrice = state->ltp;
      bidPrice = state->bids[0].price;
      askPrice = state->asks[0].price;
    }
  }

  if (optionPrice <= 0 && bidPrice <= 0 && askPrice <= 0) {
    qDebug() << "[GreeksDebug] No market data for token:" << token 
             << "LTP:" << optionPrice << "Bid:" << bidPrice << "Ask:" << askPrice;
    emit calculationFailed(token, exchangeSegment, "No market data available");
    return result;
  }

  // Step 6: Get underlying price
  double underlyingPrice = 0.0;

  // Try future-based pricing if configured
  if (m_config.basePriceMode == "future") {
    // Get future token for the underlying symbol
    uint32_t futureToken = 0;
    // TODO: Implement getNextExpiryFutureToken in RepositoryManager
    // futureToken = m_repoManager->getNextExpiryFutureToken(contract->name, exchangeSegment);
    if (futureToken > 0) {
      underlyingPrice = getUnderlyingPrice(futureToken, exchangeSegment);
    }
  }

  // Fallback to cash/spot pricing
  if (underlyingPrice <= 0) {
    if (contract->assetToken > 0) {
      // Stock options: use assetToken directly
      underlyingPrice = getUnderlyingPrice(static_cast<uint32_t>(contract->assetToken), exchangeSegment);
    } else if (!contract->name.isEmpty()) {
      // Index options (NIFTY, BANKNIFTY): assetToken is -1, use symbol lookup
      uint32_t assetToken = m_repoManager->getAssetTokenForSymbol(contract->name);
      if (assetToken > 0) {
        underlyingPrice = getUnderlyingPrice(assetToken, exchangeSegment);
      }
    }
  }

  if (underlyingPrice <= 0) {
    qDebug() << "[GreeksDebug] Underlying price not available for token:" << token 
             << "assetToken:" << contract->assetToken 
             << "symbol:" << contract->name;
    emit calculationFailed(token, exchangeSegment, "Underlying price not available");
    return result;
  }
  
  qDebug() << "[GreeksDebug] Successfully got underlying price:" << underlyingPrice 
           << "for symbol:" << contract->name;

  // Step 7: Register underlying mapping
  if (!m_cache.contains(token) && contract->assetToken > 0) {
    m_underlyingToOptions.insert(static_cast<uint32_t>(contract->assetToken), token);
  }

  // Step 8: Get strike and expiry from contract
  double strikePrice = contract->strikePrice;
  QString expiryDate = contract->expiryDate;
  bool isCall = (contract->optionType == "CE");

  // Step 9: Get time to expiry (pre-calculated or fallback)
  double T = contract->timeToExpiry;


  // Store input values
  result.spotPrice = underlyingPrice;
  result.strikePrice = strikePrice;
  result.timeToExpiry = T;
  result.optionPrice = optionPrice;

  // Step 10: Calculate Implied Volatility
  // Use cached IV as initial guess for faster convergence
  double usedIV = 0.0;
  double ivInitialGuess = m_config.ivInitialGuess;
  
  // Check if we have cached IV for this token
  auto cachedIt = m_cache.find(token);
  if (cachedIt != m_cache.end() && cachedIt.value().result.impliedVolatility > 0) {
    ivInitialGuess = cachedIt.value().result.impliedVolatility;
  }

  if (optionPrice > 0) {
    IVResult ivResult = IVCalculator::calculate(
        optionPrice, underlyingPrice, strikePrice, T, m_config.riskFreeRate,
        isCall, ivInitialGuess, m_config.ivTolerance,
        m_config.ivMaxIterations);
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
      qDebug() << "[GreeksCalculationService] IV convergence failed for token:"
               << token << "after" << result.ivIterations << "iterations";
    }
  }

  // Step 11: Calculate Greeks using Black-Scholes
  if (result.impliedVolatility > 0) {
    OptionGreeks greeks = GreeksCalculator::calculate(
        underlyingPrice, strikePrice, T, m_config.riskFreeRate,
        usedIV, isCall);

    result.delta = greeks.delta;
    result.gamma = greeks.gamma;
    result.vega = greeks.vega;
    result.theta = greeks.theta;
    result.rho = greeks.rho;
    result.theoreticalPrice = greeks.price;
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
  qDebug() << "[GreeksService] onPriceUpdate called: Token:" << token 
           << "LTP:" << ltp << "Seg:" << exchangeSegment;
  
  if (!m_config.enabled || !m_config.autoCalculate) {
    qDebug() << "[GreeksService] SKIPPED: enabled=" << m_config.enabled 
             << "autoCalculate=" << m_config.autoCalculate;
    return;
  }

  auto it = m_cache.find(token);
  if (it != m_cache.end()) {
    int64_t now = QDateTime::currentMSecsSinceEpoch();
    
    // Check throttle and price change
    if (now - it.value().lastCalculationTime < m_config.throttleMs) {
      qDebug() << "[GreeksService] THROTTLED: Token:" << token 
               << "Elapsed:" << (now - it.value().lastCalculationTime) << "ms";
      return;
    }
    
    double priceDiff = std::abs(ltp - it.value().lastPrice) / it.value().lastPrice;
    if (priceDiff < 0.001) {
      qDebug() << "[GreeksService] PRICE_CHANGE_TOO_SMALL: Token:" << token 
               << "Diff:" << (priceDiff * 100) << "%";
      return;
    }
  } else {
    qDebug() << "[GreeksService] Token not in cache (first time), will calculate:" << token;
  }

  qDebug() << "[GreeksService] CALLING calculateForToken for:" << token;
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

  for (uint32_t token : optionTokens) {
    // We need the exchange segment to calculate
    auto it = m_cache.find(token);
    if (it != m_cache.end()) {

      // HYBRID THROTTLING LOGIC
      // 1. If Liquid (Traded recently): Update immediately
      // 2. If Illiquid: Skip (will be caught by processIlliquidUpdates timer)

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

  // Look up by symbol for index options (e.g., NIFTY)
  if (underlyingToken <= 0 && !contract->name.isEmpty()) {
    underlyingToken = m_repoManager->getAssetTokenForSymbol(contract->name);
  }

  if (underlyingToken <= 0) {
    return 0.0;
  }

  if (exchangeSegment == 2) { // NSEFO
    // Try futures price first
    const auto *futureState = nsefo::g_nseFoPriceStore.getUnifiedState(
        static_cast<uint32_t>(underlyingToken));
    if (futureState && futureState->ltp > 0) {
      return futureState->ltp;
    }

    // Fallback to spot price (handles equities and indices)
    double price = nsecm::getGenericLtp(static_cast<uint32_t>(underlyingToken));
    if (price > 0) {
      return price;
    }
  } else if (exchangeSegment == 12) { // BSEFO
    // Try futures first
    const auto *futureState = bse::g_bseFoPriceStore.getUnifiedState(
        static_cast<uint32_t>(underlyingToken));
    if (futureState && futureState->ltp > 0) {
      return futureState->ltp;
    }

    // Fallback to cash market
    const auto *cashState = bse::g_bseCmPriceStore.getUnifiedState(
        static_cast<uint32_t>(underlyingToken));
    if (cashState && cashState->ltp > 0) {
      return cashState->ltp;
    }
  }

  return 0.0;
}



bool GreeksCalculationService::isOption(int instrumentType) {
  return instrumentType == 2; // 2 = Option in NSE/BSE
}

bool GreeksCalculationService::shouldRecalculate(
    uint32_t token, double currentPrice, double currentUnderlyingPrice) {
  auto it = m_cache.find(token);
  if (it == m_cache.end()) {
    return true; // Not cached, should calculate
  }

  int64_t now = QDateTime::currentMSecsSinceEpoch();
  int64_t elapsed = now - it.value().lastCalculationTime;

  // Time-based throttle
  if (elapsed < m_config.throttleMs) {
    return false;
  }

  // Price change threshold (0.1%)
  double priceChange =
      std::abs(currentPrice - it.value().lastPrice) / it.value().lastPrice;
  double underlyingChange =
      std::abs(currentUnderlyingPrice - it.value().lastUnderlyingPrice) /
      it.value().lastUnderlyingPrice;

  if (priceChange < 0.001 && underlyingChange < 0.001) {
    return false; // No significant change
  }

  return true;
}

void GreeksCalculationService::loadNSEHolidays() {
  // TODO: Load NSE market holidays from file or API
  // For now, this is a stub implementation
  qDebug() << "[GreeksCalculation] loadNSEHolidays() - stub implementation";
}
