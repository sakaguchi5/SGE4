$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
function Require([bool]$Condition,[string]$Message){if(-not $Condition){throw $Message}}

$manifestPath = Join-Path $root 'docs\spiral7\CU6_REAL_GPU_MEASUREMENT_MANIFEST_V1.json'
Require (Test-Path -LiteralPath $manifestPath -PathType Leaf) 'Spiral 7 CU6 manifest missing.'
$m = Get-Content -Raw -LiteralPath $manifestPath -Encoding UTF8 | ConvertFrom-Json
Require ($m.schema -eq 'SGE4-5.Spiral7.CU6RealGpuMeasurementManifest.V1') 'CU6 manifest schema mismatch.'
Require ($m.completionUnit -eq 'CU6') 'CU6 completion unit mismatch.'
Require ($m.cu5AcceptedCommit -eq 'c9f0b5a62e2a7f3c9e0355cdaa1c683819c6dcfa') 'CU6 accepted CU5 commit mismatch.'
Require ($m.cu5OwnerRunStatus -eq 'Passed') 'CU5 Owner run status must be Passed.'
Require ($m.cu5ArchitectureEvidenceSha256 -eq '1F1D09B4E52DBC35E961E0D3751B32292B2C30EEDCFA473800C5BB8E8CB5AB73') 'CU5 Architecture evidence binding mismatch.'
Require ($m.cu5ControlledRecoveryEvidenceSha256 -eq '7F0247B193F9BC20F4EDA5FEED590C1DA5722EF36B133640292B1ED616CFF62B') 'CU5 Controlled evidence binding mismatch.'
Require ($m.cu5FreshEvidenceSha256 -eq '091EAEC0D27287FE897F6813043FD75C090D5DA17054461DF314F99B2A6F5A92') 'CU5 Fresh evidence binding mismatch.'
Require (($m.candidateOrder -join ',') -eq 'FullActiveDenseRecompute,CompactDeltaIndexHistoryReuse,AffectedBlockDeltaHistoryReuse') 'CU6 Candidate order mismatch.'
Require (($m.measurementActiveCounts -join ',') -eq '64,256,1024,2048,4096') 'CU6 Active count grid mismatch.'
Require (($m.measurementTransitionCounts -join ',') -eq '0,1,8,32,64,256,1024,4096') 'CU6 Transition count grid mismatch.'
Require (($m.patterns -join ',') -eq 'PrefixControl,UniformStride,BlockClusteredPermutation,HashScatterPermutation') 'CU6 Pattern grid mismatch.'
Require ([int]$m.canonicalCaseCountPerRun -eq 160) 'CU6 canonical case count mismatch.'
Require ([int]$m.balancedCandidateOrderCount -eq 6) 'CU6 balanced Candidate order count mismatch.'
Require ($m.primaryMetric -eq 'GpuCandidateBatchNanosecondsPerEffectiveIteration') 'CU6 primary metric mismatch.'
Require ($m.binaryEvidenceSchema -eq 'S7M2' -and [int]$m.binaryEvidenceSchemaVersion -eq 2) 'CU6 binary evidence schema mismatch.'
Require ($m.timestampResolutionPolicy.zeroDispatchZeroTickInterval -eq 'ResolutionCensoredOneTickConservativeUpperBound') 'CU6 zero-dispatch timestamp policy mismatch.'
Require ([bool]$m.timestampResolutionPolicy.censoringAllowedOnlyWhenExpectedDispatchGroupsXIsZero) 'CU6 timestamp censoring boundary mismatch.'
Require ($m.timestampResolutionPolicy.nonzeroDispatchZeroTickInterval -eq 'RetryWholeBalancedOrderWithDoubledIterations') 'CU6 adaptive timestamp retry policy mismatch.'
Require ([int]$m.timestampResolutionPolicy.maximumAdaptiveIterations -eq 65536) 'CU6 adaptive iteration ceiling mismatch.'
Require ($m.runtimeCandidatePolicyAuthorization -eq 'None') 'CU6 Runtime Candidate policy must remain None.'
Require ($m.universalWinnerClaim -eq 'Forbidden') 'CU6 must forbid a universal winner claim.'
Require ($m.spiral7Closure -eq 'OwnerRequired') 'CU6 Spiral closure must remain Owner-gated.'
Require ($m.nextProgramAfterOwnerClosure -eq 'Level4V2CanonicalReconstruction') 'CU6 next program mismatch.'
Require ($m.legacySchemaMutation -eq 'None' -and $m.legacyRuntimeMutation -eq 'None' -and $m.legacyBackendMutation -eq 'None' -and $m.compositionContractMutation -eq 'None') 'CU6 changed a frozen legacy boundary.'

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
'docs\spiral7\CU6_CHANGESET.md',
'docs\spiral7\APPLY_CU6_JA.md',
'docs\spiral7\CU6_PACKAGE_MANIFEST.txt')
foreach($relative in $required){Require (Test-Path -LiteralPath (Join-Path $root $relative) -PathType Leaf) "Missing Spiral 7 CU6 file: $relative"}

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
foreach($dependency in @('172_SparseTemporalDeltaSemantic','178_SparseTemporalDeltaCandidateFamily','179_SparseTemporalDeltaCandidateFamilyPlanner','180_SparseTemporalDeltaCandidateFamilyVerifier')){Require ($experimentProject -match [regex]::Escape($dependency)) "CU6 experiment dependency missing: $dependency"}
foreach($forbidden in @('22_CompositionRuntime','14_D3D12Backend','185_Spiral7DeltaFamilyRuntime')){Require ($experimentProject -notmatch [regex]::Escape($forbidden)) "CU6 experiment has forbidden dependency: $forbidden"}

