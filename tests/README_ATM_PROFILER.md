# ATM Watch Data Structure Profiler - Documentation

## Overview

This standalone profiler benchmarks the ATM Watch data structure implementation, measuring initialization timing, memory usage, and search/filter performance.

**File**: `tests/test_atm_profiler_standalone.cpp`  
**Build**: `tests/build_profiler.bat`  
**Executable**: `tests/test_atm_profiler_standalone.exe`

---

## Quick Start

### Build
```bash
cd tests
.\build_profiler.bat
```

### Run
```bash
.\test_atm_profiler_standalone.exe
```

---

## What It Measures

### 1. CSV Loading Performance
- Loads all 4 processed CSV files (NSEFO, NSECM, BSEFO, BSECM)
- Measures parsing time for ~125,000 contracts
- Reports per-segment contract counts

### 2. Cache Building Performance
- Simulates `RepositoryManager::buildExpiryCache()`
- Builds all data structures:
  - Expiry → Symbols mapping
  - Symbol → Current Expiry mapping
  - Symbol+Expiry → Strikes mapping
  - Symbol+Expiry+Strike → Token pairs (CE/PE)
  - Symbol → Asset Token mapping
  - Symbol+Expiry → Future Token mapping
- Measures construction time

### 3. Memory Usage Analysis
- Estimates total memory footprint
- Breaks down by data structure type
- Accounts for string allocations and container overhead

### 4. Search/Filter Benchmarks
Compares cached lookup vs. old filtering approach:
1. **Get All Option Symbols** (O(1) cache vs O(N) filter)
2. **Get Symbols for Expiry** (O(1) hash lookup)
3. **Get Current Expiry for Symbol** (O(1) hash lookup)
4. **Get Strikes for Symbol+Expiry** (O(1) hash lookup)
5. **Get Asset Token** (O(1) hash lookup)
6. **OLD METHOD**: Filter 125k contracts for options

---

## Latest Results (Sample Run)

```
========================================
  ATM Watch Data Structure Profiler
========================================

PHASE 1: Loading CSV files...
  ✓ Loaded NSEFO: 103,156 contracts
  ✓ Loaded NSECM: 8,933 contracts
  ✓ Loaded BSEFO: 0 contracts
  ✓ Loaded BSECM: 13,273 contracts

✓ Total Contracts Loaded: 125,362
✓ Load Time: 484 ms

PHASE 2: Building data structure caches...
  ✓ Option Symbols: 213
  ✓ Unique Expiries: 18
  ✓ Option Contracts: 102,074
  ✓ Future Contracts: 654
  ✓ Total Strikes Cached: 51,098

✓ Cache Build Time: 86 ms

========== MEMORY USAGE REPORT ==========
Total Estimated Memory: 77.5 MB
  - Raw Contracts: 31.3 MB
  - Expiry Cache: ~1.8 KB
  - Strike Cache: ~63.9 KB
  - Token Cache: ~1.6 MB
==========================================

========== SEARCH/FILTER BENCHMARKS ==========
1. Get All Option Symbols: 3.174 μs (avg over 1000 runs)
2. Get Symbols for Expiry: 0.0813 μs
3. Get Current Expiry for Symbol: 0.0341 μs
4. Get Strikes for Symbol+Expiry: 0.1057 μs
5. Get Asset Token for Symbol: 0.0042 μs
6. OLD METHOD - Filter All Contracts: 6,415.5 μs

⚡ Speedup Factor: 2021x faster
=============================================

========== PERFORMANCE SUMMARY ==========
Total Initialization Time: 570 ms
  - CSV Loading: 484 ms (85%)
  - Cache Building: 86 ms (15%)
=========================================
```

---

## Key Findings

### ✅ Performance Wins

1. **Cached Lookup is 2000x Faster**
   - Old Method (filtering 125k contracts): 6,415 μs
   - New Method (hash lookup): 3.2 μs
   - **Speedup: 2021×**

2. **Sub-Microsecond Lookups**
   - Get Current Expiry: **0.034 μs** (34 nanoseconds)
   - Get Asset Token: **0.004 μs** (4 nanoseconds)
   - Get Strikes: **0.106 μs**

3. **Fast Initialization**
   - Total: **570ms** for 125k contracts
   - CSV Parsing: 484ms (I/O bound)
   - Cache Building: **86ms** (CPU bound)

### ✅ Memory Efficiency

- **Total: 77.5 MB** for all data structures
- **Per Contract: ~618 bytes** (including all caches)
- **Cache Overhead: ~1.7 MB** (2.2% of total)

### ✅ Scalability

