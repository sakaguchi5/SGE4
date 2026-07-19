@echo off
setlocal EnableExtensions
call "%~dp0run_suite.bat" Runtime "%~1"
exit /b %errorlevel%
