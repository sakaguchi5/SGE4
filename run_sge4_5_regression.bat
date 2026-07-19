@echo off
setlocal EnableExtensions
call "%~dp0tests\run_regression.bat" %*
exit /b %errorlevel%
