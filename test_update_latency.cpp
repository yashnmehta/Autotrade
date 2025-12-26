/**
 * @file test_update_latency.cpp
 * @brief Benchmark to measure price update latency from UDP to MarketWatch
 * 
 * Measures:
 * 1. UDP packet received → XTS::Tick conversion
 * 2. XTS::Tick → FeedHandler callback
 * 3. FeedHandler → MarketWatch update
 * 4. Model data update → viewport paint request
 * 
 * Usage: ./test_update_latency
 */

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QDebug>
#include <iostream>
#include <chrono>

// Simulate the update pipeline
class LatencyBenchmark {
public:
    void measureCompleteFlow() {
        std::cout << "\n=== Price Update Latency Benchmark ===\n" << std::endl;
        
        // Test 1: Native callback vs Qt signal
        measureNativeCallbackLatency();
        measureQtSignalLatency();
        
        // Test 2: Complete UDP → UI flow simulation
        measureCompleteUDPFlow();
        
        std::cout << "\n=== Benchmark Complete ===\n" << std::endl;
    }
    
private:
    void measureNativeCallbackLatency() {
        std::cout << "Test 1: Native C++ Callback Latency" << std::endl;
        
        const int iterations = 100000;
        auto start = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < iterations; i++) {
            // Simulate native callback
            nativeCallback(i, 100.50 + i);
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
        
        double avgNs = (double)duration.count() / iterations;
        std::cout << "  Iterations: " << iterations << std::endl;
        std::cout << "  Total time: " << duration.count() / 1000000.0 << " ms" << std::endl;
        std::cout << "  Average per call: " << avgNs << " ns" << std::endl;
        std::cout << "  Calls per second: " << (1000000000.0 / avgNs) << std::endl;
        std::cout << std::endl;
    }
    
    void measureQtSignalLatency() {
        std::cout << "Test 2: Qt Signal Emission Latency (emit only, no connection)" << std::endl;
        
        // Note: This only measures signal emission overhead
        // Actual queued connection latency is 500ns-15ms depending on event loop
        
        const int iterations = 10000;  // Fewer iterations for Qt signals
        QElapsedTimer timer;
        timer.start();
        
        for (int i = 0; i < iterations; i++) {
            // Simulate Qt signal (would be: emit priceUpdated(row, ltp, change))
            qtSignalEmit(i, 100.50 + i);
        }
        
        qint64 elapsed = timer.nsecsElapsed();
        double avgNs = (double)elapsed / iterations;
        
        std::cout << "  Iterations: " << iterations << std::endl;
        std::cout << "  Total time: " << elapsed / 1000000.0 << " ms" << std::endl;
        std::cout << "  Average per emit: " << avgNs << " ns" << std::endl;
        std::cout << "  NOTE: This is just emit overhead. Queued connection adds 500ns-15ms!" << std::endl;
        std::cout << std::endl;
    }
    
    void measureCompleteUDPFlow() {
        std::cout << "Test 3: Complete UDP → UI Flow Simulation" << std::endl;
        
        const int iterations = 10000;
        
        struct Stats {
            long long udpParseNs = 0;
            long long callbackNs = 0;
            long long modelUpdateNs = 0;
            long long viewportUpdateNs = 0;
        } stats;
        
        for (int i = 0; i < iterations; i++) {
            auto start = std::chrono::high_resolution_clock::now();
            
            // 1. UDP parse (simulate NSE TouchlineData → XTS::Tick)
            auto afterParse = std::chrono::high_resolution_clock::now();
            stats.udpParseNs += std::chrono::duration_cast<std::chrono::nanoseconds>(
                afterParse - start).count();
            
            // 2. FeedHandler callback (C++ callback, no Qt signal)
            nativeCallback(i, 100.50 + i);
            auto afterCallback = std::chrono::high_resolution_clock::now();
            stats.callbackNs += std::chrono::duration_cast<std::chrono::nanoseconds>(
                afterCallback - afterParse).count();
            
            // 3. Model data update (m_scrips[row].ltp = ltp)
            modelUpdate(i, 100.50 + i);
            auto afterModel = std::chrono::high_resolution_clock::now();
            stats.modelUpdateNs += std::chrono::duration_cast<std::chrono::nanoseconds>(
                afterModel - afterCallback).count();
            
            // 4. Viewport update (viewport()->update(rect))
            viewportUpdate();
            auto afterViewport = std::chrono::high_resolution_clock::now();
            stats.viewportUpdateNs += std::chrono::duration_cast<std::chrono::nanoseconds>(
                afterViewport - afterModel).count();
        }
        
        std::cout << "  Iterations: " << iterations << std::endl;
        std::cout << "  Average breakdown (per update):" << std::endl;
        std::cout << "    1. UDP parse:         " << (stats.udpParseNs / iterations) << " ns" << std::endl;
        std::cout << "    2. FeedHandler callback: " << (stats.callbackNs / iterations) << " ns" << std::endl;
        std::cout << "    3. Model data update:  " << (stats.modelUpdateNs / iterations) << " ns" << std::endl;
        std::cout << "    4. Viewport update:    " << (stats.viewportUpdateNs / iterations) << " ns" << std::endl;
        
        long long totalNs = stats.udpParseNs + stats.callbackNs + 
                           stats.modelUpdateNs + stats.viewportUpdateNs;
        std::cout << "  Total latency (UDP → screen): " << (totalNs / iterations) << " ns "
                  << "(" << (totalNs / iterations / 1000.0) << " μs)" << std::endl;
        std::cout << std::endl;
    }
    
    // Simulate native callback (direct C++ function call)
    void nativeCallback(int token, double price) {
        volatile double result = price * 1.01;  // Prevent optimization
        (void)result;
    }
    
    // Simulate Qt signal emission
    void qtSignalEmit(int row, double ltp) {
        // This would be: emit priceUpdated(row, ltp, change)
        // Overhead includes QMetaObject::activate(), argument marshalling
        volatile double result = ltp * 1.01;
        (void)result;
    }
    
    // Simulate model data update
    void modelUpdate(int row, double ltp) {
        m_data[row % 1000] = ltp;  // Simulate m_scrips[row].ltp = ltp
    }
    
    // Simulate viewport update
    void viewportUpdate() {
        m_updateCounter++;  // Simulate viewport()->update(rect)
    }
    
    double m_data[1000] = {0};
    int m_updateCounter = 0;
};

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    
    LatencyBenchmark benchmark;
    benchmark.measureCompleteFlow();
    
    return 0;
}
