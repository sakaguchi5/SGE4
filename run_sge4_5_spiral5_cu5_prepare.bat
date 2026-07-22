@echo off
setlocal
cd /d "%~dp0"
powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File ".\tests\tools\Register-Spiral5CU5Projects.ps1"
if errorlevel 1 exit /b %errorlevel%
powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File ".\tests\tools\Update-SourceManifest.ps1"
if errorlevel 1 exit /b %errorlevel%
echo Spiral 5 CU5 prepare passed.
endlocal
