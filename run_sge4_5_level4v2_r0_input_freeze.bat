@echo off
setlocal
set ROOT=%~dp0
powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "%ROOT%tests\Run-Level4V2R0.ps1"
exit /b %errorlevel%
