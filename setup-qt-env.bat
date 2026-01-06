@echo off
REM ============================================================
REM Trading Terminal - Environment Setup Script
REM Sets up paths for Qt, MSVC, Boost, and OpenSSL
REM ============================================================

echo.
echo ================================================
echo Trading Terminal - Environment Setup
echo ================================================
echo.

REM Initialize MSVC environment (Visual Studio 2026)
echo Setting up MSVC environment...
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
if errorlevel 1 (
    echo ERROR: Failed to initialize MSVC environment
    exit /b 1
)

REM Set Qt paths
echo Setting up Qt paths...
set "QT_DIR=C:\Users\admin\Qt\5.15.2\msvc2019_64"
set "PATH=%QT_DIR%\bin;%PATH%"
set "CMAKE_PREFIX_PATH=%QT_DIR%"
set "Qt5_DIR=%QT_DIR%\lib\cmake\Qt5"

REM Set Boost paths (Chocolatey default locations)
REM boost-msvc-14.3 installs to C:\local\boost_1_87_0 or similar
set "BOOST_ROOT=C:\local\boost_1_87_0"
set "BOOST_INCLUDEDIR=%BOOST_ROOT%"
set "BOOST_LIBRARYDIR=%BOOST_ROOT%\lib64-msvc-14.3"

REM Set OpenSSL paths (Chocolatey default location)
REM Chocolatey OpenSSL typically installs to C:\Program Files\OpenSSL-Win64
set "OPENSSL_ROOT_DIR=C:\Program Files\OpenSSL-Win64"
set "OPENSSL_INCLUDE_DIR=%OPENSSL_ROOT_DIR%\include"
set "OPENSSL_LIB_DIR=%OPENSSL_ROOT_DIR%\lib"

REM Add OpenSSL to PATH for DLLs
set "PATH=%OPENSSL_ROOT_DIR%\bin;%PATH%"

echo.
echo Environment configured:
echo   - Qt:      %QT_DIR%
echo   - Boost:   %BOOST_ROOT%
echo   - OpenSSL: %OPENSSL_ROOT_DIR%
echo   - MSVC:    Initialized
echo.
echo To build the project, run:
echo   cd build
echo   cmake -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release ..
echo   nmake
echo.
