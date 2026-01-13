# ============================================================
# DEEP CLEAN - Complete removal of Qt and MinGW
# Run as Administrator
# ============================================================

Write-Host ""
Write-Host "================================================" -ForegroundColor Cyan
Write-Host "Deep Clean - Complete Qt and MinGW Removal" -ForegroundColor Cyan
Write-Host "================================================" -ForegroundColor Cyan
Write-Host ""

# Check if running as Administrator
$isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    Write-Host "[ERROR] This script must be run as Administrator!" -ForegroundColor Red
    Write-Host ""
    Write-Host "Please right-click PowerShell and select 'Run as Administrator'" -ForegroundColor Yellow
    Write-Host ""
    Read-Host "Press Enter to exit"
    exit 1
}

Write-Host "This will completely remove:" -ForegroundColor Yellow
Write-Host "  • All Qt installations (C:\Qt)" -ForegroundColor White
Write-Host "  • Chocolatey MinGW packages" -ForegroundColor White
Write-Host "  • Build artifacts" -ForegroundColor White
Write-Host ""

$confirm = Read-Host "Continue? (Y/N)"
if ($confirm -ne "Y" -and $confirm -ne "y") {
    Write-Host "Cancelled." -ForegroundColor Yellow
    Read-Host "Press Enter to exit"
    exit 0
}

# ============================================================
# Step 1: Uninstall Chocolatey MinGW
# ============================================================

Write-Host ""
Write-Host "================================================" -ForegroundColor Cyan
Write-Host "Step 1: Removing Chocolatey MinGW" -ForegroundColor Cyan
Write-Host "================================================" -ForegroundColor Cyan
Write-Host ""

$packages = @("mingw", "qt5-default", "aqt")
foreach ($package in $packages) {
    Write-Host "Uninstalling $package..." -ForegroundColor Yellow
    try {
        choco uninstall -y $package --force 2>&1 | Out-Null
        Write-Host "[OK] $package removed" -ForegroundColor Green
    } catch {
        Write-Host "[SKIP] $package not found" -ForegroundColor Gray
    }
}

# Remove leftover Chocolatey folders
$chocoFolders = @(
    "C:\ProgramData\chocolatey\lib\mingw",
    "C:\ProgramData\chocolatey\lib\qt5-default",
    "C:\ProgramData\chocolatey\lib\aqt"
)

foreach ($folder in $chocoFolders) {
    if (Test-Path $folder) {
        Write-Host "Removing $folder..." -ForegroundColor Yellow
        Remove-Item -Path $folder -Recurse -Force -ErrorAction SilentlyContinue
        Write-Host "[OK] Removed" -ForegroundColor Green
    }
}

# ============================================================
# Step 2: Remove Qt Installation Completely
# ============================================================

Write-Host ""
Write-Host "================================================" -ForegroundColor Cyan
Write-Host "Step 2: Removing Qt Installation" -ForegroundColor Cyan
Write-Host "================================================" -ForegroundColor Cyan
Write-Host ""

if (Test-Path "C:\Qt") {
    Write-Host "Removing C:\Qt directory..." -ForegroundColor Yellow
    try {
        # Stop any Qt processes
        Get-Process | Where-Object { $_.Path -like "C:\Qt*" } | Stop-Process -Force -ErrorAction SilentlyContinue
        
        # Remove the directory
        Remove-Item -Path "C:\Qt" -Recurse -Force -ErrorAction Stop
        Write-Host "[OK] Qt directory removed" -ForegroundColor Green
    } catch {
        Write-Host "[WARNING] Could not remove C:\Qt completely" -ForegroundColor Red
        Write-Host "         Error: $_" -ForegroundColor Red
        Write-Host "         You may need to delete it manually after restart" -ForegroundColor Yellow
    }
} else {
    Write-Host "[INFO] Qt directory not found" -ForegroundColor Gray
}

# Remove Qt settings
$qtSettings = @(
    "$env:APPDATA\Qt",
    "$env:APPDATA\QtProject",
    "$env:LOCALAPPDATA\Qt"
)

foreach ($setting in $qtSettings) {
    if (Test-Path $setting) {
        Write-Host "Removing Qt settings: $setting..." -ForegroundColor Yellow
        Remove-Item -Path $setting -Recurse -Force -ErrorAction SilentlyContinue
        Write-Host "[OK] Removed" -ForegroundColor Green
    }
}

# ============================================================
# Step 3: Clean Build Artifacts
# ============================================================

