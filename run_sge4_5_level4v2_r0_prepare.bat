@echo off
setlocal
set ROOT=%~dp0
powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "%ROOT%tests\tools\Update-SourceManifest.ps1"
if errorlevel 1 exit /b %errorlevel%
echo ============================================================
echo LEVEL 4 V2 R0 PREPARATION COMPLETE
echo SOURCE_MANIFEST regenerated. No Git operation was performed.
echo Next: .\run_sge4_5_level4v2_r0_input_freeze.bat
echo ============================================================
endlocal
