@echo off
powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "%~dp0tests\Invoke-Spiral2Tests.ps1" -Suite Gate -Configuration Debug
exit /b %errorlevel%
