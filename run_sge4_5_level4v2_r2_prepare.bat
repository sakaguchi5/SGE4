@echo off
setlocal
set ROOT=%~dp0
powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "%ROOT%tests\tools\Register-Level4V2R2Projects.ps1"
if errorlevel 1 exit /b %errorlevel%
powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "%ROOT%tests\tools\Update-SourceManifest.ps1"
if errorlevel 1 exit /b %errorlevel%
echo ============================================================
echo LEVEL 4 V2 R2 PREPARATION COMPLETE
echo R2 Projects registered and SOURCE_MANIFEST regenerated.
echo No Git operation was performed.
echo Next: .\run_sge4_5_level4v2_r2_unified_authority.bat
echo ============================================================
endlocal
