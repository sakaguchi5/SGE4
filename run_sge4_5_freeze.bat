@echo off
setlocal EnableExtensions
call "%~dp0tests\run_freeze.bat" %*
exit /b %errorlevel%
