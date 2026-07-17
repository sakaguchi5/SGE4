@echo off
setlocal EnableExtensions
call "%~dp0run_suite.bat" Regression "%~1"
exit /b %errorlevel%
