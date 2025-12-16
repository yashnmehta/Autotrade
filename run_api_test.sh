#!/bin/bash

# Helper script to run XTS API tests
# This avoids having to paste the auth token in command line

echo "================================================"
echo "XTS API Test Runner"
echo "================================================"
echo ""
echo "This will test:"
echo "  1. getQuote endpoint with different message codes"
echo "  2. Subscribe endpoint (first-time vs re-subscription)"
echo "  3. Multiple instrument subscription"
echo ""

# Default values
DEFAULT_BASE_URL="http://192.168.102.9:3000/apimarketdata"

# Get base URL
read -p "Enter base URL [$DEFAULT_BASE_URL]: " BASE_URL
BASE_URL=${BASE_URL:-$DEFAULT_BASE_URL}

# Get auth token
echo ""
echo "Enter your XTS auth token (from login response):"
echo "Example: eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9..."
read -p "Token: " AUTH_TOKEN

if [ -z "$AUTH_TOKEN" ]; then
    echo "❌ Error: Auth token is required"
    exit 1
fi

echo ""
echo "================================================"
echo "Running tests..."
echo "================================================"
echo ""

# Run the test
./build/test_xts_api "$AUTH_TOKEN" "$BASE_URL"

echo ""
echo "================================================"
echo "Test complete!"
echo "================================================"
echo ""
echo "Review the output above to determine:"
echo "  - Does getQuote endpoint work? (HTTP 200 vs 400/404)"
echo "  - Does first subscription return listQuotes data?"
echo "  - Does re-subscription return empty listQuotes?"
echo "  - What message codes are supported?"
echo ""
