@echo off
REM ============================================================
REM Trading Terminal Launcher Script
REM Sets up environment and runs the application
REM ============================================================

echo.
echo ================================================
echo Trading Terminal Launcher
echo ================================================
echo.

REM Setup environment first
call setup-development-env.bat
if %errorlevel% neq 0 (
    echo Failed to setup development environment
    pause
    exit /b 1
)

REM Check if executable exists
if not exist "build\TradingTerminal.exe" (
    echo.
    echo ERROR: TradingTerminal.exe not found!
    echo.
    echo Please build the project first:
    echo   .\build-project.bat
    echo.
    pause
    exit /b 1
)

REM Navigate to build directory and run
cd build
echo Starting Trading Terminal...
echo.
TradingTerminal.exe

REM Return to project root
cd ..

echo.
echo Trading Terminal closed.
echo.
