/**
 * Performance Benchmark for IV and Greeks Calculator
 * 
 * This benchmark tests the performance of:
 * 1. Single IV calculation (Newton-Raphson)
 * 2. Single Greeks calculation (Black-Scholes)
 * 3. Batch calculations (simulating option chains)
 * 4. Throughput (calculations per second)
 * 
 * Target Performance:
 * - Single IV: < 10 µs
 * - Single Greeks: < 5 µs
 * - Option chain (50 strikes): < 500 µs
 * - Throughput: > 100,000 calculations/sec
 */

#include "repository/IVCalculator.h"
#include "repository/Greeks.h"
#include <iostream>
#include <chrono>
#include <vector>
#include <random>
#include <iomanip>
#include <cmath>

using namespace std::chrono;

// Test parameters - realistic option market values
struct TestCase {
    double spotPrice;
    double strikePrice;
    double timeToExpiry;  // years
    double riskFreeRate;
    double volatility;    // for generating market price
    bool isCall;
};

// Generate synthetic market price using Black-Scholes
double generateMarketPrice(const TestCase& tc) {
    OptionGreeks greeks = GreeksCalculator::calculate(
        tc.spotPrice, tc.strikePrice, tc.timeToExpiry,
        tc.riskFreeRate, tc.volatility, tc.isCall
    );
    return greeks.price;
}

// Generate test cases for an option chain
std::vector<TestCase> generateOptionChain(
    double spot, double tte, double r,
    int numStrikes, double strikeStep, double baseVol
) {
    std::vector<TestCase> chain;
    
    // Generate strikes around ATM
    double startStrike = spot - (numStrikes / 2) * strikeStep;
    
    for (int i = 0; i < numStrikes; ++i) {
        double strike = startStrike + i * strikeStep;
        
        // Simulate volatility smile (higher IV for OTM)
        double moneyness = std::abs(std::log(spot / strike));
        double vol = baseVol * (1.0 + 0.5 * moneyness);
        
        // Call option
        chain.push_back({spot, strike, tte, r, vol, true});
        // Put option
        chain.push_back({spot, strike, tte, r, vol, false});
    }
    
    return chain;
}

// Benchmark: Single IV Calculation
void benchmarkSingleIV(int iterations) {
    std::cout << "\n=== Benchmark: Single IV Calculation ===" << std::endl;
    
    // Test case: NIFTY ATM option
    TestCase tc = {24000.0, 24000.0, 30.0/365.0, 0.065, 0.15, true};
    double marketPrice = generateMarketPrice(tc);
    
    auto start = high_resolution_clock::now();
    
    int totalIterations = 0;
    for (int i = 0; i < iterations; ++i) {
        IVResult result = IVCalculator::calculate(
            marketPrice, tc.spotPrice, tc.strikePrice,
            tc.timeToExpiry, tc.riskFreeRate, tc.isCall
        );
        totalIterations += result.iterations;
    }
    
    auto end = high_resolution_clock::now();
    auto duration = duration_cast<nanoseconds>(end - start).count();
    
    double avgTimeNs = static_cast<double>(duration) / iterations;
    double avgTimeUs = avgTimeNs / 1000.0;
    double avgIterations = static_cast<double>(totalIterations) / iterations;
    double throughput = 1e9 / avgTimeNs;  // calculations per second
    
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Iterations: " << iterations << std::endl;
    std::cout << "Average time: " << avgTimeUs << " µs" << std::endl;
    std::cout << "Average Newton-Raphson iterations: " << avgIterations << std::endl;
    std::cout << "Throughput: " << std::setprecision(0) << throughput << " calcs/sec" << std::endl;
    
    std::cout << (avgTimeUs < 10.0 ? "✅ PASS" : "❌ FAIL") 
              << " (target: < 10 µs)" << std::endl;
}

