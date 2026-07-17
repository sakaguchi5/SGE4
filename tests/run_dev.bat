@echo off
setlocal EnableExtensions
call "%~dp0run_suite.bat" Dev "%~1"
exit /b %errorlevel%
