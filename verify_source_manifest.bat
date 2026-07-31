@echo off
setlocal
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\verify_source_manifest.ps1"
exit /b %errorlevel%
