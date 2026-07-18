@echo off
setlocal
powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "%~dp0Prepare-Level4v1R3R5.ps1"
exit /b %errorlevel%
