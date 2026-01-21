@echo off
REM ============================================================
REM Build Trading Terminal Project
REM ============================================================

echo.
echo ================================================
echo Building Trading Terminal
echo ================================================
echo.

REM Setup environment first
call setup-development-env.bat
if %errorlevel% neq 0 (
    echo Failed to setup development environment
    pause
    exit /b 1
)

REM Check if project is configured
if not exist "build\CMakeCache.txt" (
    echo.
    echo ERROR: Project not configured yet!
    echo Please run: configure-project.bat
    echo.
    pause
    exit /b 1
)

REM Navigate to build directory
cd build

REM Build with make
echo.
echo Building with MinGW Make...
echo Using %NUMBER_OF_PROCESSORS% parallel jobs
echo.

mingw32-make -j%NUMBER_OF_PROCESSORS%

if %errorlevel% equ 0 (
    echo.
    echo ================================================
    echo Build completed successfully!
    echo ================================================
    echo.
    echo Executable: build\TradingTerminal.exe
    echo.
    echo To run application: run-app.bat
    echo.
) else (
    echo.
    echo ================================================
    echo Build FAILED!
    echo ================================================
    echo.
    echo Try single-threaded build: mingw32-make
    echo Or clean rebuild: clean-rebuild.bat
)

cd ..
pause
