#include "repository/IVCalculator.h"
#include "repository/Greeks.h"
#include <cmath>
#include <algorithm>
#include <limits>

// ============================================================================
// NEWTON-RAPHSON IV SOLVER
// ============================================================================

IVResult IVCalculator::calculate(
    double marketPrice,
    double S,
    double K,
    double T,
    double r,
    bool isCall,
    double initialGuess,
    double tolerance,
    int maxIterations
) {
    // Validate inputs
    if (!isCalculable(marketPrice, S, K, T, r, isCall)) {
        return IVResult(0.0, 0, false, std::numeric_limits<double>::quiet_NaN());
    }
    
    // Use intelligent initial guess if default
    double sigma = (initialGuess <= 0) 
        ? initialGuessFromMoneyness(S, K, T) 
        : initialGuess;
    
    // Clamp to valid range
    sigma = std::clamp(sigma, MIN_VOLATILITY, MAX_VOLATILITY);
    
    double lastSigma = sigma;
    
    for (int i = 0; i < maxIterations; ++i) {
        // Calculate theoretical price and Greeks at current sigma
        OptionGreeks greeks = GreeksCalculator::calculate(S, K, T, r, sigma, isCall);
        
        double priceDiff = greeks.price - marketPrice;
        
        // Check convergence
        if (std::abs(priceDiff) < tolerance) {
            return IVResult(sigma, i + 1, true, priceDiff);
        }
        
        // Vega is scaled by 100 in GreeksCalculator (per 1% vol change)
        // So actual vega = greeks.vega * 100
        double actualVega = greeks.vega * 100.0;
        
        // Check if vega is too small (near expiry or deep ITM/OTM)
        if (std::abs(actualVega) < MIN_VEGA_THRESHOLD) {
            // Try Brent's method as fallback
            return calculateBrent(marketPrice, S, K, T, r, isCall);
        }
        
        // Newton-Raphson update: σ_new = σ - f(σ)/f'(σ)
        // where f(σ) = BS_Price(σ) - Market_Price
        // and f'(σ) = Vega
        lastSigma = sigma;
        sigma = sigma - priceDiff / actualVega;
        
        // Clamp sigma to reasonable bounds
        sigma = std::clamp(sigma, MIN_VOLATILITY, MAX_VOLATILITY);
        
        // Check for oscillation (alternating between two values)
        if (std::abs(sigma - lastSigma) < tolerance * 0.01) {
            // Converged on sigma change
            return IVResult(sigma, i + 1, true, priceDiff);
        }
    }
    
    // Max iterations reached - return best estimate
    OptionGreeks finalGreeks = GreeksCalculator::calculate(S, K, T, r, sigma, isCall);
    double finalError = finalGreeks.price - marketPrice;
    
    // Consider it converged if error is small enough
    bool closeEnough = std::abs(finalError) < tolerance * 100;
    
    return IVResult(sigma, maxIterations, closeEnough, finalError);
}

// ============================================================================
// BRENT'S METHOD (FALLBACK)
// ============================================================================

