@echo off
setlocal EnableExtensions
call "%~dp0run_suite.bat" Adversarial "%~1"
exit /b %errorlevel%
