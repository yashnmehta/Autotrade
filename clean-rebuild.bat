@echo off
REM ============================================================
REM Clean and rebuild Trading Terminal Project
REM ============================================================

echo.
echo ================================================
echo Clean and Rebuild Trading Terminal
echo ================================================
echo.

set /p confirm="This will delete build directory. Continue? (Y/N): "
if /i not "%confirm%"=="Y" (
    echo Cancelled.
    pause
    exit /b 0
)

echo.
echo Removing build directory...
if exist "build" (
    rmdir /s /q build
    echo Build directory removed
)

if exist "benchmark_qt_vs_native_autogen" (
    rmdir /s /q benchmark_qt_vs_native_autogen
    echo Autogen directory removed
)

echo.
echo Recreating build directory...
mkdir build

echo.
echo Clean completed!
echo.
echo Next steps:
echo   1. Run: configure-project.bat
echo   2. Run: build-project.bat
echo.
pause
