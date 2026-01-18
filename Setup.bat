@echo off
REM ============================================================
REM Temporary Build Script
REM ============================================================
set "QT_ROOT=C:\Qt\Qt5.14.2\5.14.2\mingw73_64"
set "QT_TOOLS_MINGW=C:\Qt\Qt5.14.2\Tools\mingw730_64"
set "MINGW_ROOT=%QT_TOOLS_MINGW%"
set "OPENSSL_ROOT=C:\Qt\Qt5.14.2\Tools\mingw730_64\opt"
set "BOOST_ROOT=C:\local\boost_1_82_0"
set "PATH=%MINGW_ROOT%\bin;%QT_ROOT%\bin;%OPENSSL_ROOT%\bin;%PATH%"
REM Clean and Rebuild
if exist build rmdir /s /q build
mkdir build
cd build
cmake -G "MinGW Makefiles" ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_PREFIX_PATH="%QT_ROOT%" ^
    -DOPENSSL_ROOT_DIR="%OPENSSL_ROOT%" ^
    -DOPENSSL_INCLUDE_DIR="%OPENSSL_ROOT%\include" ^
    -DOPENSSL_CRYPTO_LIBRARY="%OPENSSL_ROOT%\lib\libcrypto.dll.a" ^
    -DOPENSSL_SSL_LIBRARY="%OPENSSL_ROOT%\lib\libssl.dll.a" ^
    -DBOOST_ROOT="%BOOST_ROOT%" ^
    -DBoost_INCLUDE_DIR="%BOOST_ROOT%" ^
    -DBoost_NO_SYSTEM_PATHS=ON ^
    -DBoost_DEBUG=ON ^
    ..
if %errorlevel% neq 0 (
    echo CMake Failed
    exit /b 1
)
mingw32-make -j%NUMBER_OF_PROCESSORS%
if %errorlevel% neq 0 (
    echo Build Failed
    exit /b 1
)
echo Build Success
TradingTerminal.exe