param([ValidateSet('Auto','Applied','Regression')][string]$Mode='Auto')
$ErrorActionPreference = 'Stop'
if($Mode -eq 'Auto'){ $Mode = if(Test-Path -LiteralPath (Join-Path (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)) 'docs\level4v2\R2_UNIFIED_AUTHORITY_MANIFEST_V1.json')){'Regression'}else{'Applied'} }
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
function Require([bool]$Condition,[string]$Message){if(-not $Condition){throw $Message}}

$required = @(
'189_Level4V2CanonicalVocabulary\189_Level4V2CanonicalVocabulary.vcxproj',
'189_Level4V2CanonicalVocabulary\CanonicalVocabulary.h',
'189_Level4V2CanonicalVocabulary\CanonicalVocabulary.cpp',
'190_Level4V2CanonicalVocabularyTests\190_Level4V2CanonicalVocabularyTests.vcxproj',
'190_Level4V2CanonicalVocabularyTests\main.cpp',
'docs\level4v2\R1_CANONICAL_VOCABULARY_MANIFEST_V1.json',
'docs\level4v2\R1_CANONICAL_VOCABULARY.md',
'docs\level4v2\R1_TYPE_OWNERSHIP_V1.md',
'docs\level4v2\R1_NEGATIVE_TEST_MATRIX_V1.md',
'docs\level4v2\R1_EVIDENCE_LEDGER_V1.md',
'docs\level4v2\R1_PACKAGE_MANIFEST.txt',
'docs\level4v2\R1_PACKAGE_FILES.sha256',
'docs\level4v2\R1_CHANGESET.md',
'docs\level4v2\R2_UNIFIED_AUTHORITY_ENTRY_CONTRACT_V1.md',
'tests\Run-Level4V2R1.ps1',
'tests\tools\Register-Level4V2R1Projects.ps1',
'tests\tools\Verify-Level4V2R1.ps1',
'run_sge4_5_level4v2_r1_prepare.bat',
'run_sge4_5_level4v2_r1_canonical_vocabulary.bat')
foreach($relative in $required){Require (Test-Path -LiteralPath (Join-Path $root $relative) -PathType Leaf) "R1 file missing: $relative"}


