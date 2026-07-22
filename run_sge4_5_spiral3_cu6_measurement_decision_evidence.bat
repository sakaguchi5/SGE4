@echo off
powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "%~dp0tests\Run-Spiral3CU6.ps1" -Mode All
exit /b %errorlevel%
