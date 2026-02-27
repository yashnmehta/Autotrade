# Trading Terminal C++ - Clean Development Setup Guide

## Current Issues
- MinGW g++ 8.1.0 from Chocolatey is crashing during compilation
- AutoMoc preprocessing fails consistently
- Build environment is unstable

## Cleanup Steps

### 1. Uninstall Problematic Packages
Run PowerShell as Administrator:

```powershell
# Uninstall broken MinGW and Qt from Chocolatey
choco uninstall qt5-default -y --remove-dependencies
choco uninstall mingw -y
choco uninstall aqt -y
```

### 2. Clean Build Artifacts
```powershell
cd C:\Users\admin\Desktop\trading_terminal_cpp
Remove-Item -Recurse -Force build -ErrorAction SilentlyContinue
Remove-Item -Recurse -Force CMakeCache.txt -ErrorAction SilentlyContinue
```

### 3. Install Proper MinGW-w64 (Standalone)
Download and install MinGW-w64 from WinLibs:
- URL: https://winlibs.com/
- Download: `winlibs-x86_64-posix-seh-gcc-13.2.0-mingw-w64msvcrt-11.0.1-r5.7z`
- Extract to: `C:\mingw64`
- Add to PATH: `C:\mingw64\bin`

Or use winget (simpler):
```powershell
winget install --id BrechtSanders.WinLibs.POSIX.UCRT --accept-source-agreements
```

### 4. Install Qt 5.15 with MinGW (Official Installer)
- Download: https://www.qt.io/download-qt-installer
- Install Qt 5.15.2 with MinGW 8.1.0 64-bit component
- Install to: `C:\Qt\5.15.2\mingw81_64`

## Proper Development Environment Setup

### Required Tools (Keep):
✅ **Git** 2.52.0 (Already installed)
✅ **CMake** 4.2.1 (Already installed)  
✅ **PowerShell** 7.5.4 (Already installed)
✅ **vcpkg** (Already installed at C:\vcpkg)
✅ **Boost** 1.90.0 via vcpkg (Already installed)
✅ **OpenSSL** 3.6.0 via vcpkg (Already installed)

### Tools to Install/Fix:
⚠️ **MinGW-w64 GCC 13.2+** (Replace broken 8.1.0)
⚠️ **Qt 5.15.2 MinGW** (Reinstall properly)

## Post-Cleanup Setup

### 1. Update Environment Variables
```powershell
# Add MinGW to PATH (permanent)
[Environment]::SetEnvironmentVariable("PATH", "C:\mingw64\bin;$env:PATH", "Machine")

# Add Qt to PATH (permanent)  
[Environment]::SetEnvironmentVariable("PATH", "C:\Qt\5.15.2\mingw81_64\bin;$env:PATH", "Machine")

# Refresh current session
$env:PATH = [Environment]::GetEnvironmentVariable('PATH', 'Machine') + ';' + [Environment]::GetEnvironmentVariable('PATH', 'User')
```

### 2. Verify Installation
```powershell
# Check compiler
g++ --version  # Should show GCC 13.2.0 or newer

# Check CMake
cmake --version  # Should show 4.2.1

# Check Qt
qmake --version  # Should show Qt 5.15.2

# Check vcpkg packages
C:\vcpkg\vcpkg.exe list
```

### 3. Configure Project
```powershell
cd C:\Users\admin\Desktop\trading_terminal_cpp
mkdir build -Force
cd build

cmake -G "MinGW Makefiles" `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_PREFIX_PATH=C:\Qt\5.15.2\mingw81_64 `
  -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake `
  -DVCPKG_TARGET_TRIPLET=x64-mingw-dynamic `
  -DBoost_INCLUDE_DIR=C:\vcpkg\installed\x64-mingw-dynamic\include `
  -DOPENSSL_ROOT_DIR=C:\vcpkg\installed\x64-mingw-dynamic `
  ..
```

### 4. Build Project
```powershell
mingw32-make -j4
```

