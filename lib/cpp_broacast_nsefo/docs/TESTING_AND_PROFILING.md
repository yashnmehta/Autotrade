# Unit Testing and Performance Profiling Guide

## Unit Testing with Google Test

### Setup

#### 1. Install Google Test
```bash
# Run setup script
./setup_tests.sh

# Or manual installation
sudo apt-get install libgtest-dev cmake
cd /usr/src/gtest
sudo cmake CMakeLists.txt
sudo make
sudo cp lib/*.a /usr/lib
```

#### 2. Build and Run Tests
```bash
# Build tests
make test

# Run tests
./test_runner

# Expected output:
# [==========] Running 12 tests from 2 test suites.
# [----------] Global test environment set-up.
# [----------] 6 tests from ConfigTest
# [ RUN      ] ConfigTest.DefaultValues
# [       OK ] ConfigTest.DefaultValues (0 ms)
# ...
# [==========] 12 tests from 2 test suites ran. (15 ms total)
# [  PASSED  ] 12 tests.
```

### Test Coverage

#### Config Tests (`tests/test_config.cpp`)
- ✅ Default values verification
- ✅ INI file parsing
- ✅ Environment variable overrides
- ✅ Boolean value parsing
- ✅ Non-existent file handling

#### Protocol Tests (`tests/test_protocol.cpp`)
- ✅ Endianness conversion (16-bit, 32-bit)
- ✅ Structure sizes (BCAST_HEADER = 40 bytes)
- ✅ Field offsets (TransactionCode @ offset 10)
- ✅ Structure alignment (packed, no padding)
- ✅ All message structure sizes

### Adding New Tests

Create a new test file:
```cpp
// tests/test_myfeature.cpp
#include <gtest/gtest.h>
#include "myfeature.h"

TEST(MyFeatureTest, BasicFunctionality) {
    MyFeature feature;
    EXPECT_EQ(feature.getValue(), 42);
}

TEST(MyFeatureTest, EdgeCase) {
    MyFeature feature;
    EXPECT_THROW(feature.invalidOperation(), std::runtime_error);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
```

Add to Makefile:
```makefile
TEST_SRCS = tests/test_config.cpp tests/test_protocol.cpp tests/test_myfeature.cpp
```

---

## Performance Profiling

### 1. Built-in Performance Timer

#### Usage in Code
```cpp
#include "performance.h"

PerformanceTimer perf_timer;

// Method 1: Manual timing
perf_timer.start("packet_processing");
// ... process packet ...
double us = perf_timer.stop("packet_processing");

// Method 2: Scoped timing (RAII)
{
    PROFILE_SCOPE(perf_timer, "decompression");
    // ... decompress ...
}  // Automatically stops timer

// Print statistics
perf_timer.printStats();
```

#### Output
```
=== Performance Statistics ===
Operation                      Count       Avg(μs)     Min(μs)     Max(μs)   Total(ms)
----------------------------------------------------------------------------------------
packet_processing              10000        125.45       89.23      1234.56     1254.50
decompression                   8500        456.78      123.45      2345.67     3882.63
parsing                        10000         45.23       12.34       234.56      452.30
```

### 2. GNU gprof Profiling

#### Build with Profiling
```bash
# Build with profiling enabled
make profile

# Run application
./nse_reader

# Generate profile report
gprof nse_reader gmon.out > profile_analysis.txt

# View report
less profile_analysis.txt
```

#### Interpreting gprof Output
```
Flat profile:

Each sample counts as 0.01 seconds.
  %   cumulative   self              self     total           
 time   seconds   seconds    calls  ms/call  ms/call  name    
 33.33      0.10     0.10    10000     0.01     0.01  LzoDecompressor::decompress
 20.00      0.16     0.06     5000     0.01     0.02  parse_compressed_message
 13.33      0.20     0.04    10000     0.00     0.00  be16toh_func
```

### 3. Linux perf Tool

#### Record Performance Data
```bash
# Record with call graph
sudo perf record -g ./nse_reader

# View report
sudo perf report

# Generate flamegraph
sudo perf script | stackcollapse-perf.pl | flamegraph.pl > flamegraph.svg
```

