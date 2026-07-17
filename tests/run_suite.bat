@echo off
setlocal EnableExtensions

set "SUITE=%~1"
set "CONFIG=%~2"

if not defined SUITE (
  echo SGE4 suite name was not specified.
  exit /b 2
)

if not defined CONFIG set "CONFIG=Debug"
if /I "%CONFIG%"=="Debug" goto :config_ok
if /I "%CONFIG%"=="Release" goto :config_ok

echo Unsupported configuration: %CONFIG%
echo Expected Debug or Release.
exit /b 2

:config_ok
powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass ^
  -File "%~dp0tools\Verify-ScriptContracts.ps1"
if errorlevel 1 exit /b 1

powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass ^
  -File "%~dp0Invoke-SGE4Tests.ps1" ^
  -Suite "%SUITE%" ^
  -Configuration "%CONFIG%"
exit /b %errorlevel%
