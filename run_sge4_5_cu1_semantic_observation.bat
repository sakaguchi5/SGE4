@echo off
setlocal
powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "%~dp0tests\Run-Spiral1CU1.ps1" -Mode CU1
if errorlevel 1 exit /b %errorlevel%
endlocal