if($Mode -eq 'Applied'){
    $packageHashPath = Join-Path $root 'docs\level4v2\R1_PACKAGE_FILES.sha256'
    $packageLines = @(Get-Content -LiteralPath $packageHashPath -Encoding UTF8 | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })
    Require ($packageLines.Count -eq 23) 'R1 package hash-entry count mismatch.'
    foreach($line in $packageLines){
        Require ($line -match '^([0-9a-f]{64})  (.+)$') "Malformed R1 package hash line: $line"
        $expected=$Matches[1]; $relative=$Matches[2]
        $path=Join-Path $root $relative
        Require (Test-Path -LiteralPath $path -PathType Leaf) "R1 package-hashed file missing: $relative"
        $actual=(Get-FileHash -Algorithm SHA256 -LiteralPath $path).Hash.ToLowerInvariant()
        Require ($actual -eq $expected) "R1 package file changed after packaging: $relative"
    }

}
$m = Get-Content -Raw -LiteralPath (Join-Path $root 'docs\level4v2\R1_CANONICAL_VOCABULARY_MANIFEST_V1.json') -Encoding UTF8 | ConvertFrom-Json
Require ($m.schema -eq 'SGE4.Level4V2.R1CanonicalVocabularyManifest.V1') 'R1 manifest schema mismatch.'
Require ($m.completionUnit -eq 'R1') 'R1 completion unit mismatch.'
if($Mode -eq 'Applied'){Require ($m.status -eq 'CompletePackageSuppliedOwnerGatePending') 'R1 supplied status mismatch.'}else{Require ($m.status -eq 'Passed') 'R1 accepted status mismatch.'; Require ($m.acceptedCommit -eq 'eee68ce6e9be5537d3041964679e55f4c5d2c448') 'R1 accepted commit mismatch.'}
Require ($m.baseCommit -eq 'b8b5b4f675e4186a3ae202d718e091b63c53e264') 'R1 base commit mismatch.'
Require ($m.r0InputFreezeStatus -eq 'Passed') 'R1 requires accepted R0.'
Require ($m.mode -eq 'ReconstructionNotNewCapability') 'R1 mode mismatch.'
Require (@($m.strongIdentityConcepts).Count -eq 11) 'R1 strong identity count mismatch.'
Require (@($m.strongDynamicConcepts).Count -eq 8) 'R1 strong dynamic count mismatch.'
Require ($m.identityConstruction.algorithm -eq 'SHA256') 'R1 canonical identity algorithm mismatch.'
Require ($m.opaqueVerifiedBoundary.constructionFromRawCandidate -eq 'CompileTimeUnavailable') 'R1 Raw-to-Verified boundary mismatch.'
Require (-not [bool]$m.opaqueVerifiedBoundary.verifierBehaviorImplementedInR1) 'R1 must not implement Verifier behavior.'
Require ($m.runtimeCapabilityAdded -eq 'None') 'R1 added Runtime capability.'
Require ($m.schema17Mutation -eq 'None' -and $m.runtime17Mutation -eq 'None') 'R1 changed the frozen ABI baseline.'
Require ($m.backendMutation -eq 'None' -and $m.compositionMutation -eq 'None') 'R1 changed Backend or Composition.'
Require ($m.referenceProjectDeletion -eq 'None') 'R1 deleted reference Projects.'
Require ($m.runtimeCandidatePolicyAuthorization -eq 'None') 'R1 authorized Runtime Candidate policy.'
Require ($m.deterministicEvidence.expectedSha256 -eq '35e751e131dcf4c88cad1b2da7bbc8e745f497e56d5ba22bd23079c7e2ec2abf') 'R1 deterministic Evidence identity mismatch.'
Require ($m.nextStageOnSuccessfulGate -eq 'R2UnifiedAuthorityChain') 'R1 next stage mismatch.'

$coreHashes=@{
'189_Level4V2CanonicalVocabulary\189_Level4V2CanonicalVocabulary.vcxproj'='d7e24f5b0c0e585c17d964962a950eb3836b1b7d04226a083ed986caaf8b6826';
'189_Level4V2CanonicalVocabulary\CanonicalVocabulary.cpp'='11fe9870be89368ecb92b28d7e2f5095341782df0efeaff005cedf71b73f784b';
'189_Level4V2CanonicalVocabulary\CanonicalVocabulary.h'='81c59f383fbc0897ddac9ada304966d7ab207fc069637f050ce192c5ff2e7021';
'190_Level4V2CanonicalVocabularyTests\190_Level4V2CanonicalVocabularyTests.vcxproj'='94af1b3ce591dcc99045fd72e6a613efff0faf4967920801d7592562858b300b';
'190_Level4V2CanonicalVocabularyTests\main.cpp'='ecd09c54ecb59bbdcf2daa1862bc31e5ab5e253553e137e06149bc0a7a8004df'}
foreach($entry in $coreHashes.GetEnumerator()){ $actual=(Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $root $entry.Key)).Hash.ToLowerInvariant(); Require ($actual -eq $entry.Value) "Accepted R1 core source changed: $($entry.Key)" }

$expectedIdentities = @('SemanticIdentity','InvocationIdentity','CandidateIdentity','VerifiedPlanIdentity','FrozenArtifactIdentity','ResourceContractIdentity','ResourceInstanceIdentity','WriteSetIdentity','HistoryValidityIdentity','TargetProfileIdentity','ObservationContractIdentity')
Require ((@($m.strongIdentityConcepts) -join ',') -eq ($expectedIdentities -join ',')) 'R1 identity ordering mismatch.'
$expectedDynamic = @('ActiveCount','TransitionCount','ReuseCount','UpdateInterval','BatchSize','AffectedBlockCount','TimelineOrdinal','DeviceEpoch')
Require ((@($m.strongDynamicConcepts) -join ',') -eq ($expectedDynamic -join ',')) 'R1 dynamic concept ordering mismatch.'
foreach($id in @('V2-R0-I001','V2-R0-I006','V2-R0-I020','V2-R0-I040')){Require (@($m.r0CarriedInvariantsSatisfiedInR1) -contains $id) "R1 does not close required R0 invariant: $id"}

