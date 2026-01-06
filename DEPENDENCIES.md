# Trading Terminal C++ - Dependency Installation Guide

## Required Dependencies

1. **Qt 5.15.2** ✅ Already installed at `C:\Users\admin\Qt\5.15.2\msvc2019_64`
2. **MSVC (Visual Studio 2026)** ✅ Already installed
3. **Boost 1.70+** ❌ Needs installation
4. **OpenSSL** ❌ Needs installation

## Installation Options

### Option 1: Using vcpkg (Recommended)

```powershell
# Install vcpkg
cd C:\
git clone https://github.com/microsoft/vcpkg
cd vcpkg
.\bootstrap-vcpkg.bat

# Install dependencies
.\vcpkg install boost-beast:x64-windows
.\vcpkg install boost-asio:x64-windows
.\vcpkg install openssl:x64-windows

# Integrate with Visual Studio
.\vcpkg integrate install
```

Then update `setup-qt-env.bat` with vcpkg paths:
```bat
set "VCPKG_ROOT=C:\vcpkg"
set "BOOST_ROOT=%VCPKG_ROOT%\installed\x64-windows"
set "OPENSSL_ROOT_DIR=%VCPKG_ROOT%\installed\x64-windows"
```

### Option 2: Manual Installation

#### Boost (Header-Only)

1. Download from: https://www.boost.org/users/download/
2. Extract to: `C:\boost_1_84_0`
3. No compilation needed (Beast and Asio are header-only)

#### OpenSSL

1. Download from: https://slproweb.com/products/Win64OpenSSL.html
2. Install Win64 OpenSSL v3.x
3. Default location: `C:\Program Files\OpenSSL-Win64`

### Option 3: Using install-dependencies.ps1 Script

Run the provided PowerShell script:
```powershell
.\install-dependencies.ps1
```

## After Installation

1. Update paths in `setup-qt-env.bat` if needed
2. Run setup script:
   ```cmd
   setup-qt-env.bat
   ```
3. Build the project:
   ```cmd
   cd build
   cmake -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release ..
   nmake
   ```

## Verification

Check if dependencies are found:
```cmd
cmake -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release ..
```

Look for:
- ✅ Found Boost
- ✅ Found OpenSSL
- ✅ Found Qt5
