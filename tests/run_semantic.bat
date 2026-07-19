@echo off
setlocal EnableExtensions
call "%~dp0run_suite.bat" Semantic "%~1"
exit /b %errorlevel%
