# Development Mode Guide
## Testing UI Without XTS Server

---

## 🔧 Quick Start

### Enable Development Mode

```bash
cd /Users/yashmehta/Desktop/go_proj/trading_terminal_cpp

# Configure with DEV_MODE enabled
cmake -B build -DDEV_MODE=ON

# Build
cmake --build build

# Run
cd build
./TradingTerminal.app/Contents/MacOS/TradingTerminal
```

### Switch Back to Production Mode

```bash
# Reconfigure with DEV_MODE disabled
cmake -B build -DDEV_MODE=OFF

# Rebuild
cmake --build build
```

---

## 📋 What Development Mode Does

### ✅ Enabled Features:
- Loads master contracts from cached files
- Shows main window directly (no login)
- Creates TradingDataService (empty but functional)
- File logging still works
- UI is fully functional for testing

### ❌ Disabled Features:
- XTS API login (bypassed)
- Market Data API connection (skipped)
- Interactive API connection (skipped)
- WebSocket connection (skipped)
- Real-time data updates (no API)
- Order placement (no API)
- Position/trade fetching (no API)

### ⚠️ Expected Behavior:
- Symbol search works (if masters cached)
- UI layout and navigation works
- Windows can be opened/closed
- Filters and sorting work
- **Trading operations will fail gracefully** (no server connection)

---

## 📁 Required Files

For symbol search to work, you need cached master files in:
```
build/TradingTerminal.app/Contents/MacOS/Masters/
```

Required files (at least one):
- `contract_nsefo_latest.txt`
- `contract_nsecm_latest.txt`
- `master_contracts_latest.txt`

### How to Get Master Files:

**Option 1: Run with server online once**
```bash
# Switch to production mode
cmake -B build -DDEV_MODE=OFF
cmake --build build

# Run and login (downloads masters)
cd build
./TradingTerminal.app/Contents/MacOS/TradingTerminal

# Then switch back to dev mode
cmake -B build -DDEV_MODE=ON
cmake --build build
```

**Option 2: Copy from another installation**
```bash
cp /path/to/existing/Masters/*.txt build/TradingTerminal.app/Contents/MacOS/Masters/
```

**Option 3: Work without masters**
- UI will still work
- Symbol search will be limited
- You'll see a warning on startup

---

## 🎯 Use Cases

### When to Use Development Mode:
- ✅ XTS server is down/offline
- ✅ Testing UI layout changes
- ✅ Testing window management
- ✅ Testing filters and sorting
- ✅ Testing visual elements
- ✅ Debugging UI issues
- ✅ Working on non-trading features

### When to Use Production Mode:
- ✅ XTS server is online
- ✅ Testing login flow
- ✅ Testing API integration
- ✅ Testing real-time data
- ✅ Testing order placement
- ✅ Testing WebSocket connection
- ✅ End-to-end testing

---

## 🔍 Checking Current Mode

When you run cmake, you'll see:
```
-- 🔧 DEV_MODE enabled - using main_work.cpp (login bypassed)
```
or
```
-- 🚀 Production mode - using main.cpp (full login flow)
```

You can also check the log file:
```bash
cat logs/trading_terminal_*.log | head -10
```

Development mode will show:
```
========================================
DEVELOPMENT MODE - LOGIN BYPASSED
========================================
```

---

## 💡 Tips

### 1. Keep Both Modes Working
- Don't break `main.cpp` while working in dev mode
- Test both modes before committing changes

### 2. Use Logs
- Check `logs/` directory for detailed output
- Logs work in both modes

### 3. API Calls Will Fail
- This is expected in dev mode
- Code should handle missing XTS clients gracefully
- Check for null pointers before calling API methods

### 4. Quick Toggle
```bash
# Quick switch to dev mode
cmake -B build -DDEV_MODE=ON && cmake --build build

# Quick switch to production
cmake -B build -DDEV_MODE=OFF && cmake --build build
```

---

## 🐛 Troubleshooting

### "No master files found"
**Solution**: Run with server online once, or copy master files manually

### "Segmentation fault"
**Cause**: Code trying to use null XTS clients
**Solution**: Add null checks before API calls:
```cpp
if (m_mdClient) {
    m_mdClient->subscribe(...);
} else {
    qWarning() << "MD client not available (dev mode?)";
}
```

### "UI looks broken"
**Cause**: Missing data from API
**Solution**: Use mock data or default values:
```cpp
// Instead of:
double ltp = tick.lastTradedPrice;

// Use:
double ltp = tick.lastTradedPrice > 0 ? tick.lastTradedPrice : 100.0;
```

---

## 📊 Comparison

| Feature | Production Mode | Development Mode |
|---------|----------------|------------------|
| XTS Login | ✅ Required | ❌ Bypassed |
| Master Loading | 📥 Download | 📁 Cached files |
| Real-time Data | ✅ WebSocket | ❌ None |
| Order Placement | ✅ API | ❌ None |
| UI Testing | ✅ Full | ✅ Full |
| Build Time | ~60s | ~60s |
| Startup Time | ~10s | ~2s |
| Server Required | ✅ Yes | ❌ No |

---

## 🎓 Example Workflow

### Morning: Server Down, UI Work
```bash
# Enable dev mode
cmake -B build -DDEV_MODE=ON
cmake --build build

# Work on UI improvements
# Test layout changes
# Test window management
```

### Afternoon: Server Up, Integration Testing
```bash
# Switch to production
cmake -B build -DDEV_MODE=OFF
cmake --build build

# Test full login flow
# Test real-time data
# Test order placement
```

---

*Happy Development! 🚀*
