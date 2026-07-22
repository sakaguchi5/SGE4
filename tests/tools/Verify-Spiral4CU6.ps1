$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)

function Require([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}

$manifestPath = Join-Path $root 'docs\spiral4\CU6_MEASUREMENT_MANIFEST_V1.json'
Require (Test-Path -LiteralPath $manifestPath -PathType Leaf) 'CU6 measurement manifest missing.'
$m = Get-Content -Raw -LiteralPath $manifestPath -Encoding UTF8 | ConvertFrom-Json

Require ($m.schema -eq 'SGE4-5.Spiral4.CU6MeasurementManifest.V1') 'CU6 manifest schema mismatch.'
Require ($m.baselineCommit -eq '302e31569c2cec5d69fbc53f3ddb363cd0af777c') 'CU6 baseline mismatch.'
Require ([int]$m.candidateCount -eq 7) 'CU6 Candidate count mismatch.'
Require ([int]$m.activeCountCaseCount -eq 19) 'CU6 Active Count count mismatch.'
Require ([int]$m.balancedOrderCount -eq 14) 'CU6 balanced order count mismatch.'
Require ($m.primaryMetric -eq 'GpuCandidatePathNanoseconds') 'CU6 primary metric mismatch.'
Require ($m.runtimePolicyAuthorization -eq 'None') 'CU6 Runtime policy must remain None.'
Require ($m.nextCapabilitySelection -eq 'DeferredByOwner') 'CU6 next capability selection is not Owner-deferred.'
Require ($m.selectionStatus -eq 'OWNER_DECISION_REQUIRED') 'CU6 Owner selection status mismatch.'
Require ([bool]$m.experimentCompleteOnPass) 'CU6 Experiment Complete declaration missing.'
Require (-not [bool]$m.spiralClosedOnPass) 'CU6 must not close Spiral 4 before Owner selection.'
Require ($m.architectureEvidenceSha256 -eq '60CF81FB8BAC565CD35D696612B7050F464576DEDE5CEF7E2FE61B0CDFA7C32E') 'CU6 does not bind the CU5 Architecture evidence.'

$required = @(
    '133_Spiral4PerformanceExperiment\133_Spiral4PerformanceExperiment.vcxproj',
    '133_Spiral4PerformanceExperiment\Spiral4PerformanceExperiment.h',
    '133_Spiral4PerformanceExperiment\Spiral4PerformanceExperiment.cpp',
    '133_Spiral4PerformanceExperiment\Spiral4RealGpuMeasurement.cpp',
    '144_Spiral4PerformanceTests\144_Spiral4PerformanceTests.vcxproj',
    '144_Spiral4PerformanceTests\main.cpp',
    'tests\tools\Register-Spiral4CU6Projects.ps1',
    'tests\tools\Verify-Spiral4CU6.ps1',
    'tests\Run-Spiral4CU6.ps1',
    'run_sge4_5_spiral4_cu6_prepare.bat',
    'run_sge4_5_spiral4_cu6_measurement_decision_evidence.bat',
    'docs\spiral4\CU6_MEASUREMENT_MANIFEST_V1.json',
    'docs\spiral4\CU6_REAL_GPU_MEASUREMENT.md',
    'docs\spiral4\CU6_DECISION_RULES.md',
    'docs\spiral4\CU6_CHANGESET.md',
    'docs\spiral4\CU6_EVIDENCE_LEDGER.md'
)
foreach ($relative in $required) {
    Require (Test-Path -LiteralPath (Join-Path $root $relative) -PathType Leaf) "Missing CU6 file: $relative"
}

$solution = Get-Content -Raw -LiteralPath (Join-Path $root 'SemanticGpuEngine4-5.sln') -Encoding UTF8
$projects = @(
    [pscustomobject]@{ Path='133_Spiral4PerformanceExperiment\133_Spiral4PerformanceExperiment.vcxproj'; Guid='{00000085-0000-5000-8000-000000000085}' },
    [pscustomobject]@{ Path='144_Spiral4PerformanceTests\144_Spiral4PerformanceTests.vcxproj'; Guid='{00000090-0000-5000-8000-000000000090}' }
)
foreach ($project in $projects) {
    Require ($solution.Contains($project.Path)) "CU6 project missing from Solution: $($project.Path)"
    foreach ($entry in @(
        "$($project.Guid).Debug|x64.ActiveCfg = Debug|x64",
        "$($project.Guid).Debug|x64.Build.0 = Debug|x64",
        "$($project.Guid).Release|x64.ActiveCfg = Release|x64",
        "$($project.Guid).Release|x64.Build.0 = Release|x64"
    )) {
        Require ($solution.Contains($entry)) "CU6 Solution configuration missing: $entry"
    }
}

$header = Get-Content -Raw -LiteralPath (Join-Path $root '133_Spiral4PerformanceExperiment\Spiral4PerformanceExperiment.h') -Encoding UTF8
foreach ($token in @(
    'CanonicalCandidateCountV1 = 7',
    'CanonicalActiveCountCaseCountV1 = 19',
    'CanonicalOrderCountV1 = 14',
    'GpuCandidatePathNanoseconds',
    'gpuArgumentGenerationNanoseconds',
    'gpuStateTransitionNanoseconds',
    'gpuExecutionNanoseconds',
    'gpuOutputSynchronizationNanoseconds',
    'gpuObservationCopyNanoseconds',
    'activeInputPreparationNanoseconds',
    'commandRecordingNanoseconds',
    'submitAndWaitNanoseconds',
    'numericObservationNanoseconds',
    'NoiseEquivalentOrUnresolved'
)) {
    Require ($header.Contains($token)) "CU6 header token missing: $token"
}

$portable = Get-Content -Raw -LiteralPath (Join-Path $root '133_Spiral4PerformanceExperiment\Spiral4PerformanceExperiment.cpp') -Encoding UTF8
foreach ($token in @(
    'Each Candidate must appear twice in every order position.',
    'Measurement evidence checksum mismatch.',
    'Corrupted measurement evidence was accepted.',
    'Fixed/Single crossover',
    'Single/Batch crossover',
    'RuntimePolicyAuthorization = None',
    'NextCapabilitySelection = DeferredByOwner',
    'SelectionStatus = OWNER_DECISION_REQUIRED',
    'SPIRAL4_DECISION_EVIDENCE_REPORT.md'
)) {
    Require ($portable.Contains($token)) "CU6 portable evidence token missing: $token"
}

$gpu = Get-Content -Raw -LiteralPath (Join-Path $root '133_Spiral4PerformanceExperiment\Spiral4RealGpuMeasurement.cpp') -Encoding UTF8
foreach ($token in @(
    'DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE',
    'D3D12_QUERY_HEAP_TYPE_TIMESTAMP',
    'GetTimestampFrequency',
    'D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH',
    'D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT',
    'ExecuteIndirect',
    'gpuCandidatePathNanoseconds',
    'CanonicalBalancedOrdersV1',
    'RunRealGpuMeasurementV1',
    '[MEASURED] Nf='
)) {
    Require ($gpu.Contains($token)) "CU6 real-GPU token missing: $token"
}

$tests = Get-Content -Raw -LiteralPath (Join-Path $root '144_Spiral4PerformanceTests\main.cpp') -Encoding UTF8
foreach ($token in @(
    '--self-test',
    '--measure',
    '--report',
    '--warmup-per-order',
    '--measurement-per-order',
    '--adapter-index',
    'Runtime dispatch-dimension decision: None',
    'OWNER_DECISION_REQUIRED'
)) {
    Require ($tests.Contains($token)) "CU6 test token missing: $token"
}

Write-Host 'SGE4-5 Spiral 4 CU6 balanced hardware measurement, binary evidence, crossover classification, and Owner-only decision boundaries passed.'
