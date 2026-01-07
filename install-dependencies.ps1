# ============================================================
# Trading Terminal - Dependency Installation Script
# Installs Boost and OpenSSL via vcpkg
# ============================================================

Write-Host ""
Write-Host "================================================" -ForegroundColor Cyan
Write-Host "Trading Terminal - Dependency Installer" -ForegroundColor Cyan
Write-Host "================================================" -ForegroundColor Cyan
Write-Host ""

$ErrorActionPreference = "Stop"

# Check if vcpkg is already installed
$vcpkgPath = "C:\vcpkg"

if (Test-Path $vcpkgPath) {
    Write-Host "✓ vcpkg already installed at $vcpkgPath" -ForegroundColor Green
} else {
    Write-Host "Installing vcpkg..." -ForegroundColor Yellow
    
    # Check if git is available
    try {
        git --version | Out-Null
    } catch {
        Write-Host "ERROR: Git is not installed. Please install Git first." -ForegroundColor Red
        Write-Host "Download from: https://git-scm.com/download/win" -ForegroundColor Yellow
        exit 1
    }
    
    # Clone vcpkg
    Set-Location C:\
    git clone https://github.com/microsoft/vcpkg
    if (-not $?) {
        Write-Host "ERROR: Failed to clone vcpkg" -ForegroundColor Red
        exit 1
    }
    
    # Bootstrap vcpkg
    Set-Location vcpkg
    .\bootstrap-vcpkg.bat
    if (-not $?) {
        Write-Host "ERROR: Failed to bootstrap vcpkg" -ForegroundColor Red
        exit 1
    }
    
    Write-Host "✓ vcpkg installed successfully" -ForegroundColor Green
}

Set-Location $vcpkgPath

# Install Boost
Write-Host ""
Write-Host "Installing Boost (Beast + Asio)..." -ForegroundColor Yellow
.\vcpkg install boost-beast:x64-windows boost-asio:x64-windows
if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: Failed to install Boost" -ForegroundColor Red
    exit 1
}
Write-Host "✓ Boost installed successfully" -ForegroundColor Green

# Install OpenSSL
Write-Host ""
Write-Host "Installing OpenSSL..." -ForegroundColor Yellow
.\vcpkg install openssl:x64-windows
if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: Failed to install OpenSSL" -ForegroundColor Red
    exit 1
}
Write-Host "✓ OpenSSL installed successfully" -ForegroundColor Green

# Integrate with Visual Studio
Write-Host ""
Write-Host "Integrating vcpkg with Visual Studio..." -ForegroundColor Yellow
.\vcpkg integrate install
if ($LASTEXITCODE -ne 0) {
    Write-Host "WARNING: vcpkg integration failed, but packages are installed" -ForegroundColor Yellow
}

Write-Host ""
Write-Host "================================================" -ForegroundColor Green
Write-Host "✓ All dependencies installed successfully!" -ForegroundColor Green
Write-Host "================================================" -ForegroundColor Green
Write-Host ""
Write-Host "Next steps:" -ForegroundColor Cyan
Write-Host "1. Update setup-qt-env.bat with vcpkg paths (if needed)" -ForegroundColor White
Write-Host "2. Run: setup-qt-env.bat" -ForegroundColor White
Write-Host "3. Build the project" -ForegroundColor White
Write-Host ""
Write-Host "Installed packages location:" -ForegroundColor Cyan
Write-Host "  $vcpkgPath\installed\x64-windows" -ForegroundColor White
Write-Host ""
