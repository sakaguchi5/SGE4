$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
function Require([bool]$Condition,[string]$Message) { if (-not $Condition) { throw $Message } }

$manifestPath = Join-Path $root 'docs\spiral6\CU6_MEASUREMENT_MANIFEST_V2.json'
Require (Test-Path -LiteralPath $manifestPath -PathType Leaf) 'Spiral 6 CU6 V2 manifest missing.'
$m = Get-Content -Raw -LiteralPath $manifestPath -Encoding UTF8 | ConvertFrom-Json
Require ($m.schema -eq 'SGE4-5.Spiral6.CU6RelativeMeasurementManifest.V2') 'CU6 V2 manifest schema mismatch.'
Require ($m.completionUnit -eq 'CU6') 'CU6 completion unit mismatch.'
Require ($m.baselineCommit -eq '18cc16db82c5cf7b5d7be00d51345196df046447') 'CU6 V2 baseline mismatch.'
Require ($m.primaryEvidence -eq 'BlockLocalPairedCandidateRatio') 'CU6 V2 primary evidence mismatch.'
Require ($m.absoluteNanosecondsRole -eq 'DescriptiveOnly') 'CU6 V2 absolute timing role mismatch.'
Require ($m.resourcePolicy -eq 'PrebuiltBeforeMeasurement') 'CU6 V2 Resource policy mismatch.'
Require ($m.queuePolicy -eq 'OneContinuousPairedSubmitPerPatternAndCardinalityBlock') 'CU6 V2 Queue policy mismatch.'
Require ($m.rejectionPolicy -eq 'RejectAndRetryWholePairedBlock') 'CU6 V2 rejection policy mismatch.'
Require ($m.telemetryPolicy -eq 'OptionalDiagnosticOnly') 'CU6 V2 telemetry role mismatch.'
Require ($m.pStatePolicy -eq 'NotAnAcceptanceCondition') 'CU6 V2 P-state policy mismatch.'
Require ($m.crossBlockAbsoluteComparison -eq 'Forbidden') 'CU6 V2 cross-block policy mismatch.'
Require ([int]$m.defaults.iterationsPerBatch -eq 512) 'CU6 V2 batch iteration mismatch.'
Require ([double]$m.defaults.maximumControlRatio -eq 1.20) 'CU6 V2 control ratio mismatch.'
Require ([double]$m.defaults.relativeNoiseFloor -eq 0.02) 'CU6 V2 noise floor mismatch.'
Require ([double]$m.defaults.minimumPairAgreement -eq 0.70) 'CU6 V2 pair agreement mismatch.'
Require ($m.runtimePolicyAuthorization -eq 'None') 'CU6 Runtime authorization must remain None.'
Require ($m.selectionStatus -eq 'OWNER_DECISION_REQUIRED') 'CU6 selection status mismatch.'

$required = @(
 '170_Spiral6PerformanceExperiment\170_Spiral6PerformanceExperiment.vcxproj',
 '170_Spiral6PerformanceExperiment\Spiral6PerformanceExperiment.h',
 '170_Spiral6PerformanceExperiment\Spiral6PerformanceExperiment.cpp',
 '170_Spiral6PerformanceExperiment\Spiral6RealGpuMeasurement.cpp',
 '171_Spiral6PerformanceTests\171_Spiral6PerformanceTests.vcxproj',
 '171_Spiral6PerformanceTests\main.cpp',
 'tests\tools\Register-Spiral6CU6Projects.ps1',
 'tests\tools\Verify-Spiral6CU6.ps1',
 'tests\Run-Spiral6CU6.ps1',
 'run_sge4_5_spiral6_cu6_prepare.bat',
 'run_sge4_5_spiral6_cu6_measurement_decision_evidence.bat',
 'docs\spiral6\CU6_MEASUREMENT_MANIFEST_V2.json',
 'docs\spiral6\CU6_REAL_GPU_MEASUREMENT_V2.md',
 'docs\spiral6\CU6_DECISION_RULES_V2.md',
 'docs\spiral6\CU6_CHANGESET_V2.md',
 'docs\spiral6\CU6_EVIDENCE_LEDGER_V2.md')
foreach ($relative in $required) {
    Require (Test-Path -LiteralPath (Join-Path $root $relative) -PathType Leaf) "Missing CU6 V2 file: $relative"
}

$solution = Get-Content -Raw -LiteralPath (Join-Path $root 'SemanticGpuEngine4-5.sln') -Encoding UTF8
foreach ($path in @(
 '170_Spiral6PerformanceExperiment\170_Spiral6PerformanceExperiment.vcxproj',
 '171_Spiral6PerformanceTests\171_Spiral6PerformanceTests.vcxproj')) {
    Require ($solution.Contains($path)) "CU6 V2 project missing from Solution: $path"
}

$header = Get-Content -Raw -LiteralPath (Join-Path $root '170_Spiral6PerformanceExperiment\Spiral6PerformanceExperiment.h') -Encoding UTF8
foreach ($token in @(
 'MeasurementEvidenceSchemaVersionV2 = 2',
 'iterationsPerBatch = 512',
 'maximumControlRatio = 1.20',
 'relativeNoiseFloor = 0.02',
 'minimumPairAgreement = 0.70',
 'controlNormalizedTime',
 'PairDecisionV2',
 'Optional NVIDIA telemetry is diagnostic only')) {
    Require ($header.Contains($token)) "CU6 V2 header token missing: $token"
}

$core = Get-Content -Raw -LiteralPath (Join-Path $root '170_Spiral6PerformanceExperiment\Spiral6PerformanceExperiment.cpp') -Encoding UTF8
foreach ($token in @(
 'SGE4-5.Spiral6.RelativePerformanceEvidence.V2',
 'PairedObservationMap',
 'BuildPairDecision',
 'Duplicate Candidate in one paired observation',
 'Absolute nanoseconds are descriptive only',
 'Block-local scale invariance failed',
 'Rejected attempt must not expose ranking samples',
 'Corrupted evidence was accepted')) {
    Require ($core.Contains($token)) "CU6 V2 portable token missing: $token"
}

$gpu = Get-Content -Raw -LiteralPath (Join-Path $root '170_Spiral6PerformanceExperiment\Spiral6RealGpuMeasurement.cpp') -Encoding UTF8
foreach ($token in @(
 'BuildAllPreparedCases',
 'RecordMeasurementBlock',
 'FixedControlNonStationaryInsidePairedBlock',
 'Vendor P-state, GpuIdle and',
 'controlNormalizedTime',
 'for (std::uint32_t iteration = 0; iteration < config.iterationsPerBatch; ++iteration)',
 'No stable measurement block was accepted')) {
    Require ($gpu.Contains($token)) "CU6 V2 real-GPU token missing: $token"
}
Require (-not $gpu.Contains('PerformanceStateOutsideAcceptedRange')) 'CU6 V2 must not gate on P-state.'
Require (-not $gpu.Contains('GraphicsClockDroppedInsideBlock')) 'CU6 V2 must not gate on vendor Graphics clock.'
Require (-not $gpu.Contains('MemoryClockDroppedInsideBlock')) 'CU6 V2 must not gate on vendor Memory clock.'
Require (-not $gpu.Contains('ExecuteMeasuredCandidate')) 'CU6 V2 must not retain the V1 per-sample submit path.'

Write-Host 'SGE4-5 Spiral 6 CU6 V2 block-local paired measurement, relative map, evidence and Owner boundaries passed.'
