# Build TA-Lib for x64 using MSVC NMake
# Run this from PowerShell

$vcvars = "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
if (-not (Test-Path $vcvars)) {
    Write-Host "✗ vcvars64.bat not found at $vcvars" -ForegroundColor Red
    exit 1
}

$builds = @("cdr", "cmd") # Release and Debug

foreach ($build in $builds) {
    Write-Host "=== Building TA-Lib x64 ($build) ===" -ForegroundColor Cyan
    $makeDir = "C:\ta-lib\c\make\$build\win32\msvc\ta_libc"
    
    if (-not (Test-Path $makeDir)) {
        Write-Host "✗ Make directory not found: $makeDir" -ForegroundColor Red
        continue
    }

    # Use a direct cmd call to avoid PowerShell quote issues with complex commands
    $cmd = "`"$vcvars`" x64 && cd /d `"$makeDir`" && nmake"
    
    Write-Host "Running: $cmd" -ForegroundColor Gray
    cmd.exe /c $cmd
    
    if ($LASTEXITCODE -eq 0) {
        Write-Host "✓ Successfully built $build" -ForegroundColor Green
    }
    else {
        Write-Host "✗ Failed to build $build" -ForegroundColor Red
    }
}

Write-Host ""
Write-Host "Verifying new libraries..." -ForegroundColor Yellow
dir C:\ta-lib\c\lib\*.lib | select Name, Length, LastWriteTime
