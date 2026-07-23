@echo off
setlocal
cd /d "%~dp0"
powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "%~dp0tests\tools\Register-Spiral7CU3Projects.ps1"
if errorlevel 1 exit /b %errorlevel%
powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "%~dp0tests\tools\Update-SourceManifest.ps1"
if errorlevel 1 exit /b %errorlevel%
echo Spiral 7 CU3 preparation complete.
endlocal
