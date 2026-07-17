@echo off
setlocal EnableExtensions
set "CONFIG=%~1"
if "%CONFIG%"=="" set "CONFIG=Debug"
set "ROOT=%~dp0"
call "%ROOT%build.bat" "%CONFIG%"
if errorlevel 1 exit /b 1

set "BIN=%ROOT%build\bin\x64\%CONFIG%"
set "TESTDIR=%ROOT%build\tests"
if not exist "%TESTDIR%" mkdir "%TESTDIR%"
set "PACKAGE=%TESTDIR%\SGE4_CommonExperiment.sgep"

echo Generating the independent Classical/SDF/PGA qualification Package...
"%BIN%\51_PackageCompiler.exe" "%PACKAGE%" all
if errorlevel 1 goto :failed

for %%T in (
  30_SemanticContractTests
  31_PackageContractTests
  32_CompilerPipelineTests
  33_PlanningTests
  34_AuthorityTests
  35_D3D12ConformanceTests
  39_FrontendEquivalenceTests
  40_SliceScenarioTests
  42_MetamorphicTests
  43_GeneratedGraphTests
  38_AdversarialBoundaryTests
  44_CanonicalFreezeTests
) do (
  echo Running %%T...
  "%BIN%\%%T.exe"
  if errorlevel 1 goto :failed
)

echo Running 37_RuntimeSemanticTests with WARP semantic readback...
"%BIN%\37_RuntimeSemanticTests.exe"
if errorlevel 1 goto :failed

echo Running 41_SliceExecutionTests with WARP...
"%BIN%\41_SliceExecutionTests.exe"
if errorlevel 1 goto :failed

echo Running 36_D3D12ReadbackTests with WARP and recovery...
"%BIN%\36_D3D12ReadbackTests.exe" "%PACKAGE%"
if errorlevel 1 goto :failed

echo Running 45_PlannedRuntimeTests with verified alternative Plans...
"%BIN%\45_PlannedRuntimeTests.exe"
if errorlevel 1 goto :failed

echo SGE4 %CONFIG% qualification passed.
exit /b 0

:failed
echo SGE4 %CONFIG% qualification failed.
exit /b 1
