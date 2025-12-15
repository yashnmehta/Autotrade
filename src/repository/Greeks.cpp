#include "repository/Greeks.h"
#include <cmath>

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

OptionGreeks GreeksCalculator::calculate(double S, double K, double T, double r, double sigma, bool isCall)
{
    OptionGreeks greeks = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};

    if (T <= 0 || sigma <= 0)
    {
        return greeks; // Expired or invalid
    }

    double d1 = (std::log(S / K) + (r + 0.5 * sigma * sigma) * T) / (sigma * std::sqrt(T));
    double d2 = d1 - sigma * std::sqrt(T);

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
