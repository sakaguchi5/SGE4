param(
    [ValidateSet('Auto','Snapshot','Regression')]
    [string]$Mode = 'Auto'
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
function Require([bool]$Condition,[string]$Message){if(-not $Condition){throw $Message}}

if ($Mode -eq 'Auto') {
    $cu3Manifest = Join-Path $root 'docs\spiral7\CU3_AUTHORITY_MANIFEST_V1.json'
    $Mode = if (Test-Path -LiteralPath $cu3Manifest -PathType Leaf) { 'Regression' } else { 'Snapshot' }
}

$manifestPath = Join-Path $root 'docs\spiral7\CU2_ARCHITECTURE_MANIFEST_V1.json'
Require (Test-Path -LiteralPath $manifestPath -PathType Leaf) 'Spiral 7 CU2 architecture manifest missing.'
$m = Get-Content -Raw -LiteralPath $manifestPath -Encoding UTF8 | ConvertFrom-Json
Require ($m.schema -eq 'SGE4-5.Spiral7.CU2ArchitectureManifest.V1') 'CU2 manifest schema mismatch.'
Require ($m.completionUnit -eq 'CU2') 'CU2 manifest completion unit mismatch.'
Require ($m.ownerBaselineCommit -eq 'd0bb0d406fca8beabed2331daff870ea414dd87d') 'CU2 Owner baseline mismatch.'
Require ($m.cu1ContractManifestSha256 -eq '429ff4174bbf366c9aab88cb1547f9cc8300e52361cd816d5fe36e14fd8c53d2') 'CU1 Contract Manifest identity mismatch.'
Require ($m.cu1CompletionSpecSha256 -eq 'f3dd4a125dd2da924599e67cf9247888987fc30784100f33cf11a909ee6949f9') 'CU1 Completion Spec identity mismatch.'
Require ($m.extensionStrategy -eq 'VersionedSparseTemporalDeltaExtensionV1') 'CU2 extension strategy mismatch.'
Require ($m.legacySchemaMutation -eq 'None') 'CU2 must not mutate Schema 17.'
Require ($m.legacyRuntimeMutation -eq 'None') 'CU2 must not mutate Runtime 17.'
Require ($m.legacyBackendMutation -eq 'None') 'CU2 must not mutate the canonical Backend.'
Require ($m.compositionContractMutation -eq 'None') 'CU2 must not mutate the Composition Contract.'
Require ($m.spiral4IndirectExtension -eq 'ReusedAndIdentityBound') 'Spiral 4 identity boundary mismatch.'
Require ($m.spiral5TemporalExtension -eq 'ReusedAndIdentityBound') 'Spiral 5 identity boundary mismatch.'
Require ($m.spiral6SparseExtension -eq 'ReusedAndIdentityBound') 'Spiral 6 identity boundary mismatch.'
Require ($m.semantic -eq 'VersionedSparseDeltaPointTransformTimelineV1') 'CU2 Semantic mismatch.'
Require ($m.candidate -eq 'CompactDeltaIndexHistoryReuse') 'CU2 Candidate mismatch.'
Require ($m.representationRole -eq 'CanonicalSortedIndexActionList') 'CU2 representation role mismatch.'
Require ([int]$m.universeCount -eq 4096) 'CU2 universe count mismatch.'
Require ([int]$m.timelineLength -eq 128) 'CU2 timeline length mismatch.'
Require ([int]$m.historyDepth -eq 1) 'CU2 history depth mismatch.'
Require ([int]$m.threadsPerGroup -eq 64) 'CU2 thread-group size mismatch.'
Require ([int]$m.recordStrideBytes -eq 8) 'CU2 Delta record stride mismatch.'
Require ([int]$m.fixedCapacityRecordCount -eq 4096) 'CU2 Delta record capacity mismatch.'
Require ([int]$m.fixedCapacityBytes -eq 32768) 'CU2 Delta record byte capacity mismatch.'
Require ([int]$m.artifactByteSize -eq 33260) 'CU2 artifact byte size mismatch.'
Require ($m.dispatchRule -eq 'CeilTransitionCountOver64ZeroWhenTransitionEmpty') 'CU2 Dispatch rule mismatch.'
Require ($m.dispatchIssuance -eq 'Spiral4DispatchCommandSignatureExecuteIndirect') 'CU2 Dispatch issuance mismatch.'
Require ($m.outputInitialization -eq 'CopyPreviousHistoryBytes') 'CU2 history initialization mismatch.'
Require ($m.legalWriteSet -eq 'ExactlyTransitionSetT_t') 'CU2 legal write-set mismatch.'
Require ($m.retainWritePolicy -eq 'Forbidden') 'CU2 Retain write policy mismatch.'
Require ([int]$m.architectureCaseCount -eq 14) 'CU2 architecture case count mismatch.'
Require ($m.authorityQualification -eq 'DeferredToCU3') 'CU2 must defer independent authority to CU3.'
Require ($m.candidateFamilyQualification -eq 'DeferredToCU4') 'CU2 must defer Candidate family to CU4.'
Require ($m.recoveryQualification -eq 'DeferredToCU5') 'CU2 must defer Recovery to CU5.'
Require ($m.runtimeTransitionPolicyDecision -eq 'None') 'CU2 Runtime transition policy must remain None.'

$cu1Contract = Join-Path $root 'docs\spiral7\CONTRACT_MANIFEST_V1.json'
$cu1Spec = Join-Path $root 'docs\spiral7\SGE4-5_Spiral7_Completion_Spec_v0.1.md'
Require ((Get-FileHash -Algorithm SHA256 -LiteralPath $cu1Contract).Hash.ToLowerInvariant() -eq $m.cu1ContractManifestSha256) 'Applied CU1 Contract Manifest changed.'
Require ((Get-FileHash -Algorithm SHA256 -LiteralPath $cu1Spec).Hash.ToLowerInvariant() -eq $m.cu1CompletionSpecSha256) 'Applied CU1 Completion Spec changed.'

$required = @(
'172_SparseTemporalDeltaSemantic\172_SparseTemporalDeltaSemantic.vcxproj','172_SparseTemporalDeltaSemantic\SparseTemporalDeltaSemantic.h','172_SparseTemporalDeltaSemantic\SparseTemporalDeltaSemantic.cpp',
'173_Spiral7DeltaContracts\173_Spiral7DeltaContracts.vcxproj','173_Spiral7DeltaContracts\Spiral7DeltaContracts.h','173_Spiral7DeltaContracts\Spiral7DeltaContracts.cpp',
'174_Spiral7DeltaExecution\174_Spiral7DeltaExecution.vcxproj','174_Spiral7DeltaExecution\Spiral7DeltaExecution.h','174_Spiral7DeltaExecution\Spiral7DeltaExecution.cpp',
'182_Spiral7DeltaArchitectureTests\182_Spiral7DeltaArchitectureTests.vcxproj','182_Spiral7DeltaArchitectureTests\main.cpp',
'tests\Spiral7Common.ps1','tests\Run-Spiral7CU2.ps1','tests\tools\Register-Spiral7CU2Projects.ps1','tests\tools\Verify-Spiral7CU2.ps1',
'run_sge4_5_spiral7_cu2_prepare.bat','run_sge4_5_spiral7_cu2_compact_delta_history.bat',
'docs\spiral7\CU2_ARCHITECTURE_MANIFEST_V1.json','docs\spiral7\CU2_BASELINE_SUFFICIENCY_AND_GAP.md','docs\spiral7\CU2_COMPACT_DELTA_HISTORY_ARCHITECTURE.md','docs\spiral7\CU2_CHANGESET.md','docs\spiral7\CU2_EVIDENCE_LEDGER.md')
foreach($relative in $required){Require (Test-Path -LiteralPath (Join-Path $root $relative) -PathType Leaf) "Missing Spiral 7 CU2 file: $relative"}

$projects = @(
    [pscustomobject]@{Path='172_SparseTemporalDeltaSemantic\172_SparseTemporalDeltaSemantic.vcxproj';Guid='{000000AC-0000-5000-8000-0000000000AC}'},
    [pscustomobject]@{Path='173_Spiral7DeltaContracts\173_Spiral7DeltaContracts.vcxproj';Guid='{000000AD-0000-5000-8000-0000000000AD}'},
    [pscustomobject]@{Path='174_Spiral7DeltaExecution\174_Spiral7DeltaExecution.vcxproj';Guid='{000000AE-0000-5000-8000-0000000000AE}'},
    [pscustomobject]@{Path='182_Spiral7DeltaArchitectureTests\182_Spiral7DeltaArchitectureTests.vcxproj';Guid='{000000B6-0000-5000-8000-0000000000B6}'}
)
$solution = Get-Content -Raw -LiteralPath (Join-Path $root 'SemanticGpuEngine4-5.sln') -Encoding UTF8
foreach($project in $projects){
    [xml]$xml = Get-Content -Raw -LiteralPath (Join-Path $root $project.Path) -Encoding UTF8
    $guid=[string](($xml.Project.PropertyGroup|Where-Object{$_.Label -eq 'Globals'}|Select-Object -First 1).ProjectGuid)
    Require ($guid.ToUpperInvariant() -eq $project.Guid.ToUpperInvariant()) "Project GUID mismatch: $($project.Path)"
    Require ($solution -match [regex]::Escape($project.Path)) "Solution registration missing: $($project.Path)"
}

foreach ($property in $m.legacyFileSha256.PSObject.Properties) {
    $path = Join-Path $root ([string]$property.Name)
    Require (Test-Path -LiteralPath $path -PathType Leaf) "Legacy boundary file missing: $($property.Name)"
    $actual = (Get-FileHash -Algorithm SHA256 -LiteralPath $path).Hash.ToLowerInvariant()
    Require ($actual -eq ([string]$property.Value).ToLowerInvariant()) "Legacy boundary file changed: $($property.Name)"
}

$composition = Get-Content -Raw -LiteralPath (Join-Path $root '17_CompositionContract\CompositionContract.h') -Encoding UTF8
Require ($composition -notmatch '\bVersionedSparseTemporalDelta') 'CU2 must not add Sparse Temporal Delta policy to the Canonical Composition Contract.'
Require ($composition -notmatch '\bPartialHistoryValidity') 'CU2 must not add partial history policy to the Canonical Composition Contract.'

$semanticHeader = Get-Content -Raw -LiteralPath (Join-Path $root '172_SparseTemporalDeltaSemantic\SparseTemporalDeltaSemantic.h') -Encoding UTF8
foreach($token in @('SparseDeltaInvocationV1','ModifiedSurvivorSet','ActivationSet','DeactivationSet','UpdateSet','RetainSet','TransitionSet','DeltaIndexActionRecordV1','BuildPointCorpusForInvocationV1')){
    Require ($semanticHeader -match [regex]::Escape($token)) "CU2 Delta Semantic token missing: $token"
}
$semanticCpp = Get-Content -Raw -LiteralPath (Join-Path $root '172_SparseTemporalDeltaSemantic\SparseTemporalDeltaSemantic.cpp') -Encoding UTF8
foreach($token in @('Modified Survivor Set must be a subset','forceFullActiveRebuild','set_difference','set_intersection','DeltaTransitionRecordsIdentityV1','InvalidGenerationV1','PointRecordV1{0.0f, 0.0f, 0.0f, 0.0f}')){
    Require ($semanticCpp -match [regex]::Escape($token)) "CU2 Delta Semantic implementation token missing: $token"
}

$contractCpp = Get-Content -Raw -LiteralPath (Join-Path $root '173_Spiral7DeltaContracts\Spiral7DeltaContracts.cpp') -Encoding UTF8
foreach($token in @('VersionedSparseTemporalDeltaExtensionV1','CompactDeltaIndexHistoryReuse','CanonicalSortedIndexActionList','CopyPreviousHistoryThenWriteExactlyTransitionSet','Spiral4IndirectExtensionIdentityV1','Spiral5TemporalExtensionIdentityV1','Spiral6SparseExtensionIdentityV1')){
    Require ($contractCpp -match [regex]::Escape($token)) "CU2 Delta contract token missing: $token"
}
$executionCpp = Get-Content -Raw -LiteralPath (Join-Path $root '174_Spiral7DeltaExecution\Spiral7DeltaExecution.cpp') -Encoding UTF8
foreach($token in @('DeltaRecords','transitionCount','action == 2','CopyBufferRegion','retainedHistoryByteStable','EnumWarpAdapter','ExecuteIndirect','D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH','TransitionAudit','exactTransitionAudit','SetComputeRootUnorderedAccessView(5')){
    Require ($executionCpp -match [regex]::Escape($token)) "CU2 Delta execution token missing: $token"
}
$testCpp = Get-Content -Raw -LiteralPath (Join-Path $root '182_Spiral7DeltaArchitectureTests\main.cpp') -Encoding UTF8
foreach($token in @('Modified Survivor outside','Cross-invocation','HoldPrefix64','BalancedChurn','EmptyReset','FullInvalidation256','Runtime transition-policy decision: None')){
    Require ($testCpp -match [regex]::Escape($token)) "CU2 Delta test token missing: $token"
}

$executionProject = Get-Content -Raw -LiteralPath (Join-Path $root '174_Spiral7DeltaExecution\174_Spiral7DeltaExecution.vcxproj') -Encoding UTF8
if ($Mode -eq 'Snapshot') {
    foreach($forbidden in @('175_SparseTemporalDeltaLoweringCandidate','176_SparseTemporalDeltaLoweringPlanner','177_SparseTemporalDeltaLoweringVerifier','22_CompositionRuntime','14_D3D12Backend')){
        Require ($executionProject -notmatch [regex]::Escape($forbidden)) "CU2 Snapshot execution has forbidden dependency: $forbidden"
    }
} elseif ($Mode -eq 'Regression') {
    foreach($forbidden in @('175_SparseTemporalDeltaLoweringCandidate','176_SparseTemporalDeltaLoweringPlanner','22_CompositionRuntime','14_D3D12Backend')){
        Require ($executionProject -notmatch [regex]::Escape($forbidden)) "CU2 Regression execution has forbidden direct dependency: $forbidden"
    }
} else { throw "Unexpected Spiral 7 CU2 verification mode: $Mode" }

Write-Host "SGE4-5 Spiral 7 CU2 static architecture boundaries passed. Mode: $Mode"
Write-Host 'Independent Spiral 4/5/6 sidecars are insufficient for partial-history Transition authority.'
Write-Host 'Legacy Schema 17 / Runtime 17 / Backend / Composition mutation: None.'
