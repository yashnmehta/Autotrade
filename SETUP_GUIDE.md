# Trading Terminal C++ - Development Setup Guide

## Overview
This guide will help you set up a clean, stable development environment for the Trading Terminal project.

---

## Prerequisites

### Required Software
1. **MinGW-w64 GCC 13.2+** (Modern C++17 compiler)
2. **Qt 5.15.2 with MinGW** (GUI framework)
3. **CMake 3.20+** (Build system)
4. **vcpkg** (C++ package manager)
5. **Git** (Version control)

---

## Step 1: Clean Up Existing Installation

### Remove Broken Chocolatey Packages

**Run PowerShell as Administrator:**

```powershell
# Force uninstall MinGW and Qt from Chocolatey
choco uninstall -y mingw --force
choco uninstall -y qt5-default --force
choco uninstall -y aqt --force

# Clean up leftover files
Remove-Item -Path "C:\ProgramData\chocolatey\lib\mingw" -Recurse -Force -ErrorAction SilentlyContinue
Remove-Item -Path "C:\ProgramData\chocolatey\lib\qt5-default" -Recurse -Force -ErrorAction SilentlyContinue
```

### Clean Build Artifacts

```powershell
cd C:\Users\admin\Desktop\trading_terminal_cpp
Remove-Item -Path "build" -Recurse -Force -ErrorAction SilentlyContinue
Remove-Item -Path "benchmark_qt_vs_native_autogen" -Recurse -Force -ErrorAction SilentlyContinue
```

---

## Step 2: Install Required Software

### 2.1 Install MinGW-w64 (GCC 13.2+)

**Option A: Using winget (Recommended)**

```powershell
winget install --id BrechtSanders.WinLibs.POSIX.UCRT
```

This installs MinGW-w64 with GCC 13.2+ to: `C:\mingw64`

**Option B: Manual Download**
- Download from: https://winlibs.com/
- Choose: `GCC 13.2.0 + LLVM/Clang/LLD/LLDB 18.1.8 + MinGW-w64 12.0.0 (UCRT) - release 3`
- Extract to: `C:\mingw64`

### 2.2 Install Qt 5.15.2 with MinGW

**Download Qt Installer:**
- URL: https://www.qt.io/download-qt-installer
- Or direct: https://download.qt.io/official_releases/online_installers/qt-unified-windows-x64-online.exe

**During Installation:**
1. Create/login to Qt account (free)
2. Select **Custom Installation**
3. Components to install:
   - `Qt 5.15.2`
     - [x] MinGW 8.1.0 64-bit
     - [x] Qt 5.15.2 Prebuilt Components for MinGW
   - `Developer and Designer Tools`
     - [x] Qt Creator (optional)
     - [x] MinGW 8.1.0 64-bit (if not selected above)

**Default Installation Path:** `C:\Qt\5.15.2\mingw81_64`

### 2.3 Verify CMake Installation

```powershell
cmake --version
# Should show: cmake version 3.20 or higher
```

If not installed:
```powershell
winget install Kitware.CMake
```

### 2.4 Verify vcpkg (Already Installed)

```powershell
cd C:\vcpkg
.\vcpkg list
# Should show: boost-beast, boost-asio, openssl for x64-mingw-dynamic
```

---

## Step 3: Configure Environment Variables

### Add to System PATH (Permanent)

**Run PowerShell as Administrator:**

```powershell
# Add MinGW to PATH
[Environment]::SetEnvironmentVariable("Path", $env:Path + ";C:\mingw64\bin", "Machine")

# Add Qt to PATH
[Environment]::SetEnvironmentVariable("Path", $env:Path + ";C:\Qt\5.15.2\mingw81_64\bin", "Machine")

# Refresh current session
$env:Path = [System.Environment]::GetEnvironmentVariable("Path", "Machine")
```

### Verify PATH Configuration

```powershell
# Check GCC version
g++ --version
# Should show: g++ (MinGW-W64 ...) 13.2.0 or higher

# Check Qt
qmake --version
# Should show: QMake version 3.1, Using Qt version 5.15.2

# Check CMake
cmake --version
# Should show: cmake version 3.x.x
```

---

## Step 4: Build Trading Terminal

### 4.1 Configure Project

```powershell
cd C:\Users\admin\Desktop\trading_terminal_cpp

# Run configuration script
.\configure-project.bat
```

This will:
- Set up environment variables
- Run CMake with correct paths
- Generate MinGW Makefiles

### 4.2 Build Project

```powershell
# Build with parallel jobs
.\build-project.bat
```

Or manually:
```powershell
cd build
mingw32-make -j8
```

### 4.3 Run Application

```powershell
.\run-app.bat
```

Or manually:
```powershell
cd build
.\TradingTerminal.exe
```

---

## Step 5: Verify Installation

### Check Build Output

After successful build, you should see:
```
build/
  ├── TradingTerminal.exe (20-25 MB)
  ├── libboost_*.dll
  ├── libssl*.dll
  ├── libcrypto*.dll
  ├── Qt5*.dll
  └── ... (other DLLs)
```

### Test Application

1. Launch: `.\run-app.bat`
2. Application should start without DLL errors
3. Check console for any warnings

---

## Troubleshooting

### Issue: "g++ not found" or "qmake not found"

**Solution:**
```powershell
# Verify PATH manually
$env:Path -split ';' | Select-String -Pattern 'mingw|Qt'

# Add paths temporarily
$env:Path = "C:\mingw64\bin;C:\Qt\5.15.2\mingw81_64\bin;$env:Path"
```

### Issue: CMake cannot find Qt

**Solution:**
```powershell
# Set Qt path explicitly
$env:CMAKE_PREFIX_PATH = "C:\Qt\5.15.2\mingw81_64"
.\configure-project.bat
```

