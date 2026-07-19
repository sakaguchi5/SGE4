@echo off
setlocal
powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "%~dp0tests\Run-Level4v1FinalIntegration.ps1" %*
exit /b %ERRORLEVEL%
