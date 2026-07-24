param([ValidateSet('Applied','Regression')][string]$Mode='Applied')
$ErrorActionPreference='Stop'
$root=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
function Require([bool]$Condition,[string]$Message){if(-not $Condition){throw $Message}}

$required=@(
'212_Level4V2MigrationCorpus\212_Level4V2MigrationCorpus.vcxproj','212_Level4V2MigrationCorpus\MigrationCorpus.h','212_Level4V2MigrationCorpus\MigrationCorpus.cpp',
'213_Level4V2MigrationCorpusTests\213_Level4V2MigrationCorpusTests.vcxproj','213_Level4V2MigrationCorpusTests\main.cpp',
'docs\level4v2\R6_MIGRATION_CORPUS_MANIFEST_V1.json','docs\level4v2\R6_INVARIANT_MIGRATION_LEDGER_V1.json','docs\level4v2\R6_MIGRATION_CORPUS.md','docs\level4v2\R6_MIGRATION_CASE_MATRIX_V1.md','docs\level4v2\R6_SOURCE_LINEAGE_MATRIX_V1.md','docs\level4v2\R6_CORRECTNESS_PERFORMANCE_SEPARATION_V1.md','docs\level4v2\R6_EVIDENCE_LEDGER_V1.md','docs\level4v2\R6_CHANGESET.md','docs\level4v2\R7_REFERENCE_RETIREMENT_ENTRY_CONTRACT_V1.md','docs\level4v2\APPLY_R6_JA.md','docs\level4v2\R6_PACKAGE_MANIFEST.txt','docs\level4v2\R6_PACKAGE_FILES.sha256',
'tests\Run-Level4V2R6.ps1','tests\tools\Register-Level4V2R6Projects.ps1','tests\tools\Verify-Level4V2R6.ps1','run_sge4_5_level4v2_r6_prepare.bat','run_sge4_5_level4v2_r6_migration_corpus.bat')
foreach($relative in $required){Require (Test-Path -LiteralPath (Join-Path $root $relative) -PathType Leaf) "R6 file missing: $relative"}

