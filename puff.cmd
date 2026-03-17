@echo off
setlocal
set ROOT=%~dp0

if exist "%ROOT%build\Debug\puff.exe" (
    "%ROOT%build\Debug\puff.exe" %*
    exit /b %ERRORLEVEL%
)

if exist "%ROOT%build\Release\puff.exe" (
    "%ROOT%build\Release\puff.exe" %*
    exit /b %ERRORLEVEL%
)

if exist "%ROOT%build\RelWithDebInfo\puff.exe" (
    "%ROOT%build\RelWithDebInfo\puff.exe" %*
    exit /b %ERRORLEVEL%
)

if exist "%ROOT%build\MinSizeRel\puff.exe" (
    "%ROOT%build\MinSizeRel\puff.exe" %*
    exit /b %ERRORLEVEL%
)

echo puff is not built yet.
echo Build it with:
echo   cmake -S . -B build
echo   cmake --build build
exit /b 1
