@echo off
powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "%~dp0tests\Run-Spiral2ReviewClosure.ps1" %*
exit /b %errorlevel%
