@echo off
setlocal
powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "%~dp0tests\Run-Level4v1R3R5.ps1" -Stage R5
exit /b %errorlevel%
