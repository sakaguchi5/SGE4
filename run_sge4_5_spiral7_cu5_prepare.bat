@echo off
setlocal
powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "%~dp0tests\tools\Finalize-Spiral7CU5Layout.ps1"
if errorlevel 1 exit /b %errorlevel%
powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "%~dp0tests\tools\Register-Spiral7CU5Projects.ps1"
if errorlevel 1 exit /b %errorlevel%
powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "%~dp0tests\tools\Update-SourceManifest.ps1"
if errorlevel 1 exit /b %errorlevel%
echo Spiral 7 CU5 final preparation complete.
endlocal
