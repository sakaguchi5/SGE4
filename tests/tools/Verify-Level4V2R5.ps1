param([ValidateSet('Applied','Regression')][string]$Mode='Applied')
$ErrorActionPreference='Stop'
$root=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
function Require([bool]$Condition,[string]$Message){if(-not $Condition){throw $Message}}
$required=@(
'206_Level4V2RuntimeModel\206_Level4V2RuntimeModel.vcxproj','206_Level4V2RuntimeModel\RuntimeModel.h','206_Level4V2RuntimeModel\RuntimeModel.cpp',
'207_Level4V2RuntimeMaterialization\207_Level4V2RuntimeMaterialization.vcxproj','207_Level4V2RuntimeMaterialization\RuntimeMaterialization.h','207_Level4V2RuntimeMaterialization\RuntimeMaterialization.cpp',
'208_Level4V2RuntimeSubmission\208_Level4V2RuntimeSubmission.vcxproj','208_Level4V2RuntimeSubmission\RuntimeSubmission.h','208_Level4V2RuntimeSubmission\RuntimeSubmission.cpp',
'209_Level4V2RuntimeRecovery\209_Level4V2RuntimeRecovery.vcxproj','209_Level4V2RuntimeRecovery\RuntimeRecovery.h','209_Level4V2RuntimeRecovery\RuntimeRecovery.cpp',
'210_Level4V2RuntimeRecoveryTests\210_Level4V2RuntimeRecoveryTests.vcxproj','210_Level4V2RuntimeRecoveryTests\main.cpp',
'211_Level4V2RuntimeWindowsQualification\211_Level4V2RuntimeWindowsQualification.vcxproj','211_Level4V2RuntimeWindowsQualification\main.cpp',
'docs\level4v2\R5_RUNTIME_RECOVERY_MANIFEST_V1.json','docs\level4v2\R5_RUNTIME_AND_RECOVERY.md','docs\level4v2\R5_AUTHORITY_OWNERSHIP_V1.md','docs\level4v2\R5_RECOVERY_STATE_MACHINE_V1.md','docs\level4v2\R5_EVIDENCE_LEDGER_V1.md','docs\level4v2\R5_CHANGESET.md','docs\level4v2\R6_MIGRATION_CORPUS_ENTRY_CONTRACT_V1.md','docs\level4v2\APPLY_R5_JA.md','docs\level4v2\R5_PACKAGE_MANIFEST.txt','docs\level4v2\R5_PACKAGE_FILES.sha256',
'tests\Run-Level4V2R5.ps1','tests\tools\Register-Level4V2R5Projects.ps1','tests\tools\Verify-Level4V2R5.ps1','run_sge4_5_level4v2_r5_prepare.bat','run_sge4_5_level4v2_r5_runtime_recovery.bat')
foreach($relative in $required){Require (Test-Path -LiteralPath (Join-Path $root $relative) -PathType Leaf) "R5 file missing: $relative"}
if($Mode -eq 'Applied'){$lines=@(Get-Content -LiteralPath (Join-Path $root 'docs\level4v2\R5_PACKAGE_FILES.sha256') -Encoding UTF8|Where-Object{-not[string]::IsNullOrWhiteSpace($_)});$m0=Get-Content -Raw -LiteralPath (Join-Path $root 'docs\level4v2\R5_RUNTIME_RECOVERY_MANIFEST_V1.json') -Encoding UTF8|ConvertFrom-Json;Require ($lines.Count -eq [int]$m0.packageHashEntryCount) 'R5 package hash-entry count mismatch.';foreach($line in $lines){Require ($line -match '^([0-9a-f]{64})  (.+)$') "Malformed R5 package hash line: $line";$expected=$Matches[1];$relative=$Matches[2];$path=Join-Path $root $relative;Require (Test-Path -LiteralPath $path -PathType Leaf) "R5 package file missing: $relative";$actual=(Get-FileHash -Algorithm SHA256 -LiteralPath $path).Hash.ToLowerInvariant();Require ($actual -eq $expected) "R5 package file changed after packaging: $relative"}}
$m=Get-Content -Raw -LiteralPath (Join-Path $root 'docs\level4v2\R5_RUNTIME_RECOVERY_MANIFEST_V1.json') -Encoding UTF8|ConvertFrom-Json
Require ($m.schema -eq 'SGE4.Level4V2.R5RuntimeRecoveryManifest.V1' -and $m.completionUnit -eq 'R5') 'R5 manifest identity mismatch.'
if($Mode -eq 'Applied'){Require ($m.status -eq 'CompletePackageSuppliedOwnerGatePending') 'R5 supplied status mismatch.'}else{Require ($m.status -eq 'Passed') 'R5 accepted status mismatch.'}
Require ($m.baseCommit -eq '7562b0f51fcd09b3840f36a4725734218b291e52' -and $m.r4AcceptedCommit -eq '7562b0f51fcd09b3840f36a4725734218b291e52') 'R5 base/R4 acceptance mismatch.'
Require (@($m.projects).Count -eq 6) 'R5 Project count mismatch.'
Require ($m.authorityConsumption.materializationInput -eq 'R3OpaqueFrozenComposition' -and $m.authorityConsumption.submissionInput -eq 'R4OpaqueFrozenDynamicInvocation') 'R5 Frozen authority input mismatch.'
Require (-not[bool]$m.authorityConsumption.rawCandidateAccepted -and -not[bool]$m.authorityConsumption.plannerProposalAccepted) 'R5 accepts Raw/Planner authority.'
Require ($m.recoveryRules.scope -eq 'WholeComposition' -and $m.recoveryRules.firstPostRecoveryInvocation -eq 'R4RecoverySeed') 'R5 Recovery rule mismatch.'
Require ($m.runtimeCandidatePolicyAuthorization -eq 'None') 'R5 authorized Runtime Candidate policy.'
Require ([int]$m.canonicalQualification.acceptedScenarioCount -eq 10 -and [int]$m.canonicalQualification.rejectionCount -eq 12) 'R5 Canonical qualification count mismatch.'
Require ($m.canonicalQualification.expectedSha256 -eq '40d78dc35894a55e50b314bd0afcbb60966f92e942ac70856155964cd8e66498') 'R5 Canonical Evidence identity mismatch.'
Require ($m.windowsQualification.expectedSha256 -eq '1e4adaa04dc8d1d235f8f3438f4295b09b36480174f6a7d9fa0fbaba79ea7a9a') 'R5 Windows Evidence identity mismatch.'
Require ($m.nextStageOnSuccessfulGate -eq 'R6MigrationCorpus') 'R5 next stage mismatch.'
$projects=@(
@('206_Level4V2RuntimeModel','{000000CE-0000-5000-8000-0000000000CE}'),@('207_Level4V2RuntimeMaterialization','{000000CF-0000-5000-8000-0000000000CF}'),@('208_Level4V2RuntimeSubmission','{000000D0-0000-5000-8000-0000000000D0}'),@('209_Level4V2RuntimeRecovery','{000000D1-0000-5000-8000-0000000000D1}'),@('210_Level4V2RuntimeRecoveryTests','{000000D2-0000-5000-8000-0000000000D2}'),@('211_Level4V2RuntimeWindowsQualification','{000000D3-0000-5000-8000-0000000000D3}'))
$solution=[IO.File]::ReadAllLines((Join-Path $root 'SemanticGpuEngine4-5.sln'));foreach($p in $projects){$name=$p[0];$guid=$p[1];$path="$name\$name.vcxproj";[xml]$xml=Get-Content -Raw -LiteralPath (Join-Path $root $path) -Encoding UTF8;$actual=[string](($xml.Project.PropertyGroup|Where-Object{$_.Label -eq 'Globals'}|Select-Object -First 1).ProjectGuid);Require ($actual.ToUpperInvariant() -eq $guid.ToUpperInvariant()) "R5 Project GUID mismatch: $name";Require (@($solution|Where-Object{$_.Contains('"'+$name+'"') -and $_.Contains('"'+$guid+'"')}).Count -eq 1) "R5 Solution registration mismatch: $name";foreach($entry in @("$($guid).Debug|x64.ActiveCfg = Debug|x64","$($guid).Debug|x64.Build.0 = Debug|x64","$($guid).Release|x64.ActiveCfg = Release|x64","$($guid).Release|x64.Build.0 = Release|x64")){Require (@($solution|Where-Object{$_.Trim() -eq $entry}).Count -eq 1) "R5 Solution configuration mismatch: $entry"}}
$modelProject=Get-Content -Raw -LiteralPath (Join-Path $root '206_Level4V2RuntimeModel\206_Level4V2RuntimeModel.vcxproj') -Encoding UTF8
$materialProject=Get-Content -Raw -LiteralPath (Join-Path $root '207_Level4V2RuntimeMaterialization\207_Level4V2RuntimeMaterialization.vcxproj') -Encoding UTF8
$submitProject=Get-Content -Raw -LiteralPath (Join-Path $root '208_Level4V2RuntimeSubmission\208_Level4V2RuntimeSubmission.vcxproj') -Encoding UTF8
$recoveryProject=Get-Content -Raw -LiteralPath (Join-Path $root '209_Level4V2RuntimeRecovery\209_Level4V2RuntimeRecovery.vcxproj') -Encoding UTF8
Require ($modelProject -match '204_Level4V2FrozenInvocationHistory') 'R5 model is not R4 Frozen-bound.'
Require ($materialProject -match '206_Level4V2RuntimeModel' -and $materialProject -notmatch 'RuntimeSubmission|RuntimeRecovery') 'R5 materialization dependency mismatch.'
Require ($submitProject -match '207_Level4V2RuntimeMaterialization' -and $submitProject -notmatch 'RuntimeRecovery') 'R5 submission dependency mismatch.'
Require ($recoveryProject -match '208_Level4V2RuntimeSubmission') 'R5 Recovery dependency mismatch.'
$public=(Get-Content -Raw -LiteralPath (Join-Path $root '206_Level4V2RuntimeModel\RuntimeModel.h') -Encoding UTF8)+"`n"+(Get-Content -Raw -LiteralPath (Join-Path $root '207_Level4V2RuntimeMaterialization\RuntimeMaterialization.h') -Encoding UTF8)+"`n"+(Get-Content -Raw -LiteralPath (Join-Path $root '208_Level4V2RuntimeSubmission\RuntimeSubmission.h') -Encoding UTF8)+"`n"+(Get-Content -Raw -LiteralPath (Join-Path $root '209_Level4V2RuntimeRecovery\RuntimeRecovery.h') -Encoding UTF8)
foreach($token in @('OpaqueFrozenCompositionV1','OpaqueFrozenDynamicInvocationV1','ActiveNeedsExternalRebind','AwaitingAdapter','ExternalBindingSetV1','ValidateRuntimeHandleEpochV1','RecoverySeed','ContainsExcludedAdapterV1')){Require ($public -match [regex]::Escape($token)) "R5 public token missing: $token"}
foreach($forbidden in @('D3D12','DXGI','WARP','RTX4070','GamePolicy','RuntimeCandidatePolicy')){Require ($public -notmatch [regex]::Escape($forbidden)) "Forbidden Canonical R5 token: $forbidden"}
$r4Header=Get-Content -Raw -LiteralPath (Join-Path $root '204_Level4V2FrozenInvocationHistory\FrozenInvocationHistory.h') -Encoding UTF8;$r4Core=Get-Content -Raw -LiteralPath (Join-Path $root '204_Level4V2FrozenInvocationHistory\FrozenInvocationHistory.cpp') -Encoding UTF8
Require ($r4Header -match 'InvocationModeV1 Mode\(\)' -and $r4Core -match 'request.mode') 'R5 cannot read verified R4 RecoverySeed mode.'
$test=Get-Content -Raw -LiteralPath (Join-Path $root '210_Level4V2RuntimeRecoveryTests\main.cpp') -Encoding UTF8
foreach($token in @('Accepted scenarios: 10','Runtime/recovery rejections:','RecoverySeedRequired','ExternalRebindRequired','InvocationEpochMismatch','AwaitingAdapter','ContainsExcludedAdapterV1')){Require ($test -match [regex]::Escape($token)) "R5 test token missing: $token"}
$windows=Get-Content -Raw -LiteralPath (Join-Path $root '211_Level4V2RuntimeWindowsQualification\main.cpp') -Encoding UTF8
foreach($token in @('ControlledRebuild','ForceRemovalForTest','AwaitingAdapter','stale-epoch','RetryAdapterReacquisition')){Require ($windows -match [regex]::Escape($token)) "R5 Windows qualification token missing: $token"}
$input=Get-Content -Raw -LiteralPath (Join-Path $root 'docs\level4v2\CANONICAL_RECONSTRUCTION_INPUT_V1.json') -Encoding UTF8|ConvertFrom-Json
Require ($input.r4DynamicInvocationAndHistory.status -eq 'Passed' -and $input.r4DynamicInvocationAndHistory.acceptedCommit -eq '7562b0f51fcd09b3840f36a4725734218b291e52') 'R5 input does not bind accepted R4.'
if($Mode -eq 'Applied'){Require ($input.nextStage -eq 'R6MigrationCorpus') 'R5 handoff does not advance to R6.'}
Write-Host "Level 4 v2 R5 Frozen-only Runtime, epoch, rebind and whole-composition Recovery boundaries passed. Mode: $Mode"
Write-Host 'Canonical scenarios: 10. Rejections: 12. Windows WARP qualification required.'
Write-Host 'Runtime Candidate-policy authorization: None.'
