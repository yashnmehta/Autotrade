# Trading Terminal - Quick Dependency Setup

## Required Dependencies

| Dependency | Status | Installation Command |
|------------|--------|---------------------|
| Qt 5.15.2  | ✅ Installed | `C:\Users\admin\Qt\5.15.2\msvc2019_64` |
| Visual Studio 2026 | ✅ Installed | MSVC 19.50 |
| Boost 1.70+ | ❌ **NEEDS INSTALL** | `choco install boost-msvc-14.3 -y` |
| OpenSSL | ❌ **NEEDS INSTALL** | `choco install openssl -y` |

## Installation Steps

### 1. Install Dependencies via Chocolatey

**Run PowerShell as Administrator**, then:

```powershell
# Install Boost (header-only libraries for Beast/Asio)
choco install boost-msvc-14.3 -y

# Install OpenSSL
choco install openssl -y
```

**Default Install Locations:**
- Boost: `C:\local\boost_1_87_0\`
- OpenSSL: `C:\Program Files\OpenSSL-Win64\`

### 2. Verify Installation

```powershell
# Check Boost
Test-Path "C:\local\boost_1_87_0"

# Check OpenSSL
Test-Path "C:\Program Files\OpenSSL-Win64"
```

### 3. Build the Project

```cmd
# Run setup script (sets environment variables)
setup-qt-env.bat

# Clean and rebuild
cd build
cmake -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release ..
nmake

# Deploy Qt dependencies
windeployqt --release --no-translations TradingTerminal.exe

# Run
TradingTerminal.exe
```

## Alternative: Manual Download

If Chocolatey installation fails:

### Boost
1. Download from: https://sourceforge.net/projects/boost/files/boost-binaries/
2. Get: `boost_1_87_0-msvc-14.3-64.exe`
3. Install to: `C:\local\boost_1_87_0`

### OpenSSL
1. Download from: https://slproweb.com/products/Win64OpenSSL.html
2. Get: `Win64 OpenSSL v3.x Light`
3. Install to default location

## Troubleshooting

### CMake can't find Boost
Set environment variable:
```cmd
set BOOST_ROOT=C:\local\boost_1_87_0
```

### CMake can't find OpenSSL
Set environment variable:
```cmd
set OPENSSL_ROOT_DIR=C:\Program Files\OpenSSL-Win64
```

### Path with spaces issues
Use quotes in CMake:
```cmd
cmake -DBOOST_ROOT="C:\local\boost_1_87_0" ..
```
