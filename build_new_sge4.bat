@echo off
setlocal
call "%~dp0tools\locate_msbuild.bat" || exit /b 1
"%MSBUILD_EXE%" "%~dp0NewSGE4.sln" /m /nr:false /t:Build /p:Configuration=Debug /p:Platform=x64 || exit /b 1
"%MSBUILD_EXE%" "%~dp0NewSGE4.sln" /m /nr:false /t:Build /p:Configuration=Release /p:Platform=x64 || exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\write_message.ps1" -Key BuildPassed
exit /b 0
