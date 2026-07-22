@echo off
setlocal
cd /d "%~dp0"
powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File ".\tests\Run-Spiral5CU5.ps1"
exit /b %errorlevel%
