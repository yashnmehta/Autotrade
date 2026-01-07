@echo off
REM Build script for NSE CM Receiver C++ on Windows
REM 
REM Prerequisites:
REM   - MinGW-w64 or Visual Studio with C++ support
REM   - Windows SDK for socket libraries
REM
REM Usage: build.bat

echo ====================================
echo NSE CM Receiver C++ Build Script
echo ====================================

echo.
echo Checking for C++ compiler...
where g++ >nul 2>nul
if %errorlevel% neq 0 (
    echo ERROR: g++ compiler not found in PATH
    echo Please install MinGW-w64 or add it to your PATH
    pause
    exit /b 1
)

echo Found g++ compiler
echo.

echo Creating output directory...
if not exist csv_output mkdir csv_output

echo.
echo Building NSE CM Receiver...

g++ -std=c++11 -Wall -Wextra -O3 -I. -Imsg_codes ^
    main.cpp ^
    utilities.cpp ^
    lzo_decompressor_safe.cpp ^
    msg_codes/message_6501_live.cpp ^
    msg_codes/message_6511_live.cpp ^
    msg_codes/message_6521_live.cpp ^
    msg_codes/message_6531_live.cpp ^
    msg_codes/message_6541_live.cpp ^
    msg_codes/message_6571_live.cpp ^
    msg_codes/message_6581_live.cpp ^
    msg_codes/message_6583_live.cpp ^
    msg_codes/message_6584_live.cpp ^
    -lws2_32 -lmswsock ^
    -o nse_cm_receiver.exe

if %errorlevel% neq 0 (
    echo.
    echo ERROR: Build failed!
    pause
    exit /b 1
)

echo.
echo ===================================
echo ✅ Build completed successfully!
echo ===================================
echo.
echo Executable: nse_cm_receiver.exe
echo.
echo Usage:
echo   nse_cm_receiver.exe ^<multicast_ip^> ^<port^> ^<message_code^>
echo.
echo Examples:
echo   nse_cm_receiver.exe 233.1.2.5 8222 6501
echo   nse_cm_receiver.exe 233.1.2.5 8222 6511
echo   nse_cm_receiver.exe 233.1.2.5 8222 6521
echo   nse_cm_receiver.exe 233.1.2.5 8222 6531
echo   nse_cm_receiver.exe 233.1.2.5 8222 6541
echo   nse_cm_receiver.exe 233.1.2.5 8222 6571
echo   nse_cm_receiver.exe 233.1.2.5 8222 6581
echo   nse_cm_receiver.exe 233.1.2.5 8222 6583
echo   nse_cm_receiver.exe 233.1.2.5 8222 6584
echo   nse_cm_receiver.exe 231.31.31.4 18901 6501
echo.

pause
