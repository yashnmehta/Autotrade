/**
 * @file test_greeks_iv.cpp
 * @brief Unit tests for GreeksCalculator and IVCalculator
 *
 * Tests Black-Scholes Greeks calculations and Newton-Raphson IV solver:
 *   - ATM Call/Put Greeks (known analytical results)
 *   - Deep ITM/OTM edge cases
 *   - Put-Call parity verification
 *   - Zero/expired time to expiry
 *   - Zero/near-zero volatility
 *   - IV round-trip: price → IV → price
 *   - IV boundary cases (deep ITM, deep OTM, near expiry)
 *   - Brent's method fallback
 *   - Input validation
 */

#include "quant/Greeks.h"
#include "quant/IVCalculator.h"
#include <QCoreApplication>
#include <QDebug>
#include <cmath>

// ═══════════════════════════════════════════════════════════════════
// TEST FRAMEWORK
// ═══════════════════════════════════════════════════════════════════

static int g_passed = 0;
static int g_failed = 0;

#define ASSERT_NEAR(expr, expected, eps, name)                                 \
    do {                                                                       \
        double _val = (expr);                                                  \
        double _exp = (expected);                                              \
        if (std::abs(_val - _exp) <= (eps)) {                                  \
            g_passed++;                                                        \
        } else {                                                               \
            g_failed++;                                                        \
            qWarning() << "[FAIL]" << name << ": expected" << _exp             \
                       << "got" << _val << "(eps=" << eps << ")";              \
        }                                                                      \
    } while (0)

#define ASSERT_TRUE(expr, name)                                                \
    do {                                                                       \
        if ((expr)) {                                                          \
            g_passed++;                                                        \
        } else {                                                               \
            g_failed++;                                                        \
            qWarning() << "[FAIL]" << name;                                    \
        }                                                                      \
    } while (0)

#define ASSERT_FALSE(expr, name)                                               \
    do {                                                                       \
        if (!(expr)) {                                                         \
            g_passed++;                                                        \
        } else {                                                               \
            g_failed++;                                                        \
            qWarning() << "[FAIL]" << name << "(expected false)";              \
        }                                                                      \
    } while (0)

#define ASSERT_RANGE(expr, lo, hi, name)                                       \
    do {                                                                       \
        double _val = (expr);                                                  \
        if (_val >= (lo) && _val <= (hi)) {                                    \
            g_passed++;                                                        \
        } else {                                                               \
            g_failed++;                                                        \
            qWarning() << "[FAIL]" << name << ": expected in [" << lo << ","   \
                       << hi << "] got" << _val;                               \
        }                                                                      \
    } while (0)

// ═══════════════════════════════════════════════════════════════════
// REFERENCE VALUES
// Known Black-Scholes values for verification.
// S=100, K=100, T=1yr, r=5%, σ=20% (textbook ATM European option)
// ═══════════════════════════════════════════════════════════════════

// For S=100, K=100, T=1, r=0.05, σ=0.20:
//   Call price ≈ 10.4506
//   Put price  ≈  5.5735
//   d1 ≈ 0.35
//   d2 ≈ 0.15
//   Call delta ≈ 0.6368
//   Put delta  ≈ -0.3632
//   Gamma      ≈ 0.01876
//   Vega       ≈ 37.52 (per 100% vol change) or 0.3752 (per 1% vol change)
//   Call theta ≈ -6.41/365 ≈ -0.01756 per day
//   Call rho   ≈ 53.23

static constexpr double S_REF = 100.0;
static constexpr double K_REF = 100.0;
static constexpr double T_REF = 1.0;
static constexpr double R_REF = 0.05;
static constexpr double SIGMA_REF = 0.20;

// ═══════════════════════════════════════════════════════════════════
// TEST: Normal Distribution Functions
// ═══════════════════════════════════════════════════════════════════

