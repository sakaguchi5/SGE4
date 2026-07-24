@echo off
setlocal
set ROOT=%~dp0
powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "%ROOT%tests\tools\Register-Level4V2R4Projects.ps1"
if errorlevel 1 exit /b %errorlevel%
powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "%ROOT%tests\tools\Update-SourceManifest.ps1"
if errorlevel 1 exit /b %errorlevel%
echo ============================================================
echo LEVEL 4 V2 R4 PREPARATION COMPLETE
echo R4 Projects registered. SOURCE_MANIFEST regenerated.
echo No Git operation was performed.
echo ============================================================
endlocal
