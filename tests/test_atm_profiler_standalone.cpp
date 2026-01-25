/**
 * @file test_atm_profiler_standalone.cpp
 * @brief Standalone profiler for ATM Watch data structures
 * 
 * Profiles initialization timing, memory usage, and search/filter performance
 * for the data structures used in ATM Watch implementation.
 * 
 * Compila with: g++ -std=c++17 -O2 -o test_atm_profiler_standalone.exe test_atm_profiler_standalone.cpp
 * Run with: ./test_atm_profiler_standalone.exe
 */

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// ============================================================================
// CONFIGURATION
// ============================================================================

#ifdef _WIN32
    const std::string CSV_PATH = "C:\\Users\\Administrator\\AppData\\Roaming\\TradingCo\\Trading Terminal\\Masters\\processed_csv\\";
#else
    const std::string CSV_PATH = "./processed_csv/";
#endif

// ============================================================================
// DATA STRUCTURES (Minimal ContractData replacement)
// ============================================================================

struct Contract {
    int64_t token = 0;
    std::string symbol;
    std::string displayName;
    std::string series;
    int lotSize = 0;
    double tickSize = 0.0;
    std::string expiryDate;
    double strikePrice = 0.0;
    std::string optionType;      // "CE", "PE", or empty for non-options
    std::string underlyingSymbol;
    int64_t assetToken = -1;
    int freezeQty = 0;
    int instrumentType = 0;       // 1=Future, 2=Option
    
    // For CM (Cash Market) contracts
    double priceBandHigh = 0.0;
    double priceBandLow = 0.0;
};

// ============================================================================
// ATM WATCH DATA STRUCTURES (from RepositoryManager)
// ============================================================================

class ATMWatchDataStructures {
public:
    // Expiry Cache (from analysis document)
    std::map<std::string, std::vector<std::string>> m_expiryToSymbols;    // "30JAN26" -> ["NIFTY", ...]
    std::map<std::string, std::string> m_symbolToCurrentExpiry;            // "NIFTY" -> "30JAN26"
    std::set<std::string> m_optionSymbols;                                 // {"NIFTY", "BANKNIFTY", ...}
    std::set<std::string> m_optionExpiries;                                // {"30JAN26", ...}
    
    // Strike and Token Caches
    std::unordered_map<std::string, std::vector<double>> m_symbolExpiryStrikes;      // "SYMBOL|EXPIRY" -> strikes
    std::unordered_map<std::string, std::pair<int64_t, int64_t>> m_strikeToTokens;   // "SYMBOL|EXPIRY|STRIKE" -> (CE, PE)
    std::unordered_map<std::string, int64_t> m_symbolToAssetToken;                   // Symbol -> asset token
    std::unordered_map<std::string, int64_t> m_symbolExpiryFutureToken;              // "SYMBOL|EXPIRY" -> future token
    std::unordered_map<int64_t, std::string> m_futureTokenToSymbol;                  // Token -> Symbol (Reverse)
    
    // Raw storage
    
    // Raw storage
    std::vector<Contract> m_allContracts;
    
    // Statistics
    struct Stats {
        size_t totalContracts = 0;
        size_t optionContracts = 0;
        size_t futureContracts = 0;
        size_t uniqueSymbols = 0;
        size_t uniqueExpiries = 0;
        size_t totalStrikes = 0;
    } stats;
};

// ============================================================================
// CSV PARSER
// ============================================================================

class CSVParser {
public:
    static std::vector<std::string> parseLine(const std::string& line) {
        std::vector<std::string> fields;
        std::stringstream ss(line);
        std::string field;
        
        while (std::getline(ss, field, ',')) {
            fields.push_back(field);
        }
        
        return fields;
    }
    
    static Contract parseNSEFOContract(const std::vector<std::string>& fields) {
        Contract c;
        if (fields.size() < 28) return c;
        
        c.token = std::stoll(fields[0]);
        c.symbol = fields[1];
        c.displayName = fields[2];
        c.series = fields[4];
        c.lotSize = std::stoi(fields[5]);
        c.tickSize = std::stod(fields[6]);
        c.expiryDate = fields[7];
        c.strikePrice = fields[8].empty() ? 0.0 : std::stod(fields[8]);
        c.optionType = fields[9];
        c.underlyingSymbol = fields[10];
        c.assetToken = std::stoll(fields[11]);
        c.freezeQty = std::stoi(fields[12]);
        c.instrumentType = std::stoi(fields[27]);
        
        return c;
    }
    
