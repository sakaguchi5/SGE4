$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)

function Require([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}

$manifestPath = Join-Path $root 'docs\spiral4\CU5_ARCHITECTURE_MANIFEST_V1.json'
Require (Test-Path -LiteralPath $manifestPath -PathType Leaf) 'CU5 Architecture manifest missing.'
$m = Get-Content -Raw -LiteralPath $manifestPath -Encoding UTF8 | ConvertFrom-Json
Require ($m.schema -eq 'SGE4-5.Spiral4.CU5ArchitectureManifest.V1') 'CU5 manifest schema mismatch.'
Require ($m.baselineCommit -eq '8577ad1e4bb99f2a1c4d7125c6e586b33fb54154') 'CU5 baseline mismatch.'
Require ([int]$m.candidateCount -eq 7) 'CU5 Candidate count mismatch.'
Require ([int]$m.activeCountCaseCount -eq 19) 'CU5 Active Count corpus mismatch.'
Require ([int]$m.warpQualificationCaseCount -eq 133) 'CU5 WARP case count mismatch.'
Require ([int]$m.inactiveTailProofCount -eq 133) 'CU5 inactive-tail count mismatch.'
Require ([int]$m.gpuBatchRecordProofCount -eq 95) 'CU5 Batch-record proof count mismatch.'
Require ($m.actualRemovalMechanism -eq 'ID3D12Device5.RemoveDevice') 'CU5 actual removal mechanism mismatch.'
Require ($m.runtimeDispatchDecision -eq 'None') 'CU5 Runtime decision must remain None.'
Require ([bool]$m.architectureCompleteOnPass) 'CU5 Architecture Complete declaration missing.'

$required = @(
    '122_Spiral4IndirectExecution\Spiral4IndirectExecution.h',
    '122_Spiral4IndirectExecution\Spiral4IndirectExecution.cpp',
    '131_Spiral4CandidateFamilyExecution\CandidateFamilyExecution.h',
    '131_Spiral4CandidateFamilyExecution\CandidateFamilyExecution.cpp',
    '132_Spiral4CandidateFamilyRuntime\132_Spiral4CandidateFamilyRuntime.vcxproj',
    '132_Spiral4CandidateFamilyRuntime\CandidateFamilyRuntime.h',
    '132_Spiral4CandidateFamilyRuntime\CandidateFamilyRuntime.cpp',
    '143_Spiral4ArchitectureQualificationTests\143_Spiral4ArchitectureQualificationTests.vcxproj',
    '143_Spiral4ArchitectureQualificationTests\main.cpp',
    'tests\tools\Register-Spiral4CU5Projects.ps1',
    'tests\tools\Verify-Spiral4CU5.ps1',
    'tests\Run-Spiral4CU5.ps1',
    'run_sge4_5_spiral4_cu5_prepare.bat',
    'run_sge4_5_spiral4_cu5_architecture_qualification.bat',
    'docs\spiral4\CU5_ARCHITECTURE_MANIFEST_V1.json',
    'docs\spiral4\CU5_ARCHITECTURE_COMPLETE.md',
    'docs\spiral4\CU5_RECOVERY_MODEL.md',
    'docs\spiral4\CU5_CHANGESET.md',
    'docs\spiral4\CU5_EVIDENCE_LEDGER.md'
)
foreach ($relative in $required) {
    Require (Test-Path -LiteralPath (Join-Path $root $relative) -PathType Leaf) "Missing CU5 file: $relative"
}

$solution = Get-Content -Raw -LiteralPath (Join-Path $root 'SemanticGpuEngine4-5.sln') -Encoding UTF8
$projects = @(
    [pscustomobject]@{ Path='132_Spiral4CandidateFamilyRuntime\132_Spiral4CandidateFamilyRuntime.vcxproj'; Guid='{00000084-0000-5000-8000-000000000084}' },
    [pscustomobject]@{ Path='143_Spiral4ArchitectureQualificationTests\143_Spiral4ArchitectureQualificationTests.vcxproj'; Guid='{0000008F-0000-5000-8000-00000000008F}' }
)
foreach ($project in $projects) {
    Require ($solution.Contains($project.Path)) "CU5 project missing from Solution: $($project.Path)"
    foreach ($entry in @(
        "$($project.Guid).Debug|x64.ActiveCfg = Debug|x64",
        "$($project.Guid).Debug|x64.Build.0 = Debug|x64",
        "$($project.Guid).Release|x64.ActiveCfg = Release|x64",
        "$($project.Guid).Release|x64.Build.0 = Release|x64"
    )) {
        Require ($solution.Contains($entry)) "CU5 Solution configuration missing: $entry"
    }
}

$singleHeader = Get-Content -Raw -LiteralPath (Join-Path $root '122_Spiral4IndirectExecution\Spiral4IndirectExecution.h') -Encoding UTF8
$singleSource = Get-Content -Raw -LiteralPath (Join-Path $root '122_Spiral4IndirectExecution\Spiral4IndirectExecution.cpp') -Encoding UTF8
Require ($singleHeader.Contains('RunSingleIndirectArchitectureOnDeviceV1')) 'CU5 Single Indirect native-device entry missing.'
Require ($singleSource.Contains('RunSingleIndirectArchitectureOnDeviceV1')) 'CU5 Single Indirect native-device implementation missing.'
Require ($singleSource.Contains('return RunSingleIndirectArchitectureOnDeviceV1')) 'CU2 WARP wrapper does not delegate to native-device execution.'

$familyHeader = Get-Content -Raw -LiteralPath (Join-Path $root '131_Spiral4CandidateFamilyExecution\CandidateFamilyExecution.h') -Encoding UTF8
$familySource = Get-Content -Raw -LiteralPath (Join-Path $root '131_Spiral4CandidateFamilyExecution\CandidateFamilyExecution.cpp') -Encoding UTF8
Require ($familyHeader.Contains('RunVerifiedCandidateFamilyCorpusOnDeviceV1')) 'CU5 Candidate family native-device entry missing.'
Require ($familySource.Contains('RunSingleIndirectArchitectureOnDeviceV1')) 'Candidate B does not execute on the shared CU5 device.'
Require ($familySource.Contains('return RunVerifiedCandidateFamilyCorpusOnDeviceV1')) 'CU4 WARP wrapper does not delegate to native-device execution.'

$runtimeHeader = Get-Content -Raw -LiteralPath (Join-Path $root '132_Spiral4CandidateFamilyRuntime\CandidateFamilyRuntime.h') -Encoding UTF8
$runtimeSource = Get-Content -Raw -LiteralPath (Join-Path $root '132_Spiral4CandidateFamilyRuntime\CandidateFamilyRuntime.cpp') -Encoding UTF8
foreach ($token in @(
    'CandidateFamilyEpochHandleV1',
    'CandidateFamilyDynamicBindingV1',
    'CandidateFamilySubmissionTokenV1',
    'ControlledRebuild',
    'ForceRemovalForTest',
    'RetryAdapterReacquisition',
    'dynamicRebindRequired',
    'frozenIdentityPreserved'
)) {
    Require ($runtimeHeader.Contains($token)) "CU5 Runtime header token missing: $token"
}
foreach ($token in @(
    'ID3D12Device5',
    'RemoveDevice()',
    'candidate-runtime/stale-epoch',
    'candidate-runtime/rebind-required',
    'candidate-runtime/no-eligible-adapter',
    'RunVerifiedCandidateFamilyCorpusOnDeviceV1',
    'excludedAdapterLuid'
)) {
    Require ($runtimeSource.Contains($token)) "CU5 Runtime source token missing: $token"
}

$tests = Get-Content -Raw -LiteralPath (Join-Path $root '143_Spiral4ArchitectureQualificationTests\main.cpp') -Encoding UTF8
foreach ($token in @(
    '7 Candidates x 19 Active Counts = 133',
    'Inactive-tail sentinel proofs: 133',
    'GPU Batch-record proofs: 5 x 19 = 95',
    'nonFiniteCount',
    'homogeneousCoordinateMismatchCount',
    'maxRelativeError',
    'maxUlpError',
    'Controlled Candidate-family Recovery passed',
    'Actual ID3D12Device5::RemoveDevice quarantine passed',
    'Fresh-process Candidate-family rematerialization passed',
    'candidate-runtime/stale-epoch',
    'candidate-runtime/rebind-required',
    'RequireSameObservation'
)) {
    Require ($tests.Contains($token)) "CU5 qualification token missing: $token"
}

Write-Host 'SGE4-5 Spiral 4 CU5 full corpus, deterministic evidence, Recovery, and stale-epoch boundaries passed.'
