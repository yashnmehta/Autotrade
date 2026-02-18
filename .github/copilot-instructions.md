# GitHub Copilot Instructions for Trading Terminal C++

## Build System

### IMPORTANT: MSVC Build Only

This project **MUST** be built with MSVC compiler in the `build_ninja` directory.

**Why MSVC?**
- MinGW compiled Qt library does **NOT** support QtWebEngine
- QtWebEngine (Chromium) requires MSVC on Windows
- TradingView chart integration depends on QtWebEngine

### Build Command

```bash
C:\Users\admin\Desktop\trading_terminal_cpp\build_ninja\build.bat
```

**Build Script Details:**
- Uses Ninja build system for fast incremental builds
- Automatically calls vcvars64.bat to setup MSVC environment
- Typical build time: <1 second for single file changes
- Full rebuild: 10-30 seconds

### Build Directory Structure

```
build_ninja/          # MSVC + Ninja (USE THIS)
├── build.bat         # Fast build script
├── build.ninja       # Ninja build file
├── TradingTerminal.exe
└── run.bat

build/                # Legacy directory (DO NOT USE)
build_msvc/           # MSVC solution files (DO NOT USE)
```

### Running the Application

```bash
cd build_ninja
.\TradingTerminal.exe
```

## Code Changes Workflow

1. Edit source files in `src/` or `include/`
2. Run: `cd build_ninja && .\build.bat`
3. If successful, run: `.\TradingTerminal.exe`
4. Monitor console output for errors

## Common Issues

- **Error: QtWebEngine not found** → You're using MinGW Qt. Must use MSVC Qt.
- **Linker errors** → Clean rebuild: `ninja clean && ninja`
- **Missing DLLs** → Ensure Qt MSVC binaries are in PATH

## Project Architecture

- **Qt Version**: 5.15.2 (MSVC 2019 64-bit)
- **Build System**: CMake + Ninja
- **Compiler**: MSVC 2019/2022
- **Key Dependencies**: QtWebEngine, QtWebChannel, QtNetwork, QtWidgets
