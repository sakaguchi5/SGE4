@echo off
powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "%~dp0tests\Run-Spiral2FinalQualification.ps1" %*
exit /b %errorlevel%