IVResult IVCalculator::calculateBrent(
    double marketPrice,
    double S,
    double K,
    double T,
    double r,
    bool isCall
) {
    // Brent's method for root finding
    // More robust than Newton-Raphson for difficult cases
    
    if (!isCalculable(marketPrice, S, K, T, r, isCall)) {
        return IVResult(0.0, 0, false, std::numeric_limits<double>::quiet_NaN());
    }
    
    // Bracket the root
    double a = MIN_VOLATILITY;
    double b = MAX_VOLATILITY;
    
    auto priceFunc = [&](double sigma) -> double {
        OptionGreeks greeks = GreeksCalculator::calculate(S, K, T, r, sigma, isCall);
        return greeks.price - marketPrice;
    };
    
    double fa = priceFunc(a);
    double fb = priceFunc(b);
    
    // Check if solution is bracketed
    if (fa * fb > 0) {
        // Not bracketed - try to find a bracket
        // This can happen if market price is outside theoretical bounds
        return IVResult(0.0, 0, false, fa);
    }
    
    // Ensure |f(a)| >= |f(b)|
    if (std::abs(fa) < std::abs(fb)) {
        std::swap(a, b);
        std::swap(fa, fb);
    }
    
    double c = a;
    double fc = fa;
    bool mflag = true;
    double s = 0.0;
    double d = 0.0;
    
    constexpr double tolerance = 1e-8;
    constexpr int maxIterations = 100;
    
    for (int i = 0; i < maxIterations; ++i) {
        if (std::abs(b - a) < tolerance) {
            return IVResult(b, i + 1, true, fb);
        }
        
        if (std::abs(fb) < tolerance) {
            return IVResult(b, i + 1, true, fb);
        }
        
        // Calculate s using inverse quadratic interpolation or secant method
        if (fa != fc && fb != fc) {
            // Inverse quadratic interpolation
            s = a * fb * fc / ((fa - fb) * (fa - fc))
              + b * fa * fc / ((fb - fa) * (fb - fc))
              + c * fa * fb / ((fc - fa) * (fc - fb));
        } else {
            // Secant method
            s = b - fb * (b - a) / (fb - fa);
        }
        
        // Conditions for bisection
        double cond1_min = (3 * a + b) / 4;
        double cond1_max = b;
        if (cond1_min > cond1_max) std::swap(cond1_min, cond1_max);
        
        bool cond1 = !(s >= cond1_min && s <= cond1_max);
        bool cond2 = mflag && std::abs(s - b) >= std::abs(b - c) / 2;
        bool cond3 = !mflag && std::abs(s - b) >= std::abs(c - d) / 2;
        bool cond4 = mflag && std::abs(b - c) < tolerance;
        bool cond5 = !mflag && std::abs(c - d) < tolerance;
        
        if (cond1 || cond2 || cond3 || cond4 || cond5) {
            // Bisection
            s = (a + b) / 2;
            mflag = true;
        } else {
            mflag = false;
        }
        
        double fs = priceFunc(s);
        d = c;
        c = b;
        fc = fb;
        
        if (fa * fs < 0) {
            b = s;
            fb = fs;
        } else {
            a = s;
            fa = fs;
        }
        
        // Ensure |f(a)| >= |f(b)|
        if (std::abs(fa) < std::abs(fb)) {
            std::swap(a, b);
            std::swap(fa, fb);
        }
    }
    
    return IVResult(b, maxIterations, std::abs(fb) < tolerance * 100, fb);
}

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

bool IVCalculator::isCalculable(
    double marketPrice,
    double S,
    double K,
    double T,
    double r,
    bool isCall
) {
    // Basic validation
    if (T <= 0) return false;           // Expired
    if (S <= 0) return false;           // Invalid spot
    if (K <= 0) return false;           // Invalid strike
    if (marketPrice <= 0) return false; // Invalid price
    
    // Check if market price is above intrinsic value
    double intrinsic = intrinsicValue(S, K, isCall);
    
    // Allow small tolerance for bid-ask spread effects
    if (marketPrice < intrinsic * 0.99) {
        // Market price below intrinsic - arbitrage condition
        // This can happen with stale data or illiquid options
        return false;
    }
    
    return true;
}

double IVCalculator::intrinsicValue(double S, double K, bool isCall) {
    if (isCall) {
        return std::max(S - K, 0.0);
    } else {
        return std::max(K - S, 0.0);
    }
}

double IVCalculator::initialGuessFromMoneyness(double S, double K, double T) {
    // Use moneyness to estimate initial volatility
    // This helps convergence for deep ITM/OTM options
    
    double moneyness = std::log(S / K);
    double absMoneyness = std::abs(moneyness);
    
    // Base volatility guess
    double baseVol = 0.20; // 20% is typical for index options
    
    // Adjust based on moneyness
    // Deep ITM/OTM options often have higher IV
    if (absMoneyness > 0.2) {
        // Far from ATM - start with higher IV guess
        baseVol = 0.30 + absMoneyness * 0.5;
    } else if (absMoneyness > 0.1) {
        // Moderately ITM/OTM
        baseVol = 0.25;
    }
    
    // Adjust for time to expiry
    // Near-expiry options can have extreme IV
    if (T < 7.0 / 365.0) {
        // Less than 7 days - can be very volatile
        baseVol *= 1.5;
    } else if (T < 30.0 / 365.0) {
        // Less than 30 days
        baseVol *= 1.2;
    }
    
    return std::clamp(baseVol, MIN_VOLATILITY, MAX_VOLATILITY);
}
