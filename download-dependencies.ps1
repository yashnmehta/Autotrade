# ============================================================
# Manual Boost and OpenSSL Download and Setup
# ============================================================

Write-Host ""
Write-Host "================================================" -ForegroundColor Cyan
Write-Host "Manual Dependency Download and Setup" -ForegroundColor Cyan
Write-Host "================================================" -ForegroundColor Cyan
Write-Host ""

$ErrorActionPreference = "Stop"

# Create local directory
$localDir = "C:\local"
if (-not (Test-Path $localDir)) {
    Write-Host "Creating C:\local directory..." -ForegroundColor Yellow
    New-Item -ItemType Directory -Path $localDir -Force | Out-Null
}

# Download Boost headers (header-only for Beast and Asio)
Write-Host ""
Write-Host "Downloading Boost 1.87.0..." -ForegroundColor Yellow
$boostUrl = "https://archives.boost.io/release/1.87.0/source/boost_1_87_0.zip"
$boostZip = "$env:TEMP\boost_1_87_0.zip"
$boostDir = "$localDir\boost_1_87_0"

if (Test-Path $boostDir) {
    Write-Host "✓ Boost already exists at $boostDir" -ForegroundColor Green
} else {
    try {
        Write-Host "  Downloading from $boostUrl..." -ForegroundColor Gray
        Write-Host "  (This is a large file ~150MB, may take several minutes)" -ForegroundColor Gray
        Invoke-WebRequest -Uri $boostUrl -OutFile $boostZip -UseBasicParsing
        
        Write-Host "  Extracting..." -ForegroundColor Gray
        Expand-Archive -Path $boostZip -DestinationPath $localDir -Force
        
        Remove-Item $boostZip -Force
        Write-Host "✓ Boost installed to $boostDir" -ForegroundColor Green
    } catch {
        Write-Host "✗ Failed to download/extract Boost: $_" -ForegroundColor Red
    }
}

# OpenSSL - provide download link
Write-Host ""
Write-Host "OpenSSL Installation:" -ForegroundColor Yellow
Write-Host "  Please download and install OpenSSL manually from:" -ForegroundColor Gray
Write-Host "  https://slproweb.com/products/Win64OpenSSL.html" -ForegroundColor Cyan
Write-Host "  Recommended: Win64 OpenSSL v3.x Light" -ForegroundColor Gray
Write-Host "  Install to default location: C:\Program Files\OpenSSL-Win64" -ForegroundColor Gray

Write-Host ""
Write-Host "================================================" -ForegroundColor Green
Write-Host "Setup Complete!" -ForegroundColor Green
Write-Host "================================================" -ForegroundColor Green
Write-Host ""
Write-Host "Next steps:" -ForegroundColor Cyan
Write-Host "1. Install OpenSSL from the link above if not already installed" -ForegroundColor White
Write-Host "2. Run: setup-qt-env.bat" -ForegroundColor White
Write-Host "3. Build the project" -ForegroundColor White
Write-Host ""
