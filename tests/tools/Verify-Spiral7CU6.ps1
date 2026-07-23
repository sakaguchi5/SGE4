$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
function Require([bool]$Condition,[string]$Message){if(-not $Condition){throw $Message}}

$manifestPath = Join-Path $root 'docs\spiral7\CU6_REAL_GPU_MEASUREMENT_MANIFEST_V1.json'
Require (Test-Path -LiteralPath $manifestPath -PathType Leaf) 'Spiral 7 CU6-2 manifest missing.'
$m = Get-Content -Raw -LiteralPath $manifestPath -Encoding UTF8 | ConvertFrom-Json
Require ($m.schema -eq 'SGE4-5.Spiral7.CU6RealGpuMeasurementManifest.V1') 'CU6-2 manifest schema mismatch.'
Require ($m.completionUnit -eq 'CU6-2') 'CU6-2 completion unit mismatch.'
Require ($m.cu5AcceptedCommit -eq 'c9f0b5a62e2a7f3c9e0355cdaa1c683819c6dcfa') 'CU6-2 accepted CU5 commit mismatch.'
Require ($m.cu6_1AcceptedCommit -eq '903e00be0f7e39ffd1758004c117fbc0233b0164') 'CU6-2 base commit mismatch.'
Require ($m.cu5OwnerRunStatus -eq 'Passed') 'CU5 Owner run status must be Passed.'
Require ($m.cu5ArchitectureEvidenceSha256 -eq '1F1D09B4E52DBC35E961E0D3751B32292B2C30EEDCFA473800C5BB8E8CB5AB73') 'CU5 Architecture evidence binding mismatch.'
Require ($m.cu5ControlledRecoveryEvidenceSha256 -eq '7F0247B193F9BC20F4EDA5FEED590C1DA5722EF36B133640292B1ED616CFF62B') 'CU5 Controlled evidence binding mismatch.'
Require ($m.cu5FreshEvidenceSha256 -eq '091EAEC0D27287FE897F6813043FD75C090D5DA17054461DF314F99B2A6F5A92') 'CU5 Fresh evidence binding mismatch.'
Require (($m.candidateOrder -join ',') -eq 'FullActiveDenseRecompute,CompactDeltaIndexHistoryReuse,AffectedBlockDeltaHistoryReuse') 'CU6-2 Candidate order mismatch.'
Require (($m.measurementActiveCounts -join ',') -eq '64,256,1024,2048,4096') 'CU6-2 Active count grid mismatch.'
Require (($m.measurementPasses.canonicalSurface.transitionCounts -join ',') -eq '0,1,8,32,64,256,1024,4096') 'CU6-2 canonical Transition grid mismatch.'
Require (($m.measurementPasses.highTransitionRefinement.transitionCounts -join ',') -eq '1024,1536,2048,3072,4096') 'CU6-2 refinement Transition grid mismatch.'
Require ([int]$m.measurementPasses.canonicalSurface.caseCountPerRun -eq 160) 'CU6-2 canonical case count mismatch.'
Require ([int]$m.measurementPasses.highTransitionRefinement.caseCountPerRun -eq 100) 'CU6-2 refinement case count mismatch.'
Require ([int]$m.measurementPasses.highTransitionRefinement.defaultRunCount -eq 3 -and [int]$m.measurementPasses.highTransitionRefinement.defaultCyclesPerOrder -eq 3) 'CU6-2 refinement repetition contract mismatch.'
Require ($m.decisionAuthority.medianRanking -eq 'DescriptiveOnly') 'CU6-2 median ranking must be descriptive only.'
Require ($m.decisionAuthority.stableWinner -eq 'CandidateMustWinPairedDecisionsAgainstBothAlternatives') 'CU6-2 paired winner authority mismatch.'
Require ($m.decisionAuthority.transitionCountZero -eq 'BAndCZeroDispatchEquivalent') 'CU6-2 T=0 classification mismatch.'
Require (-not [bool]$m.decisionAuthority.transitionCountZeroIncludedInCrossover) 'CU6-2 T=0 must be excluded from crossover.'
Require ($m.binaryEvidenceSchema -eq 'S7M3' -and [int]$m.binaryEvidenceSchemaVersion -eq 3) 'CU6-2 binary evidence schema mismatch.'
Require ($m.runtimeCandidatePolicyAuthorization -eq 'None') 'CU6-2 Runtime Candidate policy must remain None.'
Require ($m.universalWinnerClaim -eq 'Forbidden') 'CU6-2 must forbid a universal winner claim.'
Require ($m.spiral7Closure -eq 'OwnerRequired') 'CU6-2 Spiral closure must remain Owner-gated.'
Require ($m.nextProgramAfterOwnerClosure -eq 'Level4V2CanonicalReconstruction') 'CU6-2 next program mismatch.'
Require ($m.legacySchemaMutation -eq 'None' -and $m.legacyRuntimeMutation -eq 'None' -and $m.legacyBackendMutation -eq 'None' -and $m.compositionContractMutation -eq 'None') 'CU6-2 changed a frozen legacy boundary.'

