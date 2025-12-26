/*
 * Benchmark: Qt Signals vs C++ Callbacks for Cross-Component Communication
 * 
 * Tests communication pattern: Parent → Child A → Child B (sibling communication)
 * 
 * Scenario: Market data arrives → Parser processes → Multiple views update
 * (Common pattern: UDP Parser → FeedHandler → MarketWatch, OrderBook, Chart, etc.)
 * 
 * Compares:
 * 1. Qt Signals/Slots - emit signal from child A, received by child B
 * 2. C++ Callbacks - Direct function pointer invocation
 * 3. Observer Pattern - Traditional C++ observer with std::function
 */

#include <QCoreApplication>
#include <QObject>
#include <QTimer>
#include <QElapsedTimer>
#include <QDebug>
#include <iostream>
#include <vector>
#include <functional>
#include <chrono>
#include <random>
#include <numeric>
#include <memory>

// ============================================================================
// CONFIGURATION
// ============================================================================

constexpr int NUM_SUBSCRIBERS = 5;        // Number of child objects listening
constexpr int MESSAGES_PER_TEST = 10000;  // How many messages to send
constexpr int NUM_TEST_RUNS = 5;          // Repeat tests for consistency

// ============================================================================
// TEST DATA: Simulated market tick
// ============================================================================

struct MarketTick {
    int token;
    double ltp;
    double bid;
    double ask;
    long volume;
    long timestamp;
    
    MarketTick() : token(0), ltp(0), bid(0), ask(0), volume(0), timestamp(0) {}
    
    MarketTick(int t, double l, double b, double a, long v, long ts)
        : token(t), ltp(l), bid(b), ask(a), volume(v), timestamp(ts) {}
};

// ============================================================================
// METHOD 1: Qt Signals/Slots
// ============================================================================

class QtPublisher : public QObject {
    Q_OBJECT
public:
    void publishTick(const MarketTick& tick) {
        emit tickReceived(tick);
    }

signals:
    void tickReceived(const MarketTick& tick);
};

class QtSubscriber : public QObject {
    Q_OBJECT
public:
    QtSubscriber(int id, QObject* parent = nullptr) 
        : QObject(parent), m_id(id), m_ticksReceived(0) {}
    
    int getTicksReceived() const { return m_ticksReceived; }
    void reset() { m_ticksReceived = 0; }

public slots:
    void onTickReceived(const MarketTick& tick) {
        // Simulate processing (like updating UI, calculating indicators, etc.)
        volatile double calc = tick.ltp * tick.volume;
        (void)calc; // Prevent optimization
        m_ticksReceived++;
    }

private:
    int m_id;
    int m_ticksReceived;
};

// ============================================================================
// METHOD 2: C++ Callbacks (Function Pointers)
// ============================================================================

class CallbackSubscriber;

using TickCallback = void (*)(CallbackSubscriber*, const MarketTick&);

class CallbackPublisher {
public:
    void subscribe(CallbackSubscriber* subscriber, TickCallback callback) {
        m_subscribers.push_back({subscriber, callback});
    }
    
    void publishTick(const MarketTick& tick) {
        for (auto& sub : m_subscribers) {
            sub.callback(sub.subscriber, tick);
        }
    }
    
private:
    struct Subscription {
        CallbackSubscriber* subscriber;
        TickCallback callback;
    };
    std::vector<Subscription> m_subscribers;
};

class CallbackSubscriber {
public:
    CallbackSubscriber(int id) : m_id(id), m_ticksReceived(0) {}
    
    static void onTickReceived(CallbackSubscriber* self, const MarketTick& tick) {
        // Simulate processing
        volatile double calc = tick.ltp * tick.volume;
        (void)calc;
        self->m_ticksReceived++;
    }
    
    int getTicksReceived() const { return m_ticksReceived; }
    void reset() { m_ticksReceived = 0; }

private:
    int m_id;
    int m_ticksReceived;
};

// ============================================================================
// METHOD 3: Observer Pattern (std::function)
// ============================================================================

