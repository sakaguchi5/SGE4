@echo off
setlocal EnableExtensions
set "ROOT=%~dp0"
set "FINALDIR=%ROOT%build\sge4-level4v1"

if exist "%FINALDIR%" rmdir /s /q "%FINALDIR%"
if errorlevel 1 goto :failed
mkdir "%FINALDIR%"
if errorlevel 1 goto :failed

echo Verifying SGE4 Level 4 v1 architecture, scripts, and frozen source inventory...
call "%ROOT%tests\run_architecture.bat" Debug
if errorlevel 1 goto :failed

call "%ROOT%run_tests.bat" Debug
if errorlevel 1 goto :failed
call "%ROOT%run_tests.bat" Release
if errorlevel 1 goto :failed

set "DEBUGBIN=%ROOT%build\bin\x64\Debug"
set "RELEASEBIN=%ROOT%build\bin\x64\Release"
echo Emitting Frozen Composition artifacts in fresh processes...
"%DEBUGBIN%\47_LinkPlanningTests.exe" --emit-composition "%FINALDIR%\Composition_Debug_A.sgec"
if errorlevel 1 goto :failed
"%DEBUGBIN%\47_LinkPlanningTests.exe" --emit-composition "%FINALDIR%\Composition_Debug_B.sgec"
if errorlevel 1 goto :failed
fc /b "%FINALDIR%\Composition_Debug_A.sgec" "%FINALDIR%\Composition_Debug_B.sgec" >nul
if errorlevel 1 goto :failed
"%RELEASEBIN%\47_LinkPlanningTests.exe" --emit-composition "%FINALDIR%\Composition_Release.sgec"
if errorlevel 1 goto :failed
fc /b "%FINALDIR%\Composition_Debug_A.sgec" "%FINALDIR%\Composition_Release.sgec" >nul
if errorlevel 1 goto :failed

echo ============================================================
echo SGE4 LEVEL 4 V1 STATIC VERIFIED PACKAGE COMPOSITION FREEZE PASSED
echo Leaf ABI: D3D12 Schema 17 / Runtime 17 preserved
echo Authority: Contract - LinkPlan - independent LinkVerifier - Frozen Composition
echo Runtime: static DAG, shared DeviceDomain Buffers, WARP fan-out/diamond and whole-domain recovery
echo Determinism: fresh-process and Debug/Release Frozen Composition byte identity
echo ============================================================
exit /b 0

:failed
echo SGE4 Level 4 v1 qualification failed.
exit /b 1
