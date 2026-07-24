param([ValidateSet('Auto','Applied','Regression')][string]$Mode='Auto')
$ErrorActionPreference='Stop'
$root=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
function Require([bool]$Condition,[string]$Message){if(-not $Condition){throw $Message}}
if($Mode -eq 'Auto'){$Mode=if(Test-Path -LiteralPath (Join-Path $root 'docs\level4v2\R5_RUNTIME_RECOVERY_MANIFEST_V1.json') -PathType Leaf){'Regression'}else{'Applied'}}
$required=@(
'201_Level4V2DynamicInvocationModel\201_Level4V2DynamicInvocationModel.vcxproj','201_Level4V2DynamicInvocationModel\DynamicInvocationModel.h','201_Level4V2DynamicInvocationModel\DynamicInvocationModel.cpp',
'202_Level4V2DynamicInvocationPlanner\202_Level4V2DynamicInvocationPlanner.vcxproj','202_Level4V2DynamicInvocationPlanner\DynamicInvocationPlanner.h','202_Level4V2DynamicInvocationPlanner\DynamicInvocationPlanner.cpp',
'203_Level4V2DynamicInvocationVerifier\203_Level4V2DynamicInvocationVerifier.vcxproj','203_Level4V2DynamicInvocationVerifier\DynamicInvocationVerifier.h','203_Level4V2DynamicInvocationVerifier\DynamicInvocationVerifier.cpp',
'204_Level4V2FrozenInvocationHistory\204_Level4V2FrozenInvocationHistory.vcxproj','204_Level4V2FrozenInvocationHistory\FrozenInvocationHistory.h','204_Level4V2FrozenInvocationHistory\FrozenInvocationHistory.cpp',
'205_Level4V2DynamicInvocationTests\205_Level4V2DynamicInvocationTests.vcxproj','205_Level4V2DynamicInvocationTests\main.cpp',
'docs\level4v2\R4_DYNAMIC_INVOCATION_HISTORY_MANIFEST_V1.json','docs\level4v2\R4_DYNAMIC_INVOCATION_AND_HISTORY.md','docs\level4v2\R4_AUTHORITY_OWNERSHIP_V1.md','docs\level4v2\R4_MUTATION_REPLAY_MATRIX_V1.md','docs\level4v2\R4_EVIDENCE_LEDGER_V1.md','docs\level4v2\R4_CHANGESET.md','docs\level4v2\R5_RUNTIME_RECOVERY_ENTRY_CONTRACT_V1.md','docs\level4v2\APPLY_R4_JA.md','docs\level4v2\R4_PACKAGE_MANIFEST.txt','docs\level4v2\R4_PACKAGE_FILES.sha256',
'tests\Run-Level4V2R4.ps1','tests\tools\Register-Level4V2R4Projects.ps1','tests\tools\Verify-Level4V2R4.ps1','run_sge4_5_level4v2_r4_prepare.bat','run_sge4_5_level4v2_r4_dynamic_invocation_history.bat')
foreach($relative in $required){Require (Test-Path -LiteralPath (Join-Path $root $relative) -PathType Leaf) "R4 file missing: $relative"}
if($Mode -eq 'Applied'){
    $lines=@(Get-Content -LiteralPath (Join-Path $root 'docs\level4v2\R4_PACKAGE_FILES.sha256') -Encoding UTF8|Where-Object{-not [string]::IsNullOrWhiteSpace($_)})
    $m0=Get-Content -Raw -LiteralPath (Join-Path $root 'docs\level4v2\R4_DYNAMIC_INVOCATION_HISTORY_MANIFEST_V1.json') -Encoding UTF8|ConvertFrom-Json
    Require ($lines.Count -eq [int]$m0.packageHashEntryCount) 'R4 package hash-entry count mismatch.'
    foreach($line in $lines){Require ($line -match '^([0-9a-f]{64})  (.+)$') "Malformed R4 package hash line: $line";$expected=$Matches[1];$relative=$Matches[2];$path=Join-Path $root $relative;Require (Test-Path -LiteralPath $path -PathType Leaf) "R4 package file missing: $relative";$actual=(Get-FileHash -Algorithm SHA256 -LiteralPath $path).Hash.ToLowerInvariant();Require ($actual -eq $expected) "R4 package file changed after packaging: $relative"}
}
$m=Get-Content -Raw -LiteralPath (Join-Path $root 'docs\level4v2\R4_DYNAMIC_INVOCATION_HISTORY_MANIFEST_V1.json') -Encoding UTF8|ConvertFrom-Json
Require ($m.schema -eq 'SGE4.Level4V2.R4DynamicInvocationHistoryManifest.V1') 'R4 manifest schema mismatch.'
Require ($m.completionUnit -eq 'R4') 'R4 completion unit mismatch.'
if($Mode -eq 'Applied'){Require ($m.status -eq 'CompletePackageSuppliedOwnerGatePending') 'R4 supplied status mismatch.'}else{Require ($m.status -eq 'Passed') 'R4 accepted status mismatch.';Require ($m.acceptedCommit -eq '7562b0f51fcd09b3840f36a4725734218b291e52') 'R4 accepted commit mismatch.'}
Require ($m.baseCommit -eq '09b56250c88bd52b7e02a2510cd6cf2b7e814bde') 'R4 base commit mismatch.'
Require ($m.r3AcceptedCommit -eq '09b56250c88bd52b7e02a2510cd6cf2b7e814bde') 'R4 accepted R3 commit mismatch.'
Require ($m.r3DeterministicEvidenceSha256 -eq 'dab306b33d0816b742c61611c28bf054d2df5dbfe2e39179a1fae4c7fb70befb') 'R4 accepted R3 Evidence mismatch.'
Require (@($m.projects).Count -eq 5) 'R4 Project count mismatch.'
Require (@($m.exactDerivedSets).Count -eq 5) 'R4 exact derived-set count mismatch.'
Require ($m.historyReferencedSet -eq 'PreviousActiveSet') 'R4 previous-History set binding mismatch.'
Require ([bool]$m.exactSetCanonicalization.declarationOrderIdentityInvariant) 'R4 exact-set identity is not declaration-order invariant.'
Require ($m.transitionAlgebra.writeRule -eq 'ExactDynamicWriteSetEqualsTransitionSet') 'R4 exact write rule mismatch.'
Require ($m.transitionAlgebra.indirectQuantityRule -eq 'VerifiedIndirectWorkCountEqualsTransitionCount') 'R4 indirect quantity rule mismatch.'
Require ([bool]$m.historyAuthority.staleEpochAccepted -eq $false) 'R4 accepts stale History epoch.'
Require ([bool]$m.independence.modelRequiresR3OpaqueFrozenComposition) 'R4 is not R3 Frozen-Composition-bound.'
Require (-not [bool]$m.independence.verifierProjectDependsOnPlannerProject) 'R4 Verifier depends on Planner.'
Require ([bool]$m.independence.verifierReconstructsMembershipAndTransitionAlgebra) 'R4 Verifier does not reconstruct exact algebra.'
Require (-not [bool]$m.independence.runtimeMayInferMembership -and -not [bool]$m.independence.runtimeMaySelectCandidate) 'R4 authorized Runtime inference or Candidate policy.'
Require ([int]$m.acceptedScenarioCount -eq 8 -and @($m.acceptedScenarios).Count -eq 8) 'R4 accepted-scenario count mismatch.'
Require ([int]$m.rejectionCount -eq 27) 'R4 rejection count mismatch.'
Require ($m.deterministicEvidence.expectedSha256 -eq '78f16d2fa4185412144205dc54f842f2929e6e269711dfca9ced9967f37cfbe1') 'R4 Evidence identity mismatch.'
Require ($m.runtimeCapabilityAdded -eq 'None' -and $m.nativeExecutionAdded -eq 'None' -and $m.recoveryBehaviorAdded -eq 'None') 'R4 added Runtime/Recovery execution.'
Require ($m.schema17Mutation -eq 'None' -and $m.runtime17Mutation -eq 'None' -and $m.backendMutation -eq 'None' -and $m.compositionMutation -eq 'None') 'R4 changed a frozen boundary.'
Require ($m.runtimeCandidatePolicyAuthorization -eq 'None') 'R4 authorized Runtime Candidate policy.'
Require ($m.nextStageOnSuccessfulGate -eq 'R5RuntimeAndRecovery') 'R4 next stage mismatch.'
$projectTypeGuid='{BC8A1FFA-BEE3-4634-8014-F334798102B3}'
$projects=@(
[pscustomobject]@{Name='201_Level4V2DynamicInvocationModel';Path='201_Level4V2DynamicInvocationModel\201_Level4V2DynamicInvocationModel.vcxproj';Guid='{000000C9-0000-5000-8000-0000000000C9}'},
[pscustomobject]@{Name='202_Level4V2DynamicInvocationPlanner';Path='202_Level4V2DynamicInvocationPlanner\202_Level4V2DynamicInvocationPlanner.vcxproj';Guid='{000000CA-0000-5000-8000-0000000000CA}'},
[pscustomobject]@{Name='203_Level4V2DynamicInvocationVerifier';Path='203_Level4V2DynamicInvocationVerifier\203_Level4V2DynamicInvocationVerifier.vcxproj';Guid='{000000CB-0000-5000-8000-0000000000CB}'},
[pscustomobject]@{Name='204_Level4V2FrozenInvocationHistory';Path='204_Level4V2FrozenInvocationHistory\204_Level4V2FrozenInvocationHistory.vcxproj';Guid='{000000CC-0000-5000-8000-0000000000CC}'},
[pscustomobject]@{Name='205_Level4V2DynamicInvocationTests';Path='205_Level4V2DynamicInvocationTests\205_Level4V2DynamicInvocationTests.vcxproj';Guid='{000000CD-0000-5000-8000-0000000000CD}'})
$solutionLines=[IO.File]::ReadAllLines((Join-Path $root 'SemanticGpuEngine4-5.sln'))
foreach($project in $projects){
    [xml]$xml=Get-Content -Raw -LiteralPath (Join-Path $root $project.Path) -Encoding UTF8
    $guid=[string](($xml.Project.PropertyGroup|Where-Object{$_.Label -eq 'Globals'}|Select-Object -First 1).ProjectGuid)
    Require ($guid.ToUpperInvariant() -eq $project.Guid.ToUpperInvariant()) "R4 Project GUID mismatch: $($project.Path)"
    $line='Project("'+$projectTypeGuid+'") = "'+$project.Name+'", "'+$project.Path+'", "'+$project.Guid+'"'
    Require (@($solutionLines|Where-Object{$_.Trim() -eq $line}).Count -eq 1) "R4 Solution registration mismatch: $($project.Name)"
    foreach($entry in @("$($project.Guid).Debug|x64.ActiveCfg = Debug|x64","$($project.Guid).Debug|x64.Build.0 = Debug|x64","$($project.Guid).Release|x64.ActiveCfg = Release|x64","$($project.Guid).Release|x64.Build.0 = Release|x64")){Require (@($solutionLines|Where-Object{$_.Trim() -eq $entry}).Count -eq 1) "R4 Solution configuration mismatch: $entry"}
}
$modelProject=Get-Content -Raw -LiteralPath (Join-Path $root '201_Level4V2DynamicInvocationModel\201_Level4V2DynamicInvocationModel.vcxproj') -Encoding UTF8
$plannerProject=Get-Content -Raw -LiteralPath (Join-Path $root '202_Level4V2DynamicInvocationPlanner\202_Level4V2DynamicInvocationPlanner.vcxproj') -Encoding UTF8
$verifierProject=Get-Content -Raw -LiteralPath (Join-Path $root '203_Level4V2DynamicInvocationVerifier\203_Level4V2DynamicInvocationVerifier.vcxproj') -Encoding UTF8
$frozenProject=Get-Content -Raw -LiteralPath (Join-Path $root '204_Level4V2FrozenInvocationHistory\204_Level4V2FrozenInvocationHistory.vcxproj') -Encoding UTF8
Require ($modelProject -match '199_Level4V2FrozenComposition') 'R4 model is not R3 Frozen Composition-bound.'
Require ($plannerProject -notmatch 'DynamicInvocationVerifier|FrozenInvocationHistory') 'R4 Planner has forbidden downstream dependency.'
Require ($verifierProject -notmatch 'DynamicInvocationPlanner|FrozenInvocationHistory') 'R4 Verifier has forbidden Planner/Frozen dependency.'
Require ($frozenProject -notmatch 'DynamicInvocationPlanner') 'R4 Frozen builder depends on Planner.'
$model=Get-Content -Raw -LiteralPath (Join-Path $root '201_Level4V2DynamicInvocationModel\DynamicInvocationModel.h') -Encoding UTF8
$planner=Get-Content -Raw -LiteralPath (Join-Path $root '202_Level4V2DynamicInvocationPlanner\DynamicInvocationPlanner.cpp') -Encoding UTF8
$verifier=Get-Content -Raw -LiteralPath (Join-Path $root '203_Level4V2DynamicInvocationVerifier\DynamicInvocationVerifier.cpp') -Encoding UTF8
$frozen=Get-Content -Raw -LiteralPath (Join-Path $root '204_Level4V2FrozenInvocationHistory\FrozenInvocationHistory.h') -Encoding UTF8
foreach($token in @('ExactIndexSetV1','DynamicInvocationRequestV1','VerifiedHistoryStateV1','DynamicDecisionV1','VerifiedDynamicInvocationV1','InitialSeed','ContinueHistory','RecoverySeed','ActivationSetMismatch','RetainSetMismatch','DynamicWriteSetMismatch')){Require ($model -match [regex]::Escape($token)) "R4 model token missing: $token"}
Require ($model -match 'const frozen_composition::OpaqueFrozenCompositionV1&') 'R4 request factory does not require R3 Opaque Frozen Composition.'
Require ($planner -match 'Difference' -and $planner -match 'Intersection' -and $planner -match 'Union') 'R4 Planner set construction missing.'
Require ($verifier -match 'ExpectedDifference' -and $verifier -match 'ExpectedIntersection' -and $verifier -match 'ExpectedUnion') 'R4 independent expected-set reconstruction missing.'
Require ($verifier -notmatch 'DynamicInvocationPlannerV1|DynamicInvocationPlanner.h') 'R4 Verifier trusts or includes Planner.'
Require ($frozen -match 'Freeze\(const VerifiedDynamicInvocationV1&') 'R4 Frozen builder does not require Verified Dynamic Invocation.'
Require ($frozen -match 'InvocationModeV1 Mode\(\)' -and $frozen -match 'mode_') 'R4 Frozen invocation does not expose verified mode for R5.'
Require ($frozen -notmatch 'DynamicInvocationRequestV1|DynamicPlannerProposalV1') 'R4 Frozen public boundary accepts Raw/Planner input.'
$public=$model+"`n"+(Get-Content -Raw -LiteralPath (Join-Path $root '202_Level4V2DynamicInvocationPlanner\DynamicInvocationPlanner.h') -Encoding UTF8)+"`n"+(Get-Content -Raw -LiteralPath (Join-Path $root '203_Level4V2DynamicInvocationVerifier\DynamicInvocationVerifier.h') -Encoding UTF8)+"`n"+$frozen
foreach($forbidden in @('D3D12','DXGI','WARP','RTX4070','Game','ApplicationPolicy','TextureFlow','ConditionalRegion','MultiDevice','RuntimeCandidatePolicy')){Require ($public -notmatch [regex]::Escape($forbidden)) "Forbidden public R4 token: $forbidden"}
foreach($literal in @('4096','128','64')){Require ($public -notmatch "(?<![A-Za-z0-9_])$literal(?:[uUlL]*)(?![A-Za-z0-9_])") "R4 generic source freezes universal limit: $literal"}
$test=Get-Content -Raw -LiteralPath (Join-Path $root '205_Level4V2DynamicInvocationTests\main.cpp') -Encoding UTF8
foreach($token in @('accepted.reserve(8u)','observed.reserve(27u)','InitialSeed','ContinueHistory','RecoverySeed','exact-set identity depends on declaration order','update/retain partition does not reconstruct current Active set','per-item generation mismatch','retained member entered exact write set','HistoryEpochMismatch','FrozenDynamicInvocationBuilderV1::Freeze','!std::is_default_constructible_v<dyn::VerifiedDynamicInvocationV1>')){Require ($test -match [regex]::Escape($token)) "R4 test token missing: $token"}
$input=Get-Content -Raw -LiteralPath (Join-Path $root 'docs\level4v2\CANONICAL_RECONSTRUCTION_INPUT_V1.json') -Encoding UTF8|ConvertFrom-Json
Require ($input.r3CanonicalComposition.status -eq 'Passed' -and $input.r3CanonicalComposition.acceptedCommit -eq '09b56250c88bd52b7e02a2510cd6cf2b7e814bde') 'R4 input does not bind accepted R3.'
if($Mode -eq 'Applied'){Require ($input.nextStage -eq 'R5RuntimeAndRecovery') 'R4 handoff does not advance to R5.'}
Write-Host "Level 4 v2 R4 exact Dynamic Invocation, History, sparse membership and delta authority boundaries passed. Mode: $Mode"
Write-Host 'Accepted scenarios: 8. Exact-set/request/proposal rejections: 27.'
Write-Host 'Runtime capability added: None. Runtime Candidate-policy authorization: None.'