$m=Get-Content -Raw -LiteralPath (Join-Path $root 'docs\level4v2\R6_MIGRATION_CORPUS_MANIFEST_V1.json') -Encoding UTF8|ConvertFrom-Json
Require ($m.schema -eq 'SGE4.Level4V2.R6MigrationCorpusManifest.V1' -and $m.completionUnit -eq 'R6') 'R6 manifest identity mismatch.'
if($Mode -eq 'Applied'){Require ($m.status -eq 'CompletePackageSuppliedOwnerGatePending') 'R6 supplied status mismatch.'}else{Require ($m.status -eq 'Passed') 'R6 accepted status mismatch.';Require ([string]$m.acceptedCommit -match '^[0-9a-f]{40}$') 'R6 accepted commit missing.'}
Require ($m.baseCommit -eq '527cc0b2ed4c01c76ebfdef5c277f6c39b654024' -and $m.r5AcceptedCommit -eq '527cc0b2ed4c01c76ebfdef5c277f6c39b654024') 'R6 base/R5 acceptance mismatch.'
Require (@($m.projects).Count -eq 2) 'R6 Project count mismatch.'
Require ([int]$m.coverage.sourceLineageCount -eq 16 -and [int]$m.coverage.terminalInvariantOutcomeCount -eq 40) 'R6 source/Invariant coverage count mismatch.'
Require ([int]$m.coverage.migrationCaseCount -eq 26 -and [int]$m.coverage.correctnessCaseCount -eq 24 -and [int]$m.coverage.observationalPerformanceCaseCount -eq 2) 'R6 case-lane count mismatch.'
Require ([int]$m.coverage.certificateRejectionCount -eq 20) 'R6 rejection count mismatch.'
Require ([bool]$m.coverage.allR0InvariantsHaveTerminalOutcome -and [bool]$m.coverage.everyFrozenSourceRoleReferenced) 'R6 coverage declaration incomplete.'
Require (-not[bool]$m.correctnessPerformanceSeparation.correctnessGateConsumesObservationalSamples) 'R6 correctness gate consumes observational samples.'
Require (-not[bool]$m.correctnessPerformanceSeparation.observationalEvidenceMayAuthorizeRuntimePolicy) 'R6 observational Evidence may authorize Runtime policy.'
Require ([bool]$m.correctnessPerformanceSeparation.correctnessCasesRequireV2AuthorityBinding) 'R6 correctness cases are not V2-authority-bound.'
Require ([bool]$m.correctnessPerformanceSeparation.observationalCasesRequireRetainedEvidenceBinding -and -not[bool]$m.correctnessPerformanceSeparation.observationalEvidenceMayMintV2Authority) 'R6 observational cases may masquerade as v2 authority.'
Require ($m.correctnessPerformanceSeparation.externalBundleSha256 -eq '9ee852c2a2574a8cb9c163b6e94213b855317bba74319e6443e9aa98c10c24d9') 'R6 external Evidence bundle identity mismatch.'
Require ($m.deterministicEvidence.expectedSha256 -eq '41d8432bd1019a09f9ee92b384b3ce4f11e52db031fc38862c10ffcec7a91724') 'R6 deterministic Evidence identity mismatch.'
Require (-not[bool]$m.referenceProjects.deletedInR6 -and $m.referenceProjects.retirementDecisionDeferredTo -eq 'R7ReferenceRetirement') 'R6 performed or misplaced reference retirement.'
Require ($m.runtimeCapabilityAdded -eq 'None' -and $m.runtimeCandidatePolicyAuthorization -eq 'None') 'R6 added Runtime capability or policy.'
Require ($m.schema17Mutation -eq 'None' -and $m.runtime17Mutation -eq 'None' -and $m.backendMutation -eq 'None' -and $m.canonicalAuthorityMutation -eq 'None') 'R6 changed a frozen authority boundary.'
Require ($m.nextStageOnSuccessfulGate -eq 'R7ReferenceRetirement') 'R6 next stage mismatch.'

if($Mode -eq 'Applied'){
    $lines=@(Get-Content -LiteralPath (Join-Path $root 'docs\level4v2\R6_PACKAGE_FILES.sha256') -Encoding UTF8|Where-Object{-not[string]::IsNullOrWhiteSpace($_)})
    Require ($lines.Count -eq [int]$m.packageHashEntryCount) 'R6 package hash-entry count mismatch.'
    foreach($line in $lines){Require ($line -match '^([0-9a-f]{64})  (.+)$') "Malformed R6 package hash line: $line";$expected=$Matches[1];$relative=$Matches[2];$path=Join-Path $root $relative;Require (Test-Path -LiteralPath $path -PathType Leaf) "R6 package file missing: $relative";$actual=(Get-FileHash -Algorithm SHA256 -LiteralPath $path).Hash.ToLowerInvariant();Require ($actual -eq $expected) "R6 package file changed after packaging: $relative"}
}

$r0=Get-Content -Raw -LiteralPath (Join-Path $root 'docs\level4v2\R0_INPUT_FREEZE_MANIFEST_V1.json') -Encoding UTF8|ConvertFrom-Json
Require (@($r0.sourceFreeze).Count -eq 16) 'R0 source freeze count changed.'
Require (@($m.sourceLineages).Count -eq 16) 'R6 source lineage count mismatch.'
foreach($source in @($r0.sourceFreeze)){
    $matches=@($m.sourceLineages|Where-Object{$_.role -eq $source.role})
    Require ($matches.Count -eq 1) "R6 source role missing or duplicated: $($source.role)"
    Require ($matches[0].path -eq $source.path -and $matches[0].sha256 -eq $source.sha256) "R6 source identity mismatch: $($source.role)"
}

