@echo off
setlocal EnableExtensions
set "ROOT=%~dp0"

for %%C in (Debug Release) do (
  call "%ROOT%build.bat" %%C
  if errorlevel 1 exit /b 1

  if not exist "%ROOT%build\bin\x64\%%C\34_AuthorityTests.exe" (
    echo Authority test executable was not found for %%C.
    exit /b 1
  )

  "%ROOT%build\bin\x64\%%C\34_AuthorityTests.exe"
  if errorlevel 1 exit /b 1
)

echo SGE4-5 VERIFIED PLAN AUTHORITY GATE PASSED
exit /b 0
