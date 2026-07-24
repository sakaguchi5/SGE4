param([ValidateSet('Auto','Applied','Regression')][string]$Mode='Auto')
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
function Require([bool]$Condition,[string]$Message){if(-not $Condition){throw $Message}}
if($Mode -eq 'Auto'){$Mode=if(Test-Path -LiteralPath (Join-Path $root 'docs\level4v2\R3_CANONICAL_COMPOSITION_MANIFEST_V1.json') -PathType Leaf){'Regression'}else{'Applied'}}

$required=@(
'191_Level4V2AuthorityModel\191_Level4V2AuthorityModel.vcxproj','191_Level4V2AuthorityModel\AuthorityModel.h','191_Level4V2AuthorityModel\AuthorityModel.cpp',
'192_Level4V2CandidatePlanner\192_Level4V2CandidatePlanner.vcxproj','192_Level4V2CandidatePlanner\CandidatePlanner.h','192_Level4V2CandidatePlanner\CandidatePlanner.cpp',
'193_Level4V2IndependentVerifier\193_Level4V2IndependentVerifier.vcxproj','193_Level4V2IndependentVerifier\IndependentVerifier.h','193_Level4V2IndependentVerifier\IndependentVerifier.cpp',
'194_Level4V2FrozenAuthority\194_Level4V2FrozenAuthority.vcxproj','194_Level4V2FrozenAuthority\FrozenAuthority.h','194_Level4V2FrozenAuthority\FrozenAuthority.cpp',
'195_Level4V2AuthorityTests\195_Level4V2AuthorityTests.vcxproj','195_Level4V2AuthorityTests\main.cpp',
'docs\level4v2\R2_UNIFIED_AUTHORITY_MANIFEST_V1.json','docs\level4v2\R2_UNIFIED_AUTHORITY_CHAIN.md','docs\level4v2\R2_AUTHORITY_OWNERSHIP_V1.md','docs\level4v2\R2_MUTATION_REPLAY_MATRIX_V1.md','docs\level4v2\R2_EVIDENCE_LEDGER_V1.md','docs\level4v2\R2_CHANGESET.md','docs\level4v2\R3_CANONICAL_COMPOSITION_ENTRY_CONTRACT_V1.md','docs\level4v2\APPLY_R2_JA.md','docs\level4v2\R2_PACKAGE_MANIFEST.txt','docs\level4v2\R2_PACKAGE_FILES.sha256',
'tests\Run-Level4V2R2.ps1','tests\tools\Register-Level4V2R2Projects.ps1','tests\tools\Verify-Level4V2R2.ps1','run_sge4_5_level4v2_r2_prepare.bat','run_sge4_5_level4v2_r2_unified_authority.bat')
foreach($relative in $required){Require (Test-Path -LiteralPath (Join-Path $root $relative) -PathType Leaf) "R2 file missing: $relative"}

if($Mode -eq 'Applied'){
    $packageLines=@(Get-Content -LiteralPath (Join-Path $root 'docs\level4v2\R2_PACKAGE_FILES.sha256') -Encoding UTF8 | Where-Object{-not [string]::IsNullOrWhiteSpace($_)})
    Require ($packageLines.Count -eq 33) 'R2 package hash-entry count mismatch.'
    foreach($line in $packageLines){Require ($line -match '^([0-9a-f]{64})  (.+)$') "Malformed R2 package hash line: $line";$expected=$Matches[1];$relative=$Matches[2];$actual=(Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $root $relative)).Hash.ToLowerInvariant();Require ($actual -eq $expected) "R2 package file changed after packaging: $relative"}
}

