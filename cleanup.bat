@echo off
REM ============================================================
REM Trading Terminal - Clean Setup Script
REM Run this as Administrator to clean and setup environment
REM ============================================================

echo.
echo ================================================
echo Trading Terminal - Clean Setup
echo ================================================
echo.
echo This will:
echo   1. Clean build artifacts
echo   2. Remove broken Chocolatey packages
echo   3. Setup proper development environment
echo.
pause

REM Step 1: Clean build artifacts
echo.
echo [Step 1/4] Cleaning build artifacts...
cd /d "%~dp0"
if exist build (
    rmdir /s /q build
    echo   - Removed build directory
)
if exist CMakeCache.txt (
    del /f /q CMakeCache.txt
    echo   - Removed CMakeCache.txt
)

REM Step 2: Uninstall Chocolatey packages (requires admin)
echo.
echo [Step 2/4] Removing broken packages...
echo   Note: You may see dependency warnings - this is expected
choco uninstall mingw -y --force --skip-autouninstaller 2>nul
choco uninstall qt5-default -y --force --skip-autouninstaller 2>nul
choco uninstall aqt -y --force 2>nul
echo   - Chocolatey packages marked for removal

REM Step 3: Manual cleanup of package folders
echo.
echo [Step 3/4] Cleaning package folders...
if exist "C:\ProgramData\chocolatey\lib\mingw" (
    rmdir /s /q "C:\ProgramData\chocolatey\lib\mingw" 2>nul
    echo   - Removed mingw folder
)
if exist "C:\ProgramData\chocolatey\lib\qt5-default" (
    rmdir /s /q "C:\ProgramData\chocolatey\lib\qt5-default" 2>nul
    echo   - Removed qt5-default folder
)
if exist "C:\ProgramData\chocolatey\lib\aqt" (
    rmdir /s /q "C:\ProgramData\chocolatey\lib\aqt" 2>nul
    echo   - Removed aqt folder
)

REM Step 4: Instructions for next steps
echo.
echo [Step 4/4] Next steps:
echo.
echo ================================================
echo Cleanup Complete!
echo ================================================
echo.
echo NEXT: Install proper development tools
echo.
echo 1. Install MinGW-w64 (GCC 13.2+):
echo    winget install --id BrechtSanders.WinLibs.POSIX.UCRT
echo.
echo 2. Install Qt 5.15.2 with MinGW:
echo    Download from: https://www.qt.io/download-qt-installer
echo    Select: Qt 5.15.2 with MinGW 8.1.0 64-bit
echo.
echo 3. After installation, run:
echo    setup-development-env.bat
echo.
echo See CLEAN_SETUP.md for detailed instructions
echo.
pause
