@echo off
REM ============================================================
REM Trading Terminal - Environment Setup Script
REM Sets up paths for Qt, MinGW, Boost, and OpenSSL
REM ============================================================

echo.
echo ================================================
echo Trading Terminal - Environment Setup
echo ================================================
echo.

REM Set MinGW paths (assuming MinGW-w64 is installed and in PATH)
echo Setting up MinGW environment...
REM MinGW bin should be in PATH from installation

REM Set Qt paths (for MinGW)
echo Setting up Qt paths...
set "QT_DIR=C:\Qt\5.15.2\mingw81_64"
set "PATH=%QT_DIR%\bin;%PATH%"
set "CMAKE_PREFIX_PATH=%QT_DIR%"
set "Qt5_DIR=%QT_DIR%\lib\cmake\Qt5"

REM Set Boost paths (vcpkg MinGW locations)
set "BOOST_ROOT=C:\vcpkg\installed\x64-mingw-dynamic"
set "BOOST_INCLUDEDIR=%BOOST_ROOT%\include"
set "BOOST_LIBRARYDIR=%BOOST_ROOT%\lib"

REM Set OpenSSL paths (vcpkg MinGW location)
set "OPENSSL_ROOT_DIR=C:\vcpkg\installed\x64-mingw-dynamic"
set "OPENSSL_INCLUDE_DIR=%OPENSSL_ROOT_DIR%\include"
set "OPENSSL_LIB_DIR=%OPENSSL_ROOT_DIR%\lib"

REM Add OpenSSL to PATH for DLLs
set "PATH=%OPENSSL_ROOT_DIR%\bin;%PATH%"

echo.
echo Environment configured:
echo   - Qt:      %QT_DIR%
echo   - Boost:   %BOOST_ROOT%
echo   - OpenSSL: %OPENSSL_ROOT_DIR%
echo   - MinGW:   Initialized
echo.
echo To build the project, run:
echo   cd build
echo   cmake -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release ..
echo   mingw32-make
echo.