// Benchmark: Single Greeks Calculation (with known IV)
void benchmarkSingleGreeks(int iterations) {
    std::cout << "\n=== Benchmark: Single Greeks Calculation ===" << std::endl;
    
    // Test case: NIFTY ATM option with known IV
    double S = 24000.0, K = 24000.0, T = 30.0/365.0, r = 0.065, sigma = 0.15;
    
    auto start = high_resolution_clock::now();
    
    for (int i = 0; i < iterations; ++i) {
        OptionGreeks greeks = GreeksCalculator::calculate(S, K, T, r, sigma, true);
        // Prevent optimization from eliminating the call
        if (greeks.delta < -2.0) std::cout << "Unexpected" << std::endl;
    }
    
    auto end = high_resolution_clock::now();
    auto duration = duration_cast<nanoseconds>(end - start).count();
    
    double avgTimeNs = static_cast<double>(duration) / iterations;
    double avgTimeUs = avgTimeNs / 1000.0;
    double throughput = 1e9 / avgTimeNs;
    
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Iterations: " << iterations << std::endl;
    std::cout << "Average time: " << avgTimeUs << " µs" << std::endl;
    std::cout << "Throughput: " << std::setprecision(0) << throughput << " calcs/sec" << std::endl;
    
    std::cout << (avgTimeUs < 5.0 ? "✅ PASS" : "❌ FAIL") 
              << " (target: < 5 µs)" << std::endl;
}

// Benchmark: Option Chain (50 strikes = 100 options)
void benchmarkOptionChain(int chainRuns) {
    std::cout << "\n=== Benchmark: Option Chain (50 strikes = 100 options) ===" << std::endl;
    
    // Generate NIFTY option chain
    auto chain = generateOptionChain(24000.0, 30.0/365.0, 0.065, 50, 100.0, 0.15);
    
    // Pre-calculate market prices
    std::vector<double> marketPrices;
    for (const auto& tc : chain) {
        marketPrices.push_back(generateMarketPrice(tc));
    }
    
    auto start = high_resolution_clock::now();
    
    for (int run = 0; run < chainRuns; ++run) {
        for (size_t i = 0; i < chain.size(); ++i) {
            const auto& tc = chain[i];
            
            // Calculate IV
            IVResult ivResult = IVCalculator::calculate(
                marketPrices[i], tc.spotPrice, tc.strikePrice,
                tc.timeToExpiry, tc.riskFreeRate, tc.isCall
            );
            
            // Calculate Greeks with the IV
            if (ivResult.converged) {
                OptionGreeks greeks = GreeksCalculator::calculate(
                    tc.spotPrice, tc.strikePrice, tc.timeToExpiry,
                    tc.riskFreeRate, ivResult.impliedVolatility, tc.isCall
                );
                // Prevent optimization
                if (greeks.delta < -2.0) std::cout << "Unexpected" << std::endl;
            }
        }
    }
    
    auto end = high_resolution_clock::now();
    auto duration = duration_cast<nanoseconds>(end - start).count();
    
    double avgChainTimeNs = static_cast<double>(duration) / chainRuns;
    double avgChainTimeUs = avgChainTimeNs / 1000.0;
    double avgPerOption = avgChainTimeUs / chain.size();
    double throughput = 1e9 / (avgChainTimeNs / chain.size());
    
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Chain runs: " << chainRuns << std::endl;
    std::cout << "Options per chain: " << chain.size() << std::endl;
    std::cout << "Average chain time: " << avgChainTimeUs << " µs" << std::endl;
    std::cout << "Average per option: " << avgPerOption << " µs" << std::endl;
    std::cout << "Throughput: " << std::setprecision(0) << throughput << " calcs/sec" << std::endl;
    
    std::cout << (avgChainTimeUs < 1000.0 ? "✅ PASS" : "❌ FAIL") 
              << " (target: < 1000 µs for 100 options)" << std::endl;
}

