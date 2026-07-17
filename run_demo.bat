@echo off
setlocal EnableExtensions
set "ROOT=%~dp0"
set "CONFIG=%~1"
if not defined CONFIG set "CONFIG=Debug"

call "%ROOT%build.bat" "%CONFIG%"
if errorlevel 1 exit /b 1

set "BIN=%ROOT%build\bin\x64\%CONFIG%"
set "PACKAGE=%ROOT%build\SGE4_Demo.sgep"

if not exist "%BIN%\51_PackageCompiler.exe" (
  echo Package compiler was not found: "%BIN%\51_PackageCompiler.exe"
  exit /b 1
)
if not exist "%BIN%\50_Launcher.exe" (
  echo Launcher was not found: "%BIN%\50_Launcher.exe"
  exit /b 1
)

"%BIN%\51_PackageCompiler.exe" "%PACKAGE%" all
if errorlevel 1 exit /b 1

"%BIN%\50_Launcher.exe" "%PACKAGE%"
exit /b %errorlevel%
