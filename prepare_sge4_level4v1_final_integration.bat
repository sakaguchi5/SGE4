@echo off
setlocal
powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "%~dp0Prepare-Level4v1FinalIntegration.ps1"
exit /b %ERRORLEVEL%
