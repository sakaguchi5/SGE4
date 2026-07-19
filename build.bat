@echo off
setlocal EnableExtensions

set "ROOT=%~dp0"
set "CONFIG=%~1"
if not defined CONFIG set "CONFIG=Debug"

if /I "%CONFIG%"=="Debug" goto :config_ok
if /I "%CONFIG%"=="Release" goto :config_ok
echo Unsupported configuration: %CONFIG%
echo Expected Debug or Release.
exit /b 2

:config_ok
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
  echo vswhere.exe was not found: "%VSWHERE%"
  exit /b 1
)

set "MSBUILD="
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe`) do set "MSBUILD=%%i"
if not defined MSBUILD (
  echo MSBuild was not found.
  exit /b 1
)

"%MSBUILD%" "%ROOT%SemanticGpuEngine4-5.sln" /m /nologo /t:Build /p:Configuration=%CONFIG% /p:Platform=x64
exit /b %errorlevel%
