$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$buildDir = Join-Path $repoRoot "build"
$debugExe = Join-Path $buildDir "Debug\\puff.exe"
$releaseExe = Join-Path $buildDir "Release\\puff.exe"
$localBin = if ($env:LOCALAPPDATA) {
    Join-Path $env:LOCALAPPDATA "puff\\bin"
} else {
    Join-Path $HOME ".puff\\bin"
}

function Resolve-PuffExe {
    if (Test-Path $debugExe) { return $debugExe }
    if (Test-Path $releaseExe) { return $releaseExe }

    Write-Host "Building puff..." -ForegroundColor Cyan
    cmake -S $repoRoot -B $buildDir
    if ($LASTEXITCODE -ne 0) { throw "cmake configure failed" }
    cmake --build $buildDir
    if ($LASTEXITCODE -ne 0) { throw "cmake build failed" }

    if (Test-Path $debugExe) { return $debugExe }
    if (Test-Path $releaseExe) { return $releaseExe }
    throw "could not find puff.exe after build"
}

$puffExe = Resolve-PuffExe
New-Item -ItemType Directory -Force -Path $localBin | Out-Null
Copy-Item $puffExe (Join-Path $localBin "puff.exe") -Force

$launcher = @'
@echo off
set SCRIPT_DIR=%~dp0
"%SCRIPT_DIR%puff.exe" %*
'@
Set-Content -Path (Join-Path $localBin "puff.cmd") -Value $launcher -NoNewline

Write-Host ""
Write-Host "Installed puff to:" -ForegroundColor Green
Write-Host "  $localBin"
Write-Host ""
Write-Host "Use it now from this repo with:" -ForegroundColor Cyan
Write-Host "  .\\puff init .\\my_app"
Write-Host ""
Write-Host "To use 'puff' globally, add this directory to PATH:" -ForegroundColor Yellow
Write-Host "  $localBin"
