@echo off
setlocal
powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "%~dp0tests\Run-Spiral1CU1.ps1" -Mode Stage04
if errorlevel 1 exit /b %errorlevel%
endlocal
