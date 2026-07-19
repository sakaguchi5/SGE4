@echo off
setlocal EnableExtensions
call "%~dp0tests\run_gate.bat" %*
exit /b %errorlevel%