$ledger=Get-Content -Raw -LiteralPath (Join-Path $root 'docs\level4v2\R6_INVARIANT_MIGRATION_LEDGER_V1.json') -Encoding UTF8|ConvertFrom-Json
Require ($ledger.schema -eq 'SGE4.Level4V2.R6InvariantMigrationLedger.V1') 'R6 ledger schema mismatch.'
Require ([int]$ledger.sourceInvariantCount -eq 40 -and [int]$ledger.terminalOutcomeCount -eq 40) 'R6 ledger count mismatch.'
$entries=@($ledger.entries);Require ($entries.Count -eq 40) 'R6 ledger entry count mismatch.'
$allowed=@('Reproduced','VersionedReplacement','ExplicitlyNotCarriedWithOwnerReason')
$evidenceIds=@($m.stageEvidence|ForEach-Object{[string]$_.id})
for($ordinal=1;$ordinal -le 40;++$ordinal){$id=('V2-R0-I{0:D3}' -f $ordinal);$entry=@($entries|Where-Object{$_.id -eq $id});Require ($entry.Count -eq 1) "R6 terminal outcome missing or duplicated: $id";Require ($allowed -contains [string]$entry[0].terminalOutcome) "R6 non-terminal outcome: $id";Require ($entry[0].terminalOutcome -eq 'Reproduced') "R6 unexpected non-Reproduced outcome: $id";Require (@($entry[0].migrationCases).Count -gt 0) "R6 Invariant has no migration case: $id";foreach($ev in @($entry[0].evidenceIds)){Require ($evidenceIds -contains [string]$ev) "R6 ledger references unknown Evidence: $id/$ev"}}

$r5=Get-Content -Raw -LiteralPath (Join-Path $root 'docs\level4v2\R5_RUNTIME_RECOVERY_MANIFEST_V1.json') -Encoding UTF8|ConvertFrom-Json
Require ($r5.status -eq 'Passed' -and $r5.acceptedCommit -eq '527cc0b2ed4c01c76ebfdef5c277f6c39b654024') 'R6 does not bind accepted R5.'
$input=Get-Content -Raw -LiteralPath (Join-Path $root 'docs\level4v2\CANONICAL_RECONSTRUCTION_INPUT_V1.json') -Encoding UTF8|ConvertFrom-Json
Require ($input.r5RuntimeAndRecovery.status -eq 'Passed' -and $input.r5RuntimeAndRecovery.acceptedCommit -eq '527cc0b2ed4c01c76ebfdef5c277f6c39b654024') 'Canonical input does not bind accepted R5.'
if($Mode -eq 'Applied'){Require ($input.nextStage -eq 'R7ReferenceRetirement') 'R6 handoff does not advance to R7.'}else{Require (@('R7ReferenceRetirement','Complete') -contains [string]$input.nextStage) 'R6 regression does not see R7-or-complete handoff.'}
Require ($input.runtimeCandidatePolicyAuthorization -eq 'None') 'Canonical input authorized Runtime policy.'

$projectTypeGuid='{BC8A1FFA-BEE3-4634-8014-F334798102B3}'
$projects=@(
[pscustomobject]@{Name='212_Level4V2MigrationCorpus';Path='212_Level4V2MigrationCorpus\212_Level4V2MigrationCorpus.vcxproj';Guid='{000000D4-0000-5000-8000-0000000000D4}'},
[pscustomobject]@{Name='213_Level4V2MigrationCorpusTests';Path='213_Level4V2MigrationCorpusTests\213_Level4V2MigrationCorpusTests.vcxproj';Guid='{000000D5-0000-5000-8000-0000000000D5}'})
$solution=[IO.File]::ReadAllLines((Join-Path $root 'SemanticGpuEngine4-5.sln'))
foreach($p in $projects){[xml]$xml=Get-Content -Raw -LiteralPath (Join-Path $root $p.Path) -Encoding UTF8;$guid=[string](($xml.Project.PropertyGroup|Where-Object{$_.Label -eq 'Globals'}|Select-Object -First 1).ProjectGuid);Require ($guid.ToUpperInvariant() -eq $p.Guid.ToUpperInvariant()) "R6 Project GUID mismatch: $($p.Name)";$line='Project("'+$projectTypeGuid+'") = "'+$p.Name+'", "'+$p.Path+'", "'+$p.Guid+'"';Require (@($solution|Where-Object{$_.Trim() -eq $line}).Count -eq 1) "R6 Solution registration mismatch: $($p.Name)";foreach($entry in @("$($p.Guid).Debug|x64.ActiveCfg = Debug|x64","$($p.Guid).Debug|x64.Build.0 = Debug|x64","$($p.Guid).Release|x64.ActiveCfg = Release|x64","$($p.Guid).Release|x64.Build.0 = Release|x64")){Require (@($solution|Where-Object{$_.Trim() -eq $entry}).Count -eq 1) "R6 Solution configuration mismatch: $entry"}}

