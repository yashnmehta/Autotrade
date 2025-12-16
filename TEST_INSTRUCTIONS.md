# XTS API Test - Quick Start

## What This Tests

This standalone test program verifies the XTS MarketData API behavior:

1. **getQuote Endpoint** (`/instruments/quotes`)
   - Tests with message codes 1501, 1502
   - Tests with NIFTY and BANKNIFTY futures
   - Determines if endpoint works or returns 400/404

2. **Subscribe Endpoint** (`/instruments/subscription`)
   - First-time subscription (should return listQuotes with snapshot)
   - Re-subscription (should return empty listQuotes per user's report)
   - Multiple instruments (verify all get snapshots)

## How to Run

### Option 1: Interactive Script (Recommended)
```bash
cd /home/ubuntu/Desktop/trading_terminal_cpp
./run_api_test.sh
```

This will prompt you for:
- Base URL (default: http://192.168.102.9:3000/apimarketdata)
- Auth token (from login response)

### Option 2: Direct Command
```bash
cd /home/ubuntu/Desktop/trading_terminal_cpp
./build/test_xts_api "YOUR_AUTH_TOKEN" "http://192.168.102.9:3000/apimarketdata"
```

## Getting Auth Token

### From Application Login
1. Start the trading terminal
2. Login successfully
3. Check terminal output or logs for the auth token
4. Token format: `eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9...`

### From Direct Login API Call
```bash
curl -X POST "http://192.168.102.9:3000/apimarketdata/auth/login" \
  -H "Content-Type: application/json" \
  -d '{"appKey":"YOUR_APP_KEY","secretKey":"YOUR_SECRET_KEY","source":"WebAPI"}'
```

Response will contain the token in `result.token`.

## What to Look For

### 1. getQuote Tests (Tests 1-3)
- **HTTP 200**: Endpoint works, check response structure
- **HTTP 400**: Bad request - endpoint may not support this format
- **HTTP 404**: Endpoint doesn't exist
- Check if response has quote data or error message

### 2. Subscribe Test - First Time (Test 4)
Expected behavior:
```json
{
  "type": "success",
  "result": {
    "listQuotes": [
      "{\"Touchline\":{\"LastTradedPrice\":24500.50,...}}"
    ]
  }
}
```
✅ listQuotes array should have quote data

### 3. Subscribe Test - Re-Subscribe (Test 5)
Expected behavior (per user's report):
```json
{
  "type": "success",
  "result": {
    "listQuotes": []
  }
}
```
✅ listQuotes should be EMPTY (already subscribed)

### 4. Multiple Instruments (Test 6)
- Should return multiple quotes in listQuotes
- Verify all requested instruments get snapshots

## Key Questions This Answers

1. **Does getQuote work?**
   - If yes: We can use it for already-subscribed tokens
   - If no: We need PriceCache solution

2. **Does first subscription return snapshot?**
   - Confirms subscribe provides initial data
   - Validates our current implementation

3. **Does re-subscription return empty listQuotes?**
   - Confirms "0.00 flash" root cause
   - Validates need for PriceCache

4. **What message codes work?**
   - 1501: Touchline only
   - 1502: Market depth (preferred)
   - Others: Need testing

## Example Output

```
========================================
TEST 1: getQuote - NIFTY - Message Code 1502
========================================
URL: http://192.168.102.9:3000/apimarketdata/instruments/quotes
Request Body: {"instruments":[{"exchangeSegment":2,"exchangeInstrumentID":49543}],"xtsMessageCode":1502}

----------------------------------------
Response for: getQuote-NIFTY-1502
HTTP Status: 200 OK
Response Body: {"type":"success","result":{...}}

Parsed Response:
  type: success
  listQuotes count: 1
  Touchline.LastTradedPrice: 24500.5
  Touchline.Close: 24450.0
----------------------------------------
```

## Troubleshooting

### "Invalid token" or "Unauthorized"
- Token may have expired
- Get a fresh token from login API
- Tokens typically expire after 24 hours

### "Connection refused"
- Check if XTS server is running
- Verify base URL and port
- Check network connectivity

### Tests run too fast
- Tests have 2-second delays between them
- Modify delays in test_xts_api.cpp if needed

## Next Steps After Testing

Based on results:

1. **If getQuote works**: Implement proper getQuote call for already-subscribed tokens
2. **If getQuote fails**: Proceed with PriceCache solution (Week 1 roadmap)
3. **Document findings**: Update architecture recommendations based on actual API behavior
4. **Begin implementation**: Start Week 1 tasks (PriceCache, StateManager, RetryHelper)
