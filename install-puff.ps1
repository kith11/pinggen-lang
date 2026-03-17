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

function Split-PathEntries([string] $pathValue) {
    if ([string]::IsNullOrWhiteSpace($pathValue)) {
        return @()
    }
    return ($pathValue -split ';' | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })
}

function PathContainsEntry([string] $pathValue, [string] $entry) {
    $normalizedEntry = [System.IO.Path]::GetFullPath($entry).TrimEnd('\')
    foreach ($existing in Split-PathEntries $pathValue) {
        $normalizedExisting = [System.IO.Path]::GetFullPath($existing).TrimEnd('\')
        if ($normalizedExisting.Equals($normalizedEntry, [System.StringComparison]::OrdinalIgnoreCase)) {
            return $true
        }
    }
    return $false
}

function Ensure-UserPathContains([string] $entry) {
    $userPath = [Environment]::GetEnvironmentVariable("Path", "User")
    $alreadyPresent = PathContainsEntry $userPath $entry

    if (-not $alreadyPresent) {
        $newPath = if ([string]::IsNullOrWhiteSpace($userPath)) {
            $entry
        } else {
            "$userPath;$entry"
        }
        [Environment]::SetEnvironmentVariable("Path", $newPath, "User")
        $env:Path = "$env:Path;$entry"
        return "added"
    }

    if (-not (PathContainsEntry $env:Path $entry)) {
        $env:Path = "$env:Path;$entry"
        return "session-added"
    }

    return "already-present"
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

$pathUpdateStatus = Ensure-UserPathContains $localBin

Write-Host ""
Write-Host "Installed puff to:" -ForegroundColor Green
Write-Host "  $localBin"
Write-Host ""
if ($pathUpdateStatus -eq "added") {
    Write-Host "Updated your user PATH automatically." -ForegroundColor Green
    Write-Host "Open a new terminal to use 'puff' everywhere." -ForegroundColor Yellow
    Write-Host ""
} elseif ($pathUpdateStatus -eq "session-added") {
    Write-Host "Your user PATH already contains puff, and this terminal session was refreshed." -ForegroundColor Green
    Write-Host ""
} else {
    Write-Host "Your user PATH already contains puff." -ForegroundColor Green
    Write-Host ""
}
Write-Host "Use it now from this repo with:" -ForegroundColor Cyan
Write-Host "  .\\puff init .\\my_app"
Write-Host ""
Write-Host "After opening a new terminal, you can run:" -ForegroundColor Cyan
Write-Host "  puff init .\\my_app"
