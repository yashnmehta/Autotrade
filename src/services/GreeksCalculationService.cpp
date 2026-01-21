#include "services/GreeksCalculationService.h"
#include "repository/IVCalculator.h"
#include "repository/Greeks.h"
#include "repository/RepositoryManager.h"
#include "repository/ContractData.h"
#include "nsefo_price_store.h"
#include "nsecm_price_store.h"
#include "bse_price_store.h"

#include <QSettings>
#include <QDate>
#include <QTime>
#include <QDebug>
#include <cmath>

// ============================================================================
// SINGLETON INSTANCE
// ============================================================================

GreeksCalculationService& GreeksCalculationService::instance() {
    static GreeksCalculationService inst;
    return inst;
}

GreeksCalculationService::GreeksCalculationService(QObject* parent)
    : QObject(parent)
    , m_timeTickTimer(new QTimer(this))
    , m_illiquidUpdateTimer(new QTimer(this))
{
    connect(m_timeTickTimer, &QTimer::timeout, this, &GreeksCalculationService::onTimeTick);
    connect(m_illiquidUpdateTimer, &QTimer::timeout, this, &GreeksCalculationService::processIlliquidUpdates);
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

void GreeksCalculationService::initialize(const GreeksConfig& config) {
    m_config = config;
    
    // Start time tick timer
    if (m_config.enabled && m_config.timeTickIntervalSec > 0) {
        m_timeTickTimer->start(m_config.timeTickIntervalSec * 1000);
    }
    
    // Start illiquid update timer
    if (m_config.enabled && m_config.illiquidUpdateIntervalSec > 0) {
        m_illiquidUpdateTimer->start(m_config.illiquidUpdateIntervalSec * 1000);
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
    m_config.timeTickIntervalSec = settings.value("time_tick_interval", 60).toInt();
    m_config.illiquidUpdateIntervalSec = settings.value("illiquid_update_interval", 30).toInt();
    m_config.illiquidThresholdSec = settings.value("illiquid_threshold", 30).toInt();
    
    settings.endGroup();
    
    qDebug() << "[GreeksCalculationService] Configuration loaded from config.ini";
    
    // Apply configuration
    initialize(m_config);
}

void GreeksCalculationService::setRepositoryManager(RepositoryManager* repoManager) {
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

GreeksResult GreeksCalculationService::calculateForToken(uint32_t token, int exchangeSegment) {
    GreeksResult result;
    result.token = token;
    result.exchangeSegment = exchangeSegment;
    result.calculationTimestamp = QDateTime::currentMSecsSinceEpoch();
    
    if (!m_config.enabled) {
        emit calculationFailed(token, exchangeSegment, "Service disabled");
        return result;
    }
    
    if (!m_repoManager) {
        emit calculationFailed(token, exchangeSegment, "Repository manager not set");
        return result;
    }
    
    // Get contract data using unified API
    // exchangeSegment: 2=NSEFO, 4=BSEFO
    const ContractData* contract = m_repoManager->getContractByToken(exchangeSegment, token);
    
    if (!contract) {
        emit calculationFailed(token, exchangeSegment, "Contract not found");
        return result;
    }
    
    // Check if it's an option
    if (!isOption(contract->instrumentType)) {
        emit calculationFailed(token, exchangeSegment, "Not an option contract");
        return result;
    }
    
    // Get option price from price store
    double optionPrice = 0.0;
    if (exchangeSegment == 2) { // NSEFO
        const auto* state = nsefo::g_nseFoPriceStore.getUnifiedState(token);
        if (state) {
            optionPrice = state->ltp;
        }
    } else if (exchangeSegment == 4) { // BSEFO
        const auto* state = bse::g_bseFoPriceStore.getUnifiedState(token);
        if (state) {
            optionPrice = state->ltp;
        }
    }
    
    if (optionPrice <= 0) {
        emit calculationFailed(token, exchangeSegment, "No market price available");
        return result;
    }
    
    // Get Bid/Ask prices for skew Calculation
    double bidPrice = 0.0;
    double askPrice = 0.0;
    
    if (exchangeSegment == 2) {
         if (auto state = nsefo::g_nseFoPriceStore.getUnifiedState(token)) {
             bidPrice = state->bids[0].price;
             askPrice = state->asks[0].price;
         }
    } else if (exchangeSegment == 4) {
         if (auto state = bse::g_bseFoPriceStore.getUnifiedState(token)) {
             bidPrice = state->bids[0].price;
             askPrice = state->asks[0].price;
         }
    }
    
    // Get underlying price
    double underlyingPrice = getUnderlyingPrice(token, exchangeSegment);
    if (underlyingPrice <= 0) {
        emit calculationFailed(token, exchangeSegment, "Underlying price not available");
        return result;
    }
    
    // Register mapping if new (for onUnderlyingPriceUpdate)
    if (!m_cache.contains(token) && contract->assetToken > 0) {
        m_underlyingToOptions.insert(static_cast<uint32_t>(contract->assetToken), token);
    }
    
    // Get strike and expiry from contract
    double strikePrice = contract->strikePrice;
    QString expiryDate = contract->expiryDate;
    bool isCall = (contract->optionType == "CE");
    
    // Calculate time to expiry
    double T = calculateTimeToExpiry(expiryDate);
    if (T <= 0) {
        emit calculationFailed(token, exchangeSegment, "Option expired");
        return result;
    }
    
    // Store input values
    result.spotPrice = underlyingPrice;
    result.strikePrice = strikePrice;
    result.timeToExpiry = T;
    result.optionPrice = optionPrice;
    
    // -------------------------------------------------------------
    // 1. Calculate Implied Volatility (LTP IV) - Only on Option Trade
    // -------------------------------------------------------------
    // Check if we should use cached IV (if this was triggered by Spot/Timer, not Option Trade)
    // We infer this: if cached price == current price, reuse cached IV
    double usedIV = 0.0;
    bool usingCachedIV = false;
    
    if (m_cache.contains(token)) {
        const auto& entry = m_cache[token];
        // If option price hasn't changed, keep using the Snapshot IV
        if (std::abs(entry.lastPrice - optionPrice) < 0.0001 && entry.result.impliedVolatility > 0) {
            usedIV = entry.result.impliedVolatility;
            result.impliedVolatility = usedIV;
            result.bidIV = entry.result.bidIV;
            result.askIV = entry.result.askIV;
            result.ivConverged = entry.result.ivConverged;
            result.ivIterations = entry.result.ivIterations;
            usingCachedIV = true;
        }
    }
    
    if (!usingCachedIV) {
        // Calculate LTP IV
        IVResult ivResult = IVCalculator::calculate(
            optionPrice, underlyingPrice, strikePrice, T,
            m_config.riskFreeRate, isCall,
            m_config.ivInitialGuess, m_config.ivTolerance, m_config.ivMaxIterations
        );
        usedIV = ivResult.impliedVolatility;
        result.impliedVolatility = usedIV;
        result.ivConverged = ivResult.converged;
        result.ivIterations = ivResult.iterations;
        
        // Calculate Bid IV
        if (bidPrice > 0) {
            IVResult bidRes = IVCalculator::calculate(
                bidPrice, underlyingPrice, strikePrice, T,
                m_config.riskFreeRate, isCall, usedIV > 0 ? usedIV : m_config.ivInitialGuess
            );
            result.bidIV = bidRes.impliedVolatility;
        }
        
        // Calculate Ask IV
        if (askPrice > 0) {
            IVResult askRes = IVCalculator::calculate(
                askPrice, underlyingPrice, strikePrice, T,
                m_config.riskFreeRate, isCall, usedIV > 0 ? usedIV : m_config.ivInitialGuess
            );
            result.askIV = askRes.impliedVolatility;
        }
    }
    

    if (!result.ivConverged && !usingCachedIV) {
        qDebug() << "[GreeksCalculationService] IV convergence failed for token:" << token
                 << "after" << result.ivIterations << "iterations";
    }
    
    // Calculate Greeks using the calculated IV
    if (result.impliedVolatility > 0) {
        OptionGreeks greeks = GreeksCalculator::calculate(
            underlyingPrice,
            strikePrice,
            T,
            m_config.riskFreeRate,
            usedIV, // Use the decided IV (Cached or New)
            isCall
        );
        
        result.delta = greeks.delta;
        result.gamma = greeks.gamma;
        result.vega = greeks.vega;
        result.theta = greeks.theta;
        result.rho = greeks.rho;
        result.theoreticalPrice = greeks.price;
    }
    
    // Update cache
    CacheEntry entry;
    entry.result = result;
    entry.lastCalculationTime = result.calculationTimestamp;
    entry.lastPrice = optionPrice;
    entry.lastUnderlyingPrice = underlyingPrice;
    
    // If calculating fresh IV, update Last Trade Timestamp
    if (!usingCachedIV) {
        entry.lastTradeTimestamp = QDateTime::currentMSecsSinceEpoch();
    } else if (m_cache.contains(token)) {
        // Preserve original trade timestamp
        entry.lastTradeTimestamp = m_cache[token].lastTradeTimestamp;
    }
    
    m_cache[token] = entry;
    
    // Emit signal
    emit greeksCalculated(token, exchangeSegment, result);
    
    return result;
}

// ============================================================================
// CACHE MANAGEMENT
// ============================================================================

std::optional<GreeksResult> GreeksCalculationService::getCachedGreeks(uint32_t token) const {
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

void GreeksCalculationService::onPriceUpdate(uint32_t token, double ltp, int exchangeSegment) {
    if (!m_config.enabled || !m_config.autoCalculate) {
        return;
    }
    
    // Check throttle
    auto it = m_cache.find(token);
    if (it != m_cache.end()) {
        int64_t now = QDateTime::currentMSecsSinceEpoch();
        int64_t elapsed = now - it.value().lastCalculationTime;
        
        if (elapsed < m_config.throttleMs) {
            return; // Throttled
        }
        
        // Check if price changed significantly (0.1%)
        double priceDiff = std::abs(ltp - it.value().lastPrice) / it.value().lastPrice;
        if (priceDiff < 0.001) {
            return; // No significant change
        }
    }
    
    // Calculate Greeks
    calculateForToken(token, exchangeSegment);
}

void GreeksCalculationService::onUnderlyingPriceUpdate(uint32_t underlyingToken, double ltp, int exchangeSegment) {
    if (!m_config.enabled || !m_config.autoCalculate) return;
    
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
    if (!m_config.enabled || !m_config.autoCalculate) return;
    
    int64_t now = QDateTime::currentMSecsSinceEpoch();
    int cutoff = m_config.illiquidThresholdSec * 1000;
    
    // Iterate all cached tokens
    auto it = m_cache.begin();
    while (it != m_cache.end()) {
        int64_t lastTradeTime = it.value().lastTradeTimestamp;
        
        // Process only if ILLIQUID (> 30s since last option trade)
        if ((now - lastTradeTime) > cutoff) {
             
             // Check if underlying moved significantly since last calc to avoid useless work
             // (Optional optimization: reusing shouldRecalculate logic implicitly via calculateForToken)
             
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

double GreeksCalculationService::getUnderlyingPrice(uint32_t optionToken, int exchangeSegment) {
    if (!m_repoManager) {
        return 0.0;
    }
    
    // Get contract data using unified API
    const ContractData* contract = m_repoManager->getContractByToken(exchangeSegment, optionToken);
    
    if (!contract || contract->assetToken <= 0) {
        return 0.0;
    }
    
    // First try to get future price (more accurate for options)
    // Then fall back to cash market price
    
    if (exchangeSegment == 2) { // NSEFO
        // Get underlying future from NSEFO
        const auto* futureState = nsefo::g_nseFoPriceStore.getUnifiedState(
            static_cast<uint32_t>(contract->assetToken)
        );
        if (futureState && futureState->ltp > 0) {
            return futureState->ltp;
        }
        
        // Fall back to cash market from NSECM
        const auto* cashState = nsecm::g_nseCmPriceStore.getUnifiedState(
            static_cast<uint32_t>(contract->assetToken)
        );
        if (cashState && cashState->ltp > 0) {
            return cashState->ltp;
        }
    } else if (exchangeSegment == 4) { // BSEFO
        // Get underlying from BSEFO
        const auto* futureState = bse::g_bseFoPriceStore.getUnifiedState(
            static_cast<uint32_t>(contract->assetToken)
        );
        if (futureState && futureState->ltp > 0) {
            return futureState->ltp;
        }
        
        // Fall back to cash market from BSECM
        const auto* cashState = bse::g_bseCmPriceStore.getUnifiedState(
            static_cast<uint32_t>(contract->assetToken)
        );
        if (cashState && cashState->ltp > 0) {
            return cashState->ltp;
        }
    }
    
    return 0.0;
}

double GreeksCalculationService::calculateTimeToExpiry(const QString& expiryDate) {
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
        qWarning() << "[GreeksCalculationService] Failed to parse expiry date:" << expiryDate;
        return 0.0;
    }
    
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
        int tradingSeconds = 6 * 60 * 60; // 6 hours trading day
        intraDayFraction = static_cast<double>(secondsRemaining) / tradingSeconds;
        if (tradingDays > 0) {
            tradingDays--; // Don't double count today
        }
    }
    
    // Convert to years (252 trading days per year in India)
    double T = (tradingDays + intraDayFraction) / 252.0;
    
    // Minimum time to avoid division by zero
    return std::max(T, 0.0001);
}

int GreeksCalculationService::calculateTradingDays(const QDate& start, const QDate& end) {
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

bool GreeksCalculationService::isNSETradingDay(const QDate& date) {
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
        QDate(2026, 1, 26),   // Republic Day
        QDate(2026, 3, 14),   // Holi
        QDate(2026, 3, 30),   // Good Friday
        QDate(2026, 4, 2),    // Ram Navami
        QDate(2026, 4, 14),   // Dr. Ambedkar Jayanti
        QDate(2026, 5, 1),    // Maharashtra Day
        QDate(2026, 8, 15),   // Independence Day
        QDate(2026, 8, 19),   // Janmashtami
        QDate(2026, 10, 2),   // Gandhi Jayanti
        QDate(2026, 10, 24),  // Dussehra
        QDate(2026, 11, 12),  // Diwali
        QDate(2026, 11, 13),  // Diwali (Laxmi Pujan)
        QDate(2026, 11, 14),  // Diwali (Balipratipada)
        QDate(2026, 12, 25),  // Christmas
    };
}

bool GreeksCalculationService::isOption(int instrumentType) {
    return instrumentType == 2; // 2 = Option in NSE/BSE
}

bool GreeksCalculationService::shouldRecalculate(uint32_t token, double currentPrice, double currentUnderlyingPrice) {
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
    double priceChange = std::abs(currentPrice - it.value().lastPrice) / it.value().lastPrice;
    double underlyingChange = std::abs(currentUnderlyingPrice - it.value().lastUnderlyingPrice) / it.value().lastUnderlyingPrice;
    
    if (priceChange < 0.001 && underlyingChange < 0.001) {
        return false; // No significant change
    }
    
    return true;
}