Write-Host ""
Write-Host "================================================" -ForegroundColor Cyan
Write-Host "Step 3: Cleaning Build Artifacts" -ForegroundColor Cyan
Write-Host "================================================" -ForegroundColor Cyan
Write-Host ""

$projectDir = "C:\Users\admin\Desktop\trading_terminal_cpp"
Set-Location $projectDir

$buildFolders = @("build", "benchmark_qt_vs_native_autogen")
foreach ($folder in $buildFolders) {
    if (Test-Path $folder) {
        Write-Host "Removing $folder..." -ForegroundColor Yellow
        Remove-Item -Path $folder -Recurse -Force -ErrorAction SilentlyContinue
        Write-Host "[OK] Removed" -ForegroundColor Green
    }
}

# ============================================================
# Step 4: Clean PATH Environment Variable
# ============================================================

Write-Host ""
Write-Host "================================================" -ForegroundColor Cyan
Write-Host "Step 4: Cleaning PATH Variable" -ForegroundColor Cyan
Write-Host "================================================" -ForegroundColor Cyan
Write-Host ""

$path = [Environment]::GetEnvironmentVariable("Path", "Machine")
$pathArray = $path -split ';'

# Remove Qt and MinGW paths
$cleanedPath = $pathArray | Where-Object {
    $_ -notlike "*\Qt\*" -and 
    $_ -notlike "*mingw*" -and
    $_ -notlike "*chocolatey*mingw*"
}

$newPath = $cleanedPath -join ';'
[Environment]::SetEnvironmentVariable("Path", $newPath, "Machine")

Write-Host "[OK] PATH cleaned" -ForegroundColor Green

# ============================================================
# Step 5: Verification
# ============================================================

Write-Host ""
Write-Host "================================================" -ForegroundColor Cyan
Write-Host "Step 5: Verification" -ForegroundColor Cyan
Write-Host "================================================" -ForegroundColor Cyan
Write-Host ""

# Reload PATH
$env:Path = [System.Environment]::GetEnvironmentVariable("Path", "Machine")

# Check for g++
$gppPath = Get-Command g++ -ErrorAction SilentlyContinue
if ($gppPath) {
    Write-Host "[WARNING] g++ still found at: $($gppPath.Source)" -ForegroundColor Red
} else {
    Write-Host "[OK] No g++ in PATH" -ForegroundColor Green
}

# Check for qmake
$qmakePath = Get-Command qmake -ErrorAction SilentlyContinue
if ($qmakePath) {
    Write-Host "[WARNING] qmake still found at: $($qmakePath.Source)" -ForegroundColor Red
} else {
    Write-Host "[OK] No qmake in PATH" -ForegroundColor Green
}

# Check directories
if (-not (Test-Path "C:\Qt")) {
    Write-Host "[OK] C:\Qt removed" -ForegroundColor Green
} else {
    Write-Host "[WARNING] C:\Qt still exists" -ForegroundColor Red
}

# ============================================================
# Summary
# ============================================================

Write-Host ""
Write-Host "================================================" -ForegroundColor Green
Write-Host "Deep Clean Completed!" -ForegroundColor Green
Write-Host "================================================" -ForegroundColor Green
Write-Host ""
Write-Host "Next Steps:" -ForegroundColor Cyan
Write-Host ""
Write-Host "1. Restart your computer (recommended)" -ForegroundColor Yellow
Write-Host "   This ensures all file locks are released" -ForegroundColor Gray
Write-Host ""
Write-Host "2. Download Qt Online Installer:" -ForegroundColor Yellow
Write-Host "   https://www.qt.io/download-qt-installer" -ForegroundColor White
Write-Host ""
Write-Host "3. Install Qt with these components:" -ForegroundColor Yellow
Write-Host "   ✓ Qt 5.15.2 → MinGW 8.1.0 64-bit" -ForegroundColor White
Write-Host "   ✓ Developer Tools → MinGW 8.1.0 64-bit" -ForegroundColor White
Write-Host ""
Write-Host "4. After Qt installation, vcpkg dependencies are already ready:" -ForegroundColor Yellow
Write-Host "   • Boost 1.90.0 (x64-mingw-dynamic)" -ForegroundColor White
Write-Host "   • OpenSSL 3.6.0 (x64-mingw-dynamic)" -ForegroundColor White
Write-Host ""
Write-Host "5. Configure and build project:" -ForegroundColor Yellow
Write-Host "   .\configure-project.bat" -ForegroundColor White
Write-Host "   .\build-project.bat" -ForegroundColor White
Write-Host ""

Read-Host "Press Enter to exit"
