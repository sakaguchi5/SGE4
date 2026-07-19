@echo off
setlocal EnableExtensions
call "%~dp0run_suite.bat" Package "%~1"
exit /b %errorlevel%
