@echo off
setlocal EnableExtensions
call "%~dp0run_suite.bat" Authority "%~1"
exit /b %errorlevel%
