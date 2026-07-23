@echo off
setlocal
powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "%~dp0tests\Run-Spiral6CU6.ps1" -Mode CU6 %*
exit /b %errorlevel%
