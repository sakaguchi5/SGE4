@echo off
setlocal
cd /d "%~dp0"
powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "%~dp0tests\tools\Update-SourceManifest.ps1"
if errorlevel 1 exit /b %errorlevel%
call "%~dp0run_sge4_5_spiral7_cu1_research_contract.bat"
if errorlevel 1 exit /b %errorlevel%
endlocal
