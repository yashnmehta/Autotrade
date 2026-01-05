# Trading Terminal Launcher
# This script sets up the PATH and runs the application

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$buildDir = Join-Path $scriptDir "build"

# Add Qt and MinGW to PATH for this session
$env:PATH = "C:\Qt\Qt5.14.2\5.14.2\mingw73_64\bin;" + 
            "C:\Qt\Qt5.14.2\Tools\mingw730_64\bin;" + 
            "C:\msys64\mingw64\bin;" + 
            $env:PATH

# Check if executable exists
$exePath = Join-Path $buildDir "TradingTerminal.exe"
if (-not (Test-Path $exePath)) {
    Write-Host "Error: TradingTerminal.exe not found in build directory!" -ForegroundColor Red
    Write-Host "Please build the project first with: cd build; mingw32-make -j4" -ForegroundColor Yellow
    exit 1
}

# Launch the application
Write-Host "Starting Trading Terminal..." -ForegroundColor Green
Set-Location $buildDir
Start-Process -FilePath ".\TradingTerminal.exe"
Write-Host "Trading Terminal launched successfully!" -ForegroundColor Green