$required = @(
'187_Spiral7PerformanceExperiment\187_Spiral7PerformanceExperiment.vcxproj',
'187_Spiral7PerformanceExperiment\Spiral7PerformanceExperiment.h',
'187_Spiral7PerformanceExperiment\Spiral7PerformanceExperiment.cpp',
'187_Spiral7PerformanceExperiment\Spiral7RealGpuMeasurement.cpp',
'188_Spiral7PerformanceTests\188_Spiral7PerformanceTests.vcxproj',
'188_Spiral7PerformanceTests\main.cpp',
'tests\Run-Spiral7CU6.ps1',
'tests\tools\Register-Spiral7CU6Projects.ps1',
'tests\tools\Verify-Spiral7CU6.ps1',
'run_sge4_5_spiral7_cu6_prepare.bat',
'run_sge4_5_spiral7_cu6_measurement_decision_evidence.bat',
'docs\spiral7\CU6_REAL_GPU_MEASUREMENT_MANIFEST_V1.json',
'docs\spiral7\CU6_REAL_GPU_MEASUREMENT.md',
'docs\spiral7\CU6_DECISION_EVIDENCE.md',
'docs\spiral7\CU6_EVIDENCE_LEDGER.md',
'docs\spiral7\CU6_FIX02_TIMESTAMP_RESOLUTION.md',
'docs\spiral7\CU6_2_PAIRED_DECISION_REFINEMENT.md',
'docs\spiral7\CU6_CHANGESET.md',
'docs\spiral7\APPLY_CU6_JA.md',
'docs\spiral7\CU6_PACKAGE_MANIFEST.txt')
foreach($relative in $required){Require (Test-Path -LiteralPath (Join-Path $root $relative) -PathType Leaf) "Missing Spiral 7 CU6-2 file: $relative"}

$projects = @(
    [pscustomobject]@{Path='187_Spiral7PerformanceExperiment\187_Spiral7PerformanceExperiment.vcxproj';Guid='{000000BB-0000-5000-8000-0000000000BB}'},
    [pscustomobject]@{Path='188_Spiral7PerformanceTests\188_Spiral7PerformanceTests.vcxproj';Guid='{000000BC-0000-5000-8000-0000000000BC}'})
$solution = Get-Content -Raw -LiteralPath (Join-Path $root 'SemanticGpuEngine4-5.sln') -Encoding UTF8
foreach($project in $projects){
    [xml]$xml=Get-Content -Raw -LiteralPath (Join-Path $root $project.Path) -Encoding UTF8
    $guid=[string](($xml.Project.PropertyGroup|Where-Object{$_.Label -eq 'Globals'}|Select-Object -First 1).ProjectGuid)
    Require ($guid.ToUpperInvariant() -eq $project.Guid.ToUpperInvariant()) "Project GUID mismatch: $($project.Path)"
    Require ($solution -match [regex]::Escape($project.Path)) "Solution registration missing: $($project.Path)"
}

$experimentProject = Get-Content -Raw -LiteralPath (Join-Path $root '187_Spiral7PerformanceExperiment\187_Spiral7PerformanceExperiment.vcxproj') -Encoding UTF8
foreach($dependency in @('172_SparseTemporalDeltaSemantic','178_SparseTemporalDeltaCandidateFamily','179_SparseTemporalDeltaCandidateFamilyPlanner','180_SparseTemporalDeltaCandidateFamilyVerifier')){Require ($experimentProject -match [regex]::Escape($dependency)) "CU6-2 experiment dependency missing: $dependency"}
foreach($forbidden in @('22_CompositionRuntime','14_D3D12Backend','185_Spiral7DeltaFamilyRuntime')){Require ($experimentProject -notmatch [regex]::Escape($forbidden)) "CU6-2 experiment has forbidden dependency: $forbidden"}

