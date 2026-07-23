@echo off
setlocal
cd /d "%~dp0"
powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "%~dp0tests\Run-Spiral7CU1.ps1"
if errorlevel 1 exit /b %errorlevel%
endlocal
