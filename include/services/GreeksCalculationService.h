#ifndef GREEKS_CALCULATION_SERVICE_H
#define GREEKS_CALCULATION_SERVICE_H

#include <QObject>
#include <QHash>
#include <QTimer>
#include <QDateTime>
#include <QDate>
#include <QSet>
#include <optional>
#include <cstdint>

class NSEFORepository;
class NSECMRepository;
class BSEFORepository;
class RepositoryManager;

/**
 * @brief Result of Greeks calculation for an option contract
 */
struct GreeksResult {
    uint32_t token = 0;
    int exchangeSegment = 0;
    
    // Calculated values
    double impliedVolatility = 0.0;  // IV (decimal, e.g., 0.18 = 18%)
    double bidIV = 0.0;
    double askIV = 0.0;
    double delta = 0.0;
    double gamma = 0.0;
    double vega = 0.0;              // Per 1% IV change
    double theta = 0.0;             // Daily decay
    double rho = 0.0;
    double theoreticalPrice = 0.0;
    
    // Calculation metadata
    bool ivConverged = false;
    int ivIterations = 0;
    int64_t calculationTimestamp = 0;  // Unix timestamp (ms)
    
    // Input values used
    double spotPrice = 0.0;
    double strikePrice = 0.0;
    double timeToExpiry = 0.0;      // In years
    double optionPrice = 0.0;       // Market price used
    
    GreeksResult() = default;
};

/**
 * @brief Validation result for Greeks calculation
 * 
 * Provides detailed error information when Greeks calculation fails.
 * This helps identify exactly which validation step failed.
 */
struct GreeksValidationResult {
    bool valid = false;
    QString errorMessage;
    GreeksResult result;  // Only populated if valid==true
    
    // Validation details
    bool contractFound = false;
    bool isOption = false;
    bool hasValidAssetToken = false;
    bool hasUnderlyingPrice = false;
    bool notExpired = false;
    bool marketPriceValid = false;
    
    GreeksValidationResult() = default;
};

/**
 * @brief Configuration for Greeks calculation
 */
struct GreeksConfig {
    // Risk-free rate (RBI repo rate as of Jan 2026: 6.5%)
    double riskFreeRate = 0.065;
    
    // Dividend yield (0 for indices, non-zero for stocks)
    double dividendYield = 0.0;
    
    // Enable auto-calculation on price updates
    bool autoCalculate = true;
    
    // Throttle (ms between recalculations per token)
    int throttleMs = 1000;
    
    // IV solver settings
    double ivInitialGuess = 0.20;
    double ivTolerance = 1e-6;
    int ivMaxIterations = 100;
    
    // Time tick interval (seconds) for theta decay updates
    int timeTickIntervalSec = 60;
    
    // Background timer interval (seconds) for illiquid options (no trade > 30s)
    int illiquidUpdateIntervalSec = 30;
    
    // Threshold (seconds) to consider an option illiquid
    int illiquidThresholdSec = 30;
    
    // Enable/disable the service
    bool enabled = true;
    
    // Base price mode: "cash" (spot) or "future" (next expiry future)
    QString basePriceMode = "cash";
    
    // Calculate Greeks on every feed update (bypass throttling)
    // When true: Greeks calculated on every option price update AND every underlying update
    // When false: Uses hybrid throttling (immediate for liquid, timer for illiquid)
    bool calculateOnEveryFeed = false;
    
    GreeksConfig() = default;
};

/**
 * @brief Service to orchestrate IV and Greeks calculations
 * 
 * This service:
 * - Listens to price updates from UDP broadcast
 * - Calculates IV using Newton-Raphson method
 * - Calculates all Greeks using Black-Scholes
 * - Caches results with TTL to avoid excessive recalculation
 * - Supports batch processing for option chains
 * 
 * Usage:
 *   auto& service = GreeksCalculationService::instance();
 *   service.initialize(config);
 *   connect(&service, &GreeksCalculationService::greeksCalculated, ...);
 */
class GreeksCalculationService : public QObject {
    Q_OBJECT

public:
    static GreeksCalculationService& instance();
    
    /**
     * @brief Initialize the service with configuration
     */
    void initialize(const GreeksConfig& config);
    
    /**
     * @brief Load configuration from config.ini
     */
    void loadConfiguration();
    
    /**
     * @brief Set the repository manager for data access
     */
    void setRepositoryManager(RepositoryManager* repoManager);
    
    /**
     * @brief Calculate Greeks for a single option token
     * 
     * @param token Instrument token
     * @param exchangeSegment Exchange segment (NSEFO=2, BSEFO=12)
     * @return GreeksResult with calculated values
     */
    GreeksResult calculateForToken(uint32_t token, int exchangeSegment);
    
