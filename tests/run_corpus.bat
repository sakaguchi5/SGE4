@echo off
setlocal EnableExtensions
call "%~dp0run_suite.bat" Corpus "%~1"
exit /b %errorlevel%