class ObserverPublisher {
public:
    using TickHandler = std::function<void(const MarketTick&)>;
    
    void subscribe(TickHandler handler) {
        m_handlers.push_back(handler);
    }
    
    void publishTick(const MarketTick& tick) {
        for (auto& handler : m_handlers) {
            handler(tick);
        }
    }
    
private:
    std::vector<TickHandler> m_handlers;
};

class ObserverSubscriber {
public:
    ObserverSubscriber(int id) : m_id(id), m_ticksReceived(0) {}
    
    void onTickReceived(const MarketTick& tick) {
        // Simulate processing
        volatile double calc = tick.ltp * tick.volume;
        (void)calc;
        m_ticksReceived++;
    }
    
    int getTicksReceived() const { return m_ticksReceived; }
    void reset() { m_ticksReceived = 0; }

private:
    int m_id;
    int m_ticksReceived;
};

// ============================================================================
// BENCHMARK CONTROLLER
// ============================================================================

class BenchmarkController {
public:
    BenchmarkController() {
        setupTestData();
    }
    
    void runAllTests() {
        std::cout << "========================================\n";
        std::cout << "Signal vs Callback Communication Test\n";
        std::cout << "========================================\n";
        std::cout << "Configuration:\n";
        std::cout << "  Subscribers: " << NUM_SUBSCRIBERS << "\n";
        std::cout << "  Messages: " << MESSAGES_PER_TEST << "\n";
        std::cout << "  Test runs: " << NUM_TEST_RUNS << "\n";
        std::cout << "========================================\n\n";
        
        // Test 1: Qt Signals/Slots
        auto qtResults = testQtSignals();
        
        // Test 2: C++ Callbacks
        auto callbackResults = testCallbacks();
        
        // Test 3: Observer Pattern
        auto observerResults = testObserver();
        
        // Display results
        displayResults(qtResults, callbackResults, observerResults);
    }

private:
    void setupTestData() {
        // Generate random market ticks
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<> priceDist(100.0, 500.0);
        std::uniform_int_distribution<> volumeDist(100, 10000);
        std::uniform_int_distribution<> tokenDist(1000, 9999);
        
        m_testTicks.reserve(MESSAGES_PER_TEST);
        for (int i = 0; i < MESSAGES_PER_TEST; ++i) {
            int token = tokenDist(gen);
            double ltp = priceDist(gen);
            double bid = ltp - 0.5;
            double ask = ltp + 0.5;
            long volume = volumeDist(gen);
            long timestamp = i;
            
            m_testTicks.emplace_back(token, ltp, bid, ask, volume, timestamp);
        }
    }
    
    struct TestResult {
        std::string name;
        std::vector<long long> latencies; // nanoseconds
        long long totalTime;
        int messagesDelivered;
        
        double getAverageLatencyNs() const {
            if (latencies.empty()) return 0.0;
            return std::accumulate(latencies.begin(), latencies.end(), 0LL) / 
                   static_cast<double>(latencies.size());
        }
        
        double getMedianLatencyNs() const {
            if (latencies.empty()) return 0.0;
            auto sorted = latencies;
            std::sort(sorted.begin(), sorted.end());
            return sorted[sorted.size() / 2];
        }
        
        long long getMaxLatencyNs() const {
            if (latencies.empty()) return 0;
            return *std::max_element(latencies.begin(), latencies.end());
        }
        
        long long getMinLatencyNs() const {
            if (latencies.empty()) return 0;
            return *std::min_element(latencies.begin(), latencies.end());
        }
    };
    