    static Contract parseNSECMContract(const std::vector<std::string>& fields) {
        Contract c;
        if (fields.size() < 16) return c;
        
        c.token = std::stoll(fields[0]);
        c.symbol = fields[1];
        c.displayName = fields[2];
        c.series = fields[4];
        c.lotSize = std::stoi(fields[5]);
        c.tickSize = std::stod(fields[6]);
        c.priceBandHigh = std::stod(fields[7]);
        c.priceBandLow = std::stod(fields[8]);
        c.instrumentType = 0;  // Cash market
        
        return c;
    }
    
    static size_t loadCSV(const std::string& filepath, std::vector<Contract>& contracts, bool isFO) {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            std::cerr << "ERROR: Cannot open " << filepath << std::endl;
            return 0;
        }
        
        std::string line;
        std::getline(file, line);  // Skip header
        
        size_t count = 0;
        while (std::getline(file, line)) {
            auto fields = parseLine(line);
            Contract c = isFO ? parseNSEFOContract(fields) : parseNSECMContract(fields);
            if (c.token > 0) {
                contracts.push_back(c);
                count++;
            }
        }
        
        return count;
    }
};

// ============================================================================
// DATA STRUCTURE BUILDER (Simulates RepositoryManager::buildExpiryCache)
// ============================================================================

class CacheBuilder {
public:
    static void buildExpiryCache(ATMWatchDataStructures& ds) {
        auto& contracts = ds.m_allContracts;
        
        // Temporary map for collecting symbol -> expiries
        std::map<std::string, std::set<std::string>> symbolExpiries;
        
        // Build expiry-wise symbol lists
        for (const auto& contract : contracts) {
            // Only process options (instrumentType == 2)
            if (contract.instrumentType != 2) continue;
            
            std::string symbol = contract.underlyingSymbol.empty() ? contract.symbol : contract.underlyingSymbol;
            std::string expiry = contract.expiryDate;
            
            if (symbol.empty() || expiry.empty()) continue;
            
            // Add to option symbols set
            ds.m_optionSymbols.insert(symbol);
            ds.m_optionExpiries.insert(expiry);
            
            // Add to expiry -> symbols map
            auto& symbols = ds.m_expiryToSymbols[expiry];
            if (std::find(symbols.begin(), symbols.end(), symbol) == symbols.end()) {
                symbols.push_back(symbol);
            }
            
            // Collect expiries for this symbol (for current expiry calculation)
            symbolExpiries[symbol].insert(expiry);
            
            // Build strike cache
            if (contract.strikePrice > 0.0) {
                std::string key = symbol + "|" + expiry;
                auto& strikes = ds.m_symbolExpiryStrikes[key];
                if (std::find(strikes.begin(), strikes.end(), contract.strikePrice) == strikes.end()) {
                    strikes.push_back(contract.strikePrice);
                }
                
                // Build token cache
                std::string tokenKey = key + "|" + std::to_string(static_cast<int>(contract.strikePrice));
                if (contract.optionType == "CE") {
                    ds.m_strikeToTokens[tokenKey].first = contract.token;
                } else if (contract.optionType == "PE") {
                    ds.m_strikeToTokens[tokenKey].second = contract.token;
                }
            }
        }
        
        // Calculate current (nearest) expiry for each symbol
        for (const auto& [symbol, expiries] : symbolExpiries) {
            if (!expiries.empty()) {
                // First expiry = current/nearest (already sorted by std::set)
                ds.m_symbolToCurrentExpiry[symbol] = *expiries.begin();
            }
        }
        
        // Sort strike lists
        for (auto& [key, strikes] : ds.m_symbolExpiryStrikes) {
            std::sort(strikes.begin(), strikes.end());
            ds.stats.totalStrikes += strikes.size();
        }
        
        // Build asset token map (from CM contracts that match FO underlying symbols)
        for (const auto& contract : contracts) {
            if (contract.instrumentType == 0 && ds.m_optionSymbols.count(contract.symbol)) {
                ds.m_symbolToAssetToken[contract.symbol] = contract.token;
            }
        }
        
        // Build future token map
        for (const auto& contract : contracts) {
            if (contract.instrumentType == 1) {  // Future
                std::string symbol = contract.underlyingSymbol.empty() ? contract.symbol : contract.underlyingSymbol;
                std::string key = symbol + "|" + contract.expiryDate;
                ds.m_symbolExpiryFutureToken[key] = contract.token;
                ds.m_futureTokenToSymbol[contract.token] = symbol;  // Populate reverse map
            }
        }
        
        // Update stats
        ds.stats.totalContracts = contracts.size();
        ds.stats.uniqueSymbols = ds.m_optionSymbols.size();
        ds.stats.uniqueExpiries = ds.m_optionExpiries.size();
        
        for (const auto& contract : contracts) {
            if (contract.instrumentType == 2) ds.stats.optionContracts++;
            if (contract.instrumentType == 1) ds.stats.futureContracts++;
        }
    }
};

