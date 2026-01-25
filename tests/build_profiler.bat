@echo off
REM Build script for ATM Profiler Standalone Test
REM Compiles with g++ (MinGW on Windows)

echo ========================================
echo Building ATM Profiler Standalone Test
echo ========================================

g++ -std=c++17 -O2 -Wall -Wextra ^
    -o test_atm_profiler_standalone.exe ^
    test_atm_profiler_standalone.cpp

if %ERRORLEVEL% == 0 (
    echo.
    echo ========================================
    echo ✓ Build Successful!
    echo ========================================
    echo.
    echo Executable: test_atm_profiler_standalone.exe
    echo.
    echo To run: test_atm_profiler_standalone.exe
    echo.
) else (
    echo.
    echo ========================================
    echo ✗ Build Failed!
    echo ========================================
    echo.
    exit /b 1
)
