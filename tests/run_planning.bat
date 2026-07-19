@echo off
setlocal EnableExtensions
call "%~dp0run_suite.bat" Planning "%~1"
exit /b %errorlevel%
