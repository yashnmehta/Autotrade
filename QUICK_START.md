# Quick Start - Clean Setup

## Problem
The MinGW GCC 8.1.0 from Chocolatey is crashing during compilation, blocking all builds.

## Solution
Clean environment → Install stable tools → Rebuild project

---

## Step-by-Step Instructions

### 1. Clean Up (5 minutes)

**Run PowerShell as Administrator:**

```powershell
cd C:\Users\admin\Desktop\trading_terminal_cpp
.\cleanup-environment.ps1
```

Or run the batch file:
```cmd
cleanup-environment.bat
```

This will:
- Remove Chocolatey MinGW and Qt packages
- Clean build artifacts
- Remove temporary files

### 2. Install MinGW-w64 (2 minutes)

```powershell
winget install --id BrechtSanders.WinLibs.POSIX.UCRT
```

**Installation location:** `C:\mingw64`
**Compiler:** GCC 13.2+ with full C++17 support

### 3. Install Qt 5.15.2 (15-20 minutes)

1. Download: https://www.qt.io/download-qt-installer
2. Run installer (requires free Qt account)
3. Select **Custom Installation**
4. Choose components:
   - ✅ Qt 5.15.2
     - ✅ MinGW 8.1.0 64-bit
   - ✅ Developer Tools
     - ✅ Qt Creator (optional)

**Installation location:** `C:\Qt\5.15.2\mingw81_64`

### 4. Configure Environment (1 minute)

**Run PowerShell as Administrator:**

```powershell
# Add MinGW to PATH
[Environment]::SetEnvironmentVariable("Path", $env:Path + ";C:\mingw64\bin", "Machine")

# Add Qt to PATH  
[Environment]::SetEnvironmentVariable("Path", $env:Path + ";C:\Qt\5.15.2\mingw81_64\bin", "Machine")

# Restart PowerShell to load new PATH
```

### 5. Verify Installation

```powershell
# Check GCC
g++ --version
# Should show: 13.2.0 or higher

# Check Qt
qmake --version
# Should show: Qt version 5.15.2

# Check CMake
cmake --version
# Should show: 3.20 or higher
```

### 6. Build Project (5-10 minutes)

```powershell
cd C:\Users\admin\Desktop\trading_terminal_cpp

# Configure
.\configure-project.bat

# Build
.\build-project.bat
```

### 7. Run Application

```powershell
.\run-app.bat
```

---

## What Changed?

### Before (Broken)
- ❌ Chocolatey MinGW 8.1.0 (unstable, crashes on complex builds)
- ❌ Chocolatey Qt (incomplete installation)
- ❌ Build fails with AutoMoc crashes

### After (Working)
- ✅ WinLibs MinGW-w64 GCC 13.2+ (stable, modern)
- ✅ Official Qt 5.15.2 installer (complete, tested)
- ✅ Clean build environment

---

## Helper Scripts Created

| Script | Purpose |
|--------|---------|
| `cleanup-environment.ps1` | Remove broken installations (PowerShell) |
| `cleanup-environment.bat` | Remove broken installations (Batch) |
| `setup-development-env.bat` | Set environment variables |
| `configure-project.bat` | Run CMake configuration |
| `build-project.bat` | Build the project |
| `clean-rebuild.bat` | Clean and rebuild from scratch |
| `run-app.bat` | Launch the application |

---

## Time Estimate

| Task | Time |
|------|------|
| Cleanup | 5 min |
| Install MinGW | 2 min |
| Install Qt | 15-20 min |
| Configure environment | 1 min |
| Build project | 5-10 min |
| **Total** | **30-40 min** |

---

## Troubleshooting

### "g++ not found after installation"

Restart PowerShell or run:
```powershell
$env:Path = [System.Environment]::GetEnvironmentVariable("Path", "Machine")
```

### "Qt not found"

Make sure Qt was installed to `C:\Qt\5.15.2\mingw81_64`. If different location, update paths in `configure-project.bat`.

### Build still fails

1. Clean everything: `.\clean-rebuild.bat`
2. Verify tools: `g++ --version`, `qmake --version`
3. Reconfigure: `.\configure-project.bat`
4. Single-threaded build: `cd build; mingw32-make -j1`

---

## Next Steps After Successful Build

### Test BSE Parser Debug Output

1. Run application: `.\run-app.bat`
2. Add tokens to market watch:
   - Token: `1163264` (SENSEX option CE 84500)
   - Token: `1143697` (SENSEX future)
3. Watch console for debug output showing unknown field values
4. Analyze field patterns to identify:
   - `noOfTrades` (incrementing counter)
   - `ltq` (lot size multiples)
   - `totalBuyQty` / `totalSellQty` (large values)
   - `lowerCircuit` / `upperCircuit` (near LTP ±%)

Debug code location: [lib/cpp_broadcast_bsefo/src/bse_parser.cpp](lib/cpp_broadcast_bsefo/src/bse_parser.cpp#L108-L130)

---

## Support Files

- **SETUP_GUIDE.md** - Complete detailed setup guide (comprehensive)
- **QUICK_START.md** - This file (quick reference)
- **CLEAN_SETUP.md** - Original cleanup documentation

---

**Last Updated:** 2025-01-02  
**Status:** Ready for cleanup and fresh installation
