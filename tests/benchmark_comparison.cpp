/**
 * @file benchmark_comparison.cpp
 * @brief Side-by-side performance comparison: Baseline vs PreSorted
 * implementation
 *
 * Compares:
 * - NSEFORepository (current full-scan approach)
 * - NSEFORepositoryPreSorted (optimized multi-index + pre-sorted approach)
 *
 * Usage:
 *   ./tests/benchmark_comparison <path_to_masters_folder>
 */

#include "repository/NSEFORepository.h"
#include "repository/NSEFORepositoryPreSorted.h"
#include <QCoreApplication>
#include <QDebug>
#include <QElapsedTimer>
#include <iomanip>
#include <iostream>


#define RESET "\033[0m"
#define RED "\033[31m"
#define GREEN "\033[32m"
#define YELLOW "\033[33m"
#define BLUE "\033[34m"
#define CYAN "\033[36m"
#define BOLD "\033[1m"

struct BenchmarkResult {
  QString operation;
  qint64 currentTimeNanos;
  qint64 optimizedTimeNanos;
  int resultCount;

  double currentMs() const { return currentTimeNanos / 1000000.0; }
  double optimizedMs() const { return optimizedTimeNanos / 1000000.0; }
  double currentUs() const { return currentTimeNanos / 1000.0; }
  double optimizedUs() const { return optimizedTimeNanos / 1000.0; }
  double speedup() const {
    if (optimizedTimeNanos == 0)
      return 0;
    return (double)currentTimeNanos / optimizedTimeNanos;
  }
};

void printHeader() {
  std::cout << BOLD << CYAN
            << "\n╔════════════════════════════════════════════════════════════"
               "══════╗\n"
            << "║     Repository Performance: Baseline vs PreSorted Comparison "
               "    ║\n"
            << "╚══════════════════════════════════════════════════════════════"
               "════╝\n"
            << RESET << std::endl;
}

void printResult(const BenchmarkResult &result) {
  std::cout << "  " << std::left << std::setw(30)
            << result.operation.toStdString();

  // Current time
  if (result.currentTimeNanos < 1000000) {
    std::cout << YELLOW << std::right << std::setw(10) << std::fixed
              << std::setprecision(2) << result.currentUs() << " µs" << RESET;
  } else {
    std::cout << YELLOW << std::right << std::setw(10) << std::fixed
              << std::setprecision(3) << result.currentMs() << " ms" << RESET;
  }

  std::cout << " → ";

  // Optimized time
  if (result.optimizedTimeNanos < 1000000) {
    std::cout << GREEN << std::right << std::setw(10) << std::fixed
              << std::setprecision(2) << result.optimizedUs() << " µs" << RESET;
  } else {
    std::cout << GREEN << std::right << std::setw(10) << std::fixed
              << std::setprecision(3) << result.optimizedMs() << " ms" << RESET;
  }

  // Speedup
  double speedup = result.speedup();
  const char *color = (speedup > 100) ? GREEN : (speedup > 10) ? CYAN : YELLOW;
  std::cout << "  " << color << BOLD << std::right << std::setw(8) << std::fixed
            << std::setprecision(1) << speedup << "x" << RESET;

  std::cout << "  (" << result.resultCount << " results)" << std::endl;
}

