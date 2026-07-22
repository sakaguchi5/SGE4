@echo off
setlocal
powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "%~dp0tests\Run-Spiral6CU4.ps1"
exit /b %errorlevel%