### 5. Run Application
```powershell
.\TradingTerminal.exe
```

## Updated setup-qt-env.bat

Update your `setup-qt-env.bat` file:

```batch
@echo off
REM ============================================================
REM Trading Terminal - MinGW Environment Setup
REM ============================================================

echo.
echo ================================================
echo Trading Terminal - Environment Setup
echo ================================================
echo.

REM Set MinGW paths
echo Setting up MinGW environment...
set "PATH=C:\mingw64\bin;%PATH%"

REM Set Qt paths  
echo Setting up Qt paths...
set "QT_DIR=C:\Qt\5.15.2\mingw81_64"
set "PATH=%QT_DIR%\bin;%PATH%"
set "CMAKE_PREFIX_PATH=%QT_DIR%"
set "Qt5_DIR=%QT_DIR%\lib\cmake\Qt5"

REM Set vcpkg paths
set "VCPKG_ROOT=C:\vcpkg"
set "PATH=%VCPKG_ROOT%\installed\x64-mingw-dynamic\bin;%PATH%"

echo.
echo Environment configured:
echo   - MinGW:   C:\mingw64\bin
echo   - Qt:      %QT_DIR%
echo   - vcpkg:   %VCPKG_ROOT%
echo   - Boost:   %VCPKG_ROOT%\installed\x64-mingw-dynamic
echo   - OpenSSL: %VCPKG_ROOT%\installed\x64-mingw-dynamic
echo.
echo To build the project, run:
echo   cd build
echo   cmake -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release ..
echo   mingw32-make -j4
echo.
```

## CMake Configuration Helper

Create `configure.bat`:

```batch
@echo off
call setup-qt-env.bat

cd build
cmake -G "MinGW Makefiles" ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_PREFIX_PATH=C:\Qt\5.15.2\mingw81_64 ^
  -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake ^
  -DVCPKG_TARGET_TRIPLET=x64-mingw-dynamic ^
  -DBoost_INCLUDE_DIR=C:\vcpkg\installed\x64-mingw-dynamic\include ^
  -DOPENSSL_ROOT_DIR=C:\vcpkg\installed\x64-mingw-dynamic ^
  ..
```

## Build Helper

Create `build.bat`:

```batch
@echo off
call setup-qt-env.bat

cd build
mingw32-make -j4
```

## Troubleshooting

### If compiler still crashes:
1. Restart computer
2. Check Windows Defender/Antivirus isn't blocking compiler
3. Ensure you have at least 8GB free RAM
4. Try building with `-j1` (single thread)

### If Qt not found:
1. Verify Qt installation path
2. Update `CMAKE_PREFIX_PATH` in commands
3. Check `qmake --version` works

### If Boost/OpenSSL not found:
```powershell
# Reinstall with vcpkg
cd C:\vcpkg
.\vcpkg remove boost:x64-mingw-dynamic openssl:x64-mingw-dynamic
.\vcpkg install boost-asio:x64-mingw-dynamic boost-beast:x64-mingw-dynamic openssl:x64-mingw-dynamic
```

## Summary of Changes

**Removed:**
- ❌ MinGW 8.1.0 from Chocolatey (broken)
- ❌ qt5-default from Chocolatey (uses broken MinGW)
- ❌ aqt tool (no longer needed)

**Keeping:**
- ✅ Git, CMake, PowerShell
- ✅ vcpkg with Boost and OpenSSL
- ✅ Project source code and configurations

**Installing Fresh:**
- ⚙️ MinGW-w64 GCC 13.2+ (stable standalone version)
- ⚙️ Qt 5.15.2 with proper MinGW support

## Next Steps After Setup

1. Test simple C++ compilation
2. Configure CMake
3. Build project
4. Verify application runs
5. Test BSE broadcast parsing (your debug code is ready!)

Your BSE parser debug code in `bse_parser.cpp` targeting tokens **1163264** and **1143697** is already in place and will work once the build succeeds.
