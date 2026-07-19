@echo off
setlocal
powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "%~dp0tests\Run-Spiral1CU3.ps1" -Mode CU3
if errorlevel 1 exit /b %errorlevel%
endlocal
