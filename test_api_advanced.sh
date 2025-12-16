#!/bin/bash

# Test getQuote and subscription with fresh instruments
# This script ensures we test with instruments that are NOT already subscribed

cd /home/ubuntu/Desktop/trading_terminal_cpp

# Extract token from running terminal or start it
TOKEN=$(grep "token=" /tmp/trading_terminal_output.log 2>/dev/null | grep -o "token=[^&]*" | cut -d'=' -f2 | head -1)

if [ -z "$TOKEN" ]; then
    echo "⚠️ No token found. Please start the trading terminal first and login."
    exit 1
fi

BASE_URL="http://192.168.102.9:3000/apimarketdata"

echo "================================================"
echo "XTS API Advanced Tests"
echo "================================================"
echo ""
echo "Testing with token: ${TOKEN:0:30}..."
echo ""

# Test 1: Single getQuote
echo "========================================
TEST 1: Single getQuote (NIFTY FUT)"
echo "========================================"
curl -s -X POST "$BASE_URL/instruments/quotes" \
  -H "Authorization: $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{
    "instruments": [{"exchangeSegment": 2, "exchangeInstrumentID": 49543}],
    "xtsMessageCode": 1502,
    "publishFormat": "JSON"
  }' | python3 -m json.tool | head -30
echo ""

# Test 2: Multiple getQuote
echo "========================================
TEST 2: Multiple getQuote (NIFTY + BANKNIFTY)"
echo "========================================"
curl -s -X POST "$BASE_URL/instruments/quotes" \
  -H "Authorization: $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{
    "instruments": [
      {"exchangeSegment": 2, "exchangeInstrumentID": 49543},
      {"exchangeSegment": 2, "exchangeInstrumentID": 59175}
    ],
    "xtsMessageCode": 1502,
    "publishFormat": "JSON"
  }' | python3 -m json.tool | head -40
echo ""

# Test 3: Get a fresh instrument (FINNIFTY) to test subscription
echo "========================================
TEST 3: Search for FINNIFTY"
echo "========================================"
FINNIFTY=$(curl -s -X GET "$BASE_URL/search/instrumentsbystring?searchString=FINNIFTY&exchangeSegment=2" \
  -H "Authorization: $TOKEN" | python3 -c "import sys, json; data=json.load(sys.stdin); print(data['result'][0]['ExchangeInstrumentID'] if data.get('result') else '')" 2>/dev/null)

if [ -n "$FINNIFTY" ]; then
    echo "Found FINNIFTY: $FINNIFTY"
    echo ""
    
    # Test 4: Subscribe to fresh instrument
    echo "========================================
    TEST 4: Subscribe to FINNIFTY (should be fresh)"
    echo "========================================"
    curl -s -X POST "$BASE_URL/instruments/subscription" \
      -H "Authorization: $TOKEN" \
      -H "Content-Type: application/json" \
      -d "{
        \"instruments\": [{\"exchangeSegment\": 2, \"exchangeInstrumentID\": $FINNIFTY}],
        \"xtsMessageCode\": 1502
      }" | python3 -m json.tool | head -30
    echo ""
    
    # Test 5: Re-subscribe to same instrument (should get Already Subscribed error)
    echo "========================================
    TEST 5: Re-subscribe to FINNIFTY (already subscribed)"
    echo "========================================"
    curl -s -X POST "$BASE_URL/instruments/subscription" \
      -H "Authorization: $TOKEN" \
      -H "Content-Type: application/json" \
      -d "{
        \"instruments\": [{\"exchangeSegment\": 2, \"exchangeInstrumentID\": $FINNIFTY}],
        \"xtsMessageCode\": 1502
      }" | python3 -m json.tool
    echo ""
    
    # Test 6: getQuote for already subscribed
    echo "========================================
    TEST 6: getQuote for already-subscribed FINNIFTY"
    echo "========================================"
    curl -s -X POST "$BASE_URL/instruments/quotes" \
      -H "Authorization: $TOKEN" \
      -H "Content-Type: application/json" \
      -d "{
        \"instruments\": [{\"exchangeSegment\": 2, \"exchangeInstrumentID\": $FINNIFTY}],
        \"xtsMessageCode\": 1502,
        \"publishFormat\": \"JSON\"
      }" | python3 -m json.tool | head -30
else
    echo "⚠️ Could not find FINNIFTY. Skipping subscription tests."
fi

echo ""
echo "================================================"
echo "All tests completed!"
echo "================================================"
