@echo off
setlocal EnableExtensions
powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "%~dp0Prepare-Level4v1R1.ps1"
exit /b %errorlevel%
