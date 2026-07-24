param([ValidateSet('Auto','Applied','Regression')][string]$Mode='Auto')
$ErrorActionPreference='Stop'
$root=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
function Require([bool]$Condition,[string]$Message){if(-not $Condition){throw $Message}}
if($Mode -eq 'Auto'){$Mode=if(Test-Path -LiteralPath (Join-Path $root 'docs\level4v2\R4_DYNAMIC_INVOCATION_HISTORY_MANIFEST_V1.json') -PathType Leaf){'Regression'}else{'Applied'}}
$required=@(
'196_Level4V2CompositionModel\196_Level4V2CompositionModel.vcxproj','196_Level4V2CompositionModel\CompositionModel.h','196_Level4V2CompositionModel\CompositionModel.cpp',
'197_Level4V2CompositionPlanner\197_Level4V2CompositionPlanner.vcxproj','197_Level4V2CompositionPlanner\CompositionPlanner.h','197_Level4V2CompositionPlanner\CompositionPlanner.cpp',
'198_Level4V2CompositionVerifier\198_Level4V2CompositionVerifier.vcxproj','198_Level4V2CompositionVerifier\CompositionVerifier.h','198_Level4V2CompositionVerifier\CompositionVerifier.cpp',
'199_Level4V2FrozenComposition\199_Level4V2FrozenComposition.vcxproj','199_Level4V2FrozenComposition\FrozenComposition.h','199_Level4V2FrozenComposition\FrozenComposition.cpp',
'200_Level4V2CompositionTests\200_Level4V2CompositionTests.vcxproj','200_Level4V2CompositionTests\main.cpp',
'docs\level4v2\R3_CANONICAL_COMPOSITION_MANIFEST_V1.json','docs\level4v2\R3_CANONICAL_COMPOSITION.md','docs\level4v2\R3_AUTHORITY_OWNERSHIP_V1.md','docs\level4v2\R3_MUTATION_REPLAY_MATRIX_V1.md','docs\level4v2\R3_EVIDENCE_LEDGER_V1.md','docs\level4v2\R3_CHANGESET.md','docs\level4v2\R4_DYNAMIC_INVOCATION_HISTORY_ENTRY_CONTRACT_V1.md','docs\level4v2\APPLY_R3_JA.md','docs\level4v2\R3_PACKAGE_MANIFEST.txt','docs\level4v2\R3_PACKAGE_FILES.sha256',
'tests\Run-Level4V2R3.ps1','tests\tools\Register-Level4V2R3Projects.ps1','tests\tools\Verify-Level4V2R3.ps1','run_sge4_5_level4v2_r3_prepare.bat','run_sge4_5_level4v2_r3_canonical_composition.bat')
foreach($relative in $required){Require (Test-Path -LiteralPath (Join-Path $root $relative) -PathType Leaf) "R3 file missing: $relative"}
if($Mode -eq 'Applied'){
    $lines=@(Get-Content -LiteralPath (Join-Path $root 'docs\level4v2\R3_PACKAGE_FILES.sha256') -Encoding UTF8|Where-Object{-not [string]::IsNullOrWhiteSpace($_)})
    $m0=Get-Content -Raw -LiteralPath (Join-Path $root 'docs\level4v2\R3_CANONICAL_COMPOSITION_MANIFEST_V1.json') -Encoding UTF8|ConvertFrom-Json
    Require ($lines.Count -eq [int]$m0.packageHashEntryCount) 'R3 package hash-entry count mismatch.'
    foreach($line in $lines){Require ($line -match '^([0-9a-f]{64})  (.+)$') "Malformed R3 package hash line: $line";$expected=$Matches[1];$relative=$Matches[2];$path=Join-Path $root $relative;Require (Test-Path -LiteralPath $path -PathType Leaf) "R3 package file missing: $relative";$actual=(Get-FileHash -Algorithm SHA256 -LiteralPath $path).Hash.ToLowerInvariant();Require ($actual -eq $expected) "R3 package file changed after packaging: $relative"}
}
$m=Get-Content -Raw -LiteralPath (Join-Path $root 'docs\level4v2\R3_CANONICAL_COMPOSITION_MANIFEST_V1.json') -Encoding UTF8|ConvertFrom-Json
Require ($m.schema -eq 'SGE4.Level4V2.R3CanonicalCompositionManifest.V1') 'R3 manifest schema mismatch.'
Require ($m.completionUnit -eq 'R3') 'R3 completion unit mismatch.'
if($Mode -eq 'Applied'){Require ($m.status -eq 'CompletePackageSuppliedOwnerGatePending') 'R3 supplied status mismatch.'}else{Require ($m.status -eq 'Passed') 'R3 accepted status mismatch.';Require ($m.acceptedCommit -eq '09b56250c88bd52b7e02a2510cd6cf2b7e814bde') 'R3 accepted commit mismatch.'}
Require ($m.baseCommit -eq '0caa4776077e58df6473e9f5760555b437bc5305') 'R3 base commit mismatch.'
Require ($m.r2AcceptedCommit -eq '0caa4776077e58df6473e9f5760555b437bc5305') 'R3 accepted R2 commit mismatch.'
Require (@($m.projects).Count -eq 5) 'R3 Project count mismatch.'
Require ($m.provenSubset.graph -eq 'FiniteStaticDag' -and $m.provenSubset.resourceKind -eq 'BufferOnly') 'R3 proven graph subset mismatch.'
Require ($m.provenSubset.writerRule -eq 'ExactlyOneWriterPerInternalOrOutputFlow') 'R3 single-writer rule mismatch.'
Require ($m.provenSubset.presenterRule -eq 'ZeroOrOne') 'R3 Presenter rule mismatch.'
Require ($m.provenSubset.recoveryScope -eq 'WholeComposition') 'R3 Recovery scope mismatch.'
Require (-not [bool]$m.independence.compositionModelAcceptsRawR2Candidate) 'R3 Composition accepts Raw R2 Candidate.'
Require ([bool]$m.independence.leafAuthorityRequiresR2OpaqueFrozenArtifact) 'R3 leaf authority is not R2 Frozen-bound.'
Require (-not [bool]$m.independence.compositionVerifierProjectDependsOnCompositionPlannerProject) 'R3 Verifier depends on Planner.'
Require ([bool]$m.independence.compositionVerifierReconstructsExpectedPlan) 'R3 Verifier does not reconstruct expected plan.'
Require ([int]$m.mutationAndContractRejectionCount -eq 24) 'R3 rejection count mismatch.'
Require (@($m.migrationScenarios).Count -eq 7) 'R3 migration scenario count mismatch.'
Require ($m.deterministicEvidence.expectedSha256 -eq 'dab306b33d0816b742c61611c28bf054d2df5dbfe2e39179a1fae4c7fb70befb') 'R3 Evidence identity mismatch.'
Require ($m.runtimeCapabilityAdded -eq 'None' -and $m.runtimeCandidatePolicyAuthorization -eq 'None') 'R3 added Runtime authority.'
Require ($m.schema17Mutation -eq 'None' -and $m.runtime17Mutation -eq 'None' -and $m.backendMutation -eq 'None' -and $m.runtimeMutation -eq 'None') 'R3 changed a frozen boundary.'
Require ($m.nextStageOnSuccessfulGate -eq 'R4DynamicInvocationAndHistory') 'R3 historical next stage mismatch.'
$projectTypeGuid='{BC8A1FFA-BEE3-4634-8014-F334798102B3}'
$projects=@(
[pscustomobject]@{Name='196_Level4V2CompositionModel';Path='196_Level4V2CompositionModel\196_Level4V2CompositionModel.vcxproj';Guid='{000000C4-0000-5000-8000-0000000000C4}'},
[pscustomobject]@{Name='197_Level4V2CompositionPlanner';Path='197_Level4V2CompositionPlanner\197_Level4V2CompositionPlanner.vcxproj';Guid='{000000C5-0000-5000-8000-0000000000C5}'},
[pscustomobject]@{Name='198_Level4V2CompositionVerifier';Path='198_Level4V2CompositionVerifier\198_Level4V2CompositionVerifier.vcxproj';Guid='{000000C6-0000-5000-8000-0000000000C6}'},
[pscustomobject]@{Name='199_Level4V2FrozenComposition';Path='199_Level4V2FrozenComposition\199_Level4V2FrozenComposition.vcxproj';Guid='{000000C7-0000-5000-8000-0000000000C7}'},
[pscustomobject]@{Name='200_Level4V2CompositionTests';Path='200_Level4V2CompositionTests\200_Level4V2CompositionTests.vcxproj';Guid='{000000C8-0000-5000-8000-0000000000C8}'})
$solutionLines=[IO.File]::ReadAllLines((Join-Path $root 'SemanticGpuEngine4-5.sln'))
foreach($project in $projects){[xml]$xml=Get-Content -Raw -LiteralPath (Join-Path $root $project.Path) -Encoding UTF8;$guid=[string](($xml.Project.PropertyGroup|Where-Object{$_.Label -eq 'Globals'}|Select-Object -First 1).ProjectGuid);Require ($guid.ToUpperInvariant() -eq $project.Guid.ToUpperInvariant()) "R3 Project GUID mismatch: $($project.Path)";$canonicalLine='Project("'+$projectTypeGuid+'") = "'+$project.Name+'", "'+$project.Path+'", "'+$project.Guid+'"';Require (@($solutionLines|Where-Object{$_.Trim() -eq $canonicalLine}).Count -eq 1) "R3 Solution registration mismatch: $($project.Name)";foreach($entry in @("$($project.Guid).Debug|x64.ActiveCfg = Debug|x64","$($project.Guid).Debug|x64.Build.0 = Debug|x64","$($project.Guid).Release|x64.ActiveCfg = Release|x64","$($project.Guid).Release|x64.Build.0 = Release|x64")){Require (@($solutionLines|Where-Object{$_.Trim() -eq $entry}).Count -eq 1) "R3 Solution configuration mismatch: $entry"}}
$modelProject=Get-Content -Raw -LiteralPath (Join-Path $root '196_Level4V2CompositionModel\196_Level4V2CompositionModel.vcxproj') -Encoding UTF8
$plannerProject=Get-Content -Raw -LiteralPath (Join-Path $root '197_Level4V2CompositionPlanner\197_Level4V2CompositionPlanner.vcxproj') -Encoding UTF8
$verifierProject=Get-Content -Raw -LiteralPath (Join-Path $root '198_Level4V2CompositionVerifier\198_Level4V2CompositionVerifier.vcxproj') -Encoding UTF8
$frozenProject=Get-Content -Raw -LiteralPath (Join-Path $root '199_Level4V2FrozenComposition\199_Level4V2FrozenComposition.vcxproj') -Encoding UTF8
Require ($modelProject -match '194_Level4V2FrozenAuthority') 'R3 model is not R2 Frozen-bound.'
Require ($plannerProject -notmatch 'CompositionVerifier|FrozenComposition') 'R3 Planner has forbidden downstream dependency.'
Require ($verifierProject -notmatch 'CompositionPlanner|FrozenComposition') 'R3 Verifier has forbidden Planner/Frozen dependency.'
Require ($frozenProject -notmatch 'CompositionPlanner') 'R3 Frozen builder depends on Planner.'
$model=Get-Content -Raw -LiteralPath (Join-Path $root '196_Level4V2CompositionModel\CompositionModel.h') -Encoding UTF8
$planner=Get-Content -Raw -LiteralPath (Join-Path $root '197_Level4V2CompositionPlanner\CompositionPlanner.cpp') -Encoding UTF8
$verifier=Get-Content -Raw -LiteralPath (Join-Path $root '198_Level4V2CompositionVerifier\CompositionVerifier.cpp') -Encoding UTF8
$frozen=Get-Content -Raw -LiteralPath (Join-Path $root '199_Level4V2FrozenComposition\FrozenComposition.h') -Encoding UTF8
foreach($token in @('LeafAuthorityV1','EndpointAuthorityV1','RawCompositionContractV1','CompositionPlanProposalV1','VerifiedCompositionV1','RecoveryScopeV1','WholeComposition','Buffer','CompositionInput','CompositionOutput')){Require ($model -match [regex]::Escape($token)) "R3 model token missing: $token"}
Require ($model -match 'const frozen_authority::OpaqueFrozenArtifactV1&') 'R3 leaf factory does not require R2 Opaque Frozen authority.'
Require ($planner -match 'BuildPlannerSchedule' -and $planner -match 'BuildPlannerProposal') 'R3 Planner reconstruction missing.'
Require ($verifier -match 'BuildExpectedScheduleV1' -and $verifier -match 'BuildExpectedCompositionPlanV1') 'R3 independent expected-plan reconstruction missing.'
Require ($verifier -notmatch 'CompositionPlannerV1|CompositionPlanner.h') 'R3 Verifier trusts or includes Planner.'
Require ($frozen -match 'Freeze\(const VerifiedCompositionV1&') 'R3 Frozen builder does not require Verified Composition.'
Require ($frozen -notmatch 'RawCompositionContractV1|CompositionPlanProposalV1') 'R3 Frozen public boundary accepts Raw/Planner input.'
$public=$model+"`n"+(Get-Content -Raw -LiteralPath (Join-Path $root '197_Level4V2CompositionPlanner\CompositionPlanner.h') -Encoding UTF8)+"`n"+(Get-Content -Raw -LiteralPath (Join-Path $root '198_Level4V2CompositionVerifier\CompositionVerifier.h') -Encoding UTF8)+"`n"+$frozen
foreach($forbidden in @('D3D12','DXGI','WARP','RTX4070','TextureFlow','ConditionalRegion','MultiDevice','RuntimeCandidatePolicy')){Require ($public -notmatch [regex]::Escape($forbidden)) "Forbidden public R3 token: $forbidden"}
$test=Get-Content -Raw -LiteralPath (Join-Path $root '200_Level4V2CompositionTests\main.cpp') -Encoding UTF8
foreach($token in @('BuildIndependent','BuildLinear','BuildFanOut','BuildMultiInput','BuildDiamond','observed.reserve(24u)','declaration order changed frozen identity','FrozenCompositionBuilderV1::Freeze','!std::is_default_constructible_v<comp::VerifiedCompositionV1>')){Require ($test -match [regex]::Escape($token)) "R3 test token missing: $token"}
if($Mode -eq 'Regression'){
    $input=Get-Content -Raw -LiteralPath (Join-Path $root 'docs\level4v2\CANONICAL_RECONSTRUCTION_INPUT_V1.json') -Encoding UTF8|ConvertFrom-Json
    Require ($input.r3CanonicalComposition.status -eq 'Passed') 'R3 regression requires accepted R3 status.'
    Require ($input.r3CanonicalComposition.acceptedCommit -eq '09b56250c88bd52b7e02a2510cd6cf2b7e814bde') 'R3 regression accepted commit mismatch.'
    $postR3Stages=@('R4DynamicInvocationAndHistory','R5RuntimeAndRecovery','R6MigrationCorpus','R7ReferenceRetirement','Complete')
    Require ($postR3Stages -contains [string]$input.nextStage) 'R3 regression does not see an accepted R3-or-later handoff.'
}
Write-Host "Level 4 v2 R3 finite Buffer DAG, independent verification and Frozen authority boundaries passed. Mode: $Mode"
Write-Host 'Graph scenarios: 7. Contract/proposal rejections: 24.'
Write-Host 'Runtime capability added: None. Runtime Candidate-policy authorization: None.'
