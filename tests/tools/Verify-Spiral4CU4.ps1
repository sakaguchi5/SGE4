$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)

function Require([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}

$manifestPath = Join-Path $root 'docs\spiral4\CU4_CANDIDATE_FAMILY_MANIFEST_V1.json'
Require (Test-Path -LiteralPath $manifestPath -PathType Leaf) 'CU4 manifest missing.'
$m = Get-Content -Raw -LiteralPath $manifestPath -Encoding UTF8 | ConvertFrom-Json
Require ($m.schema -eq 'SGE4-5.Spiral4.CU4CandidateFamilyManifest.V1') 'CU4 manifest schema mismatch.'
Require ($m.baselineCommit -eq '04488a5203570d559ecd74372513d86abe34e4d0') 'CU4 baseline mismatch.'
Require ([int]$m.candidateCount -eq 7) 'CU4 candidate count mismatch.'
Require ([int]$m.batchPartitionCaseCount -eq 95) 'CU4 partition case count mismatch.'
Require ([int]$m.warpEquivalenceCaseCount -eq 21) 'CU4 WARP case count mismatch.'
Require ([int]$m.mutationRejectionCount -eq 24) 'CU4 mutation count mismatch.'
Require ($m.runtimeDispatchDecision -eq 'None') 'CU4 Runtime decision must be None.'

$required = @(
    '127_Spiral4CandidateFamilyContracts\127_Spiral4CandidateFamilyContracts.vcxproj',
    '128_ActiveWorkCandidateFamily\128_ActiveWorkCandidateFamily.vcxproj',
    '129_ActiveWorkCandidateFamilyPlanner\129_ActiveWorkCandidateFamilyPlanner.vcxproj',
    '130_ActiveWorkCandidateFamilyVerifier\130_ActiveWorkCandidateFamilyVerifier.vcxproj',
    '131_Spiral4CandidateFamilyExecution\131_Spiral4CandidateFamilyExecution.vcxproj',
    '142_Spiral4CandidateFamilyTests\142_Spiral4CandidateFamilyTests.vcxproj',
    'docs\spiral4\CU4_CANDIDATE_FAMILY_MANIFEST_V1.json',
    'docs\spiral4\CU4_CANDIDATE_FAMILY.md',
    'docs\spiral4\CU4_BATCH_PARTITION_AUTHORITY.md',
    'docs\spiral4\CU4_CHANGESET.md',
    'docs\spiral4\CU4_EVIDENCE_LEDGER.md',
    'tests\tools\Register-Spiral4CU4Projects.ps1',
    'tests\tools\Verify-Spiral4CU4.ps1',
    'tests\Run-Spiral4CU4.ps1',
    'run_sge4_5_spiral4_cu4_prepare.bat',
    'run_sge4_5_spiral4_cu4_candidate_family.bat'
)
foreach ($relative in $required) {
    Require (Test-Path -LiteralPath (Join-Path $root $relative) -PathType Leaf) "Missing CU4 file: $relative"
}

$solution = Get-Content -Raw -LiteralPath (Join-Path $root 'SemanticGpuEngine4-5.sln') -Encoding UTF8
$projects = @(
    [pscustomobject]@{ Path='127_Spiral4CandidateFamilyContracts\127_Spiral4CandidateFamilyContracts.vcxproj'; Guid='{0000007F-0000-5000-8000-00000000007F}' },
    [pscustomobject]@{ Path='128_ActiveWorkCandidateFamily\128_ActiveWorkCandidateFamily.vcxproj'; Guid='{00000080-0000-5000-8000-000000000080}' },
    [pscustomobject]@{ Path='129_ActiveWorkCandidateFamilyPlanner\129_ActiveWorkCandidateFamilyPlanner.vcxproj'; Guid='{00000081-0000-5000-8000-000000000081}' },
    [pscustomobject]@{ Path='130_ActiveWorkCandidateFamilyVerifier\130_ActiveWorkCandidateFamilyVerifier.vcxproj'; Guid='{00000082-0000-5000-8000-000000000082}' },
    [pscustomobject]@{ Path='131_Spiral4CandidateFamilyExecution\131_Spiral4CandidateFamilyExecution.vcxproj'; Guid='{00000083-0000-5000-8000-000000000083}' },
    [pscustomobject]@{ Path='142_Spiral4CandidateFamilyTests\142_Spiral4CandidateFamilyTests.vcxproj'; Guid='{0000008E-0000-5000-8000-00000000008E}' }
)
foreach ($project in $projects) {
    Require ($solution.Contains($project.Path)) "CU4 project missing from Solution: $($project.Path)"
    foreach ($entry in @(
        "$($project.Guid).Debug|x64.ActiveCfg = Debug|x64",
        "$($project.Guid).Debug|x64.Build.0 = Debug|x64",
        "$($project.Guid).Release|x64.ActiveCfg = Release|x64",
        "$($project.Guid).Release|x64.Build.0 = Release|x64"
    )) {
        Require ($solution.Contains($entry)) "CU4 Solution configuration missing: $entry"
    }
}

$verifierProject = Get-Content -Raw -LiteralPath (Join-Path $root '130_ActiveWorkCandidateFamilyVerifier\130_ActiveWorkCandidateFamilyVerifier.vcxproj') -Encoding UTF8
$verifierSource = Get-Content -Raw -LiteralPath (Join-Path $root '130_ActiveWorkCandidateFamilyVerifier\ActiveWorkCandidateFamilyVerifier.cpp') -Encoding UTF8
Require ($verifierProject -notmatch '129_ActiveWorkCandidateFamilyPlanner') 'CU4 Verifier references Planner project.'
Require ($verifierSource -notmatch 'ActiveWorkCandidateFamilyPlanner') 'CU4 Verifier source references Planner.'
foreach ($token in @(
    'IndependentContext',
    'DeriveExpected',
    'candidate family execution decision mismatch',
    'DeriveVerifiedBatchDispatchRecordsV1',
    'CandidateFamilyVerifierCertificate'
)) {
    Require ($verifierSource.Contains($token)) "CU4 Verifier token missing: $token"
}

$execution = Get-Content -Raw -LiteralPath (Join-Path $root '131_Spiral4CandidateFamilyExecution\CandidateFamilyExecution.cpp') -Encoding UTF8
foreach ($token in @(
    'FixedMaximumGuarded',
    'SingleExecuteIndirectDispatch',
    'BatchedExecuteIndirectDispatch',
    'D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT',
    'D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH',
    'ExecuteIndirect',
    'GPU Batch records differ from the Verified partition'
)) {
    Require ($execution.Contains($token)) "CU4 execution token missing: $token"
}

$tests = Get-Content -Raw -LiteralPath (Join-Path $root '142_Spiral4CandidateFamilyTests\main.cpp') -Encoding UTF8
foreach ($token in @(
    'raw.size() == 7',
    'attempted == 24',
    'Candidate outputs are not byte-identical',
    '5 sizes x 19 Active Counts = 95',
    '7 candidates x 3 Active Counts = 21'
)) {
    Require ($tests.Contains($token)) "CU4 test token missing: $token"
}

Write-Host 'SGE4-5 Spiral 4 CU4 candidate family, independent Batch authority, and WARP-equivalence boundaries passed.'