$m=Get-Content -Raw -LiteralPath (Join-Path $root 'docs\level4v2\R2_UNIFIED_AUTHORITY_MANIFEST_V1.json') -Encoding UTF8|ConvertFrom-Json
Require ($m.schema -eq 'SGE4.Level4V2.R2UnifiedAuthorityManifest.V1') 'R2 manifest schema mismatch.'
Require ($m.completionUnit -eq 'R2') 'R2 completion unit mismatch.'
if($Mode -eq 'Applied'){
    Require ($m.status -eq 'CompletePackageSuppliedOwnerGatePending') 'R2 supplied status mismatch.'
}else{
    Require ($m.status -eq 'Passed') 'R2 accepted status mismatch.'
    Require ($m.acceptedCommit -eq '0caa4776077e58df6473e9f5760555b437bc5305') 'R2 accepted commit mismatch.'
}
Require ($m.baseCommit -eq 'eee68ce6e9be5537d3041964679e55f4c5d2c448') 'R2 base commit mismatch.'
Require ($m.r1AcceptedCommit -eq 'eee68ce6e9be5537d3041964679e55f4c5d2c448') 'R2 accepted R1 commit mismatch.'
Require ($m.r1DeterministicEvidenceSha256 -eq '35e751e131dcf4c88cad1b2da7bbc8e745f497e56d5ba22bd23079c7e2ec2abf') 'R2 accepted R1 Evidence mismatch.'
Require (@($m.projects).Count -eq 5) 'R2 Project count mismatch.'
Require (@($m.authorityOperationKinds).Count -eq 6) 'R2 authority-operation count mismatch.'
Require ([int]$m.mutationReplayMatrixCount -eq 18) 'R2 mutation/replay count mismatch.'
Require (-not [bool]$m.independence.plannerMayConstructVerifiedPlan) 'R2 Planner may construct Verified Plan.'
Require (-not [bool]$m.independence.verifierProjectDependsOnPlannerProject) 'R2 Verifier depends on Planner.'
Require ([bool]$m.independence.verifierReconstructsExpectedOperations) 'R2 Verifier does not reconstruct expected operations.'
Require ($m.runtimeCapabilityAdded -eq 'None' -and $m.runtimeCandidatePolicyAuthorization -eq 'None') 'R2 authorized Runtime capability or policy.'
Require ($m.schema17Mutation -eq 'None' -and $m.runtime17Mutation -eq 'None' -and $m.backendMutation -eq 'None' -and $m.compositionMutation -eq 'None') 'R2 changed a frozen boundary.'
Require ($m.nextStageOnSuccessfulGate -eq 'R3CanonicalComposition') 'R2 historical next stage mismatch.'

$projects=@(
[pscustomobject]@{Path='191_Level4V2AuthorityModel\191_Level4V2AuthorityModel.vcxproj';Guid='{000000BF-0000-5000-8000-0000000000BF}'},
[pscustomobject]@{Path='192_Level4V2CandidatePlanner\192_Level4V2CandidatePlanner.vcxproj';Guid='{000000C0-0000-5000-8000-0000000000C0}'},
[pscustomobject]@{Path='193_Level4V2IndependentVerifier\193_Level4V2IndependentVerifier.vcxproj';Guid='{000000C1-0000-5000-8000-0000000000C1}'},
[pscustomobject]@{Path='194_Level4V2FrozenAuthority\194_Level4V2FrozenAuthority.vcxproj';Guid='{000000C2-0000-5000-8000-0000000000C2}'},
[pscustomobject]@{Path='195_Level4V2AuthorityTests\195_Level4V2AuthorityTests.vcxproj';Guid='{000000C3-0000-5000-8000-0000000000C3}'})
$solution=Get-Content -Raw -LiteralPath (Join-Path $root 'SemanticGpuEngine4-5.sln') -Encoding UTF8
foreach($project in $projects){[xml]$xml=Get-Content -Raw -LiteralPath (Join-Path $root $project.Path) -Encoding UTF8;$guid=[string](($xml.Project.PropertyGroup|Where-Object{$_.Label -eq 'Globals'}|Select-Object -First 1).ProjectGuid);Require ($guid.ToUpperInvariant() -eq $project.Guid.ToUpperInvariant()) "R2 Project GUID mismatch: $($project.Path)";Require (([regex]::Matches($solution,[regex]::Escape($project.Path))).Count -eq 1) "R2 Solution registration mismatch: $($project.Path)"}

