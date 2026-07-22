param(
    [ValidateSet('Auto', 'Snapshot', 'Regression')]
    [string]$Mode = 'Auto'
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)

if ($Mode -eq 'Auto') {
    $cu2Manifest = Join-Path $root 'docs\spiral5\CU2_ARCHITECTURE_MANIFEST_V1.json'
    $Mode = if (Test-Path -LiteralPath $cu2Manifest -PathType Leaf) {
        'Regression'
    } else {
        'Snapshot'
    }
}

function Require([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}

$manifestPath = Join-Path $root 'docs\spiral5\CONTRACT_MANIFEST_V1.json'
Require (Test-Path -LiteralPath $manifestPath -PathType Leaf) 'Spiral 5 contract manifest missing.'
$m = Get-Content -Raw -LiteralPath $manifestPath -Encoding UTF8 | ConvertFrom-Json

Require ($m.schema -eq 'SGE4-5.Spiral5.ContractManifest.V1') 'Spiral 5 contract schema mismatch.'
Require ($m.completionUnit -eq 'CU1') 'Spiral 5 completion unit mismatch.'
Require ($m.baselineCommit -eq 'f29d6597ec370d963c7b7dfbbc9af9590e8bd58f') 'Spiral 5 baseline mismatch.'
Require ($m.ownerSelection -eq 'TemporalStateFlowVerifiedHistoryReuseLowering') 'Spiral 5 Owner selection mismatch.'
Require ([int]$m.targetAbiBaseline.d3d12Schema -eq 17) 'Spiral 5 baseline D3D12 Schema mismatch.'
Require ([int]$m.targetAbiBaseline.runtime -eq 17) 'Spiral 5 baseline Runtime mismatch.'
Require ($m.targetAbiBaseline.spiral4IndirectExtension -eq 'VersionedSidecarIndirectExtensionV1') 'Spiral 5 indirect baseline mismatch.'
Require ($m.cu1AbiMutation -eq 'None') 'Spiral 5 CU1 must not mutate ABI.'
Require ($m.abiExtensionPolicy -eq 'VersionedTemporalExtensionOnlyAfterBaselineInsufficiencyProof') 'Spiral 5 extension policy mismatch.'

Require ([bool]$m.baselineInventory.frameNumberInvocationField) 'Spiral 5 frameNumber inventory missing.'
Require ([bool]$m.baselineInventory.perInvocationDynamicData) 'Spiral 5 dynamic input inventory missing.'
Require ([bool]$m.baselineInventory.wholeCompositionRecovery) 'Spiral 5 Recovery inventory missing.'
Require ([bool]$m.baselineInventory.spiral4VerifiedIndirectDispatch) 'Spiral 4 indirect baseline missing.'
Require (-not [bool]$m.baselineInventory.temporalHistoryFlowContract) 'CU1 baseline must not claim Temporal History Flow.'
Require (-not [bool]$m.baselineInventory.historyGenerationArtifact) 'CU1 baseline must not claim a history generation artifact.'
Require (-not [bool]$m.baselineInventory.updateScheduleArtifact) 'CU1 baseline must not claim an update schedule artifact.'
Require (-not [bool]$m.baselineInventory.retainedHistoryCompletion) 'CU1 baseline must not claim retained-history completion.'
Require ($m.baselineInventory.runtimeTemporalPolicyDecision -eq 'Forbidden') 'Runtime temporal policy boundary mismatch.'

Require ([int]$m.fixedMeaning.boneCount -eq 8) 'Spiral 5 bone count mismatch.'
Require ([int]$m.fixedMeaning.workCount -eq 4096) 'Spiral 5 Work count mismatch.'
Require ([int]$m.fixedMeaning.activeWorkCount -eq 4096) 'Spiral 5 Active Work count must be fixed.'
Require ([int]$m.fixedMeaning.threadsPerGroup -eq 64) 'Spiral 5 thread-group size mismatch.'
Require ([int]$m.fixedMeaning.timelineLength -eq 128) 'Spiral 5 timeline length mismatch.'
Require ([int]$m.fixedMeaning.historyDepth -eq 1) 'Spiral 5 history depth mismatch.'
Require ($m.fixedMeaning.temporalRule -eq 'LastUpdateWinsPiecewiseConstant') 'Spiral 5 temporal meaning mismatch.'
Require ($m.fixedMeaning.initialInvocation -eq 'UpdateRequired') 'Spiral 5 initial update rule mismatch.'
Require ($m.fixedMeaning.representationPath -eq 'DirectPgaThroughConsumerV1') 'Spiral 5 representation control mismatch.'

Require ($m.runtimeInputs.holdInputRule -eq 'MotorBytesAndGenerationEqualMostRecentUpdate') 'Spiral 5 hold input rule mismatch.'
Require ($m.runtimeInputs.scheduleAuthority -eq 'ExternalVerifiedInput') 'Spiral 5 schedule authority mismatch.'
Require (-not [bool]$m.runtimeInputs.runtimeMayChooseUpdate) 'Runtime must not choose updates.'

$candidates = @($m.candidateKinds)
Require (($candidates -join ',') -eq 'EveryInvocationRecompute,GlobalMotorHistoryReuse,FinalOutputHistoryReuse') 'Spiral 5 Candidate family mismatch.'

$intervals = @($m.measurementUpdateIntervals | ForEach-Object { [int]$_ })
Require (($intervals -join ',') -eq '1,2,4,8,16,32,64') 'Spiral 5 update interval corpus mismatch.'

$schedules = @($m.qualificationSchedules)
Require ($schedules.Count -eq 9) 'Spiral 5 qualification schedule count mismatch.'
Require ($schedules[7].id -eq 'BurstThenHold') 'Spiral 5 burst schedule missing.'
Require ($schedules[8].id -eq 'Irregular') 'Spiral 5 irregular schedule missing.'

Require ([int]$m.historyPolicy.logicalDepth -eq 1) 'Spiral 5 logical history depth mismatch.'
Require ($m.historyPolicy.initialState -eq 'InvalidUntilFirstUpdate') 'Spiral 5 initial history state mismatch.'
Require ($m.historyPolicy.generationRule -eq 'IncrementByOneOnUpdateUnchangedOnHold') 'Spiral 5 generation rule mismatch.'
Require ($m.historyPolicy.deviceEpochBinding -eq 'Required') 'Spiral 5 epoch binding mismatch.'
Require ($m.historyPolicy.staleHistory -eq 'Rejected') 'Spiral 5 stale history rule mismatch.'
Require ($m.interpolationPolicy -eq 'NotInV1SeparateSemanticMeaning') 'Spiral 5 interpolation boundary mismatch.'

Require ($m.level4Policy -eq 'MinimalVersionedTemporalHistoryExtensionOnlyIfRequired') 'Spiral 5 Level 4 policy mismatch.'
Require ($m.level5Policy -eq 'HistoryMaterializationPositionIsAVerifiedLoweringChoice') 'Spiral 5 Level 5 policy mismatch.'
Require ($m.runtimePolicyAuthorization -eq 'None') 'Spiral 5 Runtime policy must remain unauthorized.'
Require ($m.typeSafety.rawToVerified -eq 'CompileTimeUnavailable') 'Spiral 5 Raw-to-Verified boundary mismatch.'

$requiredFiles = @(
    'docs\spiral5\CONTRACT_MANIFEST_V1.json',
    'docs\spiral5\SGE4-5_Spiral5_Completion_Spec_v0.1.md',
    'docs\spiral5\NEXT_CAPABILITY_SELECTION_FROM_SPIRAL4.md',
    'docs\spiral5\NON_GOALS_V1.md',
    'docs\spiral5\PROJECT_BOUNDARIES_V1.md',
    'docs\spiral5\CORPUS_V1.md',
    'docs\spiral5\PROOF_LEDGER_V1.md',
    'docs\spiral5\STAGE_MAP_V1.md',
    'docs\spiral5\SPIRAL5_PROGRESS.md',
    'docs\spiral5\CU1_CHANGESET.md',
    'docs\spiral5\CU1_EVIDENCE_LEDGER.md',
    'docs\spiral5\README.md',
    'tests\Run-Spiral5CU1.ps1',
    'tests\tools\Verify-Spiral5CU1.ps1',
    'run_sge4_5_spiral5_cu1_research_contract.bat'
)
foreach ($relative in $requiredFiles) {
    Require (Test-Path -LiteralPath (Join-Path $root $relative) -PathType Leaf) "Missing Spiral 5 CU1 file: $relative"
}

$spec = Get-Content -Raw -LiteralPath (Join-Path $root 'docs\spiral5\SGE4-5_Spiral5_Completion_Spec_v0.1.md') -Encoding UTF8
foreach ($token in @(
    'LastUpdateWinsPiecewiseConstant',
    'EveryInvocationRecompute',
    'GlobalMotorHistoryReuse',
    'FinalOutputHistoryReuse',
    'HistoryDepth = 1',
    'Interpolation is not part of V1',
    'Runtime and Backend are forbidden to decide',
    'SPIRAL 5 ARCHITECTURE COMPLETE',
    'SPIRAL 5 EXPERIMENT COMPLETE',
    'SPIRAL 5 CLOSED',
    'RuntimePolicyAuthorization = None',
    'NextCapabilitySelection = DeferredByOwner'
)) {
    Require ($spec -match [regex]::Escape($token)) "Spiral 5 completion specification token missing: $token"
}

$runtimeHeader = Join-Path $root '22_CompositionRuntime\CompositionRuntime.h'
Require (Test-Path -LiteralPath $runtimeHeader -PathType Leaf) 'Composition Runtime header missing.'
$runtime = Get-Content -Raw -LiteralPath $runtimeHeader -Encoding UTF8
Require ($runtime -match 'StaticCompositionFrameInvocation') 'Baseline Frame Invocation type missing.'
Require ($runtime -match 'frameNumber') 'Baseline frameNumber field missing.'
Require ($runtime -match 'dynamicData') 'Baseline per-invocation dynamic data missing.'

$contractHeader = Join-Path $root '17_CompositionContract\CompositionContract.h'
Require (Test-Path -LiteralPath $contractHeader -PathType Leaf) 'Composition Contract header missing.'
$contract = Get-Content -Raw -LiteralPath $contractHeader -Encoding UTF8

if ($Mode -eq 'Snapshot') {
    Require ($contract -notmatch '\bTemporalHistory') 'CU1 Snapshot must not already implement Temporal History.'
    Require ($contract -notmatch '\bHistoryGeneration') 'CU1 Snapshot must not already implement History Generation.'
    Require ($contract -notmatch '\bUpdateSchedule') 'CU1 Snapshot must not already implement Update Schedule.'

    $solution = Get-Content -Raw -LiteralPath (Join-Path $root 'SemanticGpuEngine4-5.sln') -Encoding UTF8
    foreach ($futureProject in @(
        '134_TemporalStateSemantic',
        '135_Spiral5TemporalContracts',
        '136_Spiral5TemporalExecution',
        '137_TemporalLoweringCandidate',
        '138_TemporalLoweringPlanner',
        '139_TemporalLoweringVerifier',
        '145_Spiral5TemporalArchitectureTests'
    )) {
        Require ($solution -notmatch [regex]::Escape($futureProject)) "CU1 Snapshot must not pre-create future project: $futureProject"
    }
}
elseif ($Mode -eq 'Regression') {
    $cu2Manifest = Join-Path $root 'docs\spiral5\CU2_ARCHITECTURE_MANIFEST_V1.json'
    Require (Test-Path -LiteralPath $cu2Manifest -PathType Leaf) 'CU1 Regression mode requires a later Completion Unit marker.'
}
else {
    throw "Unexpected Spiral 5 CU1 verification mode: $Mode"
}

$spiral4Evidence = Join-Path $root 'docs\spiral4\CU6_EVIDENCE_LEDGER.md'
Require (Test-Path -LiteralPath $spiral4Evidence -PathType Leaf) 'Spiral 4 CU6 evidence must remain present.'

Write-Host "SGE4-5 Spiral 5 CU1 research contract passed. Mode: $Mode"
if ($Mode -eq 'Snapshot') {
    Write-Host 'Snapshot-only temporal absence boundaries passed.'
} else {
    Write-Host 'Later Completion Units are allowed; immutable CU1 contract regression passed.'
}