#### Common perf Commands
```bash
# CPU profiling
perf stat ./nse_reader

# Cache misses
perf stat -e cache-misses,cache-references ./nse_reader

# Branch prediction
perf stat -e branch-misses,branches ./nse_reader
```

### 4. Valgrind Profiling

#### Callgrind (Function-level profiling)
```bash
# Run with callgrind
valgrind --tool=callgrind ./nse_reader

# Analyze with kcachegrind (GUI)
kcachegrind callgrind.out.12345

# Or text output
callgrind_annotate callgrind.out.12345
```

#### Cachegrind (Cache profiling)
```bash
# Run with cachegrind
valgrind --tool=cachegrind ./nse_reader

# Analyze
cg_annotate cachegrind.out.12345
```

#### Massif (Heap profiling)
```bash
# Run with massif
valgrind --tool=massif ./nse_reader

# Analyze
ms_print massif.out.12345
```

### 5. Memory Leak Detection

```bash
# Run with memcheck
valgrind --leak-check=full --show-leak-kinds=all ./nse_reader

# Expected output (no leaks):
# HEAP SUMMARY:
#     in use at exit: 0 bytes in 0 blocks
#   total heap usage: 1,234 allocs, 1,234 frees, 5,678,901 bytes allocated
#
# All heap blocks were freed -- no leaks are possible
```

---

## Performance Benchmarks

### Target Metrics

| Metric | Target | Current | Status |
|--------|--------|---------|--------|
| Packet Processing | <100μs | TBD | ⏳ |
| LZO Decompression | <500μs | TBD | ⏳ |
| Message Parsing | <50μs | TBD | ⏳ |
| Throughput | >10K pps | TBD | ⏳ |
| Memory Usage | <100MB | TBD | ⏳ |
| CPU Usage | <50% | TBD | ⏳ |

### Running Benchmarks

```bash
# 1. Build optimized version
make clean && make

# 2. Run with performance timer
NSE_LOG_LEVEL=ERROR ./nse_reader

# 3. Let run for 60 seconds
# Ctrl+C to stop

# 4. Check statistics output
# Look for:
# - Packets per second
# - Average processing time
# - Memory usage (from 'top' or 'htop')
```

### Optimization Checklist

- [ ] Profile with gprof to find hot functions
- [ ] Check cache misses with perf
- [ ] Verify no memory leaks with valgrind
- [ ] Measure packet processing latency
- [ ] Test under high load (>50K pps)
- [ ] Optimize hot paths identified
- [ ] Re-profile after optimizations

---

## Continuous Integration

### Test Script
```bash
#!/bin/bash
# ci_test.sh

set -e

echo "Building project..."
make clean && make

echo "Running unit tests..."
make test

echo "Running with valgrind..."
timeout 10s valgrind --leak-check=full --error-exitcode=1 ./nse_reader || true

echo "All tests passed!"
```

### GitHub Actions Example
```yaml
name: CI

on: [push, pull_request]

jobs:
  test:
    runs-on: ubuntu-latest
    steps:
    - uses: actions/checkout@v2
    - name: Install dependencies
      run: |
        sudo apt-get update
        sudo apt-get install -y libgtest-dev cmake valgrind
    - name: Build
      run: make
    - name: Run tests
      run: make test
    - name: Memory check
      run: timeout 10s valgrind --leak-check=full ./nse_reader || true
```

---

## Summary

### Unit Testing
- ✅ Google Test framework integrated
- ✅ 12 initial tests created
- ✅ Config and protocol tests
- ✅ Easy to add new tests

### Performance Profiling
- ✅ Built-in performance timer
- ✅ gprof support
- ✅ perf integration
- ✅ Valgrind tools
- ✅ Memory leak detection

### Next Steps
1. Run initial tests: `make test`
2. Profile application: `make profile && ./nse_reader`
3. Identify bottlenecks
4. Optimize hot paths
5. Re-test and verify improvements
