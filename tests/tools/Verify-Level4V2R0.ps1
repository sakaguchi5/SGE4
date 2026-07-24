param(
    [ValidateSet('Auto','Applied','Package','Regression')]
    [string]$Mode = 'Auto'
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
function Require([bool]$Condition,[string]$Message){if(-not $Condition){throw $Message}}

if($Mode -eq 'Auto'){
    $r1Manifest = Join-Path $root 'docs\level4v2\R1_CANONICAL_VOCABULARY_MANIFEST_V1.json'
    $Mode = if(Test-Path -LiteralPath $r1Manifest -PathType Leaf){'Regression'}else{'Applied'}
}

$required = @(
'docs\level4v2\R0_INPUT_FREEZE_MANIFEST_V1.json',
'docs\level4v2\R0_CARRIED_INVARIANT_MAP_V1.md',
'docs\level4v2\R0_CANONICAL_LAYER_OWNERSHIP_V1.md',
'docs\level4v2\R1_CANONICAL_VOCABULARY_ENTRY_CONTRACT_V1.md',
'docs\level4v2\R0_EVIDENCE_LEDGER_V1.md',
'docs\level4v2\R0_CHANGESET.md',
'docs\level4v2\APPLY_R0_JA.md',
'docs\level4v2\README.md',
'docs\level4v2\RECONSTRUCTION_ROADMAP_V1.md',
'docs\level4v2\CANONICAL_RECONSTRUCTION_INPUT_V1.json',
'tests\Run-Level4V2R0.ps1',
'tests\tools\Verify-Level4V2R0.ps1',
'run_sge4_5_level4v2_r0_prepare.bat',
'run_sge4_5_level4v2_r0_input_freeze.bat')
foreach($relative in $required){Require (Test-Path -LiteralPath (Join-Path $root $relative) -PathType Leaf) "R0 file missing: $relative"}

$m = Get-Content -Raw -LiteralPath (Join-Path $root 'docs\level4v2\R0_INPUT_FREEZE_MANIFEST_V1.json') -Encoding UTF8 | ConvertFrom-Json
Require ($m.schema -eq 'SGE4.Level4V2.R0InputFreezeManifest.V1') 'R0 manifest schema mismatch.'
Require ($m.completionUnit -eq 'R0' -and $m.status -eq 'Complete') 'R0 completion status mismatch.'
Require ($m.baseCommit -eq 'a44b2ee3e5e151eab2f25f94e92d838b7931a4d9') 'R0 base commit mismatch.'
Require ($m.acceptedSpiral7FinalCommit -eq 'f802c12b162569a869c214da22b80142b3a4a0dd') 'R0 accepted Spiral 7 commit mismatch.'
Require ($m.mode -eq 'ReconstructionNotNewCapability') 'R0 mode mismatch.'
Require ($m.architectureImplementationMutation -eq 'None') 'R0 must not mutate implementation Architecture.'
Require ($m.solutionProjectMutation -eq 'None') 'R0 historical phase must not add a solution Project.'
Require ($m.runtimeCandidatePolicyAuthorization -eq 'None') 'R0 must not authorize Runtime Candidate policy.'
Require ($m.nextStage -eq 'R1CanonicalVocabulary') 'R0 next stage mismatch.'

$sourceFreeze = @($m.sourceFreeze)
Require ($sourceFreeze.Count -eq 16) 'R0 source-freeze count mismatch.'
$sourceRoles = @{}
foreach($entry in $sourceFreeze){
    $path = Join-Path $root ([string]$entry.path)
    Require (Test-Path -LiteralPath $path -PathType Leaf) "Frozen R0 source missing: $($entry.path)"
    Require (-not $sourceRoles.ContainsKey([string]$entry.role)) "Duplicate R0 source role: $($entry.role)"
    $sourceRoles[[string]$entry.role] = $true
    $actual = (Get-FileHash -Algorithm SHA256 -LiteralPath $path).Hash.ToLowerInvariant()
    Require ($actual -eq ([string]$entry.sha256).ToLowerInvariant()) "Frozen R0 source changed: $($entry.path)"
}
foreach($role in @('Spiral1ProofLedger','Spiral2ProofLedger','Spiral3ProofLedger','Spiral4ProofLedger','Spiral5ProofLedger','Spiral6ProofLedger','Spiral7ProofLedger','Spiral7Closure','AcceptedArchitectureQualification','AcceptedObservationalMeasurement')){Require ($sourceRoles.ContainsKey($role)) "Required R0 source role missing: $role"}

$invariants = @($m.carriedInvariants)
Require ([int]$m.carriedInvariantCount -eq 40 -and $invariants.Count -eq 40) 'R0 carried-invariant count mismatch.'
$ids = @{}; $names = @{}
$allowedOwners = @($m.allowedOwners); $allowedDestinations = @($m.allowedDestinations)
$mapText = Get-Content -Raw -LiteralPath (Join-Path $root 'docs\level4v2\R0_CARRIED_INVARIANT_MAP_V1.md') -Encoding UTF8
foreach($i in $invariants){
    $id=[string]$i.id; $name=[string]$i.canonicalName
    Require ($id -match '^V2-R0-I[0-9]{3}$') "Non-canonical R0 invariant ID: $id"
    Require (-not $ids.ContainsKey($id)) "Duplicate R0 invariant ID: $id"
    Require (-not $names.ContainsKey($name)) "Duplicate R0 canonical invariant name: $name"
    $ids[$id]=$true; $names[$name]=$true
    Require ($name -notmatch 'Spiral|CU[0-9]|RTX|WARP|D3D12') "Experimental name entered canonical invariant: $name"
    Require ($i.state -eq 'FrozenForMigration') "R0 invariant is not frozen: $id"
    Require ($allowedOwners -contains [string]$i.owner) "Unknown R0 invariant owner: $id"
    Require ($allowedDestinations -contains [string]$i.destination) "Unknown R0 invariant destination: $id"
    Require (@($i.sources).Count -gt 0) "R0 invariant has no source: $id"
    foreach($source in @($i.sources)){Require ($sourceRoles.ContainsKey([string]$source)) "R0 invariant references unknown source role: $id -> $source"}
    Require (-not [string]::IsNullOrWhiteSpace([string]$i.requiredVerification)) "R0 invariant has no verification requirement: $id"
    Require ($mapText -match [regex]::Escape($id)) "R0 invariant missing from human map: $id"
}
Require (($ids.Keys|Sort-Object|Select-Object -First 1) -eq 'V2-R0-I001') 'R0 invariant range does not start at I001.'
Require (($ids.Keys|Sort-Object|Select-Object -Last 1) -eq 'V2-R0-I040') 'R0 invariant range does not end at I040.'
foreach($token in @('NewRuntimeCapability','CanonicalAbiMutation','Schema17Mutation','Runtime17Mutation','BackendSemanticPolicy','GamePolicyInference','HardwareWinnerPolicy','ReferenceProjectDeletion')){Require (@($m.forbiddenDuringR0) -contains $token) "R0 forbidden action missing: $token"}

$ownership = Get-Content -Raw -LiteralPath (Join-Path $root 'docs\level4v2\R0_CANONICAL_LAYER_OWNERSHIP_V1.md') -Encoding UTF8
foreach($token in @('Canonical Semantic','Canonical Invocation','Candidate Proposal','Independent Verifier','Frozen Artifact','Canonical Composition','Canonical Runtime','Canonical Recovery','Single-fact rule')){Require ($ownership -match [regex]::Escape($token)) "R0 ownership token missing: $token"}
$r1 = Get-Content -Raw -LiteralPath (Join-Path $root 'docs\level4v2\R1_CANONICAL_VOCABULARY_ENTRY_CONTRACT_V1.md') -Encoding UTF8
foreach($token in @('R1 implementation = NOT STARTED','ActiveCount','TransitionCount','ReuseCount','UpdateInterval','BatchSize','AffectedBlockCount','TimelineOrdinal','DeviceEpoch','Count-type substitution must fail to compile','Canonical Semantic headers must not depend on D3D12 headers')){Require ($r1 -match [regex]::Escape($token)) "R1 entry-contract token missing: $token"}

$input = Get-Content -Raw -LiteralPath (Join-Path $root 'docs\level4v2\CANONICAL_RECONSTRUCTION_INPUT_V1.json') -Encoding UTF8 | ConvertFrom-Json
Require ($input.preparationCommit -eq 'a44b2ee3e5e151eab2f25f94e92d838b7931a4d9') 'Canonical input preparation commit mismatch.'
Require ($input.r0InputFreeze.manifest -eq 'docs/level4v2/R0_INPUT_FREEZE_MANIFEST_V1.json') 'Canonical input R0 manifest path mismatch.'
Require ([int]$input.r0InputFreeze.carriedInvariantCount -eq 40) 'Canonical input R0 count mismatch.'

$solution = Get-Content -Raw -LiteralPath (Join-Path $root 'SemanticGpuEngine4-5.sln') -Encoding UTF8
if($Mode -eq 'Regression'){
    Require ($input.r0InputFreeze.status -eq 'Passed') 'R0 regression requires accepted R0 status.'
    Require ($input.acceptedR0Commit -eq 'b8b5b4f675e4186a3ae202d718e091b63c53e264') 'R0 regression accepted commit mismatch.'
    Require ($null -ne $input.r1CanonicalVocabulary) 'R0 regression does not see the accepted R1 record.'
    Require ($input.r1CanonicalVocabulary.status -eq 'Passed') 'R0 regression requires accepted R1 status.'
    Require ($input.r1CanonicalVocabulary.acceptedCommit -eq 'eee68ce6e9be5537d3041964679e55f4c5d2c448') 'R0 regression accepted R1 commit mismatch.'
    Require ($input.r1CanonicalVocabulary.manifest -eq 'docs/level4v2/R1_CANONICAL_VOCABULARY_MANIFEST_V1.json') 'R0 regression R1 manifest path mismatch.'
    $postR1Stages = @('R2UnifiedAuthorityChain','R3CanonicalComposition','R4DynamicInvocationAndHistory','R5RuntimeAndRecovery','R6MigrationCorpus','R7ReferenceRetirement','Complete')
    Require ($postR1Stages -contains [string]$input.nextStage) 'R0 regression does not see an accepted R1-or-later handoff.'
    Require (Test-Path -LiteralPath (Join-Path $root 'docs\level4v2\R1_CANONICAL_VOCABULARY_MANIFEST_V1.json') -PathType Leaf) 'R0 regression requires the R1 phase marker.'
    foreach($project in @('189_Level4V2CanonicalVocabulary','190_Level4V2CanonicalVocabularyTests')){Require ($solution -match [regex]::Escape($project)) "R0 regression missing later-phase Project: $project"}
}else{
    Require ($input.nextStage -eq 'R1CanonicalVocabulary') 'Canonical input does not advance to R1.'
    Require ($solution -notmatch 'Level4V2|Level4_V2|CanonicalVocabularyV2') 'R0 must not pre-create a Level 4 v2 implementation Project.'
}

Write-Host "Level 4 v2 R0 source identities, invariant ownership and migration destinations passed. Mode: $Mode"
Write-Host 'Carried invariants: 40. Frozen sources: 16.'
Write-Host 'R0 historical implementation and solution mutation: None.'
Write-Host 'Runtime Candidate-policy authorization: None.'