$projects = @(
    [pscustomobject]@{Path='189_Level4V2CanonicalVocabulary\189_Level4V2CanonicalVocabulary.vcxproj';Guid='{000000BD-0000-5000-8000-0000000000BD}'},
    [pscustomobject]@{Path='190_Level4V2CanonicalVocabularyTests\190_Level4V2CanonicalVocabularyTests.vcxproj';Guid='{000000BE-0000-5000-8000-0000000000BE}'})
$solution = Get-Content -Raw -LiteralPath (Join-Path $root 'SemanticGpuEngine4-5.sln') -Encoding UTF8
foreach($project in $projects){
    [xml]$xml = Get-Content -Raw -LiteralPath (Join-Path $root $project.Path) -Encoding UTF8
    $guid=[string](($xml.Project.PropertyGroup|Where-Object{$_.Label -eq 'Globals'}|Select-Object -First 1).ProjectGuid)
    Require ($guid.ToUpperInvariant() -eq $project.Guid.ToUpperInvariant()) "R1 Project GUID mismatch: $($project.Path)"
    Require (([regex]::Matches($solution,[regex]::Escape($project.Path))).Count -eq 1) "R1 Solution registration mismatch: $($project.Path)"
}

$libraryProject = Get-Content -Raw -LiteralPath (Join-Path $root '189_Level4V2CanonicalVocabulary\189_Level4V2CanonicalVocabulary.vcxproj') -Encoding UTF8
Require ($libraryProject -match '<ConfigurationType>StaticLibrary</ConfigurationType>') 'R1 vocabulary must be a static library.'
Require (([regex]::Matches($libraryProject,'<ProjectReference Include=')).Count -eq 1) 'R1 vocabulary must have exactly one Project dependency.'
Require ($libraryProject -match '00_Foundation') 'R1 vocabulary Foundation dependency missing.'
foreach($forbidden in @('D3D12','DXGI','CompositionRuntime','CompositionContract','CandidatePlanner','ExecutionPlanVerifier','SparseTemporal','Spiral')){Require ($libraryProject -notmatch [regex]::Escape($forbidden)) "R1 vocabulary has forbidden Project dependency/token: $forbidden"}

$header = Get-Content -Raw -LiteralPath (Join-Path $root '189_Level4V2CanonicalVocabulary\CanonicalVocabulary.h') -Encoding UTF8
$core = Get-Content -Raw -LiteralPath (Join-Path $root '189_Level4V2CanonicalVocabulary\CanonicalVocabulary.cpp') -Encoding UTF8
$publicSource = $header + "`n" + $core
foreach($token in $expectedIdentities + $expectedDynamic + @('SemanticDescriptorV1','DynamicExecutionDimensionsV1','InvocationDescriptorV1','RawCandidateProposalV1','OpaqueVerifiedPlanV1','FrozenArtifactDescriptorV1','HistoryValidityDescriptorV1','RepresentationHandleV1','HistoryHandleV1','CompletionHandleV1','SubmissionHandleV1','CanonicalIdentityMaterialV1','ComputeCanonicalDigestV1')){Require ($publicSource -match [regex]::Escape($token)) "R1 canonical vocabulary token missing: $token"}
foreach($forbidden in @('Spiral','CU1','CU2','CU3','CU4','CU5','CU6','RTX4070','WARP','D3D12','Game')){Require ($publicSource -notmatch [regex]::Escape($forbidden)) "Forbidden public R1 token: $forbidden"}
foreach($include in @('windows.h','d3d12.h','dxgi','wrl.h')){Require ($publicSource -notmatch [regex]::Escape($include)) "R1 public source depends on Backend header: $include"}
foreach($literal in @('4096','128','64')){Require ($publicSource -notmatch "(?<![A-Za-z0-9_])$literal(?:[uUlL]*)(?![A-Za-z0-9_])") "R1 generic source freezes universal limit: $literal"}
foreach($processToken in @('filesystem','timestamp','uuid','random_device','GetCurrentProcess','GetTickCount')){Require ($publicSource -notmatch [regex]::Escape($processToken)) "Process-specific identity input entered R1 source: $processToken"}
Require ($core -match 'base::Sha256') 'R1 identity implementation is not bound to Foundation SHA-256.'
Require ($core -match 'AppendString' -and $core -match 'AppendU32' -and $core -match 'AppendU64') 'R1 canonical identity serialization boundary missing.'

