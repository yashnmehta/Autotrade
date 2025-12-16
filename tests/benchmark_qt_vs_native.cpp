/**
 * Performance Benchmark: Qt Classes vs Native C++
 * Measures actual latency overhead in nanoseconds
 */

#include <QTimer>
#include <QElapsedTimer>
#include <QCoreApplication>
#include <QDebug>
#include <chrono>
#include <thread>
#include <vector>
#include <numeric>
#include <iostream>
#include <iomanip>

// Benchmark configuration
constexpr int ITERATIONS = 10000;
constexpr int WARMUP = 100;

// ============================================================================
// Timer Benchmarks
// ============================================================================

// Qt Timer test
class QtTimerTest : public QObject {
    Q_OBJECT
public:
    void run() {
        m_start = std::chrono::high_resolution_clock::now();
        
        QTimer::singleShot(0, this, [this]() {
            auto end = std::chrono::high_resolution_clock::now();
            m_elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(end - m_start).count();
            emit finished();
        });
    }
    
    int64_t elapsed() const { return m_elapsed; }

signals:
    void finished();

private:
    std::chrono::high_resolution_clock::time_point m_start;
    int64_t m_elapsed = 0;
};

// Native C++ Timer test
int64_t nativeTimerTest() {
    auto start = std::chrono::high_resolution_clock::now();
    
    // Simulate minimal timer operation
    std::this_thread::sleep_for(std::chrono::microseconds(0));
    
    auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
}

// ============================================================================
// Network Send Simulation
// ============================================================================

// Qt Network overhead simulation
int64_t qtNetworkOverhead() {
    auto start = std::chrono::high_resolution_clock::now();
    
    // Simulate Qt event loop overhead
    QCoreApplication::processEvents();
    
    auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
}

// Native C++ network simulation
int64_t nativeNetworkOverhead() {
    auto start = std::chrono::high_resolution_clock::now();
    
    // Simulate minimal network operation overhead
    volatile int dummy = 0;
    dummy++;
    
    auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
}

// ============================================================================
// Statistics
// ============================================================================

struct BenchmarkStats {
    double mean;
    double min;
    double max;
    double p50;  // median
    double p95;
    double p99;
    
    static BenchmarkStats calculate(std::vector<int64_t>& measurements) {
        std::sort(measurements.begin(), measurements.end());
        
        BenchmarkStats stats;
        stats.mean = std::accumulate(measurements.begin(), measurements.end(), 0.0) / measurements.size();
        stats.min = measurements.front();
        stats.max = measurements.back();
        stats.p50 = measurements[measurements.size() * 50 / 100];
        stats.p95 = measurements[measurements.size() * 95 / 100];
        stats.p99 = measurements[measurements.size() * 99 / 100];
        
        return stats;
    }
    
    void print(const std::string& name) const {
        std::cout << "\n" << name << ":\n";
        std::cout << std::fixed << std::setprecision(2);
        std::cout << "  Mean:   " << mean << " ns\n";
        std::cout << "  Median: " << p50 << " ns\n";
        std::cout << "  P95:    " << p95 << " ns\n";
        std::cout << "  P99:    " << p99 << " ns\n";
        std::cout << "  Min:    " << min << " ns\n";
        std::cout << "  Max:    " << max << " ns\n";
    }
};