### Issue: Missing DLL errors when running

**Solution:**
The `CMakeLists.txt` has POST_BUILD commands that should copy DLLs automatically. If still missing:

```powershell
cd build

# Run Qt deployment tool
windeployqt.exe --release --no-translations TradingTerminal.exe

# Copy vcpkg DLLs manually
Copy-Item C:\vcpkg\installed\x64-mingw-dynamic\bin\*.dll .

# Copy MinGW runtime DLLs
Copy-Item C:\mingw64\bin\libgcc_s_seh-1.dll .
Copy-Item C:\mingw64\bin\libstdc++-6.dll .
Copy-Item C:\mingw64\bin\libwinpthread-1.dll .
```

### Issue: Compiler crashes during build

**Solution:**
1. This was the issue with Chocolatey's GCC 8.1.0
2. With GCC 13.2+, this should not happen
3. If it persists:
   ```powershell
   # Try single-threaded build
   cd build
   mingw32-make -j1
   ```

### Issue: AutoMoc errors

**Solution:**
```powershell
# Clean autogen files
Remove-Item -Recurse -Force *_autogen
Remove-Item -Recurse -Force CMakeFiles

# Reconfigure
cd ..
.\configure-project.bat
.\build-project.bat
```

---

## Project Structure

```
trading_terminal_cpp/
├── setup-development-env.bat    # Sets up PATH and environment
├── configure-project.bat         # Runs CMake configuration
├── build-project.bat            # Builds the project
├── clean-rebuild.bat            # Cleans and rebuilds
├── run-app.bat                  # Runs the application
├── CMakeLists.txt               # Build configuration
├── src/                         # Source code
├── include/                     # Header files
├── lib/                         # Libraries
│   └── cpp_broadcast_bsefo/    # BSE FO broadcast parser
├── tests/                       # Unit tests
├── build/                       # Build output (generated)
└── configs/                     # Configuration files
```

---

## Development Workflow

### Daily Development

1. **Open PowerShell in project directory**
2. **Make code changes**
3. **Build:**
   ```powershell
   .\build-project.bat
   ```
4. **Run:**
   ```powershell
   .\run-app.bat
   ```

### After Pulling Changes

```powershell
# Clean rebuild
.\clean-rebuild.bat
.\configure-project.bat
.\build-project.bat
```

### Debugging

**Enable Debug Build:**
Edit `configure-project.bat`, change:
```batch
-DCMAKE_BUILD_TYPE=Release
```
To:
```batch
-DCMAKE_BUILD_TYPE=Debug
```

Then rebuild:
```powershell
.\clean-rebuild.bat
.\configure-project.bat
.\build-project.bat
```

---

## Next Steps: BSE Parser Debug

Once the build is working:

### 1. Test BSE Parser with Debug Output

The debug code is already added in [lib/cpp_broadcast_bsefo/src/bse_parser.cpp](lib/cpp_broadcast_bsefo/src/bse_parser.cpp#L108-L130)

**Target Tokens:**
- `1163264` - SENSEX option CE 84500
- `1143697` - SENSEX future

### 2. Run Application

```powershell
.\run-app.bat
```

### 3. Add Tokens to Market Watch

In the application:
1. Open Market Watch window
2. Add token `1163264`
3. Add token `1143697`

### 4. Capture Debug Output

Watch the console for output like:
```
[BSE 2020 DEBUG] Token: 1163264
  Known fields: token=1163264, ltp=1234.50, ...
  Unknown fields (48-83):
    field_48=12345, field_52=67890, ...
  Unknown fields (88-103):
    field_88=11111, field_92=22222, ...
```

### 5. Analyze Field Values

Compare values over time to identify:
- **noOfTrades**: Incrementing counter
- **ltq**: Multiples of lot size, changes with each trade
- **totalBuyQty/totalSellQty**: Large values, changes frequently
- **lowerCircuit/upperCircuit**: Near LTP ±5-20%, constant during day

---

## Useful Commands

```powershell
# Check what's in PATH
$env:Path -split ';'

# Find compiler
Get-Command g++ | Select-Object -ExpandProperty Source

# Find Qt tools
Get-Command qmake | Select-Object -ExpandProperty Source

# Check vcpkg packages
cd C:\vcpkg
.\vcpkg list | Select-String 'mingw'

# View build log
Get-Content build/CMakeFiles/CMakeError.log

# Check for running processes
Get-Process | Where-Object { $_.ProcessName -like "*Trading*" }
```

---

## Support

If you encounter issues:
1. Check [Troubleshooting](#troubleshooting) section above
2. Review build logs in `build/CMakeFiles/`
3. Verify all paths are correct in batch scripts
4. Ensure environment variables are set correctly

---

## Summary Checklist

- [ ] Uninstalled Chocolatey MinGW/Qt packages
- [ ] Cleaned build artifacts
- [ ] Installed MinGW-w64 GCC 13.2+ to `C:\mingw64`
- [ ] Installed Qt 5.15.2 with MinGW to `C:\Qt\5.15.2\mingw81_64`
- [ ] Verified CMake 3.20+ is installed
- [ ] Added MinGW and Qt to system PATH
- [ ] Verified `g++ --version` shows 13.2+
- [ ] Verified `qmake --version` shows Qt 5.15.2
- [ ] Ran `configure-project.bat` successfully
- [ ] Ran `build-project.bat` successfully
- [ ] Ran `run-app.bat` - application starts without errors
- [ ] Ready to test BSE parser debug output

---

**Last Updated:** 2025-01-02
**Project:** Trading Terminal C++
**Branch:** refactored_dec
