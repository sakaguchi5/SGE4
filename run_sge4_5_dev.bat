@echo off
setlocal EnableExtensions
call "%~dp0tests\run_dev.bat" %*
exit /b %errorlevel%
