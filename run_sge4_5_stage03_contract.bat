@echo off
setlocal
powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "%~dp0tests\Run-Spiral1ResearchContract.ps1"
if errorlevel 1 exit /b %errorlevel%
endlocal
