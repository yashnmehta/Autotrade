#!/bin/bash
# XTS API Test Runner
# Builds and runs both API test suites

set -e

echo "=========================================="
echo "XTS API Test Runner"
echo "=========================================="
echo ""

# Build directory
BUILD_DIR="/Users/yashmehta/Desktop/go_proj/trading_terminal_cpp/build"
cd "$BUILD_DIR"

# Build tests
echo "Building test programs..."
make test_marketdata_api test_interactive_api

echo ""
echo "=========================================="
echo "Running Market Data API Tests"
echo "=========================================="
echo ""
cd tests
./test_marketdata_api

echo ""
echo "=========================================="
echo "Running Interactive API Tests"
echo "=========================================="
echo ""
./test_interactive_api

echo ""
echo "=========================================="
echo "All Tests Complete"
echo "=========================================="
echo ""
echo "Summary:"
echo "- Market Data API: Check results above"
echo "- Interactive API: Check results above"
echo ""
echo "See docs/XTS_API_TEST_RESULTS.md for detailed analysis"
