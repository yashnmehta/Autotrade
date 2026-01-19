@echo off
REM ============================================================
REM Configure CMake project with MinGW
REM ============================================================

echo.
echo ================================================
echo Configuring Trading Terminal Project
echo ================================================
echo.

REM Setup environment first
call setup-development-env.bat
if %errorlevel% neq 0 (
    echo Failed to setup development environment
    pause
    exit /b 1
)

REM Navigate to build directory
if not exist "build" mkdir build
cd build

REM Clean previous configuration (optional)
if exist CMakeCache.txt (
    echo Cleaning previous CMake configuration...
    del CMakeCache.txt
)

REM Configure with CMake (MSYS2 provides all dependencies)
echo.
echo Running CMake configuration...
echo.

cmake -G "MinGW Makefiles" ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_PREFIX_PATH=%QT_DIR% ^
    -DCMAKE_MAKE_PROGRAM=%MINGW_ROOT%\bin\mingw32-make.exe ^
    ..

if %errorlevel% equ 0 (
    echo.
    echo ================================================
    echo Configuration completed successfully!
    echo ================================================
    echo.
    echo To build project: build-project.bat
    echo.
) else (
    echo.
    echo ================================================
    echo Configuration FAILED!
    echo ================================================
    echo.
    echo Please check the errors above.
)

cd ..
pause
