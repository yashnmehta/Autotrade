# Trading Terminal (C++ Edition)

**Status:** ✅ **STABLE** - Critical crashes fixed (Dec 12, 2025)

This is the native C++ implementation of the Trading Terminal using Qt 5.15+.

## 🎯 Latest Update

**December 12, 2025** - Major stability fix:
- ✅ Fixed critical segmentation fault on startup
- ✅ Corrected window layout hierarchy (title bar now at top)
- ✅ Redesigned architecture for better maintainability
- ✅ Application now runs stably on macOS

See `docs/CRITICAL_FIX_SUMMARY.md` for details.

## Prerequisites

### macOS
```bash
brew install cmake qt@5
```

### Linux (Ubuntu/Debian)
```bash
sudo apt-get install build-essential cmake qt5-default qtbase5-dev libqt5websockets5-dev
```

### Windows
Install Qt 5.15 via the Qt Online Installer and ensure CMake is installed.

#### Automated Setup
- **MinGW**: Run `scripts\mingw81\setup.bat`
- **MSVC**: Run `scripts\msvc\setup.bat` (Requires Visual Studio)

## Build Instructions

1. Create a build directory:
   ```bash
   mkdir build && cd build
   ```

2. Configure with CMake:
   ```bash
   # macOS (pointing to Qt5)
   cmake -DCMAKE_PREFIX_PATH=/usr/local/opt/qt@5 ..
   
   # Linux / Windows
   cmake ..
   ```

3. Build:
   ```bash
   make -j4
   ```

4. Run:
   ```bash
   ./TradingTerminal
   ```

## Project Structure

- `src/`: Source code (.cpp)
- `include/`: Header files (.h)
- `resources/`: UI files, icons, etc.
- `tests/`: Unit tests
