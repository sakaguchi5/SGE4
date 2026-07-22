$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)

function Require([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}

$manifestPath = Join-Path $root 'docs\spiral4\CONTRACT_MANIFEST_V1.json'
Require (Test-Path -LiteralPath $manifestPath -PathType Leaf) 'Spiral 4 contract manifest missing.'
$m = Get-Content -Raw -LiteralPath $manifestPath -Encoding UTF8 | ConvertFrom-Json

Require ($m.schema -eq 'SGE4-5.Spiral4.ContractManifest.V1') 'Spiral 4 contract schema mismatch.'
Require ($m.completionUnit -eq 'CU1') 'Spiral 4 completion unit mismatch.'
Require ($m.baselineCommit -eq '8c1125394ba4b45d571d7cba4e7ad685bb90918b') 'Spiral 4 baseline mismatch.'
Require ($m.ownerSelection -eq 'DynamicActiveWorkCountVerifiedExecuteIndirectAndBatchLowering') 'Spiral 4 Owner selection mismatch.'
Require ([int]$m.targetAbiBaseline.d3d12Schema -eq 17) 'Spiral 4 baseline D3D12 Schema mismatch.'
Require ([int]$m.targetAbiBaseline.runtime -eq 17) 'Spiral 4 baseline Runtime mismatch.'
Require ($m.cu1AbiMutation -eq 'None') 'CU1 must not mutate ABI.'
Require ($m.abiExtensionPolicy -eq 'VersionedExtensionOnlyAfterSchema17InsufficiencyProof') 'Spiral 4 ABI extension policy mismatch.'

Require ([bool]$m.baselineInventory.indirectArgumentResourceState) 'Baseline IndirectArgument state inventory missing.'
Require (-not [bool]$m.baselineInventory.executeIndirectOperation) 'CU1 baseline must not claim an existing indirect Operation.'
Require (-not [bool]$m.baselineInventory.commandSignatureArtifact) 'CU1 baseline must not claim an existing Command Signature artifact.'
Require ($m.baselineInventory.runtimeDispatchDimensionDecision -eq 'Forbidden') 'Runtime dispatch-decision boundary mismatch.'

Require ([int]$m.fixedMeaning.boneCount -eq 8) 'Spiral 4 bone count mismatch.'
Require ([int]$m.fixedMeaning.maxReusePerBone -eq 512) 'Spiral 4 max reuse mismatch.'
Require ([int]$m.fixedMeaning.maxWorkCount -eq 4096) 'Spiral 4 Nmax mismatch.'
Require ([int]$m.fixedMeaning.threadsPerGroup -eq 64) 'Spiral 4 thread-group size mismatch.'
Require ($m.fixedMeaning.activeRange -eq 'Prefix[0,Nf)') 'Spiral 4 active range mismatch.'
Require ($m.fixedMeaning.inactiveRange -eq '[Nf,Nmax)') 'Spiral 4 inactive range mismatch.'
Require ($m.fixedMeaning.representationPath -eq 'DirectPgaThroughConsumerV1') 'Spiral 4 fixed representation control mismatch.'

$candidates = @($m.candidateKinds)
Require (($candidates -join ',') -eq 'FixedMaximumGuarded,SingleExecuteIndirectDispatch,BatchedExecuteIndirectDispatch') 'Spiral 4 Candidate family mismatch.'

$batchSizes = @($m.batchPolicy.batchSizes | ForEach-Object { [int]$_ })
Require (($batchSizes -join ',') -eq '64,128,256,512,1024') 'Spiral 4 Batch-size corpus mismatch.'
Require ([int]$m.batchPolicy.maximumBatchSlots -eq 64) 'Spiral 4 maximum Batch slots mismatch.'
Require ($m.batchPolicy.emptyBatchEncoding -eq 'ZeroDispatchGroups') 'Spiral 4 empty Batch encoding mismatch.'
Require ($m.batchPolicy.indirectCountBuffer -eq 'NotInV1') 'Spiral 4 V1 must not use an indirect count Buffer.'

$activeCounts = @($m.activeCountCorpus | ForEach-Object { [int]$_ })
Require (($activeCounts -join ',') -eq '0,1,63,64,65,127,128,129,255,256,257,511,512,513,1023,1024,2048,4095,4096') 'Spiral 4 Active Count corpus mismatch.'

Require ($m.level4Policy -eq 'MinimalVersionedIndirectExtensionOnlyIfRequired') 'Spiral 4 Level 4 policy mismatch.'
Require ($m.level5Policy -eq 'DispatchAndBatchStructureAreVerifiedLoweringChoices') 'Spiral 4 Level 5 policy mismatch.'
Require ($m.runtimePolicyAuthorization -eq 'None') 'Spiral 4 Runtime policy must remain unauthorized.'
Require ($m.typeSafety.rawToVerified -eq 'CompileTimeUnavailable') 'Spiral 4 Raw-to-Verified boundary mismatch.'

$requiredFiles = @(
    'docs\spiral4\CONTRACT_MANIFEST_V1.json',
    'docs\spiral4\SGE4-5_Spiral4_Completion_Spec_v0.1.md',
    'docs\spiral4\NEXT_CAPABILITY_SELECTION_FROM_SPIRAL3.md',
    'docs\spiral4\NON_GOALS_V1.md',
    'docs\spiral4\PROJECT_BOUNDARIES_V1.md',
    'docs\spiral4\CORPUS_V1.md',
    'docs\spiral4\PROOF_LEDGER_V1.md',
    'docs\spiral4\STAGE_MAP_V1.md',
    'docs\spiral4\SPIRAL4_PROGRESS.md',
    'docs\spiral4\CU1_CHANGESET.md',
    'docs\spiral4\CU1_EVIDENCE_LEDGER.md',
    'docs\spiral4\README.md',
    'tests\Run-Spiral4CU1.ps1',
    'tests\tools\Verify-Spiral4CU1.ps1',
    'run_sge4_5_spiral4_cu1_research_contract.bat'
)
foreach ($relative in $requiredFiles) {
    Require (Test-Path -LiteralPath (Join-Path $root $relative) -PathType Leaf) "Missing Spiral 4 CU1 file: $relative"
}

$spec = Get-Content -Raw -LiteralPath (Join-Path $root 'docs\spiral4\SGE4-5_Spiral4_Completion_Spec_v0.1.md') -Encoding UTF8
foreach ($token in @(
    '0 <= Nf <= Nmax',
    'Nmax = 4096',
    'FixedMaximumGuarded',
    'SingleExecuteIndirectDispatch',
    'BatchedExecuteIndirectDispatch',
    'Runtime must not read back',
    'Schema 17 insufficiency proof',
    'Architecture Complete',
    'Experiment Complete',
    'Spiral 4 Closed',
    'RuntimePolicyAuthorization = None',
    'NextCapabilitySelection = DeferredByOwner'
)) {
    Require ($spec -match [regex]::Escape($token)) "Spiral 4 completion specification token missing: $token"
}

$schemaPath = Join-Path $root '10_D3D12PackageSchema\D3D12Schema.h'
Require (Test-Path -LiteralPath $schemaPath -PathType Leaf) 'Baseline D3D12 schema header missing.'
$schema = Get-Content -Raw -LiteralPath $schemaPath -Encoding UTF8
Require ($schema -match 'IndirectArgument\s*=\s*1u\s*<<\s*10') 'Baseline IndirectArgument state not found.'
Require ($schema -notmatch '\bExecuteIndirect\b') 'CU1 must not already implement ExecuteIndirect.'
Require ($schema -notmatch '\bDispatchIndirect\b') 'CU1 must not already implement DispatchIndirect.'
Require ($schema -notmatch '\bCommandSignature') 'CU1 must not already implement a Command Signature artifact.'

$solution = Get-Content -Raw -LiteralPath (Join-Path $root 'SemanticGpuEngine4-5.sln') -Encoding UTF8
foreach ($futureProject in @(
    '120_ActiveWorkSemantic',
    '121_Spiral4Contracts',
    '123_ActiveWorkLoweringCandidate',
    '125_ActiveWorkLoweringVerifier',
    '139_Spiral4Launcher'
)) {
    Require ($solution -notmatch [regex]::Escape($futureProject)) "CU1 must not pre-create future project: $futureProject"
}

$spiral3Report = Join-Path $root 'docs\spiral3\CU6_EVIDENCE_LEDGER.md'
Require (Test-Path -LiteralPath $spiral3Report -PathType Leaf) 'Spiral 3 CU6 evidence must remain present.'

Write-Host 'SGE4-5 Spiral 4 CU1 research contract and baseline boundaries passed.'
