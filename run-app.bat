@echo off
REM Trading Terminal Launcher Script
REM Sets up environment and runs the application

echo Starting Trading Terminal...

REM Add Qt DLLs to PATH
set "PATH=C:\Qt\5.15.2\mingw81_64\bin;%PATH%"

REM Add vcpkg MinGW DLLs to PATH
set "PATH=C:\vcpkg\installed\x64-mingw-dynamic\bin;%PATH%"

REM Add MinGW runtime DLLs to PATH
set "PATH=C:\ProgramData\chocolatey\lib\mingw\tools\install\mingw64\bin;%PATH%"

REM Run the application from build directory
cd build
start "" TradingTerminal.exe

echo Trading Terminal launched.
