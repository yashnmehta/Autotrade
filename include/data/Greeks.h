#ifndef GREEKS_H
#define GREEKS_H

#include <cmath>

struct OptionGreeks
{
    double delta;
    double gamma;
    double theta;
    double vega;
    double rho;
    double price; // Theoretical price
};

class GreeksCalculator
{
public:
    /**
     * @brief Calculate Black-Scholes Greeks for a European Option
     *
     * @param S Current Stock Price
     * @param K Strike Price
     * @param T Time to Expiry (in years)
     * @param r Risk-free Interest Rate (decimal, e.g., 0.05 for 5%)
     * @param sigma Volatility (decimal, e.g., 0.20 for 20%)
     * @param isCall True for Call, False for Put
     * @return OptionGreeks struct
     */
    static OptionGreeks calculate(double S, double K, double T, double r, double sigma, bool isCall);

private:
    static double normalCDF(double value);
    static double normalPDF(double value);
};

#endif // GREEKS_H