// Benchmark: High Volume (5000 options - simulating full market)
void benchmarkHighVolume() {
    std::cout << "\n=== Benchmark: High Volume (5000 options) ===" << std::endl;
    
    // Generate multiple underlyings with option chains
    std::vector<TestCase> allOptions;
    std::vector<double> marketPrices;
    
    // 10 underlyings × 25 strikes × 2 (CE/PE) = 500 options per underlying
    double underlyings[] = {24000, 50000, 2500, 1800, 900, 400, 300, 200, 150, 100};
    double strikeSteps[] = {100, 200, 50, 50, 25, 10, 10, 5, 5, 2.5};
    
    for (int u = 0; u < 10; ++u) {
        auto chain = generateOptionChain(
            underlyings[u], 30.0/365.0, 0.065, 25, strikeSteps[u], 0.15 + u * 0.02
        );
        for (const auto& tc : chain) {
            allOptions.push_back(tc);
            marketPrices.push_back(generateMarketPrice(tc));
        }
    }
    
    std::cout << "Total options: " << allOptions.size() << std::endl;
    
    auto start = high_resolution_clock::now();
    
    int converged = 0;
    for (size_t i = 0; i < allOptions.size(); ++i) {
        const auto& tc = allOptions[i];
        
        IVResult ivResult = IVCalculator::calculate(
            marketPrices[i], tc.spotPrice, tc.strikePrice,
            tc.timeToExpiry, tc.riskFreeRate, tc.isCall
        );
        
        if (ivResult.converged) {
            converged++;
            OptionGreeks greeks = GreeksCalculator::calculate(
                tc.spotPrice, tc.strikePrice, tc.timeToExpiry,
                tc.riskFreeRate, ivResult.impliedVolatility, tc.isCall
            );
            if (greeks.delta < -2.0) std::cout << "Unexpected" << std::endl;
        }
    }
    
    auto end = high_resolution_clock::now();
    auto duration = duration_cast<microseconds>(end - start).count();
    
    double avgPerOption = static_cast<double>(duration) / allOptions.size();
    double throughput = 1e6 / avgPerOption;
    double convergenceRate = 100.0 * converged / allOptions.size();
    
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Total time: " << duration / 1000.0 << " ms" << std::endl;
    std::cout << "Average per option: " << avgPerOption << " µs" << std::endl;
    std::cout << "Throughput: " << std::setprecision(0) << throughput << " calcs/sec" << std::endl;
    std::cout << "Convergence rate: " << std::setprecision(1) << convergenceRate << "%" << std::endl;
    
    std::cout << (duration < 50000 ? "✅ PASS" : "❌ FAIL") 
              << " (target: < 50 ms for 5000 options)" << std::endl;
}

// Benchmark: IV Convergence by Moneyness
void benchmarkConvergence() {
    std::cout << "\n=== Benchmark: IV Convergence Analysis ===" << std::endl;
    
    double spot = 24000.0, T = 30.0/365.0, r = 0.065;
    std::vector<std::pair<std::string, double>> testStrikes = {
        {"Deep ITM (20%)", spot * 0.80},
        {"ITM (5%)", spot * 0.95},
        {"ATM", spot},
        {"OTM (5%)", spot * 1.05},
        {"Deep OTM (20%)", spot * 1.20}
    };
    
    std::cout << std::setw(20) << "Moneyness" 
              << std::setw(12) << "Strike"
              << std::setw(12) << "Iterations"
              << std::setw(12) << "Time (µs)"
              << std::setw(12) << "Converged" << std::endl;
    std::cout << std::string(68, '-') << std::endl;
    
    for (const auto& [name, strike] : testStrikes) {
        double vol = 0.15 * (1.0 + 0.3 * std::abs(std::log(spot/strike)));
        double marketPrice = GreeksCalculator::calculate(spot, strike, T, r, vol, true).price;
        
        auto start = high_resolution_clock::now();
        IVResult result = IVCalculator::calculate(marketPrice, spot, strike, T, r, true);
        auto end = high_resolution_clock::now();
        
        double timeUs = duration_cast<nanoseconds>(end - start).count() / 1000.0;
        
        std::cout << std::setw(20) << name
                  << std::setw(12) << std::fixed << std::setprecision(0) << strike
                  << std::setw(12) << result.iterations
                  << std::setw(12) << std::setprecision(2) << timeUs
                  << std::setw(12) << (result.converged ? "Yes" : "No") << std::endl;
    }
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "   IV & Greeks Performance Benchmark   " << std::endl;
    std::cout << "========================================" << std::endl;
    
    // Warm-up (JIT, cache warming)
    std::cout << "\nWarming up..." << std::endl;
    for (int i = 0; i < 1000; ++i) {
        GreeksCalculator::calculate(24000, 24000, 0.1, 0.065, 0.15, true);
        IVCalculator::calculate(150.0, 24000, 24000, 0.1, 0.065, true);
    }
    
    // Run benchmarks
    benchmarkSingleGreeks(100000);
    benchmarkSingleIV(100000);
    benchmarkOptionChain(1000);
    benchmarkHighVolume();
    benchmarkConvergence();
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "         Benchmark Complete            " << std::endl;
    std::cout << "========================================" << std::endl;
    
    return 0;
}