// ============================================================================
// MEMORY PROFILER
// ============================================================================

class MemoryProfiler {
public:
    static size_t estimateMemoryUsage(const ATMWatchDataStructures& ds) {
        size_t total = 0;
        
        // Raw contracts
        total += ds.m_allContracts.size() * sizeof(Contract);
        total += ds.m_allContracts.capacity() * sizeof(Contract);
        
        // String data (rough estimate)
        for (const auto& c : ds.m_allContracts) {
            total += c.symbol.capacity();
            total += c.displayName.capacity();
            total += c.series.capacity();
            total += c.expiryDate.capacity();
            total += c.optionType.capacity();
            total += c.underlyingSymbol.capacity();
        }
        
        // Expiry caches
        total += ds.m_expiryToSymbols.size() * (sizeof(std::string) + sizeof(std::vector<std::string>));
        for (const auto& [k, v] : ds.m_expiryToSymbols) {
            total += k.capacity();
            total += v.capacity() * sizeof(std::string);
            for (const auto& s : v) total += s.capacity();
        }
        
        total += ds.m_symbolToCurrentExpiry.size() * (sizeof(std::string) * 2);
        for (const auto& [k, v] : ds.m_symbolToCurrentExpiry) {
            total += k.capacity() + v.capacity();
        }
        
        total += ds.m_optionSymbols.size() * sizeof(std::string);
        for (const auto& s : ds.m_optionSymbols) {
            total += s.capacity();
        }
        
        // Strike and token caches
        total += ds.m_symbolExpiryStrikes.size() * (sizeof(std::string) + sizeof(std::vector<double>));
        for (const auto& [k, v] : ds.m_symbolExpiryStrikes) {
            total += k.capacity();
            total += v.capacity() * sizeof(double);
        }
        
        total += ds.m_strikeToTokens.size() * (sizeof(std::string) + sizeof(std::pair<int64_t, int64_t>));
        total += ds.m_symbolToAssetToken.size() * (sizeof(std::string) + sizeof(int64_t));
        total += ds.m_symbolExpiryFutureToken.size() * (sizeof(std::string) + sizeof(int64_t));
        total += ds.m_futureTokenToSymbol.size() * (sizeof(int64_t) + sizeof(std::string));
        
        return total;
    }
    
    static void printMemoryReport(const ATMWatchDataStructures& ds) {
        size_t total = estimateMemoryUsage(ds);
        
        std::cout << "\n========== MEMORY USAGE REPORT ==========\n";
        std::cout << "Total Estimated Memory: " << (total / 1024.0 / 1024.0) << " MB\n";
        std::cout << "  - Raw Contracts: " << (ds.m_allContracts.size() * sizeof(Contract) / 1024.0) << " KB\n";
        std::cout << "  - Expiry Cache: ~" << (ds.m_expiryToSymbols.size() * 100 / 1024.0) << " KB\n";
        std::cout << "  - Strike Cache: ~" << (ds.m_symbolExpiryStrikes.size() * 100 / 1024.0) << " KB\n";
        std::cout << "  - Token Cache: ~" << (ds.m_strikeToTokens.size() * 32 / 1024.0) << " KB\n";
        std::cout << "  - Future Token Maps: ~" << (ds.m_symbolExpiryFutureToken.size() * 64 / 1024.0) << " KB\n";
        std::cout << "==========================================\n\n";
    }
};

// ============================================================================
// PERFORMANCE BENCHMARKS
// ============================================================================

class BenchmarkRunner {
public:
    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = Clock::time_point;
    using Duration = std::chrono::nanoseconds;
    
    template<typename Func>
    static double measureTime(Func&& func, int iterations = 1) {
        auto start = Clock::now();
        for (int i = 0; i < iterations; ++i) {
            func();
        }
        auto end = Clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        return duration.count() / static_cast<double>(iterations);
    }
    
