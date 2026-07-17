@echo off
setlocal EnableExtensions
call "%~dp0run_suite.bat" Architecture "%~1"
exit /b %errorlevel%
