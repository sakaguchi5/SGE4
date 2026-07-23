$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
function Require([bool]$Condition,[string]$Message) { if (-not $Condition) { throw $Message } }

$required = @(
 '170_Spiral6PerformanceExperiment\Spiral6PerformanceExperiment.h',
 '170_Spiral6PerformanceExperiment\Spiral6RealGpuMeasurement.cpp',
 '172_Spiral6EquivalentSetAuditTests\172_Spiral6EquivalentSetAuditTests.vcxproj',
 '172_Spiral6EquivalentSetAuditTests\main.cpp',
 'tests\tools\Register-Spiral6EquivalentSetAuditProject.ps1',
 'tests\tools\Verify-Spiral6EquivalentSetAudit.ps1',
 'tests\tools\Analyze-Spiral6EquivalentSetAudit.ps1',
 'tests\Run-Spiral6EquivalentSetAudit.ps1',
 'run_sge4_5_spiral6_cu6_equivalent_set_audit_prepare.bat',
 'run_sge4_5_spiral6_cu6_equivalent_set_audit.bat',
 'docs\spiral6\CU6_EQUIVALENT_SET_AUDIT.md',
 'docs\spiral6\CU6_EQUIVALENT_SET_AUDIT_MANIFEST_V1.json',
 'docs\spiral6\CU6_EQUIVALENT_SET_AUDIT_CHANGESET.md')
foreach ($relative in $required) { Require (Test-Path -LiteralPath (Join-Path $root $relative) -PathType Leaf) "Missing Equivalent Set Audit file: $relative" }

$solution = Get-Content -Raw -LiteralPath (Join-Path $root 'SemanticGpuEngine4-5.sln') -Encoding UTF8
$path = '172_Spiral6EquivalentSetAuditTests\172_Spiral6EquivalentSetAuditTests.vcxproj'
$guid = '{000000AC-0000-5000-8000-0000000000AC}'
Require ($solution.Contains($path)) 'Equivalent Set Audit project is missing from Solution.'
foreach ($entry in @(
 "$guid.Debug|x64.ActiveCfg = Debug|x64",
 "$guid.Debug|x64.Build.0 = Debug|x64",
 "$guid.Release|x64.ActiveCfg = Release|x64",
 "$guid.Release|x64.Build.0 = Release|x64")) {
 Require ($solution.Contains($entry)) "Equivalent Set Audit Solution configuration missing: $entry"
}

$header = Get-Content -Raw -LiteralPath (Join-Path $root '170_Spiral6PerformanceExperiment\Spiral6PerformanceExperiment.h') -Encoding UTF8
foreach ($token in @(
 'EquivalentSetAuditScheduleV1',
 'BalancedInterleaved',
 'FixedPatternMajor',
 'K1PrefixUniform',
 'K4096AllPatterns',
 'RunEquivalentSetAuditScheduleSelfTestV1',
 'RunEquivalentSetAuditV1')) {
 Require ($header.Contains($token)) "Equivalent Set Audit header token missing: $token"
}

$source = Get-Content -Raw -LiteralPath (Join-Path $root '170_Spiral6PerformanceExperiment\Spiral6RealGpuMeasurement.cpp') -Encoding UTF8
foreach ($token in @(
 'PrefixControl K=1 and UniformStride K=1 identities differ',
 'K=4096 Pattern identities differ',
 'Each equivalent label/Candidate must have 24 samples',
 'GetClockCalibration',
 'QueryVideoMemoryInfo',
 'Equivalent label structural identity changed across audit samples',
 'spiral6_equivalent_set_audit_samples_v1.csv')) {
 Require ($source.Contains($token)) "Equivalent Set Audit source token missing: $token"
}

$project = Get-Content -Raw -LiteralPath (Join-Path $root $path) -Encoding UTF8
Require ($project.Contains('170_Spiral6PerformanceExperiment')) 'Equivalent Set Audit must depend on Project 170.'
Require ($project.Contains('<TreatWarningAsError>true</TreatWarningAsError>')) 'Equivalent Set Audit must use warnings as errors.'

$runner = Get-Content -Raw -LiteralPath (Join-Path $root 'tests\Run-Spiral6EquivalentSetAudit.ps1') -Encoding UTF8
foreach ($token in @('balanced-a','fixed','balanced-b','spiral6_measurement_samples_v1.csv','SPIRAL6_EQUIVALENT_SET_AUDIT_REPORT.md')) {
 Require ($runner.Contains($token)) "Equivalent Set Audit runner token missing: $token"
}

$analyzer = Get-Content -Raw -LiteralPath (Join-Path $root 'tests\tools\Analyze-Spiral6EquivalentSetAudit.ps1') -Encoding UTF8
foreach ($token in @('Original CU6 baseline','Maximum balanced equivalent-label spread','Find-ChangePoint','setIdentity','artifactIdentity','outputDigest')) {
 Require ($analyzer.Contains($token)) "Equivalent Set Audit analyzer token missing: $token"
}

Write-Host 'Spiral 6 CU6 Equivalent Set Audit structure, exact-set controls, balanced schedule and evidence analyzer passed.'
