# Trading Terminal Launcher Script
# This script copies required DLLs and launches the Trading Terminal

Write-Host "==================================================" -ForegroundColor Cyan
Write-Host "  Trading Terminal Launcher" -ForegroundColor Cyan
Write-Host "==================================================" -ForegroundColor Cyan
Write-Host ""

$buildDir = "$PSScriptRoot\build"
$exePath = Join-Path $buildDir "TradingTerminal.exe"

# Check if executable exists
if (-not (Test-Path $exePath)) {
    Write-Host "ERROR: TradingTerminal.exe not found!" -ForegroundColor Red
    Write-Host "Please build the project first using:" -ForegroundColor Yellow
    Write-Host "  cd build" -ForegroundColor Yellow
    Write-Host "  mingw32-make -j4" -ForegroundColor Yellow
    exit 1
}

# Copy DLLs using the copy_dlls.ps1 script
Write-Host "Checking and copying required DLLs..." -ForegroundColor Green
& "$buildDir\copy_dlls.ps1"

Write-Host ""
Write-Host "Starting Trading Terminal..." -ForegroundColor Green
Write-Host ""

# Launch the application with proper working directory
Start-Process -FilePath $exePath -WorkingDirectory $PSScriptRoot

Write-Host "Trading Terminal launched successfully!" -ForegroundColor Green
Write-Host ""