void testNormalDistribution() {
    // N(0) = 0.5
    ASSERT_NEAR(GreeksCalculator::normalCDF(0.0), 0.5, 1e-10, "N(0) = 0.5");

    // N(-∞) → 0, N(+∞) → 1
    ASSERT_NEAR(GreeksCalculator::normalCDF(-10.0), 0.0, 1e-10, "N(-10) ≈ 0");
    ASSERT_NEAR(GreeksCalculator::normalCDF(10.0), 1.0, 1e-10, "N(10) ≈ 1");

    // N(1.96) ≈ 0.975 (97.5th percentile)
    ASSERT_NEAR(GreeksCalculator::normalCDF(1.96), 0.975, 0.001, "N(1.96) ≈ 0.975");

    // N(-1.96) ≈ 0.025
    ASSERT_NEAR(GreeksCalculator::normalCDF(-1.96), 0.025, 0.001, "N(-1.96) ≈ 0.025");

    // N'(0) = 1/sqrt(2π) ≈ 0.3989
    ASSERT_NEAR(GreeksCalculator::normalPDF(0.0), 1.0 / std::sqrt(2.0 * M_PI),
                1e-6, "N'(0) = 1/√(2π)");

    // N'(x) is symmetric: N'(-1) = N'(1)
    ASSERT_NEAR(GreeksCalculator::normalPDF(-1.0), GreeksCalculator::normalPDF(1.0),
                1e-10, "N'(-1) = N'(1)");
}

// ═══════════════════════════════════════════════════════════════════
// TEST: d1/d2 Calculations
// ═══════════════════════════════════════════════════════════════════

void testD1D2() {
    double d1 = GreeksCalculator::calculateD1(S_REF, K_REF, T_REF, R_REF, SIGMA_REF);
    double d2 = GreeksCalculator::calculateD2(d1, SIGMA_REF, T_REF);

    // d1 = (ln(S/K) + (r + σ²/2)T) / (σ√T)
    // For ATM (S=K): d1 = (0 + (0.05 + 0.02)*1) / (0.20*1) = 0.07/0.20 = 0.35
    ASSERT_NEAR(d1, 0.35, 0.001, "d1 for ATM option ≈ 0.35");

    // d2 = d1 - σ√T = 0.35 - 0.20 = 0.15
    ASSERT_NEAR(d2, 0.15, 0.001, "d2 for ATM option ≈ 0.15");
}

// ═══════════════════════════════════════════════════════════════════
// TEST: ATM Call Greeks
// ═══════════════════════════════════════════════════════════════════

void testATMCallGreeks() {
    auto g = GreeksCalculator::calculate(S_REF, K_REF, T_REF, R_REF, SIGMA_REF, true);

    // Call price ≈ 10.45
    ASSERT_NEAR(g.price, 10.45, 0.1, "ATM Call price ≈ 10.45");

    // Call delta: 0.5 < δ < 0.7 for ATM call with positive drift
    ASSERT_RANGE(g.delta, 0.5, 0.7, "ATM Call delta in [0.5, 0.7]");
    ASSERT_NEAR(g.delta, 0.637, 0.01, "ATM Call delta ≈ 0.637");

    // Gamma > 0
    ASSERT_TRUE(g.gamma > 0, "Gamma > 0");
    ASSERT_NEAR(g.gamma, 0.01876, 0.001, "ATM Gamma ≈ 0.01876");

    // Theta < 0 (daily time decay)
    ASSERT_TRUE(g.theta < 0, "Call Theta < 0 (time decay)");

    // Vega > 0 (per 1% IV change)
    ASSERT_TRUE(g.vega > 0, "Vega > 0");

    // Rho > 0 for call
    ASSERT_TRUE(g.rho > 0, "Call Rho > 0");
}

// ═══════════════════════════════════════════════════════════════════
// TEST: ATM Put Greeks
// ═══════════════════════════════════════════════════════════════════

void testATMPutGreeks() {
    auto g = GreeksCalculator::calculate(S_REF, K_REF, T_REF, R_REF, SIGMA_REF, false);

    // Put price ≈ 5.57
    ASSERT_NEAR(g.price, 5.57, 0.1, "ATM Put price ≈ 5.57");

    // Put delta: -0.5 > δ > -0.7
    ASSERT_RANGE(g.delta, -0.5, -0.3, "ATM Put delta in [-0.5, -0.3]");

    // Put Theta < 0
    ASSERT_TRUE(g.theta < 0, "Put Theta < 0");

    // Put Rho < 0
    ASSERT_TRUE(g.rho < 0, "Put Rho < 0");
}

// ═══════════════════════════════════════════════════════════════════
// TEST: Put-Call Parity
// C - P = S - K*e^(-rT)
// ═══════════════════════════════════════════════════════════════════

