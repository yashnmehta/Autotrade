# MSVC Build Instructions

This folder contains scripts for building the Trading Terminal with Microsoft Visual C++ (MSVC) compiler.

## Why MSVC?

MSVC is required for:
- **QtWebEngine** support (Chromium-based web views)
- Better debugging with Visual Studio
- Potentially better optimizations on Windows

## Prerequisites

1. **Visual Studio 2019/2022** with C++ Desktop Development workload
2. **Qt 5.15.2 MSVC** (default path: `C:/Qt/5.15.2/msvc2019_64`)
3. **CMake** (3.14 or later)
4. **OpenSSL** (optional, for secure WebSocket connections)

## Quick Start

### First Time Setup

1. Open PowerShell or CMD in this folder
2. Run the setup script:
   ```cmd
   setup.bat
   ```
   This will:
   - Detect your Visual Studio version
   - Configure CMake with MSVC toolchain
   - Generate Visual Studio solution files

### Building

```cmd
build.bat
```

This will:
- Compile the project in Debug mode
- Automatically deploy Qt DLLs using `windeployqt`
- Copy OpenSSL DLLs if available
- Optionally offer to run the application

### Running

```cmd
run.bat
```

This will:
- Deploy any missing Qt dependencies
- Launch the Trading Terminal

Alternatively, you can:
- Open `build_msvc\TradingTerminal.sln` in Visual Studio
- Build and run from Visual Studio IDE

## Build Configurations

- **Debug**: Includes debug symbols, no optimizations (default)
- **Release**: Optimized for performance, smaller binary size
- **RelWithDebInfo**: Release optimizations + debug info
- **MinSizeRel**: Optimized for smallest binary size

To build Release:
```cmd
cmake --build build_msvc --config Release
```

## Automatic DLL Deployment

The build system automatically:
- Runs `windeployqt` after each build (MSVC only)
- Copies Qt5 DLLs (Core, Gui, Widgets, Network, WebSockets, Sql, etc.)
- Copies OpenSSL DLLs if found
- Copies required platform plugins (qwindows.dll, etc.)

## Troubleshooting

### Missing Qt DLLs

If you see errors about missing Qt DLLs:
1. Verify Qt installation path in `setup.bat` (line 46)
2. Run `run.bat` which will re-deploy dependencies
3. Check that `C:/Qt/5.15.2/msvc2019_64/bin/windeployqt.exe` exists

### OpenSSL DLL Errors

If you see OpenSSL errors:
1. Install OpenSSL from: https://slproweb.com/products/Win32OpenSSL.html
2. Choose "Win64 OpenSSL v1.1.1" (not v3.x)
3. Install to default location: `C:\Program Files\OpenSSL-Win64`

### Visual Studio Version Mismatch

The setup script auto-detects VS versions in this order:
1. Visual Studio 2026 (if available)
2. Visual Studio 2022 (fallback)
3. Visual Studio 2019 (fallback)

If detection fails:
- Ensure Visual Studio C++ workload is installed
- Run VS Installer and add "Desktop development with C++"

### QtWebEngine Not Found

If QtWebEngine features are disabled:
- Verify Qt installation includes WebEngine module
- Check CMake output during setup for QtWebEngine status
- Reinstall Qt with WebEngine component selected

## File Structure

```
build_msvc/
├── TradingTerminal.sln          # Visual Studio solution
├── TradingTerminal.vcxproj      # Project file
├── Debug/                       # Debug binaries + DLLs
│   ├── TradingTerminal.exe
│   ├── Qt5*.dll                 # Auto-deployed
│   └── platforms/               # Qt plugins
└── Release/                     # Release binaries
```

## Development Workflow

1. **Setup once**: `setup.bat`
2. **Edit code** in your favorite editor or VS
3. **Build**: Either `build.bat` or F7 in Visual Studio
4. **Run**: Either `run.bat` or F5 in Visual Studio
5. **Debug**: Use Visual Studio's debugger (F5 with breakpoints)

## Notes

- First build may take 5-10 minutes
- Subsequent builds are incremental (faster)
- Clean rebuild: Delete `build_msvc/` folder and re-run setup
- The MinGW build (`build/` folder) is separate and independent

## Performance Tips

- Use **Release** build for production/testing performance
- Use **Debug** build only for debugging
- Enable all CPU cores: `cmake --build build_msvc --config Release -j 8`

## See Also

- Main README: `../../README.md`
- CMake configuration: `../../CMakeLists.txt`
- MinGW build: `../../build/` (uses make instead of MSVC)