    TestResult testQtSignals() {
        TestResult result;
        result.name = "Qt Signals/Slots";
        
        std::cout << "Testing Qt Signals/Slots...\n";
        
        for (int run = 0; run < NUM_TEST_RUNS; ++run) {
            // Setup
            QtPublisher publisher;
            std::vector<std::unique_ptr<QtSubscriber>> subscribers;
            
            for (int i = 0; i < NUM_SUBSCRIBERS; ++i) {
                auto sub = std::make_unique<QtSubscriber>(i);
                QObject::connect(&publisher, &QtPublisher::tickReceived,
                               sub.get(), &QtSubscriber::onTickReceived);
                subscribers.push_back(std::move(sub));
            }
            
            // Test
            auto startTime = std::chrono::high_resolution_clock::now();
            
            for (const auto& tick : m_testTicks) {
                auto tickStart = std::chrono::high_resolution_clock::now();
                publisher.publishTick(tick);
                auto tickEnd = std::chrono::high_resolution_clock::now();
                
                auto latency = std::chrono::duration_cast<std::chrono::nanoseconds>(
                    tickEnd - tickStart).count();
                result.latencies.push_back(latency);
            }
            
            auto endTime = std::chrono::high_resolution_clock::now();
            result.totalTime += std::chrono::duration_cast<std::chrono::microseconds>(
                endTime - startTime).count();
            
            // Verify delivery
            for (const auto& sub : subscribers) {
                result.messagesDelivered += sub->getTicksReceived();
            }
            
            std::cout << "  Run " << (run + 1) << "/" << NUM_TEST_RUNS 
                      << " - Avg: " << (result.getAverageLatencyNs() / 1000.0) << " µs\n";
        }
        
        result.totalTime /= NUM_TEST_RUNS;
        return result;
    }
    
    TestResult testCallbacks() {
        TestResult result;
        result.name = "C++ Callbacks";
        
        std::cout << "\nTesting C++ Callbacks...\n";
        
        for (int run = 0; run < NUM_TEST_RUNS; ++run) {
            // Setup
            CallbackPublisher publisher;
            std::vector<std::unique_ptr<CallbackSubscriber>> subscribers;
            
            for (int i = 0; i < NUM_SUBSCRIBERS; ++i) {
                auto sub = std::make_unique<CallbackSubscriber>(i);
                publisher.subscribe(sub.get(), &CallbackSubscriber::onTickReceived);
                subscribers.push_back(std::move(sub));
            }
            
            // Test
            auto startTime = std::chrono::high_resolution_clock::now();
            
            for (const auto& tick : m_testTicks) {
                auto tickStart = std::chrono::high_resolution_clock::now();
                publisher.publishTick(tick);
                auto tickEnd = std::chrono::high_resolution_clock::now();
                
                auto latency = std::chrono::duration_cast<std::chrono::nanoseconds>(
                    tickEnd - tickStart).count();
                result.latencies.push_back(latency);
            }
            
            auto endTime = std::chrono::high_resolution_clock::now();
            result.totalTime += std::chrono::duration_cast<std::chrono::microseconds>(
                endTime - startTime).count();
            
            // Verify delivery
            for (const auto& sub : subscribers) {
                result.messagesDelivered += sub->getTicksReceived();
            }
            
            std::cout << "  Run " << (run + 1) << "/" << NUM_TEST_RUNS 
                      << " - Avg: " << (result.getAverageLatencyNs() / 1000.0) << " µs\n";
        }
        
        result.totalTime /= NUM_TEST_RUNS;
        return result;
    }
    
    TestResult testObserver() {
        TestResult result;
        result.name = "Observer Pattern (std::function)";
        
        std::cout << "\nTesting Observer Pattern...\n";
        
        for (int run = 0; run < NUM_TEST_RUNS; ++run) {
            // Setup
            ObserverPublisher publisher;
            std::vector<std::unique_ptr<ObserverSubscriber>> subscribers;
            
            for (int i = 0; i < NUM_SUBSCRIBERS; ++i) {
                auto sub = std::make_unique<ObserverSubscriber>(i);
                publisher.subscribe([sub = sub.get()](const MarketTick& tick) {
                    sub->onTickReceived(tick);
                });
                subscribers.push_back(std::move(sub));
            }
            
            // Test
            auto startTime = std::chrono::high_resolution_clock::now();
            
            for (const auto& tick : m_testTicks) {
                auto tickStart = std::chrono::high_resolution_clock::now();
                publisher.publishTick(tick);
                auto tickEnd = std::chrono::high_resolution_clock::now();
                
                auto latency = std::chrono::duration_cast<std::chrono::nanoseconds>(
                    tickEnd - tickStart).count();
                result.latencies.push_back(latency);
            }
            
            auto endTime = std::chrono::high_resolution_clock::now();
            result.totalTime += std::chrono::duration_cast<std::chrono::microseconds>(
                endTime - startTime).count();
            
            // Verify delivery
            for (const auto& sub : subscribers) {
                result.messagesDelivered += sub->getTicksReceived();
            }
            
            std::cout << "  Run " << (run + 1) << "/" << NUM_TEST_RUNS 
                      << " - Avg: " << (result.getAverageLatencyNs() / 1000.0) << " µs\n";
        }
        
        result.totalTime /= NUM_TEST_RUNS;
        return result;
    }
    
