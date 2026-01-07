# Self-elevate the script if required
if (-Not ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole] 'Administrator')) {
    if ([int](Get-CimInstance -Class Win32_OperatingSystem | Select-Object -ExpandProperty BuildNumber) -ge 6000) {
        $CommandLine = "-File `"" + $MyInvocation.MyCommand.Path + "`" " + $MyInvocation.UnboundArguments
        Start-Process -FilePath PowerShell.exe -Verb Runas -ArgumentList $CommandLine
        Exit
    }
}

Write-Host ""
Write-Host "================================================" -ForegroundColor Cyan
Write-Host "Installing Boost and OpenSSL via Chocolatey" -ForegroundColor Cyan
Write-Host "================================================" -ForegroundColor Cyan
Write-Host ""

# Install Boost
Write-Host "Installing Boost MSVC 14.3..." -ForegroundColor Yellow
choco install boost-msvc-14.3 -y
if ($LASTEXITCODE -eq 0) {
    Write-Host "✓ Boost installed successfully" -ForegroundColor Green
} else {
    Write-Host "✗ Boost installation failed" -ForegroundColor Red
}

Write-Host ""

# Install OpenSSL
Write-Host "Installing OpenSSL..." -ForegroundColor Yellow
choco install openssl -y
if ($LASTEXITCODE -eq 0) {
    Write-Host "✓ OpenSSL installed successfully" -ForegroundColor Green
} else {
    Write-Host "✗ OpenSSL installation failed" -ForegroundColor Red
}

Write-Host ""
Write-Host "================================================" -ForegroundColor Green
Write-Host "Installation Complete!" -ForegroundColor Green
Write-Host "================================================" -ForegroundColor Green
Write-Host ""
Write-Host "Press any key to continue..."
$null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
