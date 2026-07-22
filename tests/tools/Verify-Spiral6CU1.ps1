param(
    [ValidateSet('Auto', 'Snapshot', 'Regression')]
    [string]$Mode = 'Auto'
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)

if ($Mode -eq 'Auto') {
    $cu2Manifest = Join-Path $root 'docs\spiral6\CU2_ARCHITECTURE_MANIFEST_V1.json'
    $Mode = if (Test-Path -LiteralPath $cu2Manifest -PathType Leaf) { 'Regression' } else { 'Snapshot' }
}

function Require([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}

$manifestPath = Join-Path $root 'docs\spiral6\CONTRACT_MANIFEST_V1.json'
Require (Test-Path -LiteralPath $manifestPath -PathType Leaf) 'Spiral 6 contract manifest missing.'
$m = Get-Content -Raw -LiteralPath $manifestPath -Encoding UTF8 | ConvertFrom-Json

Require ($m.schema -eq 'SGE4-5.Spiral6.ContractManifest.V1') 'Spiral 6 contract schema mismatch.'
Require ($m.completionUnit -eq 'CU1') 'Spiral 6 completion unit mismatch.'
Require ($m.baselineCommit -eq '46554ab55e532c438c9c4214ff1df3e7cd68638e') 'Spiral 6 baseline mismatch.'
Require ($m.ownerSelection -eq 'ExactSparseWorkSetVerifiedLowering') 'Spiral 6 Owner selection mismatch.'
Require ([int]$m.targetAbiBaseline.d3d12Schema -eq 17) 'Spiral 6 baseline D3D12 Schema mismatch.'
Require ([int]$m.targetAbiBaseline.runtime -eq 17) 'Spiral 6 baseline Runtime mismatch.'
Require ($m.targetAbiBaseline.spiral4IndirectExtension -eq 'VersionedSidecarIndirectExtensionV1') 'Spiral 6 indirect baseline mismatch.'
Require ($m.targetAbiBaseline.spiral5TemporalExtension -eq 'VersionedSidecarTemporalExtensionV1') 'Spiral 6 temporal baseline mismatch.'
Require ($m.cu1AbiMutation -eq 'None') 'Spiral 6 CU1 must not mutate ABI.'
Require ($m.abiExtensionPolicy -eq 'VersionedSparseExtensionOnlyAfterBaselineInsufficiencyProof') 'Spiral 6 extension policy mismatch.'

Require ([bool]$m.baselineInventory.spiral4ActivePrefixCount) 'Spiral 4 prefix baseline missing.'
Require ([bool]$m.baselineInventory.spiral4VerifiedIndirectDispatch) 'Spiral 4 indirect baseline missing.'
Require ([bool]$m.baselineInventory.spiral5VerifiedTemporalHistory) 'Spiral 5 Temporal baseline missing.'
Require (-not [bool]$m.baselineInventory.arbitrarySparseWorkSetContract) 'CU1 baseline must not claim arbitrary Sparse Work-Set support.'
Require (-not [bool]$m.baselineInventory.sparseMembershipArtifact) 'CU1 baseline must not claim Sparse membership artifact support.'
Require (-not [bool]$m.baselineInventory.compactIndexListArtifact) 'CU1 baseline must not claim compact index-list support.'
Require (-not [bool]$m.baselineInventory.activeBlockListArtifact) 'CU1 baseline must not claim active block-list support.'
Require (-not [bool]$m.baselineInventory.generalSparseWriteSetAuthority) 'CU1 baseline must not claim general Sparse write-set authority.'
Require ($m.baselineInventory.runtimeSparsePolicyDecision -eq 'Forbidden') 'Runtime Sparse policy boundary mismatch.'

Require ($m.fixedMeaning.semantic -eq 'ExactSparsePointTransformWorkSetV1') 'Spiral 6 Semantic mismatch.'
Require ($m.fixedMeaning.representationPath -eq 'DirectPgaThroughConsumerV1') 'Spiral 6 representation path mismatch.'
Require ([int]$m.fixedMeaning.boneCount -eq 8) 'Spiral 6 bone count mismatch.'
Require ([int]$m.fixedMeaning.workUniverseCount -eq 4096) 'Spiral 6 universe count mismatch.'
Require ([int]$m.fixedMeaning.outputCapacity -eq 4096) 'Spiral 6 output capacity mismatch.'
Require ([int]$m.fixedMeaning.threadsPerGroup -eq 64) 'Spiral 6 thread-group size mismatch.'
Require ([int]$m.fixedMeaning.timelineLength -eq 1) 'Spiral 6 must freeze Temporal dimension to one invocation.'
Require ([int]$m.fixedMeaning.historyDepth -eq 0) 'Spiral 6 must not compare Temporal History.'
Require ($m.fixedMeaning.inactiveOutputSentinel -eq 'Float4Zero') 'Spiral 6 inactive sentinel mismatch.'

Require ($m.sparseSet.type -eq 'ExactSparseWorkSetV1') 'Spiral 6 exact set type mismatch.'
Require ([int]$m.sparseSet.universeFirstIndex -eq 0) 'Spiral 6 universe first index mismatch.'
Require ([int]$m.sparseSet.universeLastIndex -eq 4095) 'Spiral 6 universe last index mismatch.'
Require ([int]$m.sparseSet.minimumCardinality -eq 0) 'Spiral 6 minimum cardinality mismatch.'
Require ([int]$m.sparseSet.maximumCardinality -eq 4096) 'Spiral 6 maximum cardinality mismatch.'
Require ($m.sparseSet.canonicalEncoding -eq 'StrictlyIncreasingUniqueU32Indices') 'Spiral 6 canonical set encoding mismatch.'
Require ($m.sparseSet.setAuthority -eq 'ExternalVerifiedInput') 'Spiral 6 set authority mismatch.'
Require (-not [bool]$m.sparseSet.runtimeMayChooseMembership) 'Runtime must not choose Sparse membership.'
Require (-not [bool]$m.sparseSet.runtimeMayChooseCardinality) 'Runtime must not choose Sparse cardinality.'
Require (-not [bool]$m.sparseSet.runtimeMayChoosePattern) 'Runtime must not choose Sparse pattern.'

$candidates = @($m.candidateKinds)
Require (($candidates -join ',') -eq 'DenseMembershipMaskPredicate,CompactIndexListDispatch,ActiveBlockListLocalMask') 'Spiral 6 Candidate family mismatch.'
$patterns = @($m.patternKinds)
Require (($patterns -join ',') -eq 'PrefixControl,UniformStride,BlockClusteredPermutation,HashScatterPermutation') 'Spiral 6 pattern corpus mismatch.'

$qualification = @($m.qualificationActiveCounts | ForEach-Object { [int]$_ })
Require (($qualification -join ',') -eq '0,1,2,31,32,63,64,65,127,128,129,255,256,257,511,512,1024,2048,3072,4095,4096') 'Spiral 6 qualification count corpus mismatch.'
$measurement = @($m.measurementActiveCounts | ForEach-Object { [int]$_ })
Require (($measurement -join ',') -eq '1,8,32,64,128,256,512,1024,2048,3072,4096') 'Spiral 6 measurement count corpus mismatch.'

Require ([int]$m.derivedRepresentations.DenseMembershipMaskPredicate.membershipMaskBytes -eq 512) 'Dense mask byte count mismatch.'
Require ([int]$m.derivedRepresentations.CompactIndexListDispatch.fixedCapacityBytes -eq 16384) 'Compact list capacity mismatch.'
Require ([int]$m.derivedRepresentations.ActiveBlockListLocalMask.blockSize -eq 64) 'Sparse block size mismatch.'
Require ([int]$m.derivedRepresentations.ActiveBlockListLocalMask.fixedCapacityBytes -eq 1024) 'Sparse block-list capacity mismatch.'
Require ($m.outputPolicy.activeWriteSet -eq 'ExactlySparseSetS') 'Spiral 6 active write-set policy mismatch.'
Require ($m.outputPolicy.inactiveWritePolicy -eq 'Forbidden') 'Spiral 6 inactive write policy mismatch.'
Require ($m.temporalPolicy -eq 'FrozenSingleInvocationNoHistoryReuseDimension') 'Spiral 6 Temporal isolation mismatch.'
Require ($m.level4Policy -eq 'MinimalVersionedSparseWorkSetExtensionOnlyIfRequired') 'Spiral 6 Level 4 policy mismatch.'
Require ($m.level5Policy -eq 'SparseRepresentationGranularityIsAVerifiedLoweringChoice') 'Spiral 6 Level 5 policy mismatch.'
Require ($m.runtimePolicyAuthorization -eq 'None') 'Spiral 6 Runtime policy must remain unauthorized.'
Require ($m.typeSafety.rawToVerified -eq 'CompileTimeUnavailable') 'Spiral 6 Raw-to-Verified boundary mismatch.'

$requiredFiles = @(
    'docs\spiral6\CONTRACT_MANIFEST_V1.json',
    'docs\spiral6\SGE4-5_Spiral6_Completion_Spec_v0.1.md',
    'docs\spiral6\NEXT_CAPABILITY_SELECTION_FROM_SPIRAL5.md',
    'docs\spiral6\NON_GOALS_V1.md',
    'docs\spiral6\PROJECT_BOUNDARIES_V1.md',
    'docs\spiral6\CORPUS_V1.md',
    'docs\spiral6\PROOF_LEDGER_V1.md',
    'docs\spiral6\STAGE_MAP_V1.md',
    'docs\spiral6\SPIRAL6_PROGRESS.md',
    'docs\spiral6\CU1_CHANGESET.md',
    'docs\spiral6\CU1_EVIDENCE_LEDGER.md',
    'docs\spiral6\README.md',
    'docs\spiral5\SPIRAL5_PROGRESS.md',
    'tests\Run-Spiral6CU1.ps1',
    'tests\tools\Verify-Spiral6CU1.ps1',
    'run_sge4_5_spiral6_cu1_research_contract.bat'
)
foreach ($relative in $requiredFiles) {
    Require (Test-Path -LiteralPath (Join-Path $root $relative) -PathType Leaf) "Missing Spiral 6 CU1 file: $relative"
}

$spec = Get-Content -Raw -LiteralPath (Join-Path $root 'docs\spiral6\SGE4-5_Spiral6_Completion_Spec_v0.1.md') -Encoding UTF8
foreach ($token in @(
    'ExactSparseWorkSetV1',
    'DenseMembershipMaskPredicate',
    'CompactIndexListDispatch',
    'ActiveBlockListLocalMask',
    'Strictly increasing',
    '4096 membership bits = 512 bytes',
    'float4(0,0,0,0)',
    'Runtime and Backend are forbidden to decide',
    'SGE4-5 SPIRAL 6 ARCHITECTURE COMPLETE',
    'SGE4-5 SPIRAL 6 EXPERIMENT COMPLETE',
    'SPIRAL 6 CLOSED',
    'RuntimePolicyAuthorization = None',
    'NextCapabilitySelection = DeferredByOwner'
)) {
    Require ($spec -match [regex]::Escape($token)) "Spiral 6 completion specification token missing: $token"
}

$selection = Get-Content -Raw -LiteralPath (Join-Path $root 'docs\spiral6\NEXT_CAPABILITY_SELECTION_FROM_SPIRAL5.md') -Encoding UTF8
foreach ($token in @(
    '930ABD5D83A30B31A76551B9D8D35D89D527B7DD7EAC83FA605A94AA252DCF92',
    '8B4A6E532F424CC6525E2FA07A5C054D57343A333C211DC0451556CD4F10DBA5',
    '4C22E5D2D9CFC7CEA0038967822CCFBD7558AAA955A7B340A01ADE4591021FF6',
    'SGE4-5 SPIRAL 5 CLOSED'
)) {
    Require ($selection -match [regex]::Escape($token)) "Spiral 6 selection record token missing: $token"
}

$spiral5Progress = Get-Content -Raw -LiteralPath (Join-Path $root 'docs\spiral5\SPIRAL5_PROGRESS.md') -Encoding UTF8
Require ($spiral5Progress -match 'CU6: PASSED') 'Spiral 5 progress must record CU6 success.'
Require ($spiral5Progress -match 'SGE4-5 SPIRAL 5 CLOSED') 'Spiral 5 progress must record Owner closure.'
Require ($spiral5Progress -match 'RuntimePolicyAuthorization = None') 'Spiral 5 closure must not authorize Runtime policy.'

$activeSemantic = Join-Path $root '120_ActiveWorkSemantic\ActiveWorkSemantic.h'
Require (Test-Path -LiteralPath $activeSemantic -PathType Leaf) 'Spiral 4 Active Work Semantic baseline missing.'
$activeCandidate = Join-Path $root '123_ActiveWorkLoweringCandidate\ActiveWorkLoweringCandidate.h'
Require (Test-Path -LiteralPath $activeCandidate -PathType Leaf) 'Spiral 4 Active Work Candidate baseline missing.'
$candidateText = Get-Content -Raw -LiteralPath $activeCandidate -Encoding UTF8
Require ($candidateText -match 'PrefixZeroToActiveCount') 'Spiral 4 baseline must remain prefix-only.'

$spiral5Evidence = Join-Path $root 'docs\spiral5\CU6_EVIDENCE_LEDGER.md'
Require (Test-Path -LiteralPath $spiral5Evidence -PathType Leaf) 'Spiral 5 CU6 evidence ledger must remain present.'

if ($Mode -eq 'Snapshot') {
    $solution = Get-Content -Raw -LiteralPath (Join-Path $root 'SemanticGpuEngine4-5.sln') -Encoding UTF8
    foreach ($futureProject in @(
        '154_SparseWorkSetSemantic',
        '155_Spiral6SparseContracts',
        '156_Spiral6SparseExecution',
        '157_SparseLoweringCandidate',
        '158_SparseLoweringPlanner',
        '159_SparseLoweringVerifier'
    )) {
        Require ($solution -notmatch [regex]::Escape($futureProject)) "CU1 Snapshot must not pre-create future project: $futureProject"
    }

    $compositionContract = Get-Content -Raw -LiteralPath (Join-Path $root '17_CompositionContract\CompositionContract.h') -Encoding UTF8
    Require ($compositionContract -notmatch '\bExactSparseWorkSet') 'CU1 Snapshot must not already implement Exact Sparse Work Set.'
    Require ($compositionContract -notmatch '\bActiveBlockList') 'CU1 Snapshot must not already implement Active Block List.'
}
elseif ($Mode -eq 'Regression') {
    $cu2Manifest = Join-Path $root 'docs\spiral6\CU2_ARCHITECTURE_MANIFEST_V1.json'
    Require (Test-Path -LiteralPath $cu2Manifest -PathType Leaf) 'CU1 Regression mode requires a later Completion Unit marker.'
}
else {
    throw "Unexpected Spiral 6 CU1 verification mode: $Mode"
}

Write-Host "SGE4-5 Spiral 6 CU1 research contract passed. Mode: $Mode"
if ($Mode -eq 'Snapshot') {
    Write-Host 'Snapshot-only arbitrary Sparse Work-Set absence boundaries passed.'
} else {
    Write-Host 'Later Completion Units are allowed; immutable CU1 contract regression passed.'
}