$header = Get-Content -Raw -LiteralPath (Join-Path $root '187_Spiral7PerformanceExperiment\Spiral7PerformanceExperiment.h') -Encoding UTF8
foreach($token in @('MeasurementEvidenceSchemaVersionV3','MeasurementPassV2','HighTransitionRefinement','CaseDecisionClassV2','ZeroDispatchEquivalent','RefinementCaseCountV2','descriptiveMedianWinnerSet','pairedAuthoritySet','AnalyzeCombinedMeasurementEvidenceV2')){Require ($header -match [regex]::Escape($token)) "CU6-2 header token missing: $token"}
$core = Get-Content -Raw -LiteralPath (Join-Path $root '187_Spiral7PerformanceExperiment\Spiral7PerformanceExperiment.cpp') -Encoding UTF8
foreach($token in @('S7M3','1024u, 1536u, 2048u, 3072u, 4096u','MeasurementCaseScheduleV2','ClassifyPairedAuthority','CandidateBeats(decision','ZeroDispatchEquivalent','StableEquivalentSet','descriptiveMedianWinnerSet','pairedAuthoritySet','UnresolvedCaseIncludedInCrossover = false','AnalyzeCombinedMeasurementEvidenceV2','Refinement overrides duplicate T=1024 and T=4096 coordinates')){Require ($core -match [regex]::Escape($token)) "CU6-2 core token missing: $token"}
$gpu = Get-Content -Raw -LiteralPath (Join-Path $root '187_Spiral7PerformanceExperiment\Spiral7RealGpuMeasurement.cpp') -Encoding UTF8
foreach($token in @('DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE','D3D12_QUERY_HEAP_TYPE_TIMESTAMP','ExecuteIndirect','ValidateCaseOnGpu','MeasureOrder','NextAdaptiveIterationCount','MeasurementCaseCountPerRunV2(config.measurementPass)','MeasurementCaseScheduleV2(config.measurementPass, run)','[MEASURED] pass=')){Require ($gpu -match [regex]::Escape($token)) "CU6-2 real-GPU token missing: $token"}
$main = Get-Content -Raw -LiteralPath (Join-Path $root '188_Spiral7PerformanceTests\main.cpp') -Encoding UTF8
foreach($token in @('--pass','canonical','refinement','--combine','--base-evidence','--refinement-evidence','BuildCombinedDecisionCsvV2','BuildCombinedDecisionReportV2','Runtime Candidate-policy decision: None')){Require ($main -match [regex]::Escape($token)) "CU6-2 CLI token missing: $token"}
$runner = Get-Content -Raw -LiteralPath (Join-Path $root 'tests\Run-Spiral7CU6.ps1') -Encoding UTF8
foreach($token in @('Release canonical 4x5x8 measurement','Release high-Transition 4x5x5 refinement measurement','Canonical plus refinement paired Decision Map','combined_paired_decision_cases_v3.csv','SGE4-5 SPIRAL 7 COMPLETION UNIT 6-2 PASSED')){Require ($runner -match [regex]::Escape($token)) "CU6-2 runner token missing: $token"}

$progress = Get-Content -Raw -LiteralPath (Join-Path $root 'docs\spiral7\SPIRAL7_PROGRESS.md') -Encoding UTF8
Require ($progress -match '903e00be0f7e39ffd1758004c117fbc0233b0164') 'CU6-1 base commit is not recorded in progress.'
$proof = Get-Content -Raw -LiteralPath (Join-Path $root 'docs\spiral7\PROOF_LEDGER_V1.md') -Encoding UTF8
foreach($token in @('S7-I17','S7-I18','S7-I19','S7-I20','S7-I21')){Require ($proof -match [regex]::Escape($token)) "CU6-2 proof ledger token missing: $token"}

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
$cu2Manifest = Get-Content -Raw -LiteralPath (Join-Path $root 'docs\spiral7\CU2_ARCHITECTURE_MANIFEST_V1.json') -Encoding UTF8 | ConvertFrom-Json
foreach($property in $cu2Manifest.legacyFileSha256.PSObject.Properties){
    $path=Join-Path $root ([string]$property.Name)
    Require (Test-Path -LiteralPath $path -PathType Leaf) "Legacy boundary file missing: $($property.Name)"
    $actual=(Get-FileHash -Algorithm SHA256 -LiteralPath $path).Hash.ToLowerInvariant()
    Require ($actual -eq ([string]$property.Value).ToLowerInvariant()) "Legacy boundary file changed: $($property.Name)"
}
Write-Host 'SGE4-5 Spiral 7 CU6-2 static paired Decision and high-Transition refinement boundaries passed.'
Write-Host 'Median ranking is descriptive; paired authority is required; T=0 and unresolved cases cannot create crossovers.'
