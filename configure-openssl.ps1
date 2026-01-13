# OpenSSL Configuration Helper for MSYS2 MinGW64
# Run this script to verify OpenSSL installation and configure CMake

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "OpenSSL Configuration Helper" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# Check if MSYS2 is installed
$msys2Path = "C:\msys64\mingw64"
if (-Not (Test-Path $msys2Path)) {
    Write-Host "❌ ERROR: MSYS2 MinGW64 not found at $msys2Path" -ForegroundColor Red
    Write-Host ""
    Write-Host "Please install MSYS2 and OpenSSL:" -ForegroundColor Yellow
    Write-Host "1. Install MSYS2 from https://www.msys2.org/" -ForegroundColor Yellow
    Write-Host "2. Open MSYS2 MinGW 64-bit terminal" -ForegroundColor Yellow
    Write-Host "3. Run: pacman -S mingw-w64-x86_64-openssl" -ForegroundColor Yellow
    Write-Host ""
    exit 1
}

Write-Host "✓ MSYS2 MinGW64 found: $msys2Path" -ForegroundColor Green

# Check OpenSSL headers
$opensslHeader = "$msys2Path\include\openssl\ssl.h"
if (-Not (Test-Path $opensslHeader)) {
    Write-Host "❌ ERROR: OpenSSL headers not found at $opensslHeader" -ForegroundColor Red
    Write-Host ""
    Write-Host "Please install OpenSSL in MSYS2:" -ForegroundColor Yellow
    Write-Host "1. Open MSYS2 MinGW 64-bit terminal" -ForegroundColor Yellow
    Write-Host "2. Run: pacman -S mingw-w64-x86_64-openssl" -ForegroundColor Yellow
    Write-Host ""
    exit 1
}

Write-Host "✓ OpenSSL headers found: $opensslHeader" -ForegroundColor Green

# Check OpenSSL libraries
$cryptoLib = "$msys2Path\lib\libcrypto.dll.a"
$sslLib = "$msys2Path\lib\libssl.dll.a"

if (-Not (Test-Path $cryptoLib)) {
    Write-Host "❌ ERROR: OpenSSL crypto library not found at $cryptoLib" -ForegroundColor Red
    Write-Host ""
    Write-Host "Please install OpenSSL in MSYS2:" -ForegroundColor Yellow
    Write-Host "1. Open MSYS2 MinGW 64-bit terminal" -ForegroundColor Yellow
    Write-Host "2. Run: pacman -S mingw-w64-x86_64-openssl" -ForegroundColor Yellow
    Write-Host ""
    exit 1
}

if (-Not (Test-Path $sslLib)) {
    Write-Host "❌ ERROR: OpenSSL SSL library not found at $sslLib" -ForegroundColor Red
    Write-Host ""
    Write-Host "Please install OpenSSL in MSYS2:" -ForegroundColor Yellow
    Write-Host "1. Open MSYS2 MinGW 64-bit terminal" -ForegroundColor Yellow
    Write-Host "2. Run: pacman -S mingw-w64-x86_64-openssl" -ForegroundColor Yellow
    Write-Host ""
    exit 1
}

Write-Host "✓ OpenSSL crypto library found: $cryptoLib" -ForegroundColor Green
Write-Host "✓ OpenSSL SSL library found: $sslLib" -ForegroundColor Green

# Check OpenSSL DLLs (runtime)
$cryptoDll = "$msys2Path\bin\libcrypto-3-x64.dll"
$sslDll = "$msys2Path\bin\libssl-3-x64.dll"

if ((Test-Path $cryptoDll) -and (Test-Path $sslDll)) {
    Write-Host "✓ OpenSSL runtime DLLs found" -ForegroundColor Green
} else {
    Write-Host "⚠ WARNING: OpenSSL runtime DLLs not found" -ForegroundColor Yellow
    Write-Host "  Expected: $cryptoDll" -ForegroundColor Yellow
    Write-Host "  Expected: $sslDll" -ForegroundColor Yellow
    Write-Host "  You may need to copy these DLLs to the build directory" -ForegroundColor Yellow
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Configuration Summary" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "OpenSSL Root:    $msys2Path" -ForegroundColor White
Write-Host "Include Dir:     $msys2Path\include" -ForegroundColor White
Write-Host "Crypto Library:  $cryptoLib" -ForegroundColor White
Write-Host "SSL Library:     $sslLib" -ForegroundColor White
Write-Host ""

# Ask if user wants to run CMake
Write-Host "Do you want to configure CMake now? (Y/N)" -ForegroundColor Yellow
$response = Read-Host
if ($response -eq "Y" -or $response -eq "y") {
    Write-Host ""
    Write-Host "========================================" -ForegroundColor Cyan
    Write-Host "Running CMake Configuration" -ForegroundColor Cyan
    Write-Host "========================================" -ForegroundColor Cyan
    
    # Create build directory if it doesn't exist
    if (-Not (Test-Path "build")) {
        New-Item -ItemType Directory -Path "build" | Out-Null
        Write-Host "✓ Created build directory" -ForegroundColor Green
    }
    
    # Clear CMake cache
    if (Test-Path "build\CMakeCache.txt") {
        Remove-Item "build\CMakeCache.txt" -Force
        Write-Host "✓ Cleared CMake cache" -ForegroundColor Green
    }
    
    # Run CMake
    Set-Location build
    Write-Host ""
    Write-Host "Running: cmake .." -ForegroundColor Cyan
    Write-Host ""
    
    cmake ..
    
    $exitCode = $LASTEXITCODE
    Set-Location ..
    
    if ($exitCode -eq 0) {
        Write-Host ""
        Write-Host "========================================" -ForegroundColor Green
        Write-Host "✓ CMake configuration successful!" -ForegroundColor Green
        Write-Host "========================================" -ForegroundColor Green
        Write-Host ""
        Write-Host "Next steps:" -ForegroundColor Cyan
        Write-Host "1. cd build" -ForegroundColor White
        Write-Host "2. make -j8" -ForegroundColor White
        Write-Host "   or: mingw32-make -j8" -ForegroundColor White
        Write-Host ""
    } else {
        Write-Host ""
        Write-Host "========================================" -ForegroundColor Red
        Write-Host "✗ CMake configuration failed!" -ForegroundColor Red
        Write-Host "========================================" -ForegroundColor Red
        Write-Host ""
        Write-Host "Please check the error messages above." -ForegroundColor Yellow
        Write-Host ""
        exit 1
    }
} else {
    Write-Host ""
    Write-Host "Manual CMake configuration:" -ForegroundColor Cyan
    Write-Host "1. cd build" -ForegroundColor White
    Write-Host "2. Remove-Item CMakeCache.txt -Force" -ForegroundColor White
    Write-Host "3. cmake .." -ForegroundColor White
    Write-Host ""
}