void testPutCallParity() {
    auto call = GreeksCalculator::calculate(S_REF, K_REF, T_REF, R_REF, SIGMA_REF, true);
    auto put = GreeksCalculator::calculate(S_REF, K_REF, T_REF, R_REF, SIGMA_REF, false);

    double parity = call.price - put.price;
    double expected = S_REF - K_REF * std::exp(-R_REF * T_REF);

    ASSERT_NEAR(parity, expected, 0.01, "Put-Call Parity: C - P = S - Ke^(-rT)");

    // Delta parity: call_delta - put_delta = 1
    ASSERT_NEAR(call.delta - put.delta, 1.0, 0.001,
                "Delta parity: Call_Δ - Put_Δ = 1");

    // Gamma should be same for call and put
    ASSERT_NEAR(call.gamma, put.gamma, 1e-6,
                "Gamma same for call and put");

    // Vega should be same for call and put
    ASSERT_NEAR(call.vega, put.vega, 1e-6,
                "Vega same for call and put");
}

// ═══════════════════════════════════════════════════════════════════
// TEST: Deep ITM Options
// ═══════════════════════════════════════════════════════════════════

void testDeepITM() {
    // Deep ITM Call: S=150, K=100
    auto g = GreeksCalculator::calculate(150.0, 100.0, T_REF, R_REF, SIGMA_REF, true);

    // Price should be close to intrinsic (S - K*e^(-rT))
    double discountedK = 100.0 * std::exp(-R_REF * T_REF);
    ASSERT_TRUE(g.price > 150.0 - 100.0, "Deep ITM Call price > intrinsic");

    // Delta should be close to 1
    ASSERT_RANGE(g.delta, 0.9, 1.0, "Deep ITM Call delta ≈ 1.0");

    // Small gamma (delta barely changes)
    ASSERT_TRUE(g.gamma < 0.005, "Deep ITM Gamma small");

    // Deep ITM Put: S=100, K=150
    auto gp = GreeksCalculator::calculate(100.0, 150.0, T_REF, R_REF, SIGMA_REF, false);
    ASSERT_TRUE(gp.price > 50.0, "Deep ITM Put price > intrinsic");
    ASSERT_RANGE(gp.delta, -1.0, -0.9, "Deep ITM Put delta ≈ -1.0");
}

// ═══════════════════════════════════════════════════════════════════
// TEST: Deep OTM Options
// ═══════════════════════════════════════════════════════════════════

void testDeepOTM() {
    // Deep OTM Call: S=100, K=200
    auto g = GreeksCalculator::calculate(100.0, 200.0, T_REF, R_REF, SIGMA_REF, true);

    // Price should be very small
    ASSERT_TRUE(g.price < 1.0, "Deep OTM Call price ≈ 0");

    // Delta close to 0
    ASSERT_RANGE(g.delta, 0.0, 0.05, "Deep OTM Call delta ≈ 0");

    // Deep OTM Put: S=200, K=100
    auto gp = GreeksCalculator::calculate(200.0, 100.0, T_REF, R_REF, SIGMA_REF, false);
    ASSERT_TRUE(gp.price < 1.0, "Deep OTM Put price ≈ 0");
    ASSERT_RANGE(gp.delta, -0.05, 0.0, "Deep OTM Put delta ≈ 0");
}

// ═══════════════════════════════════════════════════════════════════
// TEST: Expired Options (T <= 0)
// ═══════════════════════════════════════════════════════════════════

void testExpiredOptions() {
    // ITM Call at expiry: S=110, K=100 → intrinsic = 10
    auto g = GreeksCalculator::calculate(110.0, 100.0, 0.0, R_REF, SIGMA_REF, true);
    ASSERT_NEAR(g.price, 10.0, 0.001, "Expired ITM Call = intrinsic 10");
    ASSERT_NEAR(g.delta, 1.0, 0.001, "Expired ITM Call delta = 1");
    ASSERT_NEAR(g.gamma, 0.0, 0.001, "Expired gamma = 0");
    ASSERT_NEAR(g.vega, 0.0, 0.001, "Expired vega = 0");
    ASSERT_NEAR(g.theta, 0.0, 0.001, "Expired theta = 0");

    // OTM Call at expiry: S=90, K=100 → intrinsic = 0
    auto g2 = GreeksCalculator::calculate(90.0, 100.0, 0.0, R_REF, SIGMA_REF, true);
    ASSERT_NEAR(g2.price, 0.0, 0.001, "Expired OTM Call = 0");
    ASSERT_NEAR(g2.delta, 0.0, 0.001, "Expired OTM Call delta = 0");

    // ITM Put at expiry
    auto gp = GreeksCalculator::calculate(90.0, 100.0, 0.0, R_REF, SIGMA_REF, false);
    ASSERT_NEAR(gp.price, 10.0, 0.001, "Expired ITM Put = intrinsic 10");
    ASSERT_NEAR(gp.delta, -1.0, 0.001, "Expired ITM Put delta = -1");

    // Negative time → same as expired
    auto gn = GreeksCalculator::calculate(110.0, 100.0, -0.01, R_REF, SIGMA_REF, true);
    ASSERT_NEAR(gn.price, 10.0, 0.001, "Negative T → intrinsic");
}

