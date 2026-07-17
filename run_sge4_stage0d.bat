@echo off
setlocal EnableExtensions
set "ROOT=%~dp0"
set "FINALDIR=%ROOT%build\sge4-stage0d"

if exist "%FINALDIR%" rmdir /s /q "%FINALDIR%"
if errorlevel 1 goto :failed
mkdir "%FINALDIR%"
if errorlevel 1 goto :failed

echo Verifying SGE4 architecture, scripts, and frozen source inventory...
call "%ROOT%tests\run_architecture.bat" Debug
if errorlevel 1 goto :failed

call "%ROOT%run_tests.bat" Debug
if errorlevel 1 goto :failed

call "%ROOT%run_tests.bat" Release
if errorlevel 1 goto :failed

call "%ROOT%run_authority_gate.bat"
if errorlevel 1 goto :failed

set "DEBUGBIN=%ROOT%build\bin\x64\Debug"
set "RELEASEBIN=%ROOT%build\bin\x64\Release"

echo Emitting Canonical manifests in fresh Debug processes...
"%DEBUGBIN%\44_CanonicalFreezeTests.exe" --emit-manifest "%FINALDIR%\Canonical_Debug_A.freeze-manifest.txt"
if errorlevel 1 goto :failed
"%DEBUGBIN%\44_CanonicalFreezeTests.exe" --emit-manifest "%FINALDIR%\Canonical_Debug_B.freeze-manifest.txt"
if errorlevel 1 goto :failed
fc /b "%FINALDIR%\Canonical_Debug_A.freeze-manifest.txt" "%FINALDIR%\Canonical_Debug_B.freeze-manifest.txt" >nul
if errorlevel 1 goto :failed

"%RELEASEBIN%\44_CanonicalFreezeTests.exe" --emit-manifest "%FINALDIR%\Canonical_Release.freeze-manifest.txt"
if errorlevel 1 goto :failed
fc /b "%FINALDIR%\Canonical_Debug_A.freeze-manifest.txt" "%FINALDIR%\Canonical_Release.freeze-manifest.txt" >nul
if errorlevel 1 goto :failed

echo Emitting Planning manifests in fresh processes...
"%DEBUGBIN%\33_PlanningTests.exe" --emit-manifest "%FINALDIR%\Planning_Debug_A.freeze-manifest.txt"
if errorlevel 1 goto :failed
"%DEBUGBIN%\33_PlanningTests.exe" --emit-manifest "%FINALDIR%\Planning_Debug_B.freeze-manifest.txt"
if errorlevel 1 goto :failed
fc /b "%FINALDIR%\Planning_Debug_A.freeze-manifest.txt" "%FINALDIR%\Planning_Debug_B.freeze-manifest.txt" >nul
if errorlevel 1 goto :failed

"%RELEASEBIN%\33_PlanningTests.exe" --emit-manifest "%FINALDIR%\Planning_Release.freeze-manifest.txt"
if errorlevel 1 goto :failed
fc /b "%FINALDIR%\Planning_Debug_A.freeze-manifest.txt" "%FINALDIR%\Planning_Release.freeze-manifest.txt" >nul
if errorlevel 1 goto :failed

echo ============================================================
echo SGE4 STAGE 0D FOUNDATION FREEZE PASSED
echo Independent solution/namespace: SemanticGpuEngine4 / sge4
echo Canonical corpus: 54 deterministic Packages
echo Target ABI: proven D3D12 Schema 17 / Runtime 17
echo Orchestration: CandidatePlanner is Package-free; SGE4Compiler alone lowers verifier-sealed Plans
echo Regression: SGE3 canonical Packages, planning manifests and selected Plan identities preserved
echo Runtime: WARP observations, readback, external boundaries, temporal state and recovery qualified
echo Frontends: Classical, SDF and direct PGA converge to one execution contract
echo ============================================================
exit /b 0

:failed
echo SGE4 Stage 0D qualification failed.
exit /b 1
