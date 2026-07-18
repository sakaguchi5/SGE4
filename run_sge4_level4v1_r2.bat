@echo off
setlocal EnableExtensions
powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "%~dp0tests\Run-Level4v1R2.ps1"
exit /b %errorlevel%