// ============================================================================
// Main Benchmark
// ============================================================================

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    
    std::cout << "=======================================================\n";
    std::cout << "Performance Benchmark: Qt vs Native C++\n";
    std::cout << "Iterations: " << ITERATIONS << " (after " << WARMUP << " warmup)\n";
    std::cout << "=======================================================\n";
    
    // ========================================================================
    // 1. Event Loop Overhead
    // ========================================================================
    
    std::cout << "\n[1] Qt Event Loop Overhead\n";
    std::vector<int64_t> qtEventLoop;
    qtEventLoop.reserve(ITERATIONS);
    
    // Warmup
    for (int i = 0; i < WARMUP; i++) {
        QCoreApplication::processEvents();
    }
    
    // Actual benchmark
    for (int i = 0; i < ITERATIONS; i++) {
        qtEventLoop.push_back(qtNetworkOverhead());
    }
    
    auto qtEventStats = BenchmarkStats::calculate(qtEventLoop);
    qtEventStats.print("Qt processEvents()");
    
    // Native baseline
    std::vector<int64_t> nativeBaseline;
    nativeBaseline.reserve(ITERATIONS);
    
    for (int i = 0; i < ITERATIONS; i++) {
        nativeBaseline.push_back(nativeNetworkOverhead());
    }
    
    auto nativeStats = BenchmarkStats::calculate(nativeBaseline);
    nativeStats.print("Native C++ (baseline)");
    
    double overheadMultiplier = qtEventStats.mean / nativeStats.mean;
    std::cout << "\n⚡ Qt overhead: " << std::fixed << std::setprecision(2) 
              << overheadMultiplier << "x slower (+" 
              << (qtEventStats.mean - nativeStats.mean) << " ns)\n";
    
    // ========================================================================
    // 2. Timer Latency
    // ========================================================================
    
    std::cout << "\n\n[2] Timer Latency Comparison\n";
    
    // Native timer
    std::vector<int64_t> nativeTimers;
    nativeTimers.reserve(ITERATIONS);
    
    for (int i = 0; i < WARMUP; i++) {
        nativeTimerTest();
    }
    
    for (int i = 0; i < ITERATIONS; i++) {
        nativeTimers.push_back(nativeTimerTest());
    }
    
    auto nativeTimerStats = BenchmarkStats::calculate(nativeTimers);
    nativeTimerStats.print("std::chrono + std::thread");
    
    // ========================================================================
    // 3. Memory Allocation
    // ========================================================================
    
    std::cout << "\n\n[3] Memory Allocation Overhead\n";
    
    std::vector<int64_t> qtAlloc;
    qtAlloc.reserve(ITERATIONS);
    
    for (int i = 0; i < ITERATIONS; i++) {
        auto start = std::chrono::high_resolution_clock::now();
        QString str = "test";
        auto end = std::chrono::high_resolution_clock::now();
        qtAlloc.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
    }
    
    auto qtAllocStats = BenchmarkStats::calculate(qtAlloc);
    qtAllocStats.print("QString allocation");
    
    std::vector<int64_t> nativeAlloc;
    nativeAlloc.reserve(ITERATIONS);
    
    for (int i = 0; i < ITERATIONS; i++) {
        auto start = std::chrono::high_resolution_clock::now();
        std::string str = "test";
        auto end = std::chrono::high_resolution_clock::now();
        nativeAlloc.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
    }
    
    auto nativeAllocStats = BenchmarkStats::calculate(nativeAlloc);
    nativeAllocStats.print("std::string allocation");
    
    double allocOverhead = qtAllocStats.mean / nativeAllocStats.mean;
    std::cout << "\n⚡ QString overhead: " << std::fixed << std::setprecision(2) 
              << allocOverhead << "x slower (+" 
              << (qtAllocStats.mean - nativeAllocStats.mean) << " ns)\n";
    
    // ========================================================================
    // Summary
    // ========================================================================
    
    std::cout << "\n\n=======================================================\n";
    std::cout << "SUMMARY - Latency Impact Analysis\n";
    std::cout << "=======================================================\n";
    std::cout << "\nFor 1,000 messages/second:\n";
    std::cout << "  Qt overhead per msg:     " << std::fixed << std::setprecision(0) 
              << qtEventStats.mean << " ns\n";
    std::cout << "  Total overhead/sec:      " << (qtEventStats.mean * 1000) / 1000.0 << " μs\n";
    std::cout << "  Annual latency cost:     " << (qtEventStats.mean * 1000 * 86400 * 365) / 1000000000.0 
              << " seconds/year\n";
    
    std::cout << "\nFor 10,000 messages/second:\n";
    std::cout << "  Total overhead/sec:      " << (qtEventStats.mean * 10000) / 1000.0 << " μs\n";
    std::cout << "  Annual latency cost:     " << (qtEventStats.mean * 10000 * 86400 * 365) / 1000000000.0 
              << " seconds/year\n";
    
    std::cout << "\n🎯 RECOMMENDATION:\n";
    if (overheadMultiplier > 2.0) {
        std::cout << "  ⚠️  Qt overhead is significant (" << overheadMultiplier << "x)\n";
        std::cout << "  ✅ Use native C++ for time-critical operations\n";
        std::cout << "  ✅ Reserve Qt for UI components only\n";
    } else {
        std::cout << "  ℹ️  Qt overhead is acceptable for this use case\n";
    }
    
    std::cout << "\n=======================================================\n";
    
    return 0;
}

#include "benchmark_qt_vs_native.moc"
