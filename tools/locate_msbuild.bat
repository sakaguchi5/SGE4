@echo off
if defined MSBUILD_EXE if exist "%MSBUILD_EXE%" exit /b 0
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
  powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0write_message.ps1" -Key VsWhereNotFound
  exit /b 1
)
for /f "usebackq tokens=*" %%I in (`"%VSWHERE%" -latest -products * -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe`) do set "MSBUILD_EXE=%%I"
if not defined MSBUILD_EXE (
  powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0write_message.ps1" -Key MsBuildNotFound
  exit /b 1
)
exit /b 0
