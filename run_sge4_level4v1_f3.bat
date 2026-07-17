@echo off
setlocal
powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "%~dp0tests\Run-Level4v1F3.ps1" %*
exit /b %ERRORLEVEL%