$libraryProject=Get-Content -Raw -LiteralPath (Join-Path $root '212_Level4V2MigrationCorpus\212_Level4V2MigrationCorpus.vcxproj') -Encoding UTF8
Require ($libraryProject -match '209_Level4V2RuntimeRecovery') 'R6 corpus is not bound to the complete R1-R5 authority chain.'
Require ($libraryProject -notmatch '211_Level4V2RuntimeWindowsQualification') 'R6 Canonical corpus depends on Windows qualification executable.'
$public=(Get-Content -Raw -LiteralPath (Join-Path $root '212_Level4V2MigrationCorpus\MigrationCorpus.h') -Encoding UTF8)+"`n"+(Get-Content -Raw -LiteralPath (Join-Path $root '212_Level4V2MigrationCorpus\MigrationCorpus.cpp') -Encoding UTF8)
foreach($token in @('SourceLineageV1','EvidenceLaneV1','BindingKindV1','InvariantOutcomeV1','MigrationCaseV1','OpaqueMigrationCertificateV1','SourceCoverageIncomplete','InvariantCoverageIncomplete','EvidenceLaneMismatch','BindingKindMismatch','NonTerminalOutcome','InvariantEvidenceMissing','InvalidBoundIdentity','RuntimePolicyLeak','ReferenceDeletionBeforeDecision')){Require ($public -match [regex]::Escape($token)) "R6 migration token missing: $token"}
foreach($forbidden in @('D3D12','DXGI','WARP','RTX4070','ChooseWinner','UniversalWinner')){Require ($public -notmatch [regex]::Escape($forbidden)) "Forbidden R6 Canonical token: $forbidden"}
$test=Get-Content -Raw -LiteralPath (Join-Path $root '213_Level4V2MigrationCorpusTests\main.cpp') -Encoding UTF8
foreach($token in @('Source lineages: 16','Terminal invariant outcomes: 40','Migration cases: 26','Migration-certificate rejections:','C23-ObservationIdentityAndPairedDecision','C25-ExternalRawEvidenceRetention','runtimeCandidatePolicyAuthorized=true','referenceProjectsDeleted=true','!std::is_default_constructible_v<mig::OpaqueMigrationCertificateV1>')){Require ($test -match [regex]::Escape($token)) "R6 test token missing: $token"}

foreach($relative in @('16_FrozenCompositionArtifact','20_CompositionDeviceDomain','23_CompositionRecovery','60_PgaRigidTransformSemantic','134_TemporalStateSemantic','154_SparseWorkSetSemantic','172_SparseTemporalDeltaSemantic','188_Spiral7PerformanceTests')){Require (Test-Path -LiteralPath (Join-Path $root $relative) -PathType Container) "R6 reference family deleted before R7: $relative"}

Write-Host "Level 4 v2 R6 source lineage, terminal Invariant outcomes and Migration Certificate boundaries passed. Mode: $Mode"
Write-Host 'Source lineages: 16. Terminal outcomes: 40. Migration cases: 26. Rejections: 20.'
Write-Host 'Correctness cases: 24. Observational performance cases: 2.'
Write-Host 'Runtime Candidate-policy authorization: None. Reference Project deletion: None.'
