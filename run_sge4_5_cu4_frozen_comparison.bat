@echo off
setlocal
powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "%~dp0tests\Run-Spiral1CU4.ps1" -Mode CU4
if errorlevel 1 exit /b %errorlevel%
endlocal
