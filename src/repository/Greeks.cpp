#define _USE_MATH_DEFINES
#include "repository/Greeks.h"
#include <cmath>
#include <algorithm>

// ============================================================================
// STANDARD NORMAL DISTRIBUTION FUNCTIONS
// ============================================================================

// Standard Normal Cumulative Distribution Function
double GreeksCalculator::normalCDF(double value)
{
    return 0.5 * std::erfc(-value * M_SQRT1_2);
}

// Standard Normal Probability Density Function
double GreeksCalculator::normalPDF(double value)
{
    return (1.0 / std::sqrt(2.0 * M_PI)) * std::exp(-0.5 * value * value);
}

// ============================================================================
// BLACK-SCHOLES D1/D2 CALCULATIONS
// ============================================================================

double GreeksCalculator::calculateD1(double S, double K, double T, double r, double sigma)
{
    return (std::log(S / K) + (r + 0.5 * sigma * sigma) * T) / (sigma * std::sqrt(T));
}

double GreeksCalculator::calculateD2(double d1, double sigma, double T)
{
    return d1 - sigma * std::sqrt(T);
}

// ============================================================================
// THEORETICAL PRICE ONLY (NO GREEKS)
// ============================================================================

double GreeksCalculator::calculateTheoPrice(double S, double K, double T, double r, double sigma, bool isCall)
{
    if (T <= 0 || sigma <= 0) {
        // Expired or invalid - return intrinsic value
        if (isCall) {
            return std::max(S - K, 0.0);
        } else {
            return std::max(K - S, 0.0);
        }
    }

    double d1 = calculateD1(S, K, T, r, sigma);
    double d2 = calculateD2(d1, sigma, T);

    double nd1 = normalCDF(d1);
    double nd2 = normalCDF(d2);
    double expRT = std::exp(-r * T);

    if (isCall) {
        return S * nd1 - K * expRT * nd2;
    } else {
        double n_d1 = normalCDF(-d1);
        double n_d2 = normalCDF(-d2);
        return K * expRT * n_d2 - S * n_d1;
    }
}

// ============================================================================
// STRUCTURED INPUT OVERLOAD
// ============================================================================

OptionGreeks GreeksCalculator::calculate(const GreeksInput& input)
{
    return calculate(
        input.spotPrice,
        input.strikePrice,
        input.timeToExpiry,
        input.riskFreeRate,
        input.volatility,
        input.isCall
    );
}

// ============================================================================
// FULL GREEKS CALCULATION
// ============================================================================

OptionGreeks GreeksCalculator::calculate(double S, double K, double T, double r, double sigma, bool isCall)
{
    OptionGreeks greeks;

    if (T <= 0 || sigma <= 0)
    {
        // Expired or invalid - return intrinsic value only
        if (isCall) {
            greeks.price = std::max(S - K, 0.0);
            greeks.delta = (S > K) ? 1.0 : 0.0;
        } else {
            greeks.price = std::max(K - S, 0.0);
            greeks.delta = (K > S) ? -1.0 : 0.0;
        }
        return greeks;
    }

    double d1 = calculateD1(S, K, T, r, sigma);
    double d2 = calculateD2(d1, sigma, T);

    double nd1 = normalCDF(d1);
    double nd2 = normalCDF(d2);
    double nPd1 = normalPDF(d1); // N'(d1)

    double sqrtT = std::sqrt(T);
    double expRT = std::exp(-r * T);

    if (isCall)
    {
        greeks.price = S * nd1 - K * expRT * nd2;
        greeks.delta = nd1;
        greeks.rho = K * T * expRT * nd2;
        greeks.theta = (-S * nPd1 * sigma / (2 * sqrtT)) - (r * K * expRT * nd2);
    }
    else
    {
        // Put Option
        double n_d1 = normalCDF(-d1);
        double n_d2 = normalCDF(-d2);

        greeks.price = K * expRT * n_d2 - S * n_d1;
        greeks.delta = nd1 - 1.0;
        greeks.rho = -K * T * expRT * n_d2;
        greeks.theta = (-S * nPd1 * sigma / (2 * sqrtT)) + (r * K * expRT * n_d2);
    }

    // Gamma and Vega are the same for Calls and Puts
    greeks.gamma = nPd1 / (S * sigma * sqrtT);
    greeks.vega = S * sqrtT * nPd1 / 100.0; // Divided by 100 to get change per 1% vol change

    // Annualize Theta (usually shown as daily decay)
    greeks.theta = greeks.theta / 365.0;

    return greeks;
}
