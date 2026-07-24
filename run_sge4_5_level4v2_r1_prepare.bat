@echo off
setlocal
set ROOT=%~dp0
powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "%ROOT%tests\tools\Register-Level4V2R1Projects.ps1"
if errorlevel 1 exit /b %errorlevel%
powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "%ROOT%tests\tools\Update-SourceManifest.ps1"
if errorlevel 1 exit /b %errorlevel%
echo ============================================================
echo LEVEL 4 V2 R1 PREPARATION COMPLETE
echo R1 Projects registered and SOURCE_MANIFEST regenerated.
echo No Git operation was performed.
echo Next: .\run_sge4_5_level4v2_r1_canonical_vocabulary.bat
echo ============================================================
endlocal
