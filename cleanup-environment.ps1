# ============================================================
# CLEANUP SCRIPT - Remove broken MinGW and Qt installations
# Run as Administrator: powershell -ExecutionPolicy Bypass -File cleanup-environment.ps1
# ============================================================

Write-Host ""
Write-Host "================================================" -ForegroundColor Cyan
Write-Host "Trading Terminal - Environment Cleanup" -ForegroundColor Cyan
Write-Host "================================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "This script will:" -ForegroundColor Yellow
Write-Host "  1. Remove Chocolatey MinGW and Qt packages"
Write-Host "  2. Clean build artifacts"
Write-Host "  3. Remove temporary files"
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

$confirm = Read-Host "Continue with cleanup? (Y/N)"
if ($confirm -ne "Y" -and $confirm -ne "y") {
    Write-Host "Cleanup cancelled." -ForegroundColor Yellow
    Read-Host "Press Enter to exit"
    exit 0
}

# ============================================================
# Step 1: Uninstall Chocolatey Packages
# ============================================================

Write-Host ""
Write-Host "================================================" -ForegroundColor Cyan
Write-Host "Step 1: Uninstalling Chocolatey Packages" -ForegroundColor Cyan
Write-Host "================================================" -ForegroundColor Cyan
Write-Host ""

$packages = @("mingw", "qt5-default", "aqt")
foreach ($package in $packages) {
    Write-Host "Uninstalling $package..." -ForegroundColor Yellow
    try {
        $result = choco uninstall -y $package --force 2>&1
        if ($LASTEXITCODE -eq 0) {
            Write-Host "[OK] $package uninstalled" -ForegroundColor Green
        } else {
            Write-Host "[SKIP] $package not found or already removed" -ForegroundColor Gray
        }
    } catch {
        Write-Host "[SKIP] Error uninstalling $package" -ForegroundColor Gray
    }
}

# ============================================================
# Step 2: Remove Leftover Files
# ============================================================

Write-Host ""
Write-Host "================================================" -ForegroundColor Cyan
Write-Host "Step 2: Removing Leftover Files" -ForegroundColor Cyan
Write-Host "================================================" -ForegroundColor Cyan
Write-Host ""

$foldersToRemove = @(
    "C:\ProgramData\chocolatey\lib\mingw",
    "C:\ProgramData\chocolatey\lib\qt5-default",
    "C:\ProgramData\chocolatey\lib\aqt"
)

foreach ($folder in $foldersToRemove) {
    if (Test-Path $folder) {
        Write-Host "Removing $folder..." -ForegroundColor Yellow
        try {
            Remove-Item -Path $folder -Recurse -Force -ErrorAction Stop
            Write-Host "[OK] Removed $(Split-Path $folder -Leaf)" -ForegroundColor Green
        } catch {
            Write-Host "[WARNING] Could not remove $folder" -ForegroundColor Red
            Write-Host "         Error: $_" -ForegroundColor Red
        }
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

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $scriptDir

$buildFolders = @("build", "benchmark_qt_vs_native_autogen")
foreach ($folder in $buildFolders) {
    if (Test-Path $folder) {
        Write-Host "Removing $folder directory..." -ForegroundColor Yellow
        try {
            Remove-Item -Path $folder -Recurse -Force -ErrorAction Stop
            Write-Host "[OK] $folder directory removed" -ForegroundColor Green
        } catch {
            Write-Host "[WARNING] Could not remove $folder" -ForegroundColor Red
            Write-Host "         Error: $_" -ForegroundColor Red
        }
    }
}

# ============================================================
# Step 4: Verification
# ============================================================

Write-Host ""
Write-Host "================================================" -ForegroundColor Cyan
Write-Host "Step 4: Verification" -ForegroundColor Cyan
Write-Host "================================================" -ForegroundColor Cyan
Write-Host ""

Write-Host "Checking for remaining MinGW installations..." -ForegroundColor Yellow
$gppPath = Get-Command g++ -ErrorAction SilentlyContinue
if ($gppPath) {
    Write-Host "[WARNING] g++ still found at: $($gppPath.Source)" -ForegroundColor Red
    Write-Host "          You may need to remove it manually or update PATH" -ForegroundColor Yellow
} else {
    Write-Host "[OK] No g++ found in PATH" -ForegroundColor Green
}

Write-Host ""
Write-Host "Checking for remaining Qt installations..." -ForegroundColor Yellow
$qmakePath = Get-Command qmake -ErrorAction SilentlyContinue
if ($qmakePath) {
    Write-Host "[INFO] qmake found at: $($qmakePath.Source)" -ForegroundColor Cyan
    Write-Host "       This may be from another installation (OK if intentional)" -ForegroundColor Gray
} else {
    Write-Host "[OK] No qmake found in PATH" -ForegroundColor Green
}

# ============================================================
# Summary and Next Steps
# ============================================================

Write-Host ""
Write-Host "================================================" -ForegroundColor Green
Write-Host "Cleanup Completed!" -ForegroundColor Green
Write-Host "================================================" -ForegroundColor Green
Write-Host ""
Write-Host "Next steps:" -ForegroundColor Cyan
Write-Host ""
Write-Host "1. Install MinGW-w64 GCC 13.2+" -ForegroundColor Yellow
Write-Host "   winget install --id BrechtSanders.WinLibs.POSIX.UCRT" -ForegroundColor White
Write-Host ""
Write-Host "2. Install Qt 5.15.2 with MinGW" -ForegroundColor Yellow
Write-Host "   Download from: https://www.qt.io/download-qt-installer" -ForegroundColor White
Write-Host "   Select: Qt 5.15.2 -> MinGW 8.1.0 64-bit" -ForegroundColor White
Write-Host ""
Write-Host "3. Configure environment variables" -ForegroundColor Yellow
Write-Host "   See SETUP_GUIDE.md for detailed instructions" -ForegroundColor White
Write-Host ""
Write-Host "4. After installation, run:" -ForegroundColor Yellow
Write-Host "   .\configure-project.bat" -ForegroundColor White
Write-Host "   .\build-project.bat" -ForegroundColor White
Write-Host ""
Write-Host "For complete setup instructions, see: SETUP_GUIDE.md" -ForegroundColor Cyan
Write-Host ""

Read-Host "Press Enter to exit"
