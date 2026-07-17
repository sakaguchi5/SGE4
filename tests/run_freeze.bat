@echo off
setlocal EnableExtensions
call "%~dp0run_suite.bat" Freeze "Debug"
exit /b %errorlevel%
