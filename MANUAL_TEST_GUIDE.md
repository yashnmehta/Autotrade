# Manual Testing Guide for Global Search

## Overview
The flexible multi-token search is now integrated into `RepositoryManager::searchScripsGlobal()`. This guide shows how to test it.

## Where to Test

The search is used in:
1. **TradingView Chart Widget** - Symbol search box
2. **SnapQuote Window** - Contract lookup

## How the Python Script Helps

The `test_search_queries.py` file is a **reference document** that:
- Lists all test cases you should try
- Shows expected behavior for each query
- Can generate JSON for automated testing later

**It doesn't test the C++ code directly** - it's documentation!

## Manual Testing Steps

### 1. Run the Application
```powershell
cd build_ninja
.\TradingTerminal.exe
```

### 2. Open TradingView Chart
- The chart widget should show a symbol search box
- Type queries and observe the search results

### 3. Test These Query Patterns

Use the Python script to see all test cases:
```powershell
python test_search_queries.py --queries
```

#### Quick Test Queries (Copy and paste these):

**Symbol Only:**
- `nifty`
- `reliance`
- `banknifty`

**Symbol + Strike (Any Order):**
- `nifty 26000` ✓
- `26000 nifty` ✓ (reversed order)
- `50000 banknifty` ✓

**Symbol + Option Type (Any Order):**
- `nifty ce` ✓
- `ce nifty` ✓ (reversed)
- `banknifty pe` ✓

**Symbol + Strike + Type (Any Order):**
- `nifty 26000 ce` ✓
- `26000 ce nifty` ✓
- `ce 26000 nifty` ✓
- `nifty ce 26000` ✓

**Symbol + Expiry (Flexible Formats):**
- `nifty 17feb` ✓
- `nifty 17feb2026` ✓
- `nifty 17 feb 2026` ✓
- `gold 26feb` ✓

**All Tokens (Any Order):**
- `nifty 17feb 26000 ce` ✓
- `26000 ce nifty 17feb` ✓
- `ce 26000 17feb nifty` ✓

**Edge Cases:**
- `26000` (strike only)
- `ce` (option type only)
- `reliance EQ` (symbol + series)

## Observing Results

### In Application Logs
Check the debug output for lines like:
```
[RepositoryManager] Global search found X results for query: "nifty 26000"
```

### Expected Behavior

For `nifty 26000`:
- Should find NIFTY contracts with strike near 26000
- Should show both CE and PE options
- Results ordered by relevance (nearest expiry first)

For `26000 nifty` (reversed):
- **Should produce same results!**
- Order of tokens doesn't matter

For `nifty ce 26000`:
- Should filter to only CE (Call) options
- Strike must be around 26000
- Symbol must contain "NIFTY"

## Verifying Tokenizer Output

You can add debug output to see parsed tokens. In [RepositoryManager.cpp](c:/Users/admin/Desktop/trading_terminal_cpp/src/repository/RepositoryManager.cpp#L892), the line:
```cpp
auto parsed = SearchTokenizer::parse(searchStripped);
```

You could add after it:
```cpp
qDebug() << "[Search] Parsed:" 
         << "Symbol=" << parsed.symbol
         << "Strike=" << parsed.strike
         << "Type=" << parsed.optionType
         << "Expiry=" << parsed.expiry;
```

This will show what the tokenizer extracted from each query.

## Common Issues

### No Results Found
- Check if master files are loaded
- Verify the contract exists (e.g., strike 26000 might not exist)
- Try simpler queries like just "nifty"

### Order Still Matters
- If reversed order doesn't work, the tokenizer may need debugging
- Check logs for parsing errors

### Wrong Contracts Returned
- The scoring algorithm may need tuning
- Verify token classification is correct

## Success Criteria

✅ **Pass:** Query "nifty 26000" and "26000 nifty" return same results  
✅ **Pass:** Query "nifty ce" returns only Call options  
✅ **Pass:** Query "nifty 26000 ce" returns NIFTY 26000 CE options  
✅ **Pass:** Query "ce 26000 nifty" returns same as above  
✅ **Pass:** Query "nifty 17feb" finds contracts expiring in February  
✅ **Pass:** Query "reliance EQ" finds RELIANCE equity  

## Next Steps

Once manual testing confirms it works:
1. Fix the C++ unit test build configuration
2. Add automated test suite
3. Integrate search into more UI components (SnapQuote, Order Entry, etc.)

## Reference

- Implementation: [SearchTokenizer.cpp](c:/Users/admin/Desktop/trading_terminal_cpp/src/search/SearchTokenizer.cpp)
- Integration: [RepositoryManager.cpp](c:/Users/admin/Desktop/trading_terminal_cpp/src/repository/RepositoryManager.cpp#L883-L995)
- Documentation: [GLOBAL_SEARCH_IMPLEMENTATION_GUIDE.md](c:/Users/admin/Desktop/trading_terminal_cpp/docs/GLOBAL_SEARCH_IMPLEMENTATION_GUIDE.md)
- Test Cases: `test_search_queries.py --queries`