int main(int argc, char *argv[]) {
  QCoreApplication app(argc, argv);

  QString csvPath = (argc > 1) ? QString(argv[1]) + "/nsefo_processed.csv"
                               : "../MasterFiles/nsefo_processed.csv";

  printHeader();

  // Load repositories
  std::cout << "Loading data into repositories...\n" << std::endl;

  NSEFORepository currentRepo;
  NSEFORepositoryPreSorted preSortedRepo;

  QElapsedTimer loadTimer;
  loadTimer.start();
  if (!currentRepo.loadProcessedCSV(csvPath)) {
    std::cerr << RED << "Failed to load current repo from "
              << csvPath.toStdString() << RESET << std::endl;
    return 1;
  }
  qint64 currentLoadTime = loadTimer.elapsed();

  loadTimer.restart();
  if (!preSortedRepo.loadProcessedCSV(csvPath)) {
    std::cerr << RED << "Failed to load pre-sorted repo from "
              << csvPath.toStdString() << RESET << std::endl;
    return 1;
  }
  qint64 preSortedLoadTime = loadTimer.elapsed();

  std::cout << "Load Times:\n";
  std::cout << "  Baseline (Full Scan):  " << currentLoadTime << " ms\n";
  std::cout << "  PreSorted (Indexed):   " << preSortedLoadTime << " ms\n";
  std::cout << "  Index Build Overhead:  "
            << (preSortedLoadTime - currentLoadTime)
            << " ms (one-time index build + date-based sorting)\n"
            << std::endl;

  QVector<BenchmarkResult> results;

  // Test 1: Series Filtering
  std::cout << BOLD << YELLOW
            << "Test 1: Series Filtering (Full Scan vs Multi-Index)\n"
            << RESET << std::endl;
  QStringList seriesToTest = {"OPTIDX", "OPTSTK", "FUTIDX", "FUTSTK"};

  for (const QString &series : seriesToTest) {
    BenchmarkResult result;
    result.operation = "Series: " + series;

    QElapsedTimer timer;

    // Current: Full scan
    timer.start();
    QVector<ContractData> res1 = currentRepo.getContractsBySeries(series);
    result.currentTimeNanos = timer.nsecsElapsed();

    // Optimized: Multi-Index lookup
    timer.restart();
    QVector<ContractData> res2 = preSortedRepo.getContractsBySeries(series);
    result.optimizedTimeNanos = timer.nsecsElapsed();

    result.resultCount = res1.size();
    printResult(result);
  }

  // Test 2: Symbol Filtering (Option Chains)
  std::cout << std::endl;
  std::cout << BOLD << YELLOW << "Test 2: Symbol Filtering (Option Chains)\n"
            << RESET << std::endl;
  QStringList symbolsToTest = {"NIFTY", "BANKNIFTY", "FINNIFTY", "RELIANCE",
                               "TCS",   "INFY",      "HDFCBANK", "SBIN"};

  for (const QString &symbol : symbolsToTest) {
    BenchmarkResult result;
    result.operation = "Symbol: " + symbol;

    QElapsedTimer timer;
    timer.start();
    QVector<ContractData> res1 = currentRepo.getContractsBySymbol(symbol);
    result.currentTimeNanos = timer.nsecsElapsed();

    // Optimized: Multi-Index lookup
    timer.restart();
    QVector<ContractData> res2 = preSortedRepo.getContractsBySymbol(symbol);
    result.optimizedTimeNanos = timer.nsecsElapsed();

    result.resultCount = res1.size();
    printResult(result);
  }

  // Test 3: Chained Filter + Sort (ATM Watch Scenario)
  std::cout << std::endl;
  std::cout << BOLD << YELLOW
            << "Test 3: Chained Filter + Sort (ATM Watch Scenario)\n"
            << RESET << std::endl;
  std::cout << "  Filter by Symbol → Filter by Expiry → Sort by Strike\n"
            << std::endl;

  QString testSymbol = "NIFTY";
  QString testExpiry = "27JAN2026";

  {
    BenchmarkResult result;
    result.operation = "NIFTY + 27JAN2026 + Sort";

    QElapsedTimer timer;
    timer.start();

    // Current Implementation (Full Scan)
    QVector<ContractData> niftyContracts =
        currentRepo.getContractsBySymbol(testSymbol);
    QVector<ContractData> filteredCurrent;
    for (const auto &contract : niftyContracts) {
      if (contract.expiryDate == testExpiry && contract.instrumentType == 2) {
        filteredCurrent.append(contract);
      }
    }
    std::sort(filteredCurrent.begin(), filteredCurrent.end(),
              [](const ContractData &a, const ContractData &b) {
                return a.strikePrice < b.strikePrice;
              });

    result.currentTimeNanos = timer.nsecsElapsed();

    // PreSorted Implementation (Binary Search + Range Extraction)
    timer.restart();
    QVector<ContractData> preSortedResults =
        preSortedRepo.getContractsBySymbolAndExpiry(testSymbol, testExpiry,
                                                    2 // 2 = options only
        );
    // Note: PreSorted results are ALREADY sorted by strike price by design!

    result.optimizedTimeNanos = timer.nsecsElapsed();
    result.resultCount = preSortedResults.size();

    results.append(result);
    printResult(result);
  }

  // Summary
  std::cout << std::endl;
  std::cout << BOLD << CYAN
            << "╔══════════════════════════════════════════════════════════════"
               "════╗\n"
            << "║                         Summary Statistics                   "
               "    ║\n"
            << "╚══════════════════════════════════════════════════════════════"
               "════╝\n"
            << RESET << std::endl;

  if (!results.isEmpty()) {
    double avgSpeedup = 0;
    for (const auto &res : results)
      avgSpeedup += res.speedup();
    avgSpeedup /= results.size();

    std::cout << "  Total tests: " << results.size() << std::endl;
    std::cout << "  Average speedup: " << BOLD << GREEN << std::fixed
              << std::setprecision(1) << avgSpeedup << "x" << RESET
              << std::endl;
  }

  std::cout << "\n✓ Baseline vs PreSorted Comparison Complete!\n";

  return 0;
}
