param(
    [ValidateSet('Auto','Snapshot','Regression')]
    [string]$Mode = 'Auto'
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
function Require([bool]$Condition,[string]$Message){if(-not $Condition){throw $Message}}
if ($Mode -eq 'Auto') {
    $cu6Manifest = Join-Path $root 'docs\spiral7\CU6_REAL_GPU_MEASUREMENT_MANIFEST_V1.json'
    $Mode = if (Test-Path -LiteralPath $cu6Manifest -PathType Leaf) { 'Regression' } else { 'Snapshot' }
}

$manifestPath = Join-Path $root 'docs\spiral7\CU5_ARCHITECTURE_QUALIFICATION_MANIFEST_V1.json'
Require (Test-Path -LiteralPath $manifestPath -PathType Leaf) 'Spiral 7 CU5 Architecture Qualification manifest missing.'
$m = Get-Content -Raw -LiteralPath $manifestPath -Encoding UTF8 | ConvertFrom-Json
Require ($m.schema -eq 'SGE4-5.Spiral7.CU5ArchitectureQualificationManifest.V1') 'CU5 manifest schema mismatch.'
Require ($m.completionUnit -eq 'CU5') 'CU5 completion unit mismatch.'
Require ($m.cu4AcceptedCommit -eq '8f3b17e25d2a500beb2b658e6bc0a1d3f646ec26') 'CU5 accepted CU4 commit mismatch.'
Require ($m.cu4OwnerRunStatus -eq 'Passed') 'CU4 Owner run status must be Passed.'
Require ([int]$m.timelineInvocationCount -eq 128) 'CU5 timeline count mismatch.'
Require ([int]$m.candidateExecutionCountPerQualificationRun -eq 384) 'CU5 Candidate execution count mismatch.'
Require (($m.candidateOrder -join ',') -eq 'FullActiveDenseRecompute,CompactDeltaIndexHistoryReuse,AffectedBlockDeltaHistoryReuse') 'CU5 Candidate order mismatch.'
Require (($m.qualificationActiveCounts -join ',') -eq '0,1,32,63,64,65,256,1024,2048,4095,4096') 'CU5 Active count corpus mismatch.'
Require (($m.qualificationTransitionCounts -join ',') -eq '0,1,8,31,32,63,64,65,256,1024,4096') 'CU5 Transition count corpus mismatch.'
Require (($m.transitionKinds -join ',') -eq 'Hold,DirtyOnly,ActivateOnly,DeactivateOnly,BalancedChurn,PatternMigration,FullInvalidation,EmptyReset') 'CU5 transition-kind corpus mismatch.'
Require ($m.controlledRecovery.firstPostRecoverySemanticRule -eq 'W_tEqualsA_tAndH_tEmpty') 'CU5 Recovery semantic rule mismatch.'
Require ([int]$m.controlledRecovery.recoveryModifiedSurvivorCountInQualification -eq 64) 'CU5 Recovery M_t count mismatch.'
Require ([bool]$m.controlledRecovery.exactPerItemGenerationRebuild) 'CU5 exact-generation Recovery rebuild must be required.'
Require ([bool]$m.actualRemoval.removedAdapterLuidExcludedFromRetry) 'CU5 removed-adapter exclusion must be required.'
Require ($m.runtimeCandidatePolicyDecision -eq 'None') 'CU5 Runtime Candidate policy must remain None.'
Require ($m.architectureStatusOnSuccessfulGate -eq 'Complete') 'CU5 successful status must be Architecture Complete.'
Require ($m.executionOptimization.version -eq 'ReusableBatchedWarpExecutionV2') 'CU5 execution optimization version mismatch.'
Require ([int]$m.executionOptimization.candidateDispatchesPerQualificationRun -eq 384) 'CU5 optimized Candidate dispatch count mismatch.'
Require ([int]$m.executionOptimization.commandListSubmissionsPerQualificationRun -eq 128) 'CU5 optimized CommandList submission count mismatch.'
Require ([int]$m.executionOptimization.fenceWaitsPerQualificationRun -eq 128) 'CU5 optimized Fence wait count mismatch.'
Require ([int]$m.executionOptimization.candidateDispatchesPerSubmission -eq 3) 'CU5 A/B/C batching width mismatch.'
Require ([int]$m.executionOptimization.committedBufferCountPerExecutionContext -eq 22) 'CU5 reusable committed Buffer count mismatch.'
Require ($m.executionOptimization.semanticAndEvidenceFormatMutation -eq 'None' -and $m.executionOptimization.cu4EvidenceSerialization -eq 'Unchanged') 'CU5 optimization changed Semantic or evidence format.'
Require ($m.legacySchemaMutation -eq 'None' -and $m.legacyRuntimeMutation -eq 'None' -and $m.legacyBackendMutation -eq 'None' -and $m.compositionContractMutation -eq 'None') 'CU5 changed a frozen legacy boundary.'

$stableHashes = @{
    'docs\spiral7\CONTRACT_MANIFEST_V1.json' = '429ff4174bbf366c9aab88cb1547f9cc8300e52361cd816d5fe36e14fd8c53d2'
    'docs\spiral7\SGE4-5_Spiral7_Completion_Spec_v0.1.md' = 'f3dd4a125dd2da924599e67cf9247888987fc30784100f33cf11a909ee6949f9'
}
foreach($entry in $stableHashes.GetEnumerator()){
    $path=Join-Path $root $entry.Key
    Require (Test-Path -LiteralPath $path -PathType Leaf) "Stable Spiral 7 contract file missing: $($entry.Key)"
    $actual=(Get-FileHash -Algorithm SHA256 -LiteralPath $path).Hash.ToLowerInvariant()
    Require ($actual -eq $entry.Value) "Stable Spiral 7 contract file changed: $($entry.Key)"
}

$required = @(
'185_Spiral7DeltaFamilyRuntime\185_Spiral7DeltaFamilyRuntime.vcxproj',
'185_Spiral7DeltaFamilyRuntime\Spiral7DeltaFamilyRuntime.h',
'185_Spiral7DeltaFamilyRuntime\Spiral7DeltaFamilyRuntime.cpp',
'186_Spiral7ArchitectureQualificationTests\186_Spiral7ArchitectureQualificationTests.vcxproj',
'186_Spiral7ArchitectureQualificationTests\main.cpp',
'tests\Run-Spiral7CU5.ps1',
'tests\tools\Register-Spiral7CU5Projects.ps1',
'tests\tools\Verify-Spiral7CU5.ps1',
'run_sge4_5_spiral7_cu5_prepare.bat',
'run_sge4_5_spiral7_cu5_architecture_qualification.bat',
'docs\spiral7\CU5_ARCHITECTURE_QUALIFICATION_MANIFEST_V1.json',
'docs\spiral7\CU5_ARCHITECTURE_QUALIFICATION.md',
'docs\spiral7\CU5_RECOVERY_CONTRACT.md',
'docs\spiral7\CU5_EVIDENCE_LEDGER.md',
'docs\spiral7\CU5_CHANGESET.md',
'docs\spiral7\APPLY_CU5_JA.md',
'docs\spiral7\CU5_EXECUTION_OPTIMIZATION_V2.md'
)
foreach($relative in $required){Require (Test-Path -LiteralPath (Join-Path $root $relative) -PathType Leaf) "Missing Spiral 7 CU5 file: $relative"}

$projects = @(
    [pscustomobject]@{Path='185_Spiral7DeltaFamilyRuntime\185_Spiral7DeltaFamilyRuntime.vcxproj';Guid='{000000B9-0000-5000-8000-0000000000B9}'},
    [pscustomobject]@{Path='186_Spiral7ArchitectureQualificationTests\186_Spiral7ArchitectureQualificationTests.vcxproj';Guid='{000000BA-0000-5000-8000-0000000000BA}'}
)
$solution = Get-Content -Raw -LiteralPath (Join-Path $root 'SemanticGpuEngine4-5.sln') -Encoding UTF8
foreach($project in $projects){
    [xml]$xml = Get-Content -Raw -LiteralPath (Join-Path $root $project.Path) -Encoding UTF8
    $guid=[string](($xml.Project.PropertyGroup|Where-Object{$_.Label -eq 'Globals'}|Select-Object -First 1).ProjectGuid)
    Require ($guid.ToUpperInvariant() -eq $project.Guid.ToUpperInvariant()) "Project GUID mismatch: $($project.Path)"
    Require ($solution -match [regex]::Escape($project.Path)) "Solution registration missing: $($project.Path)"
}

$runtimeProject = Get-Content -Raw -LiteralPath (Join-Path $root '185_Spiral7DeltaFamilyRuntime\185_Spiral7DeltaFamilyRuntime.vcxproj') -Encoding UTF8
foreach($requiredDependency in @('172_SparseTemporalDeltaSemantic','178_SparseTemporalDeltaCandidateFamily','180_SparseTemporalDeltaCandidateFamilyVerifier','181_Spiral7DeltaFamilyExecution')){Require ($runtimeProject -match [regex]::Escape($requiredDependency)) "CU5 Runtime dependency missing: $requiredDependency"}
foreach($forbidden in @('179_SparseTemporalDeltaCandidateFamilyPlanner','22_CompositionRuntime','14_D3D12Backend')){Require ($runtimeProject -notmatch [regex]::Escape($forbidden)) "CU5 Runtime has forbidden dependency: $forbidden"}
$testProject = Get-Content -Raw -LiteralPath (Join-Path $root '186_Spiral7ArchitectureQualificationTests\186_Spiral7ArchitectureQualificationTests.vcxproj') -Encoding UTF8
foreach($requiredDependency in @('179_SparseTemporalDeltaCandidateFamilyPlanner','180_SparseTemporalDeltaCandidateFamilyVerifier','181_Spiral7DeltaFamilyExecution','185_Spiral7DeltaFamilyRuntime')){Require ($testProject -match [regex]::Escape($requiredDependency)) "CU5 test dependency missing: $requiredDependency"}

$runtimeHeader = Get-Content -Raw -LiteralPath (Join-Path $root '185_Spiral7DeltaFamilyRuntime\Spiral7DeltaFamilyRuntime.h') -Encoding UTF8
foreach($token in @('DeltaRuntimeEpochHandleV1','DeltaTimelineBindingV1','DeltaRepresentationSetHandleV1','DeltaHistoryHandleV1','DeltaRuntimeSubmissionTokenV1','DeltaRecoverySeedBindingV1','RecoverySeedRequired','BindDeltaRecoverySeedV1','RebuildDeltaRecoverySeedRepresentationsV1','SubmitDeltaRecoverySeedV1')){Require ($runtimeHeader -match [regex]::Escape($token)) "CU5 Runtime header token missing: $token"}
$runtimeCpp = Get-Content -Raw -LiteralPath (Join-Path $root '185_Spiral7DeltaFamilyRuntime\Spiral7DeltaFamilyRuntime.cpp') -Encoding UTF8
foreach($token in @('CU5 requires exactly 128 authority invocations','delta-runtime/stale-epoch','delta-runtime/recovery-seed-required','Semantic Recovery invocation must explicitly rebind current A_t and force W_t=A_t, H_t=empty','Recovery execution invocation must reproduce the rebound Active/Modified sets and exact generations','ID3D12Device5','RemoveDevice','delta-runtime/excluded-adapter','authorityBefore == runtime.impl_->authorityBytes','initializationSeedCase, executionRecoveryCase')){Require ($runtimeCpp -match [regex]::Escape($token)) "CU5 Runtime implementation token missing: $token"}
$executionCpp = Get-Content -Raw -LiteralPath (Join-Path $root '181_Spiral7DeltaFamilyExecution\Spiral7DeltaFamilyExecution.cpp') -Encoding UTF8
foreach($token in @('ReusableFamilyResourcesV1','ExecuteInvocationBatch','candidateCount * 2','gpu.SubmitAndWait();','resources.outputsAreCopySource = true','[WARP-BATCH]','previousHistory = std::move(outputs[0])')){Require ($executionCpp -match [regex]::Escape($token)) "CU5 optimized execution token missing: $token"}
Require (([regex]::Matches($executionCpp,[regex]::Escape('gpu.SubmitAndWait();'))).Count -eq 1) 'Optimized executor must contain one submission site for the A/B/C invocation batch.'
$testCpp = Get-Content -Raw -LiteralPath (Join-Path $root '186_Spiral7ArchitectureQualificationTests\main.cpp') -Encoding UTF8
foreach($token in @('corpus.cases.size() == s7::semantic::TimelineLengthV1','candidateCount == 384','BindBuildSubmitPrefix(runtime, handle, 128)','recoveryModified','BuildModified(active, active, 64)','Recovery M_t update did not change the materialized History output','--actual-removal','128 exact Sparse-Temporal invocations','Runtime Candidate-policy decision: None')){Require ($testCpp -match [regex]::Escape($token)) "CU5 test token missing: $token"}

$runner = Get-Content -Raw -LiteralPath (Join-Path $root 'tests\Run-Spiral7CU5.ps1') -Encoding UTF8
foreach($token in @('Invoke-CU5Stage','128-invocation WARP qualification Debug','Optimized execution: A/B/C share one CommandList submission and one Fence wait per invocation','CU5 total elapsed')){Require ($runner -match [regex]::Escape($token)) "CU5 optimized runner token missing: $token"}
$progress = Get-Content -Raw -LiteralPath (Join-Path $root 'docs\spiral7\SPIRAL7_PROGRESS.md') -Encoding UTF8
Require ($progress -match '8f3b17e25d2a500beb2b658e6bc0a1d3f646ec26') 'CU4 accepted commit is not recorded in progress.'
$proof = Get-Content -Raw -LiteralPath (Join-Path $root 'docs\spiral7\PROOF_LEDGER_V1.md') -Encoding UTF8
Require ($proof -match 'S7-I18.*CU5' -and $proof -match 'S7-I19.*CU5') 'CU5 Recovery proof ledger entries are missing.'

$cu2Manifest = Get-Content -Raw -LiteralPath (Join-Path $root 'docs\spiral7\CU2_ARCHITECTURE_MANIFEST_V1.json') -Encoding UTF8 | ConvertFrom-Json
foreach ($property in $cu2Manifest.legacyFileSha256.PSObject.Properties) {
    $path = Join-Path $root ([string]$property.Name)
    Require (Test-Path -LiteralPath $path -PathType Leaf) "Legacy boundary file missing: $($property.Name)"
    $actual = (Get-FileHash -Algorithm SHA256 -LiteralPath $path).Hash.ToLowerInvariant()
    Require ($actual -eq ([string]$property.Value).ToLowerInvariant()) "Legacy boundary file changed: $($property.Name)"
}

if ($Mode -eq 'Snapshot') {
    foreach($futureProject in @('187_Spiral7PerformanceExperiment','188_Spiral7PerformanceTests')){Require ($solution -notmatch [regex]::Escape($futureProject)) "CU5 Snapshot must not pre-create CU6 project: $futureProject"}
} elseif ($Mode -eq 'Regression') {
    Require (Test-Path -LiteralPath (Join-Path $root 'docs\spiral7\CU6_REAL_GPU_MEASUREMENT_MANIFEST_V1.json') -PathType Leaf) 'CU5 Regression mode requires a CU6 marker.'
} else { throw "Unexpected Spiral 7 CU5 verification mode: $Mode" }

Write-Host "SGE4-5 Spiral 7 CU5 static Architecture Qualification boundaries passed. Mode: $Mode"
Write-Host '128-invocation Frozen authority, exact-generation Recovery and stale-handle rejection are structurally present.'
Write-Host 'Legacy Schema 17 / Runtime 17 / Backend / Composition mutation: None.'
