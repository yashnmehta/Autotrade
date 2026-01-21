#ifndef GREEKS_H
#define GREEKS_H

#include <cmath>

/**
 * @brief Result structure containing all Option Greeks
 * 
 * All values are per-contract (not per-lot).
 * - Delta: Change in option price for ₹1 change in underlying
 * - Gamma: Rate of change of Delta
 * - Theta: Daily time decay (negative for long options)
 * - Vega: Change in option price for 1% change in IV
 * - Rho: Change in option price for 1% change in interest rate
 * - Price: Theoretical Black-Scholes price
 */
struct OptionGreeks
{
    double delta;   // Range: [0,1] for Call, [-1,0] for Put
    double gamma;   // Always positive
    double theta;   // Daily decay (usually negative)
    double vega;    // Per 1% IV change (always positive)
    double rho;     // Per 1% rate change
    double price;   // Theoretical Black-Scholes price
    
    OptionGreeks()
        : delta(0.0)
        , gamma(0.0)
        , theta(0.0)
        , vega(0.0)
        , rho(0.0)
        , price(0.0)
    {}
    
    OptionGreeks(double d, double g, double t, double v, double r, double p)
        : delta(d)
        , gamma(g)
        , theta(t)
        , vega(v)
        , rho(r)
        , price(p)
    {}
};

/**
 * @brief Structured input for Greeks calculation
 * 
 * Use this for cleaner API calls instead of multiple parameters.
 */
struct GreeksInput
{
    double spotPrice;       // S - Current underlying price
    double strikePrice;     // K - Strike price
    double timeToExpiry;    // T - Time to expiry in years (e.g., 30/365)
    double riskFreeRate;    // r - Risk-free rate (decimal, e.g., 0.065)
    double volatility;      // σ - Implied volatility (decimal, e.g., 0.20)
    bool isCall;            // true = Call, false = Put
    
    GreeksInput()
        : spotPrice(0.0)
        , strikePrice(0.0)
        , timeToExpiry(0.0)
        , riskFreeRate(0.065)
        , volatility(0.20)
        , isCall(true)
    {}
    
    GreeksInput(double s, double k, double t, double r, double sigma, bool call)
        : spotPrice(s)
        , strikePrice(k)
        , timeToExpiry(t)
        , riskFreeRate(r)
        , volatility(sigma)
        , isCall(call)
    {}
};

/**
 * @brief Black-Scholes Greeks Calculator for European Options
 * 
 * Implements the standard Black-Scholes model which assumes:
 * - European-style exercise (can only exercise at expiry)
 * - Log-normal distribution of underlying prices
 * - Constant volatility and interest rate
 * - No dividends (use adjusted spot for dividend-paying stocks)
 * 
 * This is the industry-standard model used by NSE, BSE, and global exchanges.
 */
class GreeksCalculator
{
public:
    /**
     * @brief Calculate Black-Scholes Greeks for a European Option
     *
     * @param S Current Stock/Index Price
     * @param K Strike Price
     * @param T Time to Expiry (in years, e.g., 30/365 for 30 days)
     * @param r Risk-free Interest Rate (decimal, e.g., 0.065 for 6.5%)
     * @param sigma Implied Volatility (decimal, e.g., 0.20 for 20%)
     * @param isCall True for Call option, False for Put option
     * @return OptionGreeks struct with all Greeks and theoretical price
     */
    static OptionGreeks calculate(double S, double K, double T, double r, double sigma, bool isCall);
    
    /**
     * @brief Calculate Greeks using structured input
     * 
     * @param input GreeksInput struct with all parameters
     * @return OptionGreeks struct with all Greeks and theoretical price
     */
    static OptionGreeks calculate(const GreeksInput& input);
    
    /**
     * @brief Calculate only the theoretical price (faster, no Greeks)
     * 
     * Use this when you only need the Black-Scholes price without Greeks.
     * 
     * @param S Current Stock/Index Price
     * @param K Strike Price
     * @param T Time to Expiry (in years)
     * @param r Risk-free Interest Rate (decimal)
     * @param sigma Implied Volatility (decimal)
     * @param isCall True for Call, False for Put
     * @return Theoretical Black-Scholes price
     */
    static double calculateTheoPrice(double S, double K, double T, double r, double sigma, bool isCall);
    
    /**
     * @brief Get the normal CDF value (public for IV solver)
     * @param value Input value
     * @return Cumulative probability N(value)
     */
    static double normalCDF(double value);
    
    /**
     * @brief Get the normal PDF value (public for IV solver)
     * @param value Input value
     * @return Probability density N'(value)
     */
    static double normalPDF(double value);
    
    /**
     * @brief Calculate d1 parameter in Black-Scholes formula
     */
    static double calculateD1(double S, double K, double T, double r, double sigma);
    
    /**
     * @brief Calculate d2 parameter in Black-Scholes formula
     */
    static double calculateD2(double d1, double sigma, double T);
};

#endif // GREEKS_H
