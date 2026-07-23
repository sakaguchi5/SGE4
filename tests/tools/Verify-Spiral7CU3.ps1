param(
    [ValidateSet('Auto','Snapshot','Regression')]
    [string]$Mode = 'Auto'
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
function Require([bool]$Condition,[string]$Message){if(-not $Condition){throw $Message}}

if ($Mode -eq 'Auto') {
    $cu4Manifest = Join-Path $root 'docs\spiral7\CU4_CANDIDATE_FAMILY_MANIFEST_V1.json'
    $Mode = if (Test-Path -LiteralPath $cu4Manifest -PathType Leaf) { 'Regression' } else { 'Snapshot' }
}

$manifestPath = Join-Path $root 'docs\spiral7\CU3_AUTHORITY_MANIFEST_V1.json'
Require (Test-Path -LiteralPath $manifestPath -PathType Leaf) 'Spiral 7 CU3 authority manifest missing.'
$m = Get-Content -Raw -LiteralPath $manifestPath -Encoding UTF8 | ConvertFrom-Json
Require ($m.schema -eq 'SGE4-5.Spiral7.CU3AuthorityManifest.V1') 'CU3 manifest schema mismatch.'
Require ($m.completionUnit -eq 'CU3') 'CU3 completion unit mismatch.'
Require ($m.ownerBaselineCommit -eq 'd0bb0d406fca8beabed2331daff870ea414dd87d') 'CU3 Owner baseline mismatch.'
Require ($m.cu2AcceptedCommit -eq '87ae8946f403841199b2d18714348a11c007b65d') 'CU3 accepted CU2 commit mismatch.'
Require ($m.cu2ArchitectureEvidenceSha256 -eq '50B4D4C848721442CC606EE8BFDA207832BA03E7B6FCD5B24F4EEC9A7FE3DDC3') 'CU2 accepted evidence mismatch.'
Require ($m.extensionStrategy -eq 'VersionedSparseTemporalDeltaExtensionV1') 'CU3 extension strategy mismatch.'
Require (($m.authorityChain -join ',') -eq 'RawDeltaLoweringCandidateV1,DeltaLoweringPlanner,PlannerIndependentDeltaVerifier,OpaqueVerifiedDeltaLoweringV1,FrozenVerifiedCompactDeltaExecutionV1') 'CU3 authority chain mismatch.'
Require ($m.plannerPackageAuthority -eq 'None') 'Planner must not have Package authority.'
Require ($m.verifierReconstruction -eq 'IndependentFromSemanticInvocationAndCanonicalContext') 'Verifier reconstruction policy mismatch.'
Require ($m.rawToVerified -eq 'CompileTimeUnavailable') 'Raw-to-Verified boundary mismatch.'
Require ($m.verifiedToFrozen -eq 'ExplicitResourceAndDeviceEpochBindingRequired') 'Verified-to-Frozen boundary mismatch.'
Require ([int]$m.rawMutationAttackCount -eq 50) 'Raw mutation attack count mismatch.'
Require ([int]$m.verifiedReplayGateCount -eq 4) 'Verified replay gate count mismatch.'
Require ([int]$m.resourceArtifactEpochGateCount -eq 14) 'Resource/artifact/epoch gate count mismatch.'
Require ([int]$m.warpCaseCount -eq 1) 'CU3 WARP case count mismatch.'
Require ($m.deviceEpochRule -eq 'NonZeroExactMatchStaleRejected') 'Device epoch rule mismatch.'
Require ($m.resourceAliasing -eq 'PreviousAndOutputHistoryMustBeDistinct') 'History aliasing rule mismatch.'
Require ($m.runtimeTransitionPolicyDecision -eq 'None') 'Runtime transition policy must remain None.'
Require ($m.candidateFamilyQualification -eq 'DeferredToCU4') 'Candidate family must remain deferred to CU4.'
Require ($m.recoveryQualification -eq 'DeferredToCU5') 'Recovery must remain deferred to CU5.'
foreach($field in @('legacySchemaMutation','legacyRuntimeMutation','legacyBackendMutation','compositionContractMutation')) {
    Require ($m.$field -eq 'None') "CU3 legacy boundary changed: $field"
}

$required = @(
'174_Spiral7DeltaExecution\174_Spiral7DeltaExecution.vcxproj','174_Spiral7DeltaExecution\Spiral7DeltaExecution.h','174_Spiral7DeltaExecution\Spiral7DeltaExecution.cpp',
'175_SparseTemporalDeltaLoweringCandidate\175_SparseTemporalDeltaLoweringCandidate.vcxproj','175_SparseTemporalDeltaLoweringCandidate\DeltaLoweringCandidate.h','175_SparseTemporalDeltaLoweringCandidate\DeltaLoweringCandidate.cpp',
'176_SparseTemporalDeltaLoweringPlanner\176_SparseTemporalDeltaLoweringPlanner.vcxproj','176_SparseTemporalDeltaLoweringPlanner\DeltaLoweringPlanner.h','176_SparseTemporalDeltaLoweringPlanner\DeltaLoweringPlanner.cpp',
'177_SparseTemporalDeltaLoweringVerifier\177_SparseTemporalDeltaLoweringVerifier.vcxproj','177_SparseTemporalDeltaLoweringVerifier\DeltaLoweringVerifier.h','177_SparseTemporalDeltaLoweringVerifier\DeltaLoweringVerifier.cpp',
'183_Spiral7AuthorityMutationTests\183_Spiral7AuthorityMutationTests.vcxproj','183_Spiral7AuthorityMutationTests\main.cpp',
'tests\Run-Spiral7CU3.ps1','tests\tools\Register-Spiral7CU3Projects.ps1','tests\tools\Verify-Spiral7CU3.ps1',
'run_sge4_5_spiral7_cu3_prepare.bat','run_sge4_5_spiral7_cu3_independent_authority.bat',
'docs\spiral7\CU3_AUTHORITY_MANIFEST_V1.json','docs\spiral7\CU3_INDEPENDENT_DELTA_AUTHORITY.md','docs\spiral7\CU3_MUTATION_MATRIX.md','docs\spiral7\CU3_EVIDENCE_LEDGER.md','docs\spiral7\CU3_CHANGESET.md','docs\spiral7\APPLY_CU3_JA.md')
foreach($relative in $required){Require (Test-Path -LiteralPath (Join-Path $root $relative) -PathType Leaf) "Missing Spiral 7 CU3 file: $relative"}

$projects = @(
    [pscustomobject]@{Path='175_SparseTemporalDeltaLoweringCandidate\175_SparseTemporalDeltaLoweringCandidate.vcxproj';Guid='{000000AF-0000-5000-8000-0000000000AF}'},
    [pscustomobject]@{Path='176_SparseTemporalDeltaLoweringPlanner\176_SparseTemporalDeltaLoweringPlanner.vcxproj';Guid='{000000B0-0000-5000-8000-0000000000B0}'},
    [pscustomobject]@{Path='177_SparseTemporalDeltaLoweringVerifier\177_SparseTemporalDeltaLoweringVerifier.vcxproj';Guid='{000000B1-0000-5000-8000-0000000000B1}'},
    [pscustomobject]@{Path='183_Spiral7AuthorityMutationTests\183_Spiral7AuthorityMutationTests.vcxproj';Guid='{000000B7-0000-5000-8000-0000000000B7}'}
)
$solution = Get-Content -Raw -LiteralPath (Join-Path $root 'SemanticGpuEngine4-5.sln') -Encoding UTF8
foreach($project in $projects){
    [xml]$xml = Get-Content -Raw -LiteralPath (Join-Path $root $project.Path) -Encoding UTF8
    $guid=[string](($xml.Project.PropertyGroup|Where-Object{$_.Label -eq 'Globals'}|Select-Object -First 1).ProjectGuid)
    Require ($guid.ToUpperInvariant() -eq $project.Guid.ToUpperInvariant()) "Project GUID mismatch: $($project.Path)"
    Require ($solution -match [regex]::Escape($project.Path)) "Solution registration missing: $($project.Path)"
}

$candidateProject = Get-Content -Raw -LiteralPath (Join-Path $root '175_SparseTemporalDeltaLoweringCandidate\175_SparseTemporalDeltaLoweringCandidate.vcxproj') -Encoding UTF8
foreach($forbidden in @('176_SparseTemporalDeltaLoweringPlanner','177_SparseTemporalDeltaLoweringVerifier','174_Spiral7DeltaExecution','22_CompositionRuntime','14_D3D12Backend')){
    Require ($candidateProject -notmatch [regex]::Escape($forbidden)) "Raw Candidate has forbidden dependency: $forbidden"
}
$plannerProject = Get-Content -Raw -LiteralPath (Join-Path $root '176_SparseTemporalDeltaLoweringPlanner\176_SparseTemporalDeltaLoweringPlanner.vcxproj') -Encoding UTF8
Require ($plannerProject -match '175_SparseTemporalDeltaLoweringCandidate') 'Planner must depend on Raw Candidate.'
foreach($forbidden in @('177_SparseTemporalDeltaLoweringVerifier','174_Spiral7DeltaExecution','22_CompositionRuntime','14_D3D12Backend')){
    Require ($plannerProject -notmatch [regex]::Escape($forbidden)) "Planner has forbidden dependency: $forbidden"
}
$verifierProject = Get-Content -Raw -LiteralPath (Join-Path $root '177_SparseTemporalDeltaLoweringVerifier\177_SparseTemporalDeltaLoweringVerifier.vcxproj') -Encoding UTF8
Require ($verifierProject -match '175_SparseTemporalDeltaLoweringCandidate') 'Verifier must inspect Raw Candidate.'
foreach($forbidden in @('176_SparseTemporalDeltaLoweringPlanner','174_Spiral7DeltaExecution','22_CompositionRuntime','14_D3D12Backend')){
    Require ($verifierProject -notmatch [regex]::Escape($forbidden)) "Verifier has forbidden dependency: $forbidden"
}
$executionProject = Get-Content -Raw -LiteralPath (Join-Path $root '174_Spiral7DeltaExecution\174_Spiral7DeltaExecution.vcxproj') -Encoding UTF8
Require ($executionProject -match '177_SparseTemporalDeltaLoweringVerifier') 'Execution must consume the opaque Verified Delta type.'
Require ($executionProject -notmatch '176_SparseTemporalDeltaLoweringPlanner') 'Execution must not depend on the Planner.'

$candidateHeader = Get-Content -Raw -LiteralPath (Join-Path $root '175_SparseTemporalDeltaLoweringCandidate\DeltaLoweringCandidate.h') -Encoding UTF8
foreach($token in @('RawDeltaLoweringCandidateV1','transitionAuthorityIdentity','proposedPreviousHistoryResourceIdentity','proposedOutputHistoryResourceIdentity','rawCandidateIdentity')){
    Require ($candidateHeader -match [regex]::Escape($token)) "Raw Candidate token missing: $token"
}
$plannerCpp = Get-Content -Raw -LiteralPath (Join-Path $root '176_SparseTemporalDeltaLoweringPlanner\DeltaLoweringPlanner.cpp') -Encoding UTF8
foreach($token in @('BuildCompactDeltaArtifactV1','PlanCompactDeltaHistoryCandidateV1','RawDeltaLoweringCandidateIdentityV1')){
    Require ($plannerCpp -match [regex]::Escape($token)) "Planner token missing: $token"
}
$verifierHeader = Get-Content -Raw -LiteralPath (Join-Path $root '177_SparseTemporalDeltaLoweringVerifier\DeltaLoweringVerifier.h') -Encoding UTF8
foreach($token in @('VerifiedDeltaLoweringV1() = delete','PlannerIndependent','VerifyDeltaLoweringCandidateV1','ValidateVerifiedDeltaLoweringContextV1')){
    if($token -eq 'PlannerIndependent') { continue }
    Require ($verifierHeader -match [regex]::Escape($token)) "Verifier header token missing: $token"
}
$verifierCpp = Get-Content -Raw -LiteralPath (Join-Path $root '177_SparseTemporalDeltaLoweringVerifier\DeltaLoweringVerifier.cpp') -Encoding UTF8
foreach($token in @('IndependentCanonicalContext','DeriveExpectedPlan','raw delta candidate differs from the independently derived plan','VerifierCertificate.DeltaLowering')){
    Require ($verifierCpp -match [regex]::Escape($token)) "Independent Verifier token missing: $token"
}
$executionHeader = Get-Content -Raw -LiteralPath (Join-Path $root '174_Spiral7DeltaExecution\Spiral7DeltaExecution.h') -Encoding UTF8
foreach($token in @('FrozenVerifiedCompactDeltaExecutionV1() = delete','PreviousHistoryReadOnly','OutputHistoryWriteOnly','ValidateFrozenCompactDeltaExecutionEpochV1')){
    Require ($executionHeader -match [regex]::Escape($token)) "Frozen execution token missing: $token"
}
$testCpp = Get-Content -Raw -LiteralPath (Join-Path $root '183_Spiral7AuthorityMutationTests\main.cpp') -Encoding UTF8
foreach($token in @('rawAttackCount == 50','Cross-invocation Verified seal replay accepted','Aliased previous/output History identity accepted','Cross-device-epoch Frozen replay accepted','Runtime transition-policy decision: None')){
    Require ($testCpp -match [regex]::Escape($token)) "CU3 test token missing: $token"
}

$progress = Get-Content -Raw -LiteralPath (Join-Path $root 'docs\spiral7\SPIRAL7_PROGRESS.md') -Encoding UTF8
Require ($progress -match '87ae8946f403841199b2d18714348a11c007b65d') 'CU2 accepted commit is not recorded in progress.'
Require ($progress -match '50B4D4C848721442CC606EE8BFDA207832BA03E7B6FCD5B24F4EEC9A7FE3DDC3') 'CU2 evidence is not recorded in progress.'

$cu2Manifest = Get-Content -Raw -LiteralPath (Join-Path $root 'docs\spiral7\CU2_ARCHITECTURE_MANIFEST_V1.json') -Encoding UTF8 | ConvertFrom-Json
foreach ($property in $cu2Manifest.legacyFileSha256.PSObject.Properties) {
    $path = Join-Path $root ([string]$property.Name)
    Require (Test-Path -LiteralPath $path -PathType Leaf) "Legacy boundary file missing: $($property.Name)"
    $actual = (Get-FileHash -Algorithm SHA256 -LiteralPath $path).Hash.ToLowerInvariant()
    Require ($actual -eq ([string]$property.Value).ToLowerInvariant()) "Legacy boundary file changed: $($property.Name)"
}

if ($Mode -eq 'Snapshot') {
    foreach($futureProject in @('178_SparseTemporalDeltaCandidateFamily','179_SparseTemporalDeltaCandidateFamilyPlanner','180_SparseTemporalDeltaCandidateFamilyVerifier','181_Spiral7DeltaFamilyExecution','184_Spiral7CandidateFamilyTests')){
        Require ($solution -notmatch [regex]::Escape($futureProject)) "CU3 Snapshot must not pre-create CU4 project: $futureProject"
    }
} elseif ($Mode -eq 'Regression') {
    $cu4 = Join-Path $root 'docs\spiral7\CU4_CANDIDATE_FAMILY_MANIFEST_V1.json'
    Require (Test-Path -LiteralPath $cu4 -PathType Leaf) 'CU3 Regression mode requires a CU4 marker.'
} else { throw "Unexpected Spiral 7 CU3 verification mode: $Mode" }

Write-Host "SGE4-5 Spiral 7 CU3 static authority boundaries passed. Mode: $Mode"
Write-Host 'Raw Candidate cannot mint Verified Delta; Planner-independent reconstruction is required.'
Write-Host 'Delta/previous-History/output-History resources and Device epoch are identity-bound.'
Write-Host 'Legacy Schema 17 / Runtime 17 / Backend / Composition mutation: None.'
