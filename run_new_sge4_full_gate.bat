@echo off
setlocal
call "%~dp0verify_source_manifest.bat" || exit /b 1
call "%~dp0build_new_sge4.bat" || exit /b 1
if not exist "%~dp0build\evidence" mkdir "%~dp0build\evidence"
"%~dp0build\bin\x64\Debug\60_UnifiedArchitectureTests.exe" "%~dp0build\evidence\unified_debug_a.bin" || exit /b 1
"%~dp0build\bin\x64\Debug\60_UnifiedArchitectureTests.exe" "%~dp0build\evidence\unified_debug_b.bin" || exit /b 1
"%~dp0build\bin\x64\Release\60_UnifiedArchitectureTests.exe" "%~dp0build\evidence\unified_release.bin" || exit /b 1
fc /b "%~dp0build\evidence\unified_debug_a.bin" "%~dp0build\evidence\unified_debug_b.bin" >nul || exit /b 1
fc /b "%~dp0build\evidence\unified_debug_a.bin" "%~dp0build\evidence\unified_release.bin" >nul || exit /b 1
for %%C in (Debug Release) do (
  "%~dp0build\bin\x64\%%C\62_UnifiedMigrationAcceptance.exe" || exit /b 1
  "%~dp0build\bin\x64\%%C\61_UnifiedWindowsQualification.exe" || exit /b 1
  "%~dp0build\bin\x64\%%C\61_UnifiedWindowsQualification.exe" --actual-removal || exit /b 1
)
echo ============================================================
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\write_message.ps1" -Key FullGatePassed
echo ============================================================
exit /b 0