$header = Get-Content -Raw -LiteralPath (Join-Path $root '187_Spiral7PerformanceExperiment\Spiral7PerformanceExperiment.h') -Encoding UTF8
foreach($token in @('CanonicalCaseCountV1','TransitionShapeV1','DirtyOnly','ReplaceAndClear','MeasurementEvidenceV1','MeasurementEvidenceSchemaVersionV2','MaximumAdaptiveIterationsV2','timestampTickCount','effectiveIterations','timestampResolutionCensored','DecisionAnalysisV1','TransitionSurfaceEventV1','ActiveSurfaceEventV1')){Require ($header -match [regex]::Escape($token)) "CU6 header token missing: $token"}
$core = Get-Content -Raw -LiteralPath (Join-Path $root '187_Spiral7PerformanceExperiment\Spiral7PerformanceExperiment.cpp') -Encoding UTF8
foreach($token in @('64u, 256u, 1024u, 2048u, 4096u','0u, 1u, 8u, 32u, 64u, 256u, 1024u, 4096u','CanonicalBalancedOrdersV1','S7M2','Measurement evidence digest mismatch','Only a zero-dispatch Candidate may be resolution-censored','winnerChangesAcrossTransitionCount','winnerChangesAcrossActiveCount','sameActiveAndTransitionWinnerDependsOnPattern','RuntimeCandidatePolicyAuthorization = None','UniversalWinnerClaim = Forbidden')){Require ($core -match [regex]::Escape($token)) "CU6 core token missing: $token"}
$gpu = Get-Content -Raw -LiteralPath (Join-Path $root '187_Spiral7PerformanceExperiment\Spiral7RealGpuMeasurement.cpp') -Encoding UTF8
foreach($token in @('DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE','D3D12_QUERY_HEAP_TYPE_TIMESTAMP','GetTimestampFrequency','ExecuteIndirect','WRITE_AUDIT 0','WRITE_AUDIT 1','ValidateCaseOnGpu','MeasureOrder','NextAdaptiveIterationCount','timestampResolutionCensored','Only a zero-dispatch Candidate may be timestamp-resolution censored','maximumControlRatio','No accepted measurement block remained','[MEASURED]')){Require ($gpu -match [regex]::Escape($token)) "CU6 real-GPU token missing: $token"}
$main = Get-Content -Raw -LiteralPath (Join-Path $root '188_Spiral7PerformanceTests\main.cpp') -Encoding UTF8
foreach($token in @('--self-test','--measure','--verify','--evidence','--csv','--report','Runtime Candidate-policy decision: None','Spiral 7 closure: Owner required')){Require ($main -match [regex]::Escape($token)) "CU6 CLI token missing: $token"}

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
Write-Host 'SGE4-5 Spiral 7 CU6 static Real-GPU Measurement and Decision boundaries passed.'
Write-Host 'CU6 measures an adapter-specific local winner surface; Runtime policy remains None and closure remains Owner-gated.'
