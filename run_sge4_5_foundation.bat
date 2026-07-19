@echo off
setlocal
powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "%~dp0tests\Run-SGE4_5Foundation.ps1" %*
exit /b %ERRORLEVEL%