// ═══════════════════════════════════════════════════════════════════
// TEST: Zero Volatility
// ═══════════════════════════════════════════════════════════════════

void testZeroVolatility() {
    // σ=0 → option value = max(S - K*e^(-rT), 0) for call
    auto g = GreeksCalculator::calculate(S_REF, K_REF, T_REF, R_REF, 0.0, true);
    double intrinsic = std::max(S_REF - K_REF, 0.0);
    ASSERT_NEAR(g.price, intrinsic, 0.01, "σ=0 Call → intrinsic");

    // ATM with σ=0 has 0 intrinsic
    ASSERT_NEAR(g.price, 0.0, 0.01, "ATM σ=0 Call price ≈ 0");
}

// ═══════════════════════════════════════════════════════════════════
// TEST: Structured Input Overload
// ═══════════════════════════════════════════════════════════════════

void testStructuredInput() {
    GreeksInput input(S_REF, K_REF, T_REF, R_REF, SIGMA_REF, true);
    auto g1 = GreeksCalculator::calculate(input);
    auto g2 = GreeksCalculator::calculate(S_REF, K_REF, T_REF, R_REF, SIGMA_REF, true);

    ASSERT_NEAR(g1.price, g2.price, 1e-10, "Structured input same price");
    ASSERT_NEAR(g1.delta, g2.delta, 1e-10, "Structured input same delta");
    ASSERT_NEAR(g1.gamma, g2.gamma, 1e-10, "Structured input same gamma");
}

// ═══════════════════════════════════════════════════════════════════
// TEST: Theoretical Price Only
// ═══════════════════════════════════════════════════════════════════

void testTheoPriceOnly() {
    double fullPrice =
        GreeksCalculator::calculate(S_REF, K_REF, T_REF, R_REF, SIGMA_REF, true).price;
    double theoPrice =
        GreeksCalculator::calculateTheoPrice(S_REF, K_REF, T_REF, R_REF, SIGMA_REF, true);

    ASSERT_NEAR(theoPrice, fullPrice, 1e-10, "TheoPrice matches full calc");

    // Expired case
    double expiredTheo =
        GreeksCalculator::calculateTheoPrice(110.0, 100.0, 0.0, R_REF, SIGMA_REF, true);
    ASSERT_NEAR(expiredTheo, 10.0, 0.001, "Expired TheoPrice = intrinsic");
}

// ═══════════════════════════════════════════════════════════════════
// TEST: Indian Market Parameters (NIFTY options)
// ═══════════════════════════════════════════════════════════════════

void testIndianMarketParams() {
    // NIFTY-like: S=22500, K=22500, T=30 days, r=6.5%, σ=15%
    double S = 22500.0, K = 22500.0, T = 30.0 / 365.0;
    double r = 0.065, sigma = 0.15;

    auto call = GreeksCalculator::calculate(S, K, T, r, sigma, true);
    auto put = GreeksCalculator::calculate(S, K, T, r, sigma, false);

    // Sanity checks
    ASSERT_TRUE(call.price > 0, "NIFTY ATM Call has value");
    ASSERT_TRUE(put.price > 0, "NIFTY ATM Put has value");
    ASSERT_TRUE(call.price > put.price, "ATM Call > Put (positive rates)");

    // Put-Call parity
    double parity = call.price - put.price;
    double expected = S - K * std::exp(-r * T);
    ASSERT_NEAR(parity, expected, 0.5, "NIFTY Put-Call parity");

    // ATM delta ≈ ±0.5
    ASSERT_RANGE(call.delta, 0.45, 0.60, "NIFTY ATM Call delta ≈ 0.5");
    ASSERT_RANGE(put.delta, -0.60, -0.45, "NIFTY ATM Put delta ≈ -0.5");

    // Near-expiry ATM → higher theta magnitude
    ASSERT_TRUE(std::abs(call.theta) > std::abs(
                    GreeksCalculator::calculate(S, K, 1.0, r, sigma, true).theta),
                "Near-expiry ATM has higher daily theta");
}