    /**
     * @brief Validate inputs before Greeks calculation
     * 
     * Performs comprehensive validation of all inputs required for Greeks:
     * - Contract existence
     * - Instrument type (must be option)
     * - Asset token validity
     * - Underlying price availability
     * - Expiration date (not expired)
     * - Market price validity
     * 
     * @param token Instrument token
     * @param exchangeSegment Exchange segment (NSEFO=2, BSEFO=12)
     * @param optionPrice Market price to validate
     * @return GreeksValidationResult with detailed validation status
     */
    GreeksValidationResult validateGreeksInputs(uint32_t token, int exchangeSegment, double optionPrice) const;
    
    /**
     * @brief Get cached Greeks result
     * 
     * @param token Instrument token
     * @return Cached result if available, std::nullopt otherwise
     */
    std::optional<GreeksResult> getCachedGreeks(uint32_t token) const;
    
    /**
     * @brief Force recalculation for all cached tokens
     * 
     * Use this for time-based updates (theta decay)
     */
    void forceRecalculateAll();
    
    /**
     * @brief Clear all cached Greeks
     */
    void clearCache();
    
    /**
     * @brief Get current configuration
     */
    const GreeksConfig& config() const { return m_config; }
    
    /**
     * @brief Update risk-free rate
     */
    void setRiskFreeRate(double rate);
    
    /**
     * @brief Check if service is enabled
     */
    bool isEnabled() const { return m_config.enabled; }

signals:
    /**
     * @brief Emitted when Greeks are calculated for a token
     */
    void greeksCalculated(uint32_t token, int exchangeSegment, const GreeksResult& result);
    
    /**
     * @brief Emitted when calculation fails
     */
    void calculationFailed(uint32_t token, int exchangeSegment, const QString& reason);
    
    /**
     * @brief Emitted when configuration changes
     */
    void configurationChanged();

public slots:
    /**
     * @brief Handle price update from UDP broadcast
     * 
     * Called when option price changes. Will calculate Greeks
     * if auto-calculate is enabled and throttle allows.
     */
    void onPriceUpdate(uint32_t token, double ltp, int exchangeSegment);
    
    /**
     * @brief Handle underlying price update
     * 
     * When underlying price changes, may need to recalculate Greeks
     * for associated options.
     */
    void onUnderlyingPriceUpdate(uint32_t underlyingToken, double ltp, int exchangeSegment);

private slots:
    /**
     * @brief Time tick handler for theta decay updates
     */
    void onTimeTick();
    
    /**
     * @brief Background timer handler for illiquid options
     * Updates Greeks for options not traded recently using stored IV + new Spot
     */
    void processIlliquidUpdates();

private:
    explicit GreeksCalculationService(QObject* parent = nullptr);
    ~GreeksCalculationService() override;
    
    // Disable copy
    GreeksCalculationService(const GreeksCalculationService&) = delete;
    GreeksCalculationService& operator=(const GreeksCalculationService&) = delete;
    
    /**
     * @brief Get underlying price for an option
     * 
     * Resolves the underlying asset token and fetches current price
     * from the appropriate price store.
     */
    double getUnderlyingPrice(uint32_t optionToken, int exchangeSegment);
    
    /**
     * @brief Calculate time to expiry in years
     * 
     * @param expiryDate Expiry date string (DDMMMYYYY format)
     * @return Time to expiry in years
     */
    double calculateTimeToExpiry(const QString& expiryDate);
 
    /**
     * @brief Calculate time to expiry in years
     * 
     * @param expiryDate Parsed expiry date
     * @return Time to expiry in years
     */
    double calculateTimeToExpiry(const QDate& expiryDate);
    
    /**
     * @brief Calculate trading days between two dates
     * 
     * Excludes weekends and NSE holidays.
     */
    int calculateTradingDays(const QDate& start, const QDate& end);
    
    /**
     * @brief Check if a date is an NSE trading day
     */
    bool isNSETradingDay(const QDate& date);
    
    /**
     * @brief Check if throttle allows recalculation
     */
    bool shouldRecalculate(uint32_t token, double currentPrice, double currentUnderlyingPrice);
    
    /**
     * @brief Check if a contract is an option
     */
    bool isOption(int instrumentType);
    
    // Cache structure
    struct CacheEntry {
        GreeksResult result;
        int64_t lastCalculationTime = 0;
        int64_t lastTradeTimestamp = 0;  // Last time Option Price (LTP) changed
        double lastPrice = 0.0;
        double lastUnderlyingPrice = 0.0;
    };
    
    GreeksConfig m_config;
    QHash<uint32_t, CacheEntry> m_cache;
    
    // Map of Underlying Token -> List of Option Tokens
    // Used to trigger recalculation when underlying price changes
    QMultiHash<uint32_t, uint32_t> m_underlyingToOptions;
    
    QTimer* m_timeTickTimer = nullptr;
    QTimer* m_illiquidUpdateTimer = nullptr;
    RepositoryManager* m_repoManager = nullptr;
    
    // NSE Holiday set (loaded at initialization)
    QSet<QDate> m_nseHolidays;
    
    void loadNSEHolidays();
};

#endif // GREEKS_CALCULATION_SERVICE_H
