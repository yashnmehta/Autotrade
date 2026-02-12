# TA-Lib Download and Setup Script for Windows
# Run this from PowerShell: .\scripts\setup_talib.ps1

param(
    [string]$InstallPath = "C:/ta-lib",
    [switch]$Force
)

Write-Host "=== TA-Lib Setup for Trading Terminal ===" -ForegroundColor Cyan
Write-Host ""

# Normalize path for PowerShell
$psPath = $InstallPath.Replace("/", "\")

# Check if already installed
if ((Test-Path "$psPath\lib\ta_libc_cdr.lib") -and (-not $Force)) {
    Write-Host "✓ TA-Lib already installed at $InstallPath" -ForegroundColor Green
    Write-Host "  Use -Force to reinstall" -ForegroundColor Yellow
}
else {
    # Create installation directory
    if (-not (Test-Path $psPath)) {
        Write-Host "Creating installation directory: $psPath" -ForegroundColor Yellow
        New-Item -ItemType Directory -Path $psPath -Force | Out-Null
    }

    # Download TA-Lib (precompiled for Windows/MSVC)
    $downloadUrl = "https://versaweb.dl.sourceforge.net/project/ta-lib/ta-lib/0.4.0/ta-lib-0.4.0-msvc.zip"
    $zipFile = "$env:TEMP\ta-lib-0.4.0-msvc.zip"

    Write-Host "Downloading TA-Lib from SourceForge..." -ForegroundColor Yellow
    Write-Host "  URL: $downloadUrl" -ForegroundColor Gray

    try {
        $ProgressPreference = 'SilentlyContinue'
        Invoke-WebRequest -Uri $downloadUrl -OutFile $zipFile -UseBasicParsing
        $ProgressPreference = 'Continue'
        Write-Host "✓ Download complete" -ForegroundColor Green
    }
    catch {
        Write-Host "✗ Download failed: $_" -ForegroundColor Red
        exit 1
    }

    # Extract archive
    Write-Host "Extracting TA-Lib to $InstallPath..." -ForegroundColor Yellow
    try {
        $tempDir = "$env:TEMP\ta-lib-temp"
        if (Test-Path $tempDir) { Remove-Item -Path $tempDir -Recurse -Force }
        Expand-Archive -Path $zipFile -DestinationPath $tempDir -Force
        
        $extractedPath = "$tempDir\ta-lib"
        if (Test-Path $extractedPath) {
            Copy-Item -Path "$extractedPath\*" -Destination $psPath -Recurse -Force
        }
        else {
            Copy-Item -Path "$tempDir\*" -Destination $psPath -Recurse -Force
        }
        
        Remove-Item -Path $tempDir -Recurse -Force
        Remove-Item -Path $zipFile -Force
        Write-Host "✓ Extraction complete" -ForegroundColor Green
    }
    catch {
        Write-Host "✗ Extraction failed: $_" -ForegroundColor Red
        exit 1
    }
}

# Verify installation
Write-Host ""
Write-Host "Verifying installation..." -ForegroundColor Yellow

$headerFile = "$psPath\include\ta_libc.h"
# Note: In the zip, files might be directly in include or in include/ta-lib
if (-not (Test-Path $headerFile)) {
    $headerFile = "$psPath\include\ta-lib\ta_libc.h"
}

$libFileDebug = "$psPath\lib\ta_libc_cmd.lib"
$libFileRelease = "$psPath\lib\ta_libc_cdr.lib"

if (Test-Path $headerFile) {
    Write-Host "  ✓ Header found: $headerFile" -ForegroundColor Green
}
else {
    Write-Host "  ✗ Header NOT found in $psPath\include" -ForegroundColor Red
}

if ((Test-Path $libFileDebug) -or (Test-Path $libFileRelease)) {
    Write-Host "  ✓ Libraries found in $psPath\lib" -ForegroundColor Green
}
else {
    Write-Host "  ✗ Libraries NOT found in $psPath\lib" -ForegroundColor Red
}

# Create CMake config file
Write-Host ""
Write-Host "Creating CMake configuration..." -ForegroundColor Yellow

$configDir = "$PSScriptRoot\..\cmake"
if (-not (Test-Path $configDir)) {
    New-Item -ItemType Directory -Path $configDir -Force | Out-Null
}
$configFile = "$configDir\TALibConfig.cmake"

# Use single-quoted here-string to avoid expansion of ${} which CMake needs
$cmakeContent = @'
# TA-Lib Configuration (Auto-generated)
set(TALIB_ROOT "REPLACE_INSTALL_PATH" CACHE PATH "TA-Lib root")
set(TALIB_INCLUDE_DIR "${TALIB_ROOT}/include")
set(TALIB_LIBRARY_RELEASE "${TALIB_ROOT}/lib/ta_libc_cdr.lib")
set(TALIB_LIBRARY_DEBUG "${TALIB_ROOT}/lib/ta_libc_cmd.lib")

if(EXISTS "${TALIB_LIBRARY_RELEASE}")
    set(TALIB_FOUND TRUE)
    # Map to variables used in CMakeLists.txt
    set(TALIB_LIBRARY debug "${TALIB_LIBRARY_DEBUG}" optimized "${TALIB_LIBRARY_RELEASE}")
    message(STATUS "Found TA-Lib: ${TALIB_ROOT}")
else()
    set(TALIB_FOUND FALSE)
    message(WARNING "TA-Lib not found at ${TALIB_ROOT}")
endif()
'@

# Replace the placeholder with the actual install path (using forward slashes for CMake)
$cmakeContent = $cmakeContent.Replace("REPLACE_INSTALL_PATH", $InstallPath)

Set-Content -Path $configFile -Value $cmakeContent -Encoding UTF8
Write-Host "  ✓ Created: $configFile" -ForegroundColor Green

Write-Host ""
Write-Host "=== TA-Lib Installation Complete! ===" -ForegroundColor Green
