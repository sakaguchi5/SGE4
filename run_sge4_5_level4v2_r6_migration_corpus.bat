@echo off
setlocal
set ROOT=%~dp0
call "%ROOT%run_sge4_5_level4v2_r6_prepare.bat"
if errorlevel 1 exit /b %errorlevel%
powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "%ROOT%tests\Run-Level4V2R6.ps1"
exit /b %errorlevel%
