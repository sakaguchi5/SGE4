param(
    [ValidateSet('Auto','Snapshot','Regression')]
    [string]$Mode = 'Auto'
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
function Require([bool]$Condition,[string]$Message){if(-not $Condition){throw $Message}}
if ($Mode -eq 'Auto') {
    $cu5Manifest = Join-Path $root 'docs\spiral7\CU5_ARCHITECTURE_QUALIFICATION_MANIFEST_V1.json'
    $Mode = if (Test-Path -LiteralPath $cu5Manifest -PathType Leaf) { 'Regression' } else { 'Snapshot' }
}
$manifestPath = Join-Path $root 'docs\spiral7\CU4_CANDIDATE_FAMILY_MANIFEST_V1.json'
Require (Test-Path -LiteralPath $manifestPath -PathType Leaf) 'Spiral 7 CU4 Candidate Family manifest missing.'
$m = Get-Content -Raw -LiteralPath $manifestPath -Encoding UTF8 | ConvertFrom-Json
Require ($m.schema -eq 'SGE4-5.Spiral7.CU4CandidateFamilyManifest.V1') 'CU4 manifest schema mismatch.'
Require ($m.completionUnit -eq 'CU4') 'CU4 completion unit mismatch.'
Require ($m.ownerBaselineCommit -eq 'd0bb0d406fca8beabed2331daff870ea414dd87d') 'CU4 Owner baseline mismatch.'
Require ($m.cu3AcceptedCommit -eq '21e7e91e1f85bb5620fca50e9def76b7c9a76291') 'CU4 accepted CU3 commit mismatch.'
Require ($m.cu3OwnerRunStatus -eq 'Passed') 'CU3 Owner run status must be Passed.'
Require (($m.candidateOrder -join ',') -eq 'FullActiveDenseRecompute,CompactDeltaIndexHistoryReuse,AffectedBlockDeltaHistoryReuse') 'CU4 Candidate order mismatch.'
Require ([int]$m.candidateRepresentations.FullActiveDenseRecompute.fixedCapacityBytes -eq 512) 'Candidate A capacity mismatch.'
Require ([int]$m.candidateRepresentations.CompactDeltaIndexHistoryReuse.fixedCapacityBytes -eq 32768) 'Candidate B capacity mismatch.'
Require ([int]$m.candidateRepresentations.AffectedBlockDeltaHistoryReuse.fixedCapacityBytes -eq 1536) 'Candidate C capacity mismatch.'
Require ([int]$m.candidateRepresentations.AffectedBlockDeltaHistoryReuse.recordStrideBytes -eq 24) 'Candidate C stride mismatch.'
Require ([int]$m.timelineInvocationCount -eq 18) 'CU4 timeline count mismatch.'
Require ([int]$m.candidateExecutionCount -eq 54) 'CU4 Candidate execution count mismatch.'
Require ([int]$m.rawMutationAttackCount -eq 16) 'CU4 mutation count mismatch.'
Require ($m.pairwiseCandidateOutputPerInvocation -eq 'ByteIdentical') 'CU4 pairwise-output rule mismatch.'
Require ($m.candidateWinnerSelection -eq 'ForbiddenInCU4') 'CU4 must not select a winner.'
Require ($m.runtimeCandidatePolicyDecision -eq 'None') 'CU4 Runtime Candidate policy must remain None.'
Require ($m.legacySchemaMutation -eq 'None' -and $m.legacyRuntimeMutation -eq 'None' -and $m.legacyBackendMutation -eq 'None' -and $m.compositionContractMutation -eq 'None') 'CU4 changed a frozen legacy boundary.'
$required = @(
'178_SparseTemporalDeltaCandidateFamily\178_SparseTemporalDeltaCandidateFamily.vcxproj',
'178_SparseTemporalDeltaCandidateFamily\DeltaCandidateFamily.h',
'178_SparseTemporalDeltaCandidateFamily\DeltaCandidateFamily.cpp',
'179_SparseTemporalDeltaCandidateFamilyPlanner\179_SparseTemporalDeltaCandidateFamilyPlanner.vcxproj',
'179_SparseTemporalDeltaCandidateFamilyPlanner\DeltaCandidateFamilyPlanner.h',
'179_SparseTemporalDeltaCandidateFamilyPlanner\DeltaCandidateFamilyPlanner.cpp',
'180_SparseTemporalDeltaCandidateFamilyVerifier\180_SparseTemporalDeltaCandidateFamilyVerifier.vcxproj',
'180_SparseTemporalDeltaCandidateFamilyVerifier\DeltaCandidateFamilyVerifier.h',
'180_SparseTemporalDeltaCandidateFamilyVerifier\DeltaCandidateFamilyVerifier.cpp',
'181_Spiral7DeltaFamilyExecution\181_Spiral7DeltaFamilyExecution.vcxproj',
'181_Spiral7DeltaFamilyExecution\Spiral7DeltaFamilyExecution.h',
'181_Spiral7DeltaFamilyExecution\Spiral7DeltaFamilyExecution.cpp',
'184_Spiral7CandidateFamilyTests\184_Spiral7CandidateFamilyTests.vcxproj',
'184_Spiral7CandidateFamilyTests\main.cpp',
'tests\Run-Spiral7CU4.ps1',
'tests\tools\Register-Spiral7CU4Projects.ps1',
'tests\tools\Verify-Spiral7CU4.ps1',
'run_sge4_5_spiral7_cu4_prepare.bat',
'run_sge4_5_spiral7_cu4_candidate_family.bat',
'docs\spiral7\CU4_CANDIDATE_FAMILY_MANIFEST_V1.json',
'docs\spiral7\CU4_INCREMENTAL_HISTORY_CANDIDATE_FAMILY.md',
'docs\spiral7\CU4_CORPUS_V1.md',
'docs\spiral7\CU4_MUTATION_MATRIX.md',
'docs\spiral7\CU4_EVIDENCE_LEDGER.md',
'docs\spiral7\CU4_CHANGESET.md',
'docs\spiral7\APPLY_CU4_JA.md'
)
foreach($relative in $required){Require (Test-Path -LiteralPath (Join-Path $root $relative) -PathType Leaf) "Missing Spiral 7 CU4 file: $relative"}
$projects = @(
    [pscustomobject]@{Path='178_SparseTemporalDeltaCandidateFamily\178_SparseTemporalDeltaCandidateFamily.vcxproj';Guid='{000000B2-0000-5000-8000-0000000000B2}'},
    [pscustomobject]@{Path='179_SparseTemporalDeltaCandidateFamilyPlanner\179_SparseTemporalDeltaCandidateFamilyPlanner.vcxproj';Guid='{000000B3-0000-5000-8000-0000000000B3}'},
    [pscustomobject]@{Path='180_SparseTemporalDeltaCandidateFamilyVerifier\180_SparseTemporalDeltaCandidateFamilyVerifier.vcxproj';Guid='{000000B4-0000-5000-8000-0000000000B4}'},
    [pscustomobject]@{Path='181_Spiral7DeltaFamilyExecution\181_Spiral7DeltaFamilyExecution.vcxproj';Guid='{000000B5-0000-5000-8000-0000000000B5}'},
    [pscustomobject]@{Path='184_Spiral7CandidateFamilyTests\184_Spiral7CandidateFamilyTests.vcxproj';Guid='{000000B8-0000-5000-8000-0000000000B8}'}
)
$solution = Get-Content -Raw -LiteralPath (Join-Path $root 'SemanticGpuEngine4-5.sln') -Encoding UTF8
foreach($project in $projects){
    [xml]$xml = Get-Content -Raw -LiteralPath (Join-Path $root $project.Path) -Encoding UTF8
    $guid=[string](($xml.Project.PropertyGroup|Where-Object{$_.Label -eq 'Globals'}|Select-Object -First 1).ProjectGuid)
    Require ($guid.ToUpperInvariant() -eq $project.Guid.ToUpperInvariant()) "Project GUID mismatch: $($project.Path)"
    Require ($solution -match [regex]::Escape($project.Path)) "Solution registration missing: $($project.Path)"
}
$candidateProject = Get-Content -Raw -LiteralPath (Join-Path $root '178_SparseTemporalDeltaCandidateFamily\178_SparseTemporalDeltaCandidateFamily.vcxproj') -Encoding UTF8
foreach($forbidden in @('179_SparseTemporalDeltaCandidateFamilyPlanner','180_SparseTemporalDeltaCandidateFamilyVerifier','181_Spiral7DeltaFamilyExecution','22_CompositionRuntime','14_D3D12Backend')){Require ($candidateProject -notmatch [regex]::Escape($forbidden)) "Candidate Family has forbidden dependency: $forbidden"}
$plannerProject = Get-Content -Raw -LiteralPath (Join-Path $root '179_SparseTemporalDeltaCandidateFamilyPlanner\179_SparseTemporalDeltaCandidateFamilyPlanner.vcxproj') -Encoding UTF8
Require ($plannerProject -match '178_SparseTemporalDeltaCandidateFamily') 'Family Planner must depend on Family Candidate.'
foreach($forbidden in @('180_SparseTemporalDeltaCandidateFamilyVerifier','181_Spiral7DeltaFamilyExecution','22_CompositionRuntime','14_D3D12Backend')){Require ($plannerProject -notmatch [regex]::Escape($forbidden)) "Family Planner has forbidden dependency: $forbidden"}
$verifierProject = Get-Content -Raw -LiteralPath (Join-Path $root '180_SparseTemporalDeltaCandidateFamilyVerifier\180_SparseTemporalDeltaCandidateFamilyVerifier.vcxproj') -Encoding UTF8
Require ($verifierProject -match '178_SparseTemporalDeltaCandidateFamily') 'Family Verifier must inspect Raw Family Candidate.'
foreach($forbidden in @('179_SparseTemporalDeltaCandidateFamilyPlanner','181_Spiral7DeltaFamilyExecution','22_CompositionRuntime','14_D3D12Backend')){Require ($verifierProject -notmatch [regex]::Escape($forbidden)) "Family Verifier has forbidden dependency: $forbidden"}
$executionProject = Get-Content -Raw -LiteralPath (Join-Path $root '181_Spiral7DeltaFamilyExecution\181_Spiral7DeltaFamilyExecution.vcxproj') -Encoding UTF8
Require ($executionProject -match '180_SparseTemporalDeltaCandidateFamilyVerifier') 'Family execution must consume opaque Verified Family Candidate.'
Require ($executionProject -notmatch '179_SparseTemporalDeltaCandidateFamilyPlanner') 'Family execution must not depend on Family Planner.'
$familyHeader = Get-Content -Raw -LiteralPath (Join-Path $root '178_SparseTemporalDeltaCandidateFamily\DeltaCandidateFamily.h') -Encoding UTF8
foreach($token in @('FullActiveDenseRecompute','CompactDeltaIndexHistoryReuse','AffectedBlockDeltaHistoryReuse','AffectedBlockDeltaRecordV1','AffectedBlockRecordStrideBytesV1 = 24','AffectedBlockCapacityBytesV1')){Require ($familyHeader -match [regex]::Escape($token)) "CU4 Candidate token missing: $token"}
$familyCpp = Get-Content -Raw -LiteralPath (Join-Path $root '178_SparseTemporalDeltaCandidateFamily\DeltaCandidateFamily.cpp') -Encoding UTF8
foreach($token in @('(update[block] & clear[block]) != 0','record.updateMask & record.clearMask','Affected Block masks do not reconstruct exact W_t and R_t','DenseActiveMaskCapacityBytesV1','CompactDeltaCapacityBytesV1','AffectedBlockCapacityBytesV1')){Require ($familyCpp -match [regex]::Escape($token)) "CU4 family derivation token missing: $token"}
$verifierHeader = Get-Content -Raw -LiteralPath (Join-Path $root '180_SparseTemporalDeltaCandidateFamilyVerifier\DeltaCandidateFamilyVerifier.h') -Encoding UTF8
foreach($token in @('VerifiedDeltaFamilyCandidateV1() = delete','VerifyDeltaFamilyCandidateV1','ValidateVerifiedDeltaFamilyCandidateV1')){Require ($verifierHeader -match [regex]::Escape($token)) "CU4 family Verifier token missing: $token"}
$executionCpp = Get-Content -Raw -LiteralPath (Join-Path $root '181_Spiral7DeltaFamilyExecution\Spiral7DeltaFamilyExecution.cpp') -Encoding UTF8
foreach($token in @('ExecuteIndirect','FullActiveDenseRecompute','CompactDeltaIndexHistoryReuse','AffectedBlockDeltaHistoryReuse','pairwiseOutputByteIdentical','outputs[0] == outputs[1]','previousHistory = std::move(outputs[0])')){Require ($executionCpp -match [regex]::Escape($token)) "CU4 execution token missing: $token"}
$testCpp = Get-Content -Raw -LiteralPath (Join-Path $root '184_Spiral7CandidateFamilyTests\main.cpp') -Encoding UTF8
foreach($token in @('corpus.mutationCount == 16','corpus.cases.size() == 18','Cross-invocation Delta Family Verified replay accepted','Pairwise full-output bytes: identical after every invocation','Runtime Candidate-policy decision: None')){Require ($testCpp -match [regex]::Escape($token)) "CU4 test token missing: $token"}
$progress = Get-Content -Raw -LiteralPath (Join-Path $root 'docs\spiral7\SPIRAL7_PROGRESS.md') -Encoding UTF8
Require ($progress -match '21e7e91e1f85bb5620fca50e9def76b7c9a76291') 'CU3 accepted commit is not recorded in progress.'
$cu2Manifest = Get-Content -Raw -LiteralPath (Join-Path $root 'docs\spiral7\CU2_ARCHITECTURE_MANIFEST_V1.json') -Encoding UTF8 | ConvertFrom-Json
foreach ($property in $cu2Manifest.legacyFileSha256.PSObject.Properties) {
    $path = Join-Path $root ([string]$property.Name)
    Require (Test-Path -LiteralPath $path -PathType Leaf) "Legacy boundary file missing: $($property.Name)"
    $actual = (Get-FileHash -Algorithm SHA256 -LiteralPath $path).Hash.ToLowerInvariant()
    Require ($actual -eq ([string]$property.Value).ToLowerInvariant()) "Legacy boundary file changed: $($property.Name)"
}
if ($Mode -eq 'Snapshot') {
    foreach($futureProject in @('185_Spiral7ArchitectureQualificationTests','186_Spiral7PerformanceExperiment','187_Spiral7PerformanceTests')){Require ($solution -notmatch [regex]::Escape($futureProject)) "CU4 Snapshot must not pre-create future project: $futureProject"}
} elseif ($Mode -eq 'Regression') {
    Require (Test-Path -LiteralPath (Join-Path $root 'docs\spiral7\CU5_ARCHITECTURE_QUALIFICATION_MANIFEST_V1.json') -PathType Leaf) 'CU4 Regression mode requires a CU5 marker.'
} else { throw "Unexpected Spiral 7 CU4 verification mode: $Mode" }
Write-Host "SGE4-5 Spiral 7 CU4 static Candidate Family boundaries passed. Mode: $Mode"
Write-Host 'A/B/C are independently verified; Block masks reconstruct W_t/R_t; no winner is authorized.'
Write-Host 'Legacy Schema 17 / Runtime 17 / Backend / Composition mutation: None.'
