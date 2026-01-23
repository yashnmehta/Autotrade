#ifndef IV_CALCULATOR_H
#define IV_CALCULATOR_H

#include <cmath>
#include <algorithm>

/**
 * @brief Result of Implied Volatility calculation
 */
struct IVResult {
    double impliedVolatility;   // Calculated IV (decimal, e.g., 0.20 = 20%)
    int iterations;             // Number of iterations to converge
    bool converged;             // True if converged successfully
    double finalError;          // Final price error (market - theoretical)
    
    IVResult()
        : impliedVolatility(0.0)
        , iterations(0)
        , converged(false)
        , finalError(0.0)
    {}
    
    IVResult(double iv, int iter, bool conv, double err)
        : impliedVolatility(iv)
        , iterations(iter)
        , converged(conv)
        , finalError(err)
    {}
};

/**
 * @brief Implied Volatility Calculator using Black-Scholes model
 * 
 * This class provides Newton-Raphson based IV calculation, which is the
 * industry-standard approach used by exchanges and trading platforms.
 * 
 * Newton-Raphson Method:
 *   σ_{n+1} = σ_n - (BS_Price(σ_n) - Market_Price) / Vega(σ_n)
 * 
 * Typically converges in 3-5 iterations for normal options.
 */
class IVCalculator {
public:
    /**
     * @brief Calculate Implied Volatility using Newton-Raphson method
     * 
     * @param marketPrice Market price of the option
     * @param S Current spot/underlying price
     * @param K Strike price
     * @param T Time to expiry (in years, e.g., 30/365 for 30 days)
     * @param r Risk-free interest rate (decimal, e.g., 0.065 for 6.5%)
     * @param isCall True for Call option, False for Put option
     * @param initialGuess Starting IV guess (default 0.20 = 20%)
     * @param tolerance Convergence tolerance (default 1e-6)
     * @param maxIterations Maximum iterations (default 100)
     * @return IVResult with IV and convergence status
     */
    static IVResult calculate(
        double marketPrice,
        double S,
        double K,
        double T,
        double r,
        bool isCall,
        double initialGuess = 0.20,
        double tolerance = 1e-6,
        int maxIterations = 100
    );
    
    /**
     * @brief Calculate IV using Brent's method (fallback for edge cases)
     * 
     * More robust but slower than Newton-Raphson.
     * Use when Newton-Raphson fails to converge.
     * 
     * @param marketPrice Market price of the option
     * @param S Current spot/underlying price
     * @param K Strike price
     * @param T Time to expiry (in years)
     * @param r Risk-free interest rate (decimal)
     * @param isCall True for Call option, False for Put option
     * @return IVResult with IV and convergence status
     */
    static IVResult calculateBrent(
        double marketPrice,
        double S,
        double K,
        double T,
        double r,
        bool isCall
    );
    
    /**
     * @brief Check if IV calculation is feasible for given inputs
     * 
     * Returns false for:
     * - Expired options (T <= 0)
     * - Market price below intrinsic value
     * - Zero or negative prices
     * 
     * @param marketPrice Market price of the option
     * @param S Current spot/underlying price
     * @param K Strike price
     * @param T Time to expiry (in years)
     * @param r Risk-free interest rate
     * @param isCall True for Call option, False for Put option
     * @return true if IV calculation is feasible
     */
    static bool isCalculable(
        double marketPrice,
        double S,
        double K,
        double T,
        double r,
        bool isCall
    );
    
    /**
     * @brief Calculate intrinsic value of an option
     * 
     * @param S Spot price
     * @param K Strike price
     * @param isCall True for Call, False for Put
     * @return Intrinsic value (max(S-K, 0) for Call, max(K-S, 0) for Put)
     */
    static double intrinsicValue(double S, double K, bool isCall);
    
    /**
     * @brief Get intelligent initial guess based on moneyness
     * 
     * @param S Spot price
     * @param K Strike price
     * @param T Time to expiry
     * @return Suggested initial volatility guess
     */
    static double initialGuessFromMoneyness(double S, double K, double T);
    
    // Constants
    static constexpr double MIN_VOLATILITY = 0.001;   // 0.1% minimum
    static constexpr double MAX_VOLATILITY = 5.0;     // 500% maximum
    static constexpr double MIN_VEGA_THRESHOLD = 1e-10;
};

#endif // IV_CALCULATOR_H
