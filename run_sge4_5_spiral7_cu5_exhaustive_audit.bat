@echo off
setlocal
powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "%~dp0tests\Run-Spiral7CU5ExhaustiveAudit.ps1"
exit /b %errorlevel%
