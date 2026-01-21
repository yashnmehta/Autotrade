/**
 * @file benchmark_repository_filters.cpp
 * @brief Performance benchmark for repository filter operations
 * 
 * Tests various filtering and search operations on NSEFORepository to measure:
 * - Query execution time
 * - Memory overhead of indexes
 * - Comparison between indexed vs full-scan approaches
 * 
 * Usage:
 *   ./benchmark_repository_filters <path_to_masters>
 * 
 * Example:
 *   ./benchmark_repository_filters ../MasterFiles
 */

#include "repository/NSEFORepository.h"
#include "repository/RepositoryManager.h"
#include <QCoreApplication>
#include <QDebug>
#include <QElapsedTimer>
#include <iostream>
#include <iomanip>

// ANSI color codes for terminal output
#define RESET   "\033[0m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN    "\033[36m"
#define BOLD    "\033[1m"

struct BenchmarkResult {
    QString operation;
    qint64 timeNanos;
    int resultCount;
    double timeMs() const { return timeNanos / 1000000.0; }
    double timeUs() const { return timeNanos / 1000.0; }
};

class RepositoryBenchmark {
public:
    RepositoryBenchmark(RepositoryManager* repoMgr) 
        : m_repoMgr(repoMgr)
        , m_nsefo(repoMgr->getNSEFORepository())
    {}
    
    void runAll() {
        std::cout << BOLD << CYAN 
                  << "\n╔══════════════════════════════════════════════════════════╗\n"
                  << "║       Repository Filter Performance Benchmark          ║\n"
                  << "╚══════════════════════════════════════════════════════════╝\n"
                  << RESET << std::endl;
        
        printRepositoryStats();
        
        benchmarkSeriesFilters();
        benchmarkSymbolFilters();
        benchmarkTokenLookup();
        benchmarkCombinedOperations();
        
        printSummary();
    }
    
private:
    void printRepositoryStats() {
        std::cout << BOLD << "Repository Statistics:\n" << RESET;
        std::cout << "  Total Contracts: " << m_nsefo->getTotalCount() << "\n";
        std::cout << "  Regular: " << m_nsefo->getRegularCount() << "\n";
        std::cout << "  Spread: " << m_nsefo->getSpreadCount() << "\n";
        std::cout << std::endl;
    }
    
    void benchmarkSeriesFilters() {
        std::cout << BOLD << YELLOW << "Test 1: Series Filtering\n" << RESET;
        std::cout << "  Testing getContractsBySeries() with different series types\n\n";
        
        QStringList seriesTypes = {"OPTIDX", "OPTSTK", "FUTIDX", "FUTSTK"};
        
        for (const QString& series : seriesTypes) {
            BenchmarkResult result = measureSeriesFilter(series);
            m_results.append(result);
            
            printResult(result, true);
        }
        std::cout << std::endl;
    }
    
    void benchmarkSymbolFilters() {
        std::cout << BOLD << YELLOW << "Test 2: Symbol Filtering (Option Chains)\n" << RESET;
        std::cout << "  Testing getContractsBySymbol() for various underlyings\n\n";
        
        QStringList symbols = {"NIFTY", "BANKNIFTY", "FINNIFTY", "RELIANCE", 
                               "TCS", "INFY", "HDFCBANK", "SBIN"};
        
        for (const QString& symbol : symbols) {
            BenchmarkResult result = measureSymbolFilter(symbol);
            m_results.append(result);
            
            printResult(result, true);
        }
        std::cout << std::endl;
    }
    
