@echo off
setlocal
powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "%~dp0Prepare-Level4v1F7F9.ps1"
if errorlevel 1 exit /b %errorlevel%
endlocal
