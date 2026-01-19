@echo off
REM ============================================================
REM Trading Terminal - Development Environment Setup
REM Sets up paths for MinGW, Qt, vcpkg
REM ============================================================

echo.
echo ================================================
echo Trading Terminal - Development Environment
echo ================================================
echo.

REM Set MinGW paths (MSYS2 MinGW64 - installed on this system)
echo Setting up MinGW environment...
set "MINGW_ROOT=C:\msys64\mingw64"
if not exist "%MINGW_ROOT%\bin\g++.exe" (
    echo WARNING: MinGW not found at %MINGW_ROOT%
    echo Please install MSYS2 with MinGW64
    echo Download from: https://www.msys2.org/
    pause
    exit /b 1
)
set "PATH=%MINGW_ROOT%\bin;%PATH%"

REM Set Qt paths (using MSYS2 Qt)
echo Setting up Qt paths...
set "QT_DIR=%MINGW_ROOT%"
if not exist "%QT_DIR%\bin\qmake.exe" (
    echo WARNING: Qt not found at %QT_DIR%
    echo Please install Qt via MSYS2: pacman -S mingw-w64-x86_64-qt5
    pause
    exit /b 1
)
set "PATH=%QT_DIR%\bin;%PATH%"
set "CMAKE_PREFIX_PATH=%QT_DIR%"
set "Qt5_DIR=%QT_DIR%\lib\cmake\Qt5"

REM Set vcpkg paths
set "VCPKG_ROOT=C:\vcpkg"
if exist "%VCPKG_ROOT%" (
    echo Setting up vcpkg paths...
    set "PATH=%VCPKG_ROOT%\installed\x64-mingw-dynamic\bin;%PATH%"
) else (
    echo WARNING: vcpkg not found at %VCPKG_ROOT%
    echo Some dependencies may not be available
)

REM Verify installations
echo.
echo Verifying installations...
echo.

g++ --version >nul 2>&1
if %errorlevel% equ 0 (
    echo [OK] MinGW/GCC found
    g++ --version | findstr /C:"g++"
) else (
    echo [FAIL] MinGW/GCC not found
)

qmake --version >nul 2>&1
if %errorlevel% equ 0 (
    echo [OK] Qt found  
    qmake --version | findstr /C:"Qt"
) else (
    echo [FAIL] Qt not found
)

cmake --version >nul 2>&1
if %errorlevel% equ 0 (
    echo [OK] CMake found
    cmake --version | findstr /C:"cmake"
) else (
    echo [FAIL] CMake not found
)

echo.
echo ================================================
echo Environment configured successfully!
echo ================================================
echo.
echo Tools available:
echo   - MinGW:   %MINGW_ROOT%
echo   - Qt:      %QT_DIR%
echo   - vcpkg:   %VCPKG_ROOT%
echo   - CMake:   Available in PATH
echo.
echo To configure project:  configure-project.bat
echo To build project:      build-project.bat
echo.