$test = Get-Content -Raw -LiteralPath (Join-Path $root '190_Level4V2CanonicalVocabularyTests\main.cpp') -Encoding UTF8
foreach($token in @('BraceConstructibleFrom','!std::is_default_constructible_v<canonical::OpaqueVerifiedPlanV1>','!std::is_constructible_v<canonical::OpaqueVerifiedPlanV1, canonical::RawCandidateProposalV1>','!std::is_convertible_v<canonical::RawCandidateProposalV1, canonical::OpaqueVerifiedPlanV1>','DeviceEpoch::TryCreate(0u)','Canonical identity is not deterministic','Domain separation did not change identity','L4V2R1V1')){Require ($test -match [regex]::Escape($token)) "R1 negative/determinism test token missing: $token"}

$input = Get-Content -Raw -LiteralPath (Join-Path $root 'docs\level4v2\CANONICAL_RECONSTRUCTION_INPUT_V1.json') -Encoding UTF8 | ConvertFrom-Json
Require ($input.acceptedR0Commit -eq 'b8b5b4f675e4186a3ae202d718e091b63c53e264') 'Canonical input R0 acceptance mismatch.'
Require ($input.r0InputFreeze.status -eq 'Passed') 'Canonical input does not record R0 success.'
Require ($input.r1CanonicalVocabulary.manifest -eq 'docs/level4v2/R1_CANONICAL_VOCABULARY_MANIFEST_V1.json') 'Canonical input R1 manifest mismatch.'
Require ([int]$input.r1CanonicalVocabulary.strongIdentityCount -eq 11 -and [int]$input.r1CanonicalVocabulary.strongDynamicCount -eq 8) 'Canonical input R1 counts mismatch.'
if($Mode -eq 'Applied'){Require ($input.nextStage -eq 'R2UnifiedAuthorityChain') 'Canonical input does not advance to R2.'}else{Require ($input.nextStage -eq 'R3CanonicalComposition') 'Canonical input does not record the R2/R3 handoff.'}
Require ($input.runtimeCandidatePolicyAuthorization -eq 'None') 'Canonical input authorized Runtime policy.'

$r2 = Get-Content -Raw -LiteralPath (Join-Path $root 'docs\level4v2\R2_UNIFIED_AUTHORITY_ENTRY_CONTRACT_V1.md') -Encoding UTF8
foreach($token in @('Raw Candidate proposal','Candidate Planner','Planner-independent Verifier','opaque Verified Plan','Frozen Artifact','Planner cannot construct','Raw Candidate cannot freeze or execute')){Require ($r2 -match [regex]::Escape($token)) "R2 entry token missing: $token"}

Write-Host 'Level 4 v2 R1 canonical vocabulary static boundaries passed.'
Write-Host 'Strong identities: 11. Strong dynamic concepts: 8.'
Write-Host 'Raw Candidate cannot construct an opaque Verified Plan.'
Write-Host 'Runtime capability added: None. Schema 17 / Runtime 17 mutation: None.'
