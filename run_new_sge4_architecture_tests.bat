@echo off
setlocal
call "%~dp0tools\locate_msbuild.bat" || exit /b 1
for %%C in (Debug Release) do (
  "%MSBUILD_EXE%" "%~dp0tests\60_UnifiedArchitectureTests\60_UnifiedArchitectureTests.vcxproj" /m /nr:false /t:Build /p:Configuration=%%C /p:Platform=x64 /p:SolutionDir="%~dp0" || exit /b 1
  "%~dp0build\bin\x64\%%C\60_UnifiedArchitectureTests.exe" || exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\write_message.ps1" -Key ArchitecturePassed
exit /b 0
