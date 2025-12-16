# XTS API Test Scripts

These standalone test scripts help understand the exact behavior of XTS MarketData API endpoints.

## Purpose

- **test_getquote.cpp**: Test `/instruments/quotes` endpoint with different parameters
- **test_subscription.cpp**: Test `/instruments/subscription` endpoint for various scenarios

## Prerequisites

```bash
# Install libcurl development package
sudo apt-get install libcurl4-openssl-dev
```

## Compilation

```bash
# Compile getQuote test
g++ -std=c++11 test_getquote.cpp -o test_getquote -lcurl

# Compile subscription test
g++ -std=c++11 test_subscription.cpp -o test_subscription -lcurl
```

## Usage

### 1. Get Authentication Token

First, login to get your auth token (from application logs or use the interactive API):

```bash
# Check application logs for token
grep "Authorization" /tmp/debug_getquote.log | head -1
```

### 2. Run getQuote Tests

```bash
./test_getquote "YOUR_AUTH_TOKEN" "http://192.168.102.9:3000/apimarketdata"
```

**What to observe:**
- Which message codes work (1501, 1502, 1510, 1512)?
- Response format differences
- HTTP status codes
- Error messages for invalid parameters
- Actual JSON structure returned

### 3. Run Subscription Tests

```bash
./test_subscription "YOUR_AUTH_TOKEN" "http://192.168.102.9:3000/apimarketdata"
```

**What to observe:**
- First subscription: Does it return `listQuotes` with initial snapshot?
- Re-subscription (already subscribed): Does it return success but no snapshot?
- Multiple instruments: How does the response differ?
- Unsubscribe: What method/endpoint is used?
- After unsubscribe then re-subscribe: Does snapshot return again?

## Test Scenarios Covered

### getQuote Tests
1. ✅ NSE F&O instruments (NIFTY, BANKNIFTY futures)
2. ✅ NSE Options
3. ✅ NSE Cash Market (equities)
4. ✅ BSE instruments
5. ✅ Different message codes (1501, 1502, 1510, 1512)

### Subscription Tests
1. ✅ First-time subscription (expect snapshot)
2. ✅ Re-subscription of already subscribed token (expect no snapshot)
3. ✅ Multiple instruments in single request
4. ✅ Different exchange segments (NSE CM vs NSE FO)
5. ✅ Different message codes (1501 vs 1502)
6. ✅ Unsubscription
7. ✅ Re-subscribe after unsubscribe (expect snapshot again)

## Key Questions to Answer

### For getQuote API:
- [ ] Does `/instruments/quotes` endpoint exist and work?
- [ ] What HTTP status does it return?
- [ ] What's the exact response format?
- [ ] Which message codes are supported?
- [ ] Is there a difference between quotes and subscription responses?

### For Subscription API:
- [ ] Does first subscription always return `listQuotes` array?
- [ ] What's inside `listQuotes` - JSON string or object?
- [ ] Does re-subscription (token already subscribed) skip listQuotes?
- [ ] How to properly unsubscribe?
- [ ] Can multiple instruments be subscribed in one call?

## Expected Response Formats

### Subscription Response (First Time)
```json
{
  "type": "success",
  "code": "s-session-0001",
  "description": "Instrument subscribed successfully!",
  "result": {
    "mdp": 1502,
    "quotesList": [...],
    "listQuotes": [
      "{\"Touchline\":{\"LastTradedPrice\":25960,\"Close\":26108.7,...}}"
    ],
    "Remaining_Subscription_Count": 48
  }
}
```

### Subscription Response (Already Subscribed)
```json
{
  "type": "success",
  "code": "s-session-0001",
  "description": "Instrument subscribed successfully!",
  "result": {
    "mdp": 1502,
    "quotesList": [...],
    "listQuotes": [],  // Empty - no snapshot
    "Remaining_Subscription_Count": 48
  }
}
```

## Debugging Tips

1. **Enable verbose mode**: The tests use `CURLOPT_VERBOSE` to show full HTTP traffic
2. **Check HTTP status codes**: 200=OK, 400=Bad Request, 401=Unauthorized, 404=Not Found
3. **Inspect response structure**: Is it JSON? Is it nested?
4. **Compare with Python examples**: XTS might have Python SDK with working examples
5. **Check XTS API documentation**: Version-specific endpoint changes

## Integration Back to Application

Once you understand the API behavior from these tests:

1. Update `XTSMarketDataClient::getQuote()` with correct format
2. Handle empty `listQuotes` in re-subscription scenario
3. Add proper error handling for different HTTP status codes
4. Update documentation with actual API behavior

## Cleanup

```bash
# Remove compiled binaries
rm -f test_getquote test_subscription
```
