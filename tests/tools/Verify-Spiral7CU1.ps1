param(
    [ValidateSet('Auto', 'Snapshot', 'Regression')]
    [string]$Mode = 'Auto'
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)

if ($Mode -eq 'Auto') {
    $cu2Manifest = Join-Path $root 'docs\spiral7\CU2_ARCHITECTURE_MANIFEST_V1.json'
    $Mode = if (Test-Path -LiteralPath $cu2Manifest -PathType Leaf) { 'Regression' } else { 'Snapshot' }
}

function Require([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}

$manifestPath = Join-Path $root 'docs\spiral7\CONTRACT_MANIFEST_V1.json'
Require (Test-Path -LiteralPath $manifestPath -PathType Leaf) 'Spiral 7 contract manifest missing.'
$m = Get-Content -Raw -LiteralPath $manifestPath -Encoding UTF8 | ConvertFrom-Json

Require ($m.schema -eq 'SGE4-5.Spiral7.ContractManifest.V1') 'Spiral 7 contract schema mismatch.'
Require ($m.completionUnit -eq 'CU1') 'Spiral 7 completion unit mismatch.'
Require ($m.baselineCommit -eq 'd0bb0d406fca8beabed2331daff870ea414dd87d') 'Spiral 7 baseline mismatch.'
Require ($m.ownerSelection -eq 'VersionedSparseDeltaFlowVerifiedIncrementalHistoryLowering') 'Spiral 7 Owner selection mismatch.'
Require ([int]$m.targetAbiBaseline.d3d12Schema -eq 17) 'Spiral 7 baseline D3D12 Schema mismatch.'
Require ([int]$m.targetAbiBaseline.runtime -eq 17) 'Spiral 7 baseline Runtime mismatch.'
Require ($m.targetAbiBaseline.spiral4IndirectExtension -eq 'VersionedSidecarIndirectExtensionV1') 'Spiral 7 indirect baseline mismatch.'
Require ($m.targetAbiBaseline.spiral5TemporalExtension -eq 'VersionedSidecarTemporalExtensionV1') 'Spiral 7 temporal baseline mismatch.'
Require ($m.targetAbiBaseline.spiral6SparseExtension -eq 'VersionedSidecarSparseExtensionV1') 'Spiral 7 sparse baseline mismatch.'
Require ($m.cu1AbiMutation -eq 'None') 'Spiral 7 CU1 must not mutate ABI.'
Require ($m.abiExtensionPolicy -eq 'VersionedSparseTemporalDeltaExtensionOnlyAfterCrossCapabilityInsufficiencyProof') 'Spiral 7 extension policy mismatch.'

Require ([bool]$m.baselineInventory.spiral4VerifiedIndirectDispatch) 'Spiral 4 indirect baseline missing.'
Require ([bool]$m.baselineInventory.spiral5VerifiedTemporalHistory) 'Spiral 5 temporal baseline missing.'
Require ([bool]$m.baselineInventory.spiral6VerifiedExactSparseWorkSet) 'Spiral 6 sparse baseline missing.'
Require (-not [bool]$m.baselineInventory.perInvocationExactActiveSetTimeline) 'CU1 baseline must not claim a Sparse timeline.'
Require (-not [bool]$m.baselineInventory.partialHistoryValidityArtifact) 'CU1 baseline must not claim partial history validity.'
Require (-not [bool]$m.baselineInventory.sparseDeltaTransitionContract) 'CU1 baseline must not claim Sparse Delta transition authority.'
Require (-not [bool]$m.baselineInventory.activationDeactivationAuthority) 'CU1 baseline must not claim activation/deactivation authority.'
Require (-not [bool]$m.baselineInventory.exactUpdateClearWriteSetAuthority) 'CU1 baseline must not claim exact update/clear authority.'
Require ($m.baselineInventory.runtimeTransitionPolicyDecision -eq 'Forbidden') 'Runtime transition policy boundary mismatch.'

Require ($m.fixedMeaning.semantic -eq 'VersionedSparseDeltaPointTransformTimelineV1') 'Spiral 7 Semantic mismatch.'
Require ($m.fixedMeaning.representationPath -eq 'DirectPgaThroughConsumerV1') 'Spiral 7 representation path mismatch.'
Require ([int]$m.fixedMeaning.boneCount -eq 8) 'Spiral 7 bone count mismatch.'
Require ([int]$m.fixedMeaning.workUniverseCount -eq 4096) 'Spiral 7 universe count mismatch.'
Require ([int]$m.fixedMeaning.outputCapacity -eq 4096) 'Spiral 7 output capacity mismatch.'
Require ([int]$m.fixedMeaning.threadsPerGroup -eq 64) 'Spiral 7 thread-group size mismatch.'
Require ([int]$m.fixedMeaning.timelineLength -eq 128) 'Spiral 7 timeline length mismatch.'
Require ([int]$m.fixedMeaning.historyDepth -eq 1) 'Spiral 7 history depth mismatch.'
Require ($m.fixedMeaning.initialPreviousActiveSet -eq 'Empty') 'Spiral 7 initial Active Set mismatch.'
Require ($m.fixedMeaning.initialHistoryState -eq 'Invalid') 'Spiral 7 initial history mismatch.'
Require ($m.fixedMeaning.inactiveOutputSentinel -eq 'Float4Zero') 'Spiral 7 inactive sentinel mismatch.'

Require ($m.externalInputs.activeSetSymbol -eq 'A_t') 'Spiral 7 Active Set symbol mismatch.'
Require ($m.externalInputs.modifiedSurvivorSetSymbol -eq 'M_t') 'Spiral 7 Modified Set symbol mismatch.'
Require ($m.externalInputs.activeSetAuthority -eq 'ExternalVerifiedInput') 'Spiral 7 Active authority mismatch.'
Require ($m.externalInputs.modifiedSurvivorAuthority -eq 'ExternalVerifiedInput') 'Spiral 7 Modified authority mismatch.'
Require ($m.externalInputs.modifiedSurvivorConstraint -eq 'M_tSubsetOfIntersectionA_tAndA_tMinus1') 'Spiral 7 Modified subset rule mismatch.'
Require (-not [bool]$m.externalInputs.runtimeMayChooseActiveMembership) 'Runtime must not choose Active membership.'
Require (-not [bool]$m.externalInputs.runtimeMayChooseModifiedMembership) 'Runtime must not choose Modified membership.'
Require (-not [bool]$m.externalInputs.runtimeMayInferGamePolicy) 'Runtime must not infer game policy.'

Require ($m.derivedTransitionSets.activationSet.symbol -eq 'N_t') 'Activation symbol mismatch.'
Require ($m.derivedTransitionSets.deactivationSet.symbol -eq 'R_t') 'Deactivation symbol mismatch.'
Require ($m.derivedTransitionSets.updateSet.symbol -eq 'W_t') 'Update symbol mismatch.'
Require ($m.derivedTransitionSets.retainSet.symbol -eq 'H_t') 'Retain symbol mismatch.'
Require ($m.derivedTransitionSets.transitionSet.symbol -eq 'T_t') 'Transition symbol mismatch.'
Require ($m.derivedTransitionSets.firstInvocationRule -eq 'W_0EqualsA_0H_0EmptyR_0Empty') 'First invocation rule mismatch.'
Require ($m.derivedTransitionSets.recoveryRule -eq 'AfterDeviceEpochChangeW_tEqualsA_tAndH_tEmpty') 'Recovery transition rule mismatch.'

$candidates = @($m.candidateKinds)
Require (($candidates -join ',') -eq 'FullActiveDenseRecompute,CompactDeltaIndexHistoryReuse,AffectedBlockDeltaHistoryReuse') 'Spiral 7 Candidate family mismatch.'
$patterns = @($m.activePatternKinds)
Require (($patterns -join ',') -eq 'PrefixControl,UniformStride,BlockClusteredPermutation,HashScatterPermutation') 'Spiral 7 Active pattern corpus mismatch.'
$transitions = @($m.transitionKinds)
Require (($transitions -join ',') -eq 'Hold,DirtyOnly,ActivateOnly,DeactivateOnly,BalancedChurn,PatternMigration,FullInvalidation,EmptyReset') 'Spiral 7 transition corpus mismatch.'

$activeCounts = @($m.qualificationActiveCounts | ForEach-Object { [int]$_ })
Require (($activeCounts -join ',') -eq '0,1,32,63,64,65,256,1024,2048,4095,4096') 'Spiral 7 qualification Active count corpus mismatch.'
$transitionCounts = @($m.qualificationTransitionCounts | ForEach-Object { [int]$_ })
Require (($transitionCounts -join ',') -eq '0,1,8,31,32,63,64,65,256,1024,4096') 'Spiral 7 qualification transition count corpus mismatch.'

Require ([int]$m.derivedRepresentations.FullActiveDenseRecompute.activeMaskBytes -eq 512) 'Full Active mask size mismatch.'
Require ([int]$m.derivedRepresentations.CompactDeltaIndexHistoryReuse.recordStrideBytes -eq 8) 'Compact Delta record stride mismatch.'
Require ([int]$m.derivedRepresentations.CompactDeltaIndexHistoryReuse.fixedCapacityBytes -eq 32768) 'Compact Delta capacity mismatch.'
Require ([int]$m.derivedRepresentations.AffectedBlockDeltaHistoryReuse.recordStrideBytes -eq 24) 'Affected Block record stride mismatch.'
Require ([int]$m.derivedRepresentations.AffectedBlockDeltaHistoryReuse.fixedCapacityBytes -eq 1536) 'Affected Block capacity mismatch.'
Require ($m.outputPolicy.retainWritePolicy -eq 'Forbidden') 'Retained-history write policy mismatch.'
Require ($m.outputPolicy.compactAndBlockCandidateLegalWriteSet -eq 'ExactlyTransitionSetT_t') 'Incremental legal write-set mismatch.'
Require ($m.recoveryPolicy.firstPostRecoveryInvocation -eq 'ForcedFullActiveRebuild') 'Post-Recovery rebuild rule mismatch.'
Require ($m.level4Policy -eq 'MinimalVersionedSparseTemporalDeltaExtensionOnlyIfRequired') 'Spiral 7 Level 4 policy mismatch.'
Require ($m.level5Policy -eq 'IncrementalHistoryMaterializationGranularityIsAVerifiedLoweringChoice') 'Spiral 7 Level 5 policy mismatch.'
Require ($m.runtimePolicyAuthorization -eq 'None') 'Spiral 7 Runtime policy must remain unauthorized.'
Require ($m.typeSafety.rawToVerified -eq 'CompileTimeUnavailable') 'Spiral 7 Raw-to-Verified boundary mismatch.'

$requiredFiles = @(
    'docs\spiral7\CONTRACT_MANIFEST_V1.json',
    'docs\spiral7\SGE4-5_Spiral7_Completion_Spec_v0.1.md',
    'docs\spiral7\NEXT_CAPABILITY_SELECTION_FROM_SPIRAL6.md',
    'docs\spiral7\NON_GOALS_V1.md',
    'docs\spiral7\PROJECT_BOUNDARIES_V1.md',
    'docs\spiral7\CORPUS_V1.md',
    'docs\spiral7\PROOF_LEDGER_V1.md',
    'docs\spiral7\STAGE_MAP_V1.md',
    'docs\spiral7\SPIRAL7_PROGRESS.md',
    'docs\spiral7\CU1_CHANGESET.md',
    'docs\spiral7\CU1_EVIDENCE_LEDGER.md',
    'docs\spiral7\README.md',
    'docs\spiral6\SPIRAL6_PROGRESS.md',
    'tests\Run-Spiral7CU1.ps1',
    'tests\tools\Verify-Spiral7CU1.ps1',
    'run_sge4_5_spiral7_cu1_research_contract.bat',
    'run_sge4_5_spiral7_cu1_prepare.bat'
)
foreach ($relative in $requiredFiles) {
    Require (Test-Path -LiteralPath (Join-Path $root $relative) -PathType Leaf) "Missing Spiral 7 CU1 file: $relative"
}

$spec = Get-Content -Raw -LiteralPath (Join-Path $root 'docs\spiral7\SGE4-5_Spiral7_Completion_Spec_v0.1.md') -Encoding UTF8
foreach ($token in @(
    'VersionedSparseDeltaPointTransformTimelineV1',
    'FullActiveDenseRecompute',
    'CompactDeltaIndexHistoryReuse',
    'AffectedBlockDeltaHistoryReuse',
    'VersionedSparseTemporalDeltaExtensionV1',
    'Runtime and Backend are forbidden to decide',
    'SGE4-5 SPIRAL 7 ARCHITECTURE COMPLETE',
    'SGE4-5 SPIRAL 7 EXPERIMENT COMPLETE',
    'SPIRAL 7 CLOSED',
    'RuntimePolicyAuthorization = None',
    'NextCapabilitySelection = DeferredByOwner',
    'Level 4 v2 Canonical reconstruction'
)) {
    Require ($spec -match [regex]::Escape($token)) "Spiral 7 completion specification token missing: $token"
}

$spiral6Progress = Get-Content -Raw -LiteralPath (Join-Path $root 'docs\spiral6\SPIRAL6_PROGRESS.md') -Encoding UTF8
Require ($spiral6Progress -match 'CU5: PASSED') 'Spiral 6 progress must record CU5 success.'
Require ($spiral6Progress -match 'CU6 V2: PASSED') 'Spiral 6 progress must record CU6 V2 success.'
Require ($spiral6Progress -match 'SGE4-5 SPIRAL 6 CLOSED') 'Spiral 6 progress must record Owner closure.'
Require ($spiral6Progress -match 'VersionedSparseDeltaFlowVerifiedIncrementalHistoryLowering') 'Spiral 6 progress must record the selected Spiral 7 capability.'
Require ($spiral6Progress -match 'RuntimePolicyAuthorization = None') 'Spiral 6 closure must not authorize Runtime policy.'

foreach ($baseline in @(
    '121_Spiral4Contracts\121_Spiral4Contracts.vcxproj',
    '134_TemporalStateSemantic\TemporalStateSemantic.h',
    '154_SparseWorkSetSemantic\SparseWorkSetSemantic.h',
    'docs\spiral4\CU6_EVIDENCE_LEDGER.md',
    'docs\spiral5\CU6_EVIDENCE_LEDGER.md',
    'docs\spiral6\CU6_EVIDENCE_LEDGER_V2.md'
)) {
    Require (Test-Path -LiteralPath (Join-Path $root $baseline) -PathType Leaf) "Required prior capability evidence missing: $baseline"
}

if ($Mode -eq 'Snapshot') {
    $solution = Get-Content -Raw -LiteralPath (Join-Path $root 'SemanticGpuEngine4-5.sln') -Encoding UTF8
    foreach ($futureProject in @(
        '172_SparseTemporalDeltaSemantic',
        '173_Spiral7DeltaContracts',
        '174_Spiral7DeltaExecution',
        '175_SparseTemporalDeltaLoweringCandidate',
        '176_SparseTemporalDeltaLoweringPlanner',
        '177_SparseTemporalDeltaLoweringVerifier',
        '178_SparseTemporalDeltaCandidateFamily',
        '179_SparseTemporalDeltaCandidateFamilyPlanner',
        '180_SparseTemporalDeltaCandidateFamilyVerifier',
        '181_Spiral7DeltaFamilyExecution'
    )) {
        Require ($solution -notmatch [regex]::Escape($futureProject)) "CU1 Snapshot must not pre-create future project: $futureProject"
    }

    $compositionContract = Get-Content -Raw -LiteralPath (Join-Path $root '17_CompositionContract\CompositionContract.h') -Encoding UTF8
    Require ($compositionContract -notmatch '\bVersionedSparseTemporalDelta') 'CU1 Snapshot must not already implement Sparse Temporal Delta.'
    Require ($compositionContract -notmatch '\bPartialHistoryValidity') 'CU1 Snapshot must not already implement partial history validity.'
}
elseif ($Mode -eq 'Regression') {
    $cu2Manifest = Join-Path $root 'docs\spiral7\CU2_ARCHITECTURE_MANIFEST_V1.json'
    Require (Test-Path -LiteralPath $cu2Manifest -PathType Leaf) 'CU1 Regression mode requires a later Completion Unit marker.'
}
else {
    throw "Unexpected Spiral 7 CU1 verification mode: $Mode"
}

Write-Host "SGE4-5 Spiral 7 CU1 research contract passed. Mode: $Mode"
if ($Mode -eq 'Snapshot') {
    Write-Host 'Snapshot-only Sparse x Temporal x Indirect absence boundaries passed.'
} else {
    Write-Host 'Later Completion Units are allowed; immutable CU1 contract regression passed.'
}