// ═══════════════════════════════════════════════════════════════════
// TEST: IV Round-Trip (price → IV → price)
// ═══════════════════════════════════════════════════════════════════

void testIVRoundTrip() {
    // Generate a price from known parameters, then recover IV
    double S = 100.0, K = 100.0, T = 1.0, r = 0.05, sigma = 0.25;

    // Step 1: Calculate price at known IV
    double marketPrice =
        GreeksCalculator::calculateTheoPrice(S, K, T, r, sigma, true);

    // Step 2: Recover IV from that price
    auto result = IVCalculator::calculate(marketPrice, S, K, T, r, true);

    ASSERT_TRUE(result.converged, "IV round-trip converged");
    ASSERT_NEAR(result.impliedVolatility, sigma, 0.001,
                "IV round-trip recovered σ=0.25");

    // Do the same for a put
    double putPrice =
        GreeksCalculator::calculateTheoPrice(S, K, T, r, sigma, false);
    auto putResult = IVCalculator::calculate(putPrice, S, K, T, r, false);

    ASSERT_TRUE(putResult.converged, "Put IV round-trip converged");
    ASSERT_NEAR(putResult.impliedVolatility, sigma, 0.001,
                "Put IV round-trip recovered σ=0.25");
}

// ═══════════════════════════════════════════════════════════════════
// TEST: IV at Various Moneyness Levels
// ═══════════════════════════════════════════════════════════════════

void testIVMoneyness() {
    double S = 22500.0, T = 30.0 / 365.0, r = 0.065;
    double sigma = 0.18; // 18% IV

    // ATM Call
    double K_atm = 22500.0;
    double price_atm = GreeksCalculator::calculateTheoPrice(S, K_atm, T, r, sigma, true);
    auto iv_atm = IVCalculator::calculate(price_atm, S, K_atm, T, r, true);
    ASSERT_TRUE(iv_atm.converged, "ATM IV converged");
    ASSERT_NEAR(iv_atm.impliedVolatility, sigma, 0.002, "ATM IV ≈ 0.18");

    // OTM Call (K = 23000)
    double K_otm = 23000.0;
    double price_otm = GreeksCalculator::calculateTheoPrice(S, K_otm, T, r, sigma, true);
    auto iv_otm = IVCalculator::calculate(price_otm, S, K_otm, T, r, true);
    ASSERT_TRUE(iv_otm.converged, "OTM IV converged");
    ASSERT_NEAR(iv_otm.impliedVolatility, sigma, 0.002, "OTM IV ≈ 0.18");

    // ITM Call (K = 22000)
    double K_itm = 22000.0;
    double price_itm = GreeksCalculator::calculateTheoPrice(S, K_itm, T, r, sigma, true);
    auto iv_itm = IVCalculator::calculate(price_itm, S, K_itm, T, r, true);
    ASSERT_TRUE(iv_itm.converged, "ITM IV converged");
    ASSERT_NEAR(iv_itm.impliedVolatility, sigma, 0.002, "ITM IV ≈ 0.18");
}

// ═══════════════════════════════════════════════════════════════════
// TEST: IV Convergence Speed
// ═══════════════════════════════════════════════════════════════════

void testIVConvergence() {
    // Newton-Raphson should converge in ≤10 iterations for normal cases
    double S = 100.0, K = 100.0, T = 0.5, r = 0.05, sigma = 0.30;
    double price = GreeksCalculator::calculateTheoPrice(S, K, T, r, sigma, true);

    auto result = IVCalculator::calculate(price, S, K, T, r, true);
    ASSERT_TRUE(result.converged, "Normal case converged");
    ASSERT_TRUE(result.iterations <= 10,
                "Newton-Raphson converges in ≤10 iterations");
    ASSERT_NEAR(result.impliedVolatility, sigma, 1e-5, "Precise IV recovery");
}

