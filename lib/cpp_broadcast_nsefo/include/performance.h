#ifndef PERFORMANCE_H
#define PERFORMANCE_H

#include <chrono>
#include <string>
#include <map>
#include <iostream>
#include <iomanip>

class PerformanceTimer {
public:
    void start(const std::string& name) {
        timers[name] = std::chrono::high_resolution_clock::now();
    }
    
    double stop(const std::string& name) {
        auto end = std::chrono::high_resolution_clock::now();
        auto it = timers.find(name);
        if (it == timers.end()) return 0.0;
        
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - it->second);
        double us = duration.count();
        
        // Update statistics
        auto& stats = statistics[name];
        stats.count++;
        stats.total_us += us;
        stats.min_us = (stats.count == 1) ? us : std::min(stats.min_us, us);
        stats.max_us = std::max(stats.max_us, us);
        
        timers.erase(it);
        return us;
    }
    
    void printStats() const {
        std::cout << "\n=== Performance Statistics ===" << std::endl;
        std::cout << std::left << std::setw(30) << "Operation"
                  << std::right << std::setw(10) << "Count"
                  << std::setw(12) << "Avg(μs)"
                  << std::setw(12) << "Min(μs)"
                  << std::setw(12) << "Max(μs)"
                  << std::setw(12) << "Total(ms)" << std::endl;
        std::cout << std::string(88, '-') << std::endl;
        
        for (const auto& pair : statistics) {
            const auto& stats = pair.second;
            double avg = stats.total_us / stats.count;
            
            std::cout << std::left << std::setw(30) << pair.first
                      << std::right << std::setw(10) << stats.count
                      << std::setw(12) << std::fixed << std::setprecision(2) << avg
                      << std::setw(12) << stats.min_us
                      << std::setw(12) << stats.max_us
                      << std::setw(12) << (stats.total_us / 1000.0) << std::endl;
        }
        std::cout << std::endl;
    }
    
    void reset() {
        timers.clear();
        statistics.clear();
    }
    
private:
    struct Stats {
        uint64_t count = 0;
        double total_us = 0.0;
        double min_us = 0.0;
        double max_us = 0.0;
    };
    
    std::map<std::string, std::chrono::high_resolution_clock::time_point> timers;
    std::map<std::string, Stats> statistics;
};

// RAII timer for automatic timing
class ScopedTimer {
public:
    ScopedTimer(PerformanceTimer& timer, const std::string& name) 
        : timer_(timer), name_(name) {
        timer_.start(name_);
    }
    
    ~ScopedTimer() {
        timer_.stop(name_);
    }
    
private:
    PerformanceTimer& timer_;
    std::string name_;
};

// Macro for easy scoped timing
#define PROFILE_SCOPE(timer, name) ScopedTimer _scoped_timer_##__LINE__(timer, name)

#endif // PERFORMANCE_H
