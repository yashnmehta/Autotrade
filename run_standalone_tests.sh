#!/bin/bash
# Helper script to compile and run standalone XTS API tests
# Usage: ./run_standalone_tests.sh

set -e

echo "================================================"
echo "XTS API Standalone Test Suite"
echo "================================================"

# Check if libcurl is installed
if ! pkg-config --exists libcurl; then
    echo "❌ Error: libcurl not found"
    echo "Install with: sudo apt-get install libcurl4-openssl-dev"
    exit 1
fi

# Compile test programs
echo ""
echo "📦 Compiling test programs..."
g++ -std=c++11 test_getquote.cpp -o test_getquote -lcurl
echo "✅ test_getquote compiled"

g++ -std=c++11 test_subscription.cpp -o test_subscription -lcurl
echo "✅ test_subscription compiled"

# Extract token and base URL from running application or config
echo ""
echo "🔍 Extracting authentication credentials..."

# Try to get token from recent logs
TOKEN=""
BASE_URL="http://192.168.102.9:3000/apimarketdata"

if [ -f /tmp/subscribe_test.log ] || [ -f /tmp/debug_getquote.log ]; then
    # Try to extract from logs
    LOG_FILE="/tmp/subscribe_test.log"
    [ ! -f "$LOG_FILE" ] && LOG_FILE="/tmp/debug_getquote.log"
    
    TOKEN=$(grep -oP 'Authorization: "\K[^"]+' "$LOG_FILE" 2>/dev/null | head -1)
fi

# If token not found, prompt user
if [ -z "$TOKEN" ]; then
    echo ""
    echo "⚠️  Could not auto-detect authentication token"
    echo ""
    echo "Please provide your XTS authentication token:"
    echo "(You can find it in application logs with: grep Authorization /tmp/*.log)"
    read -p "Token: " TOKEN
fi

if [ -z "$TOKEN" ]; then
    echo "❌ No token provided. Cannot run tests."
    exit 1
fi

echo "✅ Token found: ${TOKEN:0:20}..."
echo "✅ Base URL: $BASE_URL"

# Ask user which tests to run
echo ""
echo "================================================"
echo "Select tests to run:"
echo "1) getQuote API tests"
echo "2) Subscribe/Unsubscribe API tests"
echo "3) Both"
echo "================================================"
read -p "Choice [1-3]: " choice

case $choice in
    1)
        echo ""
        echo "🧪 Running getQuote API tests..."
        echo "================================================"
        ./test_getquote "$TOKEN" "$BASE_URL" 2>&1 | tee getquote_test_results.txt
        echo ""
        echo "✅ Results saved to: getquote_test_results.txt"
        ;;
    2)
        echo ""
        echo "🧪 Running Subscription API tests..."
        echo "================================================"
        ./test_subscription "$TOKEN" "$BASE_URL" 2>&1 | tee subscription_test_results.txt
        echo ""
        echo "✅ Results saved to: subscription_test_results.txt"
        ;;
    3)
        echo ""
        echo "🧪 Running ALL API tests..."
        echo "================================================"
        echo ""
        echo ">>> PART 1: getQuote API Tests"
        ./test_getquote "$TOKEN" "$BASE_URL" 2>&1 | tee getquote_test_results.txt
        
        echo ""
        echo ">>> PART 2: Subscription API Tests"
        ./test_subscription "$TOKEN" "$BASE_URL" 2>&1 | tee subscription_test_results.txt
        
        echo ""
        echo "✅ All results saved to:"
        echo "   - getquote_test_results.txt"
        echo "   - subscription_test_results.txt"
        ;;
    *)
        echo "❌ Invalid choice"
        exit 1
        ;;
esac

echo ""
echo "================================================"
echo "🎉 Test execution complete!"
echo "================================================"
echo ""
echo "📋 Next steps:"
echo "1. Review the output above and saved result files"
echo "2. Check HTTP status codes (200=success, 400=bad request, etc.)"
echo "3. Analyze response JSON structure"
echo "4. Compare getQuote vs subscribe response formats"
echo "5. Note which message codes work (1501, 1502, etc.)"
echo "6. Observe re-subscription behavior (already subscribed tokens)"
echo ""
echo "Use findings to update XTSMarketDataClient implementation!"