$plannerProject=Get-Content -Raw -LiteralPath (Join-Path $root '192_Level4V2CandidatePlanner\192_Level4V2CandidatePlanner.vcxproj') -Encoding UTF8
$verifierProject=Get-Content -Raw -LiteralPath (Join-Path $root '193_Level4V2IndependentVerifier\193_Level4V2IndependentVerifier.vcxproj') -Encoding UTF8
$frozenProject=Get-Content -Raw -LiteralPath (Join-Path $root '194_Level4V2FrozenAuthority\194_Level4V2FrozenAuthority.vcxproj') -Encoding UTF8
Require ($plannerProject -notmatch 'IndependentVerifier|FrozenAuthority') 'Planner has forbidden downstream dependency.'
Require ($verifierProject -notmatch 'CandidatePlanner|FrozenAuthority') 'Verifier has forbidden Planner/Frozen dependency.'
Require ($frozenProject -notmatch 'CandidatePlanner') 'Frozen builder has forbidden Planner dependency.'

$model=Get-Content -Raw -LiteralPath (Join-Path $root '191_Level4V2AuthorityModel\AuthorityModel.h') -Encoding UTF8
$planner=Get-Content -Raw -LiteralPath (Join-Path $root '192_Level4V2CandidatePlanner\CandidatePlanner.cpp') -Encoding UTF8
$verifier=Get-Content -Raw -LiteralPath (Join-Path $root '193_Level4V2IndependentVerifier\IndependentVerifier.cpp') -Encoding UTF8
$frozen=Get-Content -Raw -LiteralPath (Join-Path $root '194_Level4V2FrozenAuthority\FrozenAuthority.h') -Encoding UTF8
foreach($token in @('AuthorityRequestV1','PlannerProposalV1','VerifiedAuthorityV1','VerificationErrorV1','VerifiedPlanConstructionAccessV1','friend class sge4_5::v2::authority::IndependentVerifierV1','private:')){Require ($model -match [regex]::Escape($token)) "R2 authority-model token missing: $token"}
Require ($planner -match 'BuildPlannerOperationsV1') 'R2 Planner operation construction missing.'
Require ($verifier -match 'BuildExpectedOperationsV1') 'R2 independent expected-operation reconstruction missing.'
Require ($verifier -notmatch 'CandidatePlannerV1|CandidatePlanner.h') 'R2 Verifier source trusts or depends on Planner.'
Require ($frozen -match 'Freeze\(const authority::VerifiedAuthorityV1&') 'R2 Frozen builder does not require Verified authority.'
Require ($frozen -notmatch 'RawCandidateProposalV1|PlannerProposalV1') 'R2 Frozen public boundary accepts Raw/Planner input.'

$input=Get-Content -Raw -LiteralPath (Join-Path $root 'docs\level4v2\CANONICAL_RECONSTRUCTION_INPUT_V1.json') -Encoding UTF8|ConvertFrom-Json
if($Mode -eq 'Regression'){
    Require ($input.r2UnifiedAuthority.status -eq 'Passed') 'R2 regression requires accepted R2 status.'
    Require ($input.r2UnifiedAuthority.acceptedCommit -eq '0caa4776077e58df6473e9f5760555b437bc5305') 'R2 regression accepted commit mismatch.'
    $postR2Stages=@('R3CanonicalComposition','R4DynamicInvocationAndHistory','R5RuntimeAndRecovery','R6MigrationCorpus','R7ReferenceRetirement','Complete')
    Require ($postR2Stages -contains [string]$input.nextStage) 'R2 regression does not see an accepted R2-or-later handoff.'
}

Write-Host "Level 4 v2 R2 unified authority, dependency, mutation and replay boundaries passed. Mode: $Mode"
Write-Host 'Planner cannot mint Verified; Verifier has no Planner Project dependency; Frozen requires Verified authority.'
Write-Host 'Runtime capability added: None. Runtime Candidate-policy authorization: None.'
