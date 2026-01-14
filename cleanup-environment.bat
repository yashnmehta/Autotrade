@echo off
REM ============================================================
REM CLEANUP SCRIPT - Remove broken MinGW and Qt installations
REM Run as Administrator
REM ============================================================

echo.
echo ================================================
echo Trading Terminal - Environment Cleanup
echo ================================================
echo.
echo This script will:
echo   1. Remove Chocolatey MinGW and Qt packages
echo   2. Clean build artifacts
echo   3. Remove temporary files
echo.

set /p confirm="Continue with cleanup? (Y/N): "
if /i not "%confirm%"=="Y" (
    echo Cleanup cancelled.
    pause
    exit /b 0
)

echo.
echo ================================================
echo Step 1: Uninstalling Chocolatey Packages
echo ================================================
echo.

echo Uninstalling MinGW...
choco uninstall -y mingw --force 2>nul
if %errorlevel% equ 0 (
    echo [OK] MinGW uninstalled
) else (
    echo [SKIP] MinGW not found or already removed
)

echo Uninstalling Qt5-default...
choco uninstall -y qt5-default --force 2>nul
if %errorlevel% equ 0 (
    echo [OK] Qt5-default uninstalled
) else (
    echo [SKIP] Qt5-default not found or already removed
)

echo Uninstalling aqt...
choco uninstall -y aqt --force 2>nul
if %errorlevel% equ 0 (
    echo [OK] aqt uninstalled
) else (
    echo [SKIP] aqt not found or already removed
)

echo.
echo ================================================
echo Step 2: Removing Leftover Files
echo ================================================
echo.

echo Removing Chocolatey lib folders...
if exist "C:\ProgramData\chocolatey\lib\mingw" (
    rmdir /s /q "C:\ProgramData\chocolatey\lib\mingw" 2>nul
    echo [OK] Removed mingw lib folder
)

if exist "C:\ProgramData\chocolatey\lib\qt5-default" (
    rmdir /s /q "C:\ProgramData\chocolatey\lib\qt5-default" 2>nul
    echo [OK] Removed qt5-default lib folder
)

if exist "C:\ProgramData\chocolatey\lib\aqt" (
    rmdir /s /q "C:\ProgramData\chocolatey\lib\aqt" 2>nul
    echo [OK] Removed aqt lib folder
)

echo.
echo ================================================
echo Step 3: Cleaning Build Artifacts
echo ================================================
echo.

cd /d "%~dp0"

if exist "build" (
    echo Removing build directory...
    rmdir /s /q build
    echo [OK] Build directory removed
)

if exist "benchmark_qt_vs_native_autogen" (
    echo Removing autogen directory...
    rmdir /s /q benchmark_qt_vs_native_autogen
    echo [OK] Autogen directory removed
)

echo.
echo ================================================
echo Step 4: Verification
echo ================================================
echo.

echo Checking for remaining MinGW installations...
where g++ 2>nul
if %errorlevel% equ 0 (
    echo [WARNING] g++ still found in PATH
    echo Please check: where g++
) else (
    echo [OK] No g++ found in PATH
)

echo.
echo Checking for remaining Qt installations...
where qmake 2>nul
if %errorlevel% equ 0 (
    echo [INFO] qmake found - may be from another installation
    echo Please verify: where qmake
) else (
    echo [OK] No qmake found in PATH
)

echo.
echo ================================================
echo Cleanup Completed!
echo ================================================
echo.
echo Next steps:
echo   1. Install MinGW-w64 GCC 13.2+
echo      winget install --id BrechtSanders.WinLibs.POSIX.UCRT
echo.
echo   2. Install Qt 5.15.2 with MinGW
echo      Download from: https://www.qt.io/download-qt-installer
echo.
echo   3. Follow SETUP_GUIDE.md for complete instructions
echo.
echo   4. After installation, run: configure-project.bat
echo.

pause
