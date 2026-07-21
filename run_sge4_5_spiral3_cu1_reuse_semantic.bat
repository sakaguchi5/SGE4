@echo off
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0tests\Run-Spiral3CU1.ps1" %*
exit /b %ERRORLEVEL%