- **213 option-enabled symbols** supported instantly
- **18 unique expiries** tracked
- **51,098 strikes** cached and sorted
- Linear memory growth with contract count

---

## Implementation Details

### Data Structures Used

```cpp
// Expiry Cache
std::map<std::string, std::vector<std::string>> m_expiryToSymbols;
std::map<std::string, std::string> m_symbolToCurrentExpiry;
std::set<std::string> m_optionSymbols;

// Strike and Token Caches
std::unordered_map<std::string, std::vector<double>> m_symbolExpiryStrikes;
std::unordered_map<std::string, std::pair<int64_t, int64_t>> m_strikeToTokens;
std::unordered_map<std::string, int64_t> m_symbolToAssetToken;
```

**Why These Choices?**
- `std::map` for expiries: Maintains sorted order (chronological)
- `std::unordered_map` for tokens: O(1) lookup by composite key
- `std::set` for symbols: Fast membership testing + sorted iteration

### Key Optimizations

1. **Composite Keys**: `"SYMBOL|EXPIRY"` for strike caching
2. **Sorted Strikes**: Pre-sorted during cache build (not on every access)
3. **Minimal Allocations**: Reuse vectors, no unnecessary copies
4. **Single Pass**: All caches built in one iteration

---

## Comparison with Qt Implementation

| Feature | Standalone (STL) | Production (Qt) |
|:---|:---|:---|
| **Hash Map** | `std::unordered_map` | `QHash` |
| **Ordered Map** | `std::map` | `QMap` |
| **Set** | `std::set` | `QSet` |
| **Vector** | `std::vector` | `QVector` |
| **String** | `std::string` | `QString` |
| **Performance** | ~Same (both O(1)) | ~Same |

**Note**: This profiler uses STL for standalone compilation. The production code uses Qt containers which have nearly identical performance characteristics.

---

## Build Requirements

- **Compiler**: g++ with C++17 support (MinGW on Windows, GCC on Linux)
- **Dependencies**: None (pure STL, no Qt/CMake)
- **Platform**: Cross-platform (Windows/Linux/macOS)

---

## Extending the Profiler

### Add New Benchmark

```cpp
// In BenchmarkRunner::runSearchBenchmarks()
double timeN = measureTime([&ds]() {
    // Your benchmark code here
}, iterations);
std::cout << "N. Your Benchmark: " << timeN << " μs\n";
```

### Test Different Data Structures

```cpp
// Replace std::unordered_map with std::map
std::map<std::string, int64_t> m_symbolToAssetToken;  // Test ordered map

// Or use vector for dense token ranges
std::vector<TokenInfo> m_tokenArray(100000);  // Direct array access
```

### Profile Memory with Valgrind (Linux)

```bash
g++ -std=c++17 -g -o profiler test_atm_profiler_standalone.cpp
valgrind --tool=massif ./profiler
ms_print massif.out.<PID>
```

---

## Validation

### Correctness Checks

The profiler validates:
1. ✅ All CSV files parsed successfully
2. ✅ Option symbols correctly identified (instrumentType == 2)
3. ✅ Strikes sorted in ascending order
4. ✅ Token pairs (CE/PE) correctly mapped
5. ✅ Asset tokens matched to underlying symbols

### Data Integrity

- **Zero duplicate strikes** per symbol+expiry
- **All symbols have current expiry** (earliest date)
- **All expiries sorted chronologically**

---

## Troubleshooting

### CSV Not Found

```
ERROR: Cannot open C:\Users\...\processed_csv\nsefo_processed.csv
```

**Fix**: Update `CSV_PATH` constant in source file (line 22-26)

### Build Fails

```
error: 'std::unordered_map' has not been declared
```

**Fix**: Ensure `-std=c++17` flag is set in build command

### Memory Estimation Wrong

The profiler provides **estimates only**. For exact measurement:
- Windows: Use Performance Monitor or Process Explorer
- Linux: Use `valgrind --tool=massif`

---

## Conclusion

The ATM Watch data structure implementation is **highly optimized**:
- ✅ **2000x faster** than naive filtering
- ✅ **Sub-microsecond lookups** for all operations
- ✅ **Minimal memory overhead** (~2% for caches)
- ✅ **Fast initialization** (570ms for 125k contracts)

**Recommendation**: The current cache-based approach in `RepositoryManager` is excellent. The bottlenecks identified in `atm_bottlenecks_analysis.md` are in `ATMWatchManager`'s usage, not the underlying data structures.

---

**Date**: 2026-01-23  
**Status**: Profiler Validated and Documented  
**Next Steps**: Implement local caching in ATMWatchManager per bottlenecks analysis