    void benchmarkTokenLookup() {
        std::cout << BOLD << YELLOW << "Test 3: Single Token Lookup\n" << RESET;
        std::cout << "  Testing getContract(token) with random tokens\n\n";
        
        // Test with valid tokens from different ranges
        QVector<int64_t> testTokens = {
            35000,   // First token
            40000,   // Low range
            80000,   // Mid range
            120000,  // High range
            199950   // Last token
        };
        
        qint64 totalTime = 0;
        int successCount = 0;
        
        for (int64_t token : testTokens) {
            QElapsedTimer timer;
            timer.start();
            
            const ContractData* contract = m_nsefo->getContract(token);
            
            qint64 elapsed = timer.nsecsElapsed();
            totalTime += elapsed;
            
            if (contract != nullptr) {
                successCount++;
            }
        }
        
        qint64 avgTime = totalTime / testTokens.size();
        
        BenchmarkResult result;
        result.operation = QString("Token Lookup (avg of %1)").arg(testTokens.size());
        result.timeNanos = avgTime;
        result.resultCount = successCount;
        m_results.append(result);
        
        printResult(result, false);
        std::cout << std::endl;
    }
    
    void benchmarkCombinedOperations() {
        std::cout << BOLD << YELLOW << "Test 4: Combined Operations (Real-world)\n" << RESET;
        std::cout << "  Simulating typical user workflows\n\n";
        
        // Scenario 1: Get option chain and find ATM strike
        {
            QElapsedTimer timer;
            timer.start();
            
            QVector<ContractData> optionChain = m_nsefo->getContractsBySymbol("NIFTY");
            
            // Filter for current expiry options only (simulated)
            QVector<ContractData> filtered;
            for (const auto& contract : optionChain) {
                if (contract.instrumentType == 2) { // Options only
                    filtered.append(contract);
                }
            }
            
            qint64 elapsed = timer.nsecsElapsed();
            
            BenchmarkResult result;
            result.operation = "Get NIFTY option chain + filter";
            result.timeNanos = elapsed;
            result.resultCount = filtered.size();
            m_results.append(result);
            
            printResult(result, true);
        }
        
        // Scenario 2: Search scrips by prefix
        {
            QElapsedTimer timer;
            timer.start();
            
            QVector<ContractData> results = m_repoMgr->searchScrips(
                "NSE", "FO", "OPTSTK", "REL", 50
            );
            
            qint64 elapsed = timer.nsecsElapsed();
            
            BenchmarkResult result;
            result.operation = "Search 'REL' in OPTSTK";
            result.timeNanos = elapsed;
            result.resultCount = results.size();
            m_results.append(result);
            
            printResult(result, true);
        }
        
        // Scenario 3: Get all contracts for a symbol
        {
            QElapsedTimer timer;
            timer.start();
            
            QString symbol = "BANKNIFTY";
            
            // Get all contracts for this symbol
            QVector<ContractData> contracts = m_nsefo->getContractsBySymbol(symbol);
            
            // Filter for options only
            QVector<ContractData> optionContracts;
            for (const auto& c : contracts) {
                if (c.instrumentType == 2) { // Options
                    optionContracts.append(c);
                }
            }
            
            qint64 elapsed = timer.nsecsElapsed();
            
            BenchmarkResult result;
            result.operation = QString("Get %1 options").arg(symbol);
            result.timeNanos = elapsed;
            result.resultCount = optionContracts.size();
            m_results.append(result);
            
            printResult(result, true);
        }
        
        std::cout << std::endl;
    }
    
    BenchmarkResult measureSeriesFilter(const QString& series) {
        QElapsedTimer timer;
        timer.start();
        
        QVector<ContractData> results = m_nsefo->getContractsBySeries(series);
        
        qint64 elapsed = timer.nsecsElapsed();
        
        BenchmarkResult result;
        result.operation = QString("Series: %1").arg(series);
        result.timeNanos = elapsed;
        result.resultCount = results.size();
        
        return result;
    }
    
    BenchmarkResult measureSymbolFilter(const QString& symbol) {
        QElapsedTimer timer;
        timer.start();
        
        QVector<ContractData> results = m_nsefo->getContractsBySymbol(symbol);
        
        qint64 elapsed = timer.nsecsElapsed();
        
        BenchmarkResult result;
        result.operation = QString("Symbol: %1").arg(symbol.leftJustified(12));
        result.timeNanos = elapsed;
        result.resultCount = results.size();
        
        return result;
    }
    
