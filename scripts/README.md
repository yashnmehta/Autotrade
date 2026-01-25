# Index Master Download Scripts

## Quick Start

### Download NSE CM Index Master

```bash
cd scripts
python3 download_index_master.py
```

The script will:
1. Auto-load credentials from `../configs/config.ini` (XTS section)
2. Or prompt you for API key and secret key
3. Login to XTS Market Data API
4. Fetch index list for NSE CM (segment=1)
5. Save to `MasterFiles/nse_cm_index_master.csv`
6. Save raw JSON to `MasterFiles/nse_cm_index_master.json`

### Alternative: Use Environment Variables

```bash
export XTS_API_KEY="your_public_key_here"
export XTS_SECRET_KEY="your_secret_key_here"
python3 download_index_master.py
```

### Requirements

```bash
pip install requests
```

## Output Format

The script generates a CSV file compatible with the application:

```csv
id,index_name,token,created_at
1,Nifty 50,26000,2026-01-25 18:30:00.000
2,Nifty Bank,26009,2026-01-25 18:30:00.000
3,Nifty IT,26008,2026-01-25 18:30:00.000
...
```

## Usage in Application

After downloading, the file needs to be copied to the build directory:

```bash
# Option 1: Manual copy
cp MasterFiles/nse_cm_index_master.csv build/TradingTerminal.app/Contents/MacOS/Masters/

# Option 2: Rebuild application (auto-copies)
cmake --build build
```

## XTS API Reference

- **Endpoint:** `GET /instruments/indexlist?exchangeSegment=1`
- **Auth:** Bearer token from login
- **Segments:** 1=NSECM, 2=NSEFO, 11=BSECM, 12=BSEFO
- **Documentation:** https://symphonyfintech.com/xts-market-data-front-end-api-v2/

## Troubleshooting

### Login Failed
- Check API key and secret key are correct
- Verify API subscription is active
- Try logging in via browser first

### Network Timeout
- Check internet connection
- Verify firewall not blocking requests
- XTS servers may be down (check status)

### Empty Response
- Exchange may be closed
- No indices available for that segment
- Check raw JSON file for error messages

## What This Fixes

Without the index master file:
- ❌ Index UDP price updates fail
- ❌ Greeks calculation fails for index options (~15,000 contracts)
- ❌ ATM Watch doesn't work for NIFTY/BANKNIFTY
- ⚠️  Warning in logs: "Failed to open Index Master"

With the index master file:
- ✅ Index names resolve to tokens
- ✅ UDP price updates work
- ✅ Greeks calculation succeeds
- ✅ ATM Watch functional