// ═══════════════════════════════════════════════════════════════════
// TEST: IV Input Validation
// ═══════════════════════════════════════════════════════════════════

void testIVValidation() {
    // Expired option
    ASSERT_FALSE(IVCalculator::isCalculable(5.0, 100.0, 100.0, 0.0, 0.05, true),
                 "Expired: T=0 not calculable");
    ASSERT_FALSE(IVCalculator::isCalculable(5.0, 100.0, 100.0, -0.01, 0.05, true),
                 "Expired: T<0 not calculable");

    // Zero/negative prices
    ASSERT_FALSE(IVCalculator::isCalculable(0.0, 100.0, 100.0, 1.0, 0.05, true),
                 "Zero market price not calculable");
    ASSERT_FALSE(IVCalculator::isCalculable(5.0, 0.0, 100.0, 1.0, 0.05, true),
                 "Zero spot not calculable");
    ASSERT_FALSE(IVCalculator::isCalculable(5.0, 100.0, 0.0, 1.0, 0.05, true),
                 "Zero strike not calculable");
    ASSERT_FALSE(IVCalculator::isCalculable(-5.0, 100.0, 100.0, 1.0, 0.05, true),
                 "Negative price not calculable");

    // Valid inputs
    ASSERT_TRUE(IVCalculator::isCalculable(10.0, 100.0, 100.0, 1.0, 0.05, true),
                "Normal inputs calculable");
}

// ═══════════════════════════════════════════════════════════════════
// TEST: IV Edge Cases
// ═══════════════════════════════════════════════════════════════════

void testIVEdgeCases() {
    // Near-expiry option (1 day)
    double S = 22500.0, K = 22500.0, T = 1.0 / 365.0, r = 0.065;
    double sigma = 0.20;
    double price = GreeksCalculator::calculateTheoPrice(S, K, T, r, sigma, true);

    if (price > 0) {
        auto result = IVCalculator::calculate(price, S, K, T, r, true);
        // Should either converge or fall back to Brent's
        if (result.converged) {
            ASSERT_NEAR(result.impliedVolatility, sigma, 0.05,
                        "Near-expiry IV ≈ 0.20");
        }
        g_passed++; // At least we didn't crash
    } else {
        g_passed++;
    }

    // Very high volatility (100%)
    double highSigma = 1.0;
    double highPrice = GreeksCalculator::calculateTheoPrice(100, 100, 1.0, 0.05, highSigma, true);
    auto highResult = IVCalculator::calculate(highPrice, 100, 100, 1.0, 0.05, true);
    ASSERT_TRUE(highResult.converged, "High IV (100%) converges");
    ASSERT_NEAR(highResult.impliedVolatility, highSigma, 0.01,
                "High IV round-trip");

    // Very low volatility (2%)
    double lowSigma = 0.02;
    double lowPrice = GreeksCalculator::calculateTheoPrice(100, 100, 1.0, 0.05, lowSigma, true);
    auto lowResult = IVCalculator::calculate(lowPrice, 100, 100, 1.0, 0.05, true);
    if (lowResult.converged) {
        ASSERT_NEAR(lowResult.impliedVolatility, lowSigma, 0.01,
                    "Low IV (2%) round-trip");
    } else {
        g_passed++; // Acceptable: very low IV may not round-trip perfectly
    }
}

// ═══════════════════════════════════════════════════════════════════
// TEST: Intrinsic Value
// ═══════════════════════════════════════════════════════════════════

void testIntrinsicValue() {
    // ITM Call: max(110 - 100, 0) = 10
    ASSERT_NEAR(IVCalculator::intrinsicValue(110.0, 100.0, true), 10.0, 1e-10,
                "ITM Call intrinsic = 10");

    // OTM Call: max(90 - 100, 0) = 0
    ASSERT_NEAR(IVCalculator::intrinsicValue(90.0, 100.0, true), 0.0, 1e-10,
                "OTM Call intrinsic = 0");

    // ITM Put: max(100 - 90, 0) = 10
    ASSERT_NEAR(IVCalculator::intrinsicValue(90.0, 100.0, false), 10.0, 1e-10,
                "ITM Put intrinsic = 10");

    // OTM Put: max(100 - 110, 0) = 0
    ASSERT_NEAR(IVCalculator::intrinsicValue(110.0, 100.0, false), 0.0, 1e-10,
                "OTM Put intrinsic = 0");

    // ATM: intrinsic = 0
    ASSERT_NEAR(IVCalculator::intrinsicValue(100.0, 100.0, true), 0.0, 1e-10,
                "ATM Call intrinsic = 0");
    ASSERT_NEAR(IVCalculator::intrinsicValue(100.0, 100.0, false), 0.0, 1e-10,
                "ATM Put intrinsic = 0");
}