    void printResult(const BenchmarkResult& result, bool showCount) {
        std::cout << "  " << result.operation.toStdString() << ": ";
        
        // Color code based on performance
        const char* color = GREEN;
        if (result.timeMs() > 10.0) {
            color = RED;
        } else if (result.timeMs() > 1.0) {
            color = YELLOW;
        }
        
        std::cout << color << std::fixed << std::setprecision(3);
        
        if (result.timeNanos < 1000000) {
            // Less than 1ms - show in microseconds
            std::cout << result.timeUs() << " µs";
        } else {
            // 1ms or more - show in milliseconds
            std::cout << result.timeMs() << " ms";
        }
        
        std::cout << RESET;
        
        if (showCount) {
            std::cout << "  (" << result.resultCount << " results)";
        }
        
        std::cout << std::endl;
    }
    
    void printSummary() {
        std::cout << BOLD << CYAN 
                  << "╔══════════════════════════════════════════════════════════╗\n"
                  << "║                    Summary Statistics                   ║\n"
                  << "╚══════════════════════════════════════════════════════════╝\n"
                  << RESET << std::endl;
        
        // Calculate statistics
        qint64 totalTime = 0;
        qint64 minTime = std::numeric_limits<qint64>::max();
        qint64 maxTime = 0;
        
        for (const auto& result : m_results) {
            totalTime += result.timeNanos;
            minTime = qMin(minTime, result.timeNanos);
            maxTime = qMax(maxTime, result.timeNanos);
        }
        
        double avgTimeMs = (totalTime / m_results.size()) / 1000000.0;
        double minTimeMs = minTime / 1000000.0;
        double maxTimeMs = maxTime / 1000000.0;
        
        std::cout << std::fixed << std::setprecision(3);
        std::cout << "  Total tests: " << m_results.size() << "\n";
        std::cout << "  Average time: " << avgTimeMs << " ms\n";
        std::cout << "  Min time: " << minTimeMs << " ms\n";
        std::cout << "  Max time: " << maxTimeMs << " ms\n";
        std::cout << std::endl;
        
        // Performance assessment
        std::cout << BOLD << "Performance Assessment:\n" << RESET;
        
        if (avgTimeMs < 1.0) {
            std::cout << GREEN << "  ✓ Excellent" << RESET 
                      << " - All queries under 1ms average\n";
        } else if (avgTimeMs < 5.0) {
            std::cout << YELLOW << "  ⚠ Good" << RESET 
                      << " - Queries average " << avgTimeMs << "ms\n";
        } else {
            std::cout << RED << "  ✗ Needs Optimization" << RESET 
                      << " - Queries average " << avgTimeMs << "ms\n";
            std::cout << "  " << BOLD << "Recommendation:" << RESET 
                      << " Consider implementing multi-index optimization\n";
            std::cout << "  Expected improvement: 500-1000x faster (to ~0.01ms)\n";
        }
        
        std::cout << std::endl;
    }
    
    RepositoryManager* m_repoMgr;
    const NSEFORepository* m_nsefo;
    QVector<BenchmarkResult> m_results;
};

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    
    // Get masters path from command line or use default
    QString mastersPath = (argc > 1) ? argv[1] : "../MasterFiles";
    
    std::cout << BOLD << "Loading repository from: " << RESET 
              << mastersPath.toStdString() << std::endl;
    
    // Load repository
    RepositoryManager* repoMgr = RepositoryManager::getInstance();
    
    QElapsedTimer loadTimer;
    loadTimer.start();
    
    if (!repoMgr->loadAll(mastersPath)) {
        std::cerr << RED << "Failed to load repository!" << RESET << std::endl;
        return 1;
    }
    
    qint64 loadTime = loadTimer.elapsed();
    
    std::cout << GREEN << "Repository loaded successfully in " 
              << loadTime << " ms" << RESET << std::endl;
    
    // Run benchmarks
    RepositoryBenchmark benchmark(repoMgr);
    benchmark.runAll();
    
    return 0;
}