    void displayResults(const TestResult& qt, const TestResult& callback, const TestResult& observer) {
        std::cout << "\n========================================\n";
        std::cout << "BENCHMARK RESULTS\n";
        std::cout << "========================================\n\n";
        
        // Summary table
        std::cout << "Method                        | Avg (µs) | Median (µs) | Min (ns) | Max (µs) | Total (ms) | Msgs\n";
        std::cout << "------------------------------+----------+-------------+----------+----------+------------+--------\n";
        
        auto printRow = [](const TestResult& r) {
            printf("%-29s | %8.2f | %11.2f | %8lld | %8.2f | %10lld | %6d\n",
                   r.name.c_str(),
                   r.getAverageLatencyNs() / 1000.0,
                   r.getMedianLatencyNs() / 1000.0,
                   r.getMinLatencyNs(),
                   r.getMaxLatencyNs() / 1000.0,
                   r.totalTime / 1000,
                   r.messagesDelivered);
        };
        
        printRow(qt);
        printRow(callback);
        printRow(observer);
        
        std::cout << "\n========================================\n";
        std::cout << "ANALYSIS\n";
        std::cout << "========================================\n\n";
        
        // Compare Qt vs Callback
        double qtAvg = qt.getAverageLatencyNs();
        double callbackAvg = callback.getAverageLatencyNs();
        double observerAvg = observer.getAverageLatencyNs();
        
        std::cout << "Per-message latency comparison:\n";
        printf("  Qt Signals:     %.2f µs\n", qtAvg / 1000.0);
        printf("  C++ Callbacks:  %.2f µs (%.1fx faster)\n", 
               callbackAvg / 1000.0, qtAvg / callbackAvg);
        printf("  Observer:       %.2f µs (%.1fx faster than Qt)\n\n", 
               observerAvg / 1000.0, qtAvg / observerAvg);
        
        std::cout << "Total time for " << MESSAGES_PER_TEST << " messages to " 
                  << NUM_SUBSCRIBERS << " subscribers:\n";
        printf("  Qt Signals:     %lld ms\n", qt.totalTime / 1000);
        printf("  C++ Callbacks:  %lld ms (%.1fx faster)\n", 
               callback.totalTime / 1000, 
               static_cast<double>(qt.totalTime) / callback.totalTime);
        printf("  Observer:       %lld ms (%.1fx faster than Qt)\n\n", 
               observer.totalTime / 1000,
               static_cast<double>(qt.totalTime) / observer.totalTime);
        
        // Recommendations
        std::cout << "========================================\n";
        std::cout << "RECOMMENDATIONS\n";
        std::cout << "========================================\n\n";
        
        double speedupCallback = qtAvg / callbackAvg;
        double speedupObserver = qtAvg / observerAvg;
        
        if (speedupCallback < 2.0 && speedupObserver < 2.0) {
            std::cout << "✅ Qt Signals are FAST ENOUGH\n";
            std::cout << "   - Difference: <2x slower than raw callbacks\n";
            std::cout << "   - Both methods deliver in <" << (qtAvg / 1000.0) << " µs\n";
            std::cout << "   - Recommendation: Use Qt Signals for:\n";
            std::cout << "     • Cross-thread communication (thread-safe)\n";
            std::cout << "     • Loose coupling between components\n";
            std::cout << "     • When you need Qt's connection management\n";
            std::cout << "     • GUI applications (standard Qt pattern)\n\n";
            
            std::cout << "⚡ Use C++ Callbacks when:\n";
            std::cout << "   - Same-thread communication only\n";
            std::cout << "   - Need absolute minimum latency\n";
            std::cout << "   - High-frequency updates (>10000/sec)\n";
            std::cout << "   - No Qt dependency desired\n\n";
        } else {
            std::cout << "⚡ C++ Callbacks significantly faster (>" << speedupCallback << "x)\n";
            std::cout << "   - Consider using callbacks for hot paths\n";
            std::cout << "   - Qt Signals still acceptable for <1000 msgs/sec\n\n";
        }
        
        // Memory/Complexity tradeoff
        std::cout << "========================================\n";
        std::cout << "TRADEOFFS\n";
        std::cout << "========================================\n\n";
        
        std::cout << "Qt Signals/Slots:\n";
        std::cout << "  ✅ Thread-safe by design\n";
        std::cout << "  ✅ Automatic connection management\n";
        std::cout << "  ✅ Type-safe\n";
        std::cout << "  ✅ Can cross thread boundaries\n";
        std::cout << "  ✅ Well-documented Qt pattern\n";
        std::cout << "  ❌ Slightly slower (" << (qtAvg / 1000.0) << " µs)\n";
        std::cout << "  ❌ Requires QObject inheritance\n\n";
        
        std::cout << "C++ Callbacks:\n";
        std::cout << "  ✅ Fastest (" << (callbackAvg / 1000.0) << " µs)\n";
        std::cout << "  ✅ No Qt dependency\n";
        std::cout << "  ✅ Direct function call\n";
        std::cout << "  ❌ Manual lifetime management\n";
        std::cout << "  ❌ NOT thread-safe (same thread only)\n";
        std::cout << "  ❌ More complex code\n\n";
        
        std::cout << "Observer Pattern:\n";
        std::cout << "  ✅ Fast (" << (observerAvg / 1000.0) << " µs)\n";
        std::cout << "  ✅ Flexible with std::function\n";
        std::cout << "  ✅ Can capture context with lambdas\n";
        std::cout << "  ❌ Manual lifetime management\n";
        std::cout << "  ❌ NOT thread-safe\n";
        std::cout << "  ❌ std::function overhead\n\n";
        
        std::cout << "========================================\n";
        std::cout << "CONCLUSION\n";
        std::cout << "========================================\n\n";
        
        if (qtAvg / 1000.0 < 10.0) { // Less than 10 µs
            std::cout << "For your trading terminal application:\n\n";
            std::cout << "✅ RECOMMENDED: Qt Signals/Slots\n";
            std::cout << "   Reason: " << (qtAvg / 1000.0) << " µs latency is negligible\n";
            std::cout << "           compared to network latency (1000-5000 µs)\n";
            std::cout << "           and UI refresh rate (16667 µs for 60 FPS)\n\n";
            std::cout << "   Use for: UDP Parser → FeedHandler → MarketWatch\n";
            std::cout << "            Cross-component communication\n";
            std::cout << "            Thread-safe data delivery\n\n";
            
            std::cout << "⚡ OPTIONAL: C++ Callbacks for ultra-hot paths\n";
            std::cout << "   Use ONLY if profiling shows Qt signals as bottleneck\n";
            std::cout << "   Example: Inner loops processing >50000 msgs/sec\n\n";
        } else {
            std::cout << "⚠️  Qt Signals show " << (qtAvg / 1000.0) << " µs latency\n";
            std::cout << "   Consider C++ callbacks for high-frequency paths\n\n";
        }
        
        std::cout << "========================================\n";
    }
    
    std::vector<MarketTick> m_testTicks;
};

// ============================================================================
// MAIN
// ============================================================================

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    
    BenchmarkController controller;
    controller.runAllTests();
    
    return 0;
}

#include "test_signal_vs_callback_communication.moc"