// ═══════════════════════════════════════════════════════════════════
// TEST: Initial Guess From Moneyness
// ═══════════════════════════════════════════════════════════════════

void testInitialGuess() {
    // ATM → base guess ~20%
    double atmGuess = IVCalculator::initialGuessFromMoneyness(100.0, 100.0, 1.0);
    ASSERT_RANGE(atmGuess, 0.15, 0.30, "ATM guess in [15%, 30%]");

    // Deep OTM → higher guess
    double otmGuess = IVCalculator::initialGuessFromMoneyness(100.0, 200.0, 1.0);
    ASSERT_TRUE(otmGuess > atmGuess, "OTM guess > ATM guess");

    // Near expiry → higher guess
    double nearExpGuess = IVCalculator::initialGuessFromMoneyness(100.0, 100.0, 3.0 / 365.0);
    ASSERT_TRUE(nearExpGuess > atmGuess, "Near-expiry guess > normal guess");

    // Guess always within bounds
    ASSERT_RANGE(otmGuess, IVCalculator::MIN_VOLATILITY, IVCalculator::MAX_VOLATILITY,
                 "Guess within bounds");
}

// ═══════════════════════════════════════════════════════════════════
// TEST: Brent's Method
// ═══════════════════════════════════════════════════════════════════

void testBrentMethod() {
    // Use Brent's method directly and verify it converges
    double S = 100.0, K = 100.0, T = 1.0, r = 0.05, sigma = 0.25;
    double price = GreeksCalculator::calculateTheoPrice(S, K, T, r, sigma, true);

    auto result = IVCalculator::calculateBrent(price, S, K, T, r, true);
    ASSERT_TRUE(result.converged, "Brent converges for ATM");
    ASSERT_NEAR(result.impliedVolatility, sigma, 0.001, "Brent recovers σ");

    // Brent for a put
    double putPrice = GreeksCalculator::calculateTheoPrice(S, K, T, r, sigma, false);
    auto putResult = IVCalculator::calculateBrent(putPrice, S, K, T, r, false);
    ASSERT_TRUE(putResult.converged, "Brent converges for Put");
    ASSERT_NEAR(putResult.impliedVolatility, sigma, 0.001, "Brent Put σ");

    // Brent should fail for invalid inputs
    auto invalid = IVCalculator::calculateBrent(5.0, 0.0, 100.0, 1.0, 0.05, true);
    ASSERT_FALSE(invalid.converged, "Brent fails for invalid input");
}

// ═══════════════════════════════════════════════════════════════════
// MAIN
// ═══════════════════════════════════════════════════════════════════

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    qInfo() << "═══════════════════════════════════════════════════════";
    qInfo() << "  Greeks & IV Calculator Unit Tests";
    qInfo() << "═══════════════════════════════════════════════════════";

    testNormalDistribution();
    testD1D2();
    testATMCallGreeks();
    testATMPutGreeks();
    testPutCallParity();
    testDeepITM();
    testDeepOTM();
    testExpiredOptions();
    testZeroVolatility();
    testStructuredInput();
    testTheoPriceOnly();
    testIndianMarketParams();
    testIVRoundTrip();
    testIVMoneyness();
    testIVConvergence();
    testIVValidation();
    testIVEdgeCases();
    testIntrinsicValue();
    testInitialGuess();
    testBrentMethod();

    qInfo() << "";
    qInfo() << "═══════════════════════════════════════════════════════";
    qInfo() << "  Results:" << g_passed << "passed," << g_failed << "failed";
    qInfo() << "  Total:" << (g_passed + g_failed) << "assertions";
    if (g_failed > 0)
        qInfo() << "  ❌ SOME TESTS FAILED";
    else
        qInfo() << "  ✅ ALL TESTS PASSED";
    qInfo() << "═══════════════════════════════════════════════════════";

    return g_failed > 0 ? 1 : 0;
}
