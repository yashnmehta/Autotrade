@echo off
REM ============================================================
REM Trading Terminal - Install Dependencies
REM Run this file as Administrator
REM ============================================================

echo.
echo ================================================
echo Installing Trading Terminal Dependencies
echo ================================================
echo.

REM Check if running as administrator
net session >nul 2>&1
if %errorLevel% neq 0 (
    echo ERROR: This script must be run as Administrator
    echo Right-click this file and select "Run as administrator"
    pause
    exit /b 1
)

echo Installing Boost (this may take several minutes)...
choco install boost-msvc-14.3 -y
if errorlevel 1 (
    echo WARNING: Boost installation had issues
) else (
    echo Boost installed successfully
)

echo.
echo Installing OpenSSL...
choco install openssl -y
if errorlevel 1 (
    echo WARNING: OpenSSL installation had issues
) else (
    echo OpenSSL installed successfully
)

echo.
echo ================================================
echo Installation Complete!
echo ================================================
echo.
echo Verifying installations...

if exist "C:\local\boost_1_87_0\" (
    echo [OK] Boost found at C:\local\boost_1_87_0
) else (
    echo [WARN] Boost not found at expected location
    dir C:\local\boost* /b 2>nul
)

if exist "C:\Program Files\OpenSSL-Win64\" (
    echo [OK] OpenSSL found at C:\Program Files\OpenSSL-Win64
) else (
    echo [WARN] OpenSSL not found at expected location
)

echo.
echo Next steps:
echo 1. Run: setup-qt-env.bat
echo 2. Build project in the build directory
echo.
pause