    static void runSearchBenchmarks(const ATMWatchDataStructures& ds) {
        std::cout << "\n========== SEARCH/FILTER BENCHMARKS ==========\n";
        
        // Benchmark 1: Get all option symbols
        double time1 = measureTime([&ds]() {
            auto symbols = std::vector<std::string>(ds.m_optionSymbols.begin(), ds.m_optionSymbols.end());
        }, 1000);
        std::cout << "1. Get All Option Symbols: " << time1 << " μs (avg over 1000 runs)\n";
        
        // Benchmark 2: Get symbols for expiry
        if (!ds.m_expiryToSymbols.empty()) {
            std::string testExpiry = ds.m_expiryToSymbols.begin()->first;
            double time2 = measureTime([&ds, &testExpiry]() {
                auto it = ds.m_expiryToSymbols.find(testExpiry);
                if (it != ds.m_expiryToSymbols.end()) {
                    auto symbols = it->second;
                }
            }, 10000);
            std::cout << "2. Get Symbols for Expiry (\"" << testExpiry << "\"): " << time2 << " μs\n";
        }
        
        // Benchmark 3: Get current expiry for symbol
        if (!ds.m_symbolToCurrentExpiry.empty()) {
            std::string testSymbol = ds.m_symbolToCurrentExpiry.begin()->first;
            double time3 = measureTime([&ds, &testSymbol]() {
                auto it = ds.m_symbolToCurrentExpiry.find(testSymbol);
                if (it != ds.m_symbolToCurrentExpiry.end()) {
                    auto expiry = it->second;
                }
            }, 10000);
            std::cout << "3. Get Current Expiry for Symbol (\"" << testSymbol << "\"): " << time3 << " μs\n";
        }
        
        // Benchmark 4: Get strikes for symbol+expiry
        if (!ds.m_symbolExpiryStrikes.empty()) {
            std::string testKey = ds.m_symbolExpiryStrikes.begin()->first;
            double time4 = measureTime([&ds, &testKey]() {
                auto it = ds.m_symbolExpiryStrikes.find(testKey);
                if (it != ds.m_symbolExpiryStrikes.end()) {
                    auto strikes = it->second;
                }
            }, 10000);
            std::cout << "4. Get Strikes for Symbol+Expiry: " << time4 << " μs\n";
        }
        
        // Benchmark 5: Get asset token
        if (!ds.m_symbolToAssetToken.empty()) {
            std::string testSymbol = ds.m_symbolToAssetToken.begin()->first;
            double time5 = measureTime([&ds, &testSymbol]() {
                auto it = ds.m_symbolToAssetToken.find(testSymbol);
                if (it != ds.m_symbolToAssetToken.end()) {
                    auto token = it->second;
                }
            }, 10000);
            std::cout << "5. Get Asset Token for Symbol: " << time5 << " μs\n";
        }
        
        // Benchmark 6: Filter contracts (simulate old approach)
        double time6 = measureTime([&ds]() {
            std::set<std::string> optionSymbols;
            for (const auto& contract : ds.m_allContracts) {
                if (contract.instrumentType == 2) {
                    std::string symbol = contract.underlyingSymbol.empty() ? contract.symbol : contract.underlyingSymbol;
                    optionSymbols.insert(symbol);
                }
            }
        }, 10);
        std::cout << "6. OLD METHOD - Filter All Contracts for Options: " << time6 << " μs\n";
        
        // Benchmark 7: Reverse Future Token Lookup
        if (!ds.m_futureTokenToSymbol.empty()) {
            int64_t testToken = ds.m_futureTokenToSymbol.begin()->first;
            double time7 = measureTime([&ds, testToken]() {
                auto it = ds.m_futureTokenToSymbol.find(testToken);
                if (it != ds.m_futureTokenToSymbol.end()) {
                    auto symbol = it->second;
                }
            }, 10000);
            std::cout << "7. Reverse Future Token Lookup (" << testToken << "): " << time7 << " μs\n";
        }
        
        std::cout << "\n⚡ Speedup Factor (Old Method / Cache Lookup): " 
                  << (time6 / time1) << "x\n";
        std::cout << "=============================================\n\n";
    }

