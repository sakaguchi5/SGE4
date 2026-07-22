@echo off
setlocal
powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "%~dp0tests\tools\Register-Spiral6CU5Projects.ps1"
if errorlevel 1 exit /b %errorlevel%
powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "%~dp0tests\tools\Update-SourceManifest.ps1"
exit /b %errorlevel%
