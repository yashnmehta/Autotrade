@echo off
set "PATH=C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\mingw810_64\bin;C:\Qt\5.15.2\mingw81_64\bin;C:\vcpkg\installed\x64-mingw-dynamic\bin;%PATH%"
echo Path set to: %PATH%
where cmake
where mingw32-make

if exist build rmdir /s /q build
if exist benchmark_qt_vs_native_autogen rmdir /s /q benchmark_qt_vs_native_autogen
mkdir build
cd build
cmake -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="C:\Qt\5.15.2\mingw81_64" -DCMAKE_TOOLCHAIN_FILE="C:\vcpkg\scripts\buildsystems\vcpkg.cmake" -DVCPKG_TARGET_TRIPLET=x64-mingw-dynamic -DBoost_INCLUDE_DIR="C:\vcpkg\installed\x64-mingw-dynamic\include" -DOPENSSL_ROOT_DIR="C:\vcpkg\installed\x64-mingw-dynamic" ..
if %errorlevel% neq 0 (
    echo CMake configuration failed
    exit /b %errorlevel%
)
mingw32-make -j4
if %errorlevel% neq 0 (
    echo Build failed
    exit /b %errorlevel%
)
echo Launching...
start TradingTerminal.exe