    static void dumpFutureTokenMap(const ATMWatchDataStructures& ds, const std::string& filepath) {
        std::ofstream file(filepath);
        if (!file.is_open()) {
            std::cerr << "Failed to open dump file: " << filepath << std::endl;
            return;
        }
        
        file << "Token,Symbol,Expiry\n";
        
        // We iterate the forward map to get expiry info easily
        for (const auto& [key, token] : ds.m_symbolExpiryFutureToken) {
            size_t pipePos = key.find('|');
            if (pipePos != std::string::npos) {
                std::string symbol = key.substr(0, pipePos);
                std::string expiry = key.substr(pipePos + 1);
                file << token << "," << symbol << "," << expiry << "\n";
            }
        }
        
        file.close();
        std::cout << "✓ Dumped " << ds.m_symbolExpiryFutureToken.size() << " future tokens to " << filepath << "\n";
    }
};

// ============================================================================
// MAIN PROFILER
// ============================================================================

int main() {
    std::cout << "========================================\n";
    std::cout << "  ATM Watch Data Structure Profiler\n";
    std::cout << "========================================\n\n";
    
    ATMWatchDataStructures ds;
    
    // ========== PHASE 1: Load CSV Files ==========
    std::cout << "PHASE 1: Loading CSV files...\n";
    
    auto loadStart = BenchmarkRunner::Clock::now();
    
    size_t nsefo = CSVParser::loadCSV(CSV_PATH + "nsefo_processed.csv", ds.m_allContracts, true);
    std::cout << "  ✓ Loaded NSEFO: " << nsefo << " contracts\n";
    
    size_t nsecm = CSVParser::loadCSV(CSV_PATH + "nsecm_processed.csv", ds.m_allContracts, false);
    std::cout << "  ✓ Loaded NSECM: " << nsecm << " contracts\n";
    
    size_t bsefo = CSVParser::loadCSV(CSV_PATH + "bsefo_processed.csv", ds.m_allContracts, true);
    std::cout << "  ✓ Loaded BSEFO: " << bsefo << " contracts\n";
    
    size_t bsecm = CSVParser::loadCSV(CSV_PATH + "bsecm_processed.csv", ds.m_allContracts, false);
    std::cout << "  ✓ Loaded BSECM: " << bsecm << " contracts\n";
    
    auto loadEnd = BenchmarkRunner::Clock::now();
    auto loadDuration = std::chrono::duration_cast<std::chrono::milliseconds>(loadEnd - loadStart);
    std::cout << "\n✓ Total Contracts Loaded: " << ds.m_allContracts.size() << "\n";
    std::cout << "✓ Load Time: " << loadDuration.count() << " ms\n\n";
    
    // ========== PHASE 2: Build Caches ==========
    std::cout << "PHASE 2: Building data structure caches...\n";
    
    auto buildStart = BenchmarkRunner::Clock::now();
    CacheBuilder::buildExpiryCache(ds);
    auto buildEnd = BenchmarkRunner::Clock::now();
    auto buildDuration = std::chrono::duration_cast<std::chrono::milliseconds>(buildEnd - buildStart);
    
    std::cout << "  ✓ Option Symbols: " << ds.stats.uniqueSymbols << "\n";
    std::cout << "  ✓ Unique Expiries: " << ds.stats.uniqueExpiries << "\n";
    std::cout << "  ✓ Option Contracts: " << ds.stats.optionContracts << "\n";
    std::cout << "  ✓ Future Contracts: " << ds.stats.futureContracts << "\n";
    std::cout << "  ✓ Total Strikes Cached: " << ds.stats.totalStrikes << "\n";
    std::cout << "\n✓ Cache Build Time: " << buildDuration.count() << " ms\n";
    
    // ========== PHASE 3: Memory Profiling ==========
    MemoryProfiler::printMemoryReport(ds);
    
    // ========== PHASE 4: Performance Benchmarks ==========
    BenchmarkRunner::runSearchBenchmarks(ds);
    
    // ========== PHASE 5: Dump Debug File ==========
    std::cout << "PHASE 5: Dumping Future Token debug file...\n";
    BenchmarkRunner::dumpFutureTokenMap(ds, "future_tokens_dump.csv");
    
    // ========== SUMMARY ==========
    std::cout << "\n========== PERFORMANCE SUMMARY ==========\n";
    std::cout << "Total Initialization Time: " << (loadDuration.count() + buildDuration.count()) << " ms\n";
    std::cout << "  - CSV Loading: " << loadDuration.count() << " ms\n";
    std::cout << "  - Cache Building: " << buildDuration.count() << " ms\n";
    std::cout << "=========================================\n";
    
    return 0;
}
