$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
function Require([bool]$Condition,[string]$Message){if(-not $Condition){throw $Message}}

$manifestPath = Join-Path $root 'docs\spiral6\CU4_CANDIDATE_FAMILY_MANIFEST_V1.json'
Require (Test-Path -LiteralPath $manifestPath -PathType Leaf) 'Spiral 6 CU4 Candidate family manifest missing.'
$m = Get-Content -Raw -LiteralPath $manifestPath -Encoding UTF8 | ConvertFrom-Json
Require ($m.schema -eq 'SGE4-5.Spiral6.CU4CandidateFamilyManifest.V1') 'CU4 manifest schema mismatch.'
Require ($m.completionUnit -eq 'CU4') 'CU4 completion unit mismatch.'
Require ($m.cu3AcceptedCommit -eq 'a35c51dae045cbada55b31d98f832bd310d20ce2') 'CU4 CU3 commit mismatch.'
Require (($m.candidateKinds -join ',') -eq 'DenseMembershipMaskPredicate,CompactIndexListDispatch,ActiveBlockListLocalMask') 'CU4 Candidate family mismatch.'
Require ([int]$m.caseCount -eq 12) 'CU4 case count mismatch.'
Require ([int]$m.candidateExecutionsPerBuild -eq 36) 'CU4 execution count mismatch.'
Require ([int]$m.rawMutationAttackCount -eq 40) 'CU4 Raw mutation count mismatch.'
Require ([bool]$m.pairwiseFullOutputByteIdentityRequired) 'CU4 pairwise byte identity must be required.'
Require (-not [bool]$m.dependencyBoundaries.verifierDependsOnPlanner) 'CU4 Verifier must not depend on Planner.'
Require ($m.runtimeSparsePolicyDecision -eq 'None') 'CU4 Runtime Sparse policy must remain None.'
Require ($m.legacySchemaMutation -eq 'None') 'CU4 must not mutate Schema 17.'
Require ($m.legacyRuntimeMutation -eq 'None') 'CU4 must not mutate Runtime 17.'
Require ($m.legacyBackendMutation -eq 'None') 'CU4 must not mutate canonical Backend.'

$required = @(
'160_SparseCandidateFamily\160_SparseCandidateFamily.vcxproj','160_SparseCandidateFamily\SparseCandidateFamily.h','160_SparseCandidateFamily\SparseCandidateFamily.cpp',
'161_SparseCandidateFamilyPlanner\161_SparseCandidateFamilyPlanner.vcxproj','161_SparseCandidateFamilyPlanner\SparseCandidateFamilyPlanner.h','161_SparseCandidateFamilyPlanner\SparseCandidateFamilyPlanner.cpp',
'162_SparseCandidateFamilyVerifier\162_SparseCandidateFamilyVerifier.vcxproj','162_SparseCandidateFamilyVerifier\SparseCandidateFamilyVerifier.h','162_SparseCandidateFamilyVerifier\SparseCandidateFamilyVerifier.cpp',
'163_Spiral6SparseFamilyExecution\163_Spiral6SparseFamilyExecution.vcxproj','163_Spiral6SparseFamilyExecution\Spiral6SparseFamilyExecution.h','163_Spiral6SparseFamilyExecution\Spiral6SparseFamilyExecution.cpp',
'168_Spiral6SparseCandidateFamilyTests\168_Spiral6SparseCandidateFamilyTests.vcxproj','168_Spiral6SparseCandidateFamilyTests\main.cpp',
'tests\Run-Spiral6CU4.ps1','tests\tools\Register-Spiral6CU4Projects.ps1','tests\tools\Verify-Spiral6CU4.ps1',
'run_sge4_5_spiral6_cu4_prepare.bat','run_sge4_5_spiral6_cu4_candidate_family.bat',
'docs\spiral6\CU4_CANDIDATE_FAMILY_MANIFEST_V1.json','docs\spiral6\CU4_SPARSE_CANDIDATE_FAMILY.md','docs\spiral6\CU4_CHANGESET.md','docs\spiral6\CU4_EVIDENCE_LEDGER.md')
foreach($relative in $required){Require (Test-Path -LiteralPath (Join-Path $root $relative) -PathType Leaf) "Missing Spiral 6 CU4 file: $relative"}

$projects = @(
    [pscustomobject]@{Path='160_SparseCandidateFamily\160_SparseCandidateFamily.vcxproj';Guid='{000000A0-0000-5000-8000-0000000000A0}'},
    [pscustomobject]@{Path='161_SparseCandidateFamilyPlanner\161_SparseCandidateFamilyPlanner.vcxproj';Guid='{000000A1-0000-5000-8000-0000000000A1}'},
    [pscustomobject]@{Path='162_SparseCandidateFamilyVerifier\162_SparseCandidateFamilyVerifier.vcxproj';Guid='{000000A2-0000-5000-8000-0000000000A2}'},
    [pscustomobject]@{Path='163_Spiral6SparseFamilyExecution\163_Spiral6SparseFamilyExecution.vcxproj';Guid='{000000A3-0000-5000-8000-0000000000A3}'},
    [pscustomobject]@{Path='168_Spiral6SparseCandidateFamilyTests\168_Spiral6SparseCandidateFamilyTests.vcxproj';Guid='{000000A8-0000-5000-8000-0000000000A8}'}
)
$solution = Get-Content -Raw -LiteralPath (Join-Path $root 'SemanticGpuEngine4-5.sln') -Encoding UTF8
foreach($project in $projects){
    [xml]$xml=Get-Content -Raw -LiteralPath (Join-Path $root $project.Path) -Encoding UTF8
    $guid=[string](($xml.Project.PropertyGroup|Where-Object{$_.Label -eq 'Globals'}|Select-Object -First 1).ProjectGuid)
    Require ($guid.ToUpperInvariant() -eq $project.Guid.ToUpperInvariant()) "Project GUID mismatch: $($project.Path)"
    Require ($solution -match [regex]::Escape($project.Path)) "Solution registration missing: $($project.Path)"
}

$verifierProject = Get-Content -Raw -LiteralPath (Join-Path $root '162_SparseCandidateFamilyVerifier\162_SparseCandidateFamilyVerifier.vcxproj') -Encoding UTF8
Require ($verifierProject -notmatch '161_SparseCandidateFamilyPlanner') 'Sparse Family Verifier must not depend on Planner.'
$executionProject = Get-Content -Raw -LiteralPath (Join-Path $root '163_Spiral6SparseFamilyExecution\163_Spiral6SparseFamilyExecution.vcxproj') -Encoding UTF8
Require ($executionProject -match '162_SparseCandidateFamilyVerifier') 'Sparse Family execution must depend on opaque Verifier authority.'
Require ($executionProject -notmatch '161_SparseCandidateFamilyPlanner') 'Sparse Family execution must not depend on Planner.'
$executionSource = Get-Content -Raw -LiteralPath (Join-Path $root '163_Spiral6SparseFamilyExecution\Spiral6SparseFamilyExecution.cpp') -Encoding UTF8
Require ($executionSource -notmatch '\bRawSparseFamilyCandidateV1\b') 'Sparse Family execution must not consume Raw Candidates.'

$candidateHeader = Get-Content -Raw -LiteralPath (Join-Path $root '160_SparseCandidateFamily\SparseCandidateFamily.h') -Encoding UTF8
foreach($token in @('DenseMembershipMaskPredicate','CompactIndexListDispatch','ActiveBlockListLocalMask','FrozenSparseFamilyArtifactV1','RawSparseFamilyCandidateV1','ActiveBlockRecordV1')){Require ($candidateHeader -match [regex]::Escape($token)) "CU4 Candidate token missing: $token"}
$verifierHeader = Get-Content -Raw -LiteralPath (Join-Path $root '162_SparseCandidateFamilyVerifier\SparseCandidateFamilyVerifier.h') -Encoding UTF8
foreach($token in @('VerifiedSparseFamilyCandidateV1() = delete','VerifiedSparseFamilyPlanV1','CertificateIdentity','ValidateVerifiedSparseFamilyCandidateV1')){Require ($verifierHeader -match [regex]::Escape($token)) "CU4 Verifier token missing: $token"}
$executionHeader = Get-Content -Raw -LiteralPath (Join-Path $root '163_Spiral6SparseFamilyExecution\Spiral6SparseFamilyExecution.h') -Encoding UTF8
foreach($token in @('FrozenVerifiedSparseFamilyCandidateV1() = delete','actualRepresentationResourceIdentity','pairwiseOutputByteIdentical','RunVerifiedSparseCandidateFamilyOnWarpV1')){Require ($executionHeader -match [regex]::Escape($token)) "CU4 execution token missing: $token"}
$test = Get-Content -Raw -LiteralPath (Join-Path $root '168_Spiral6SparseCandidateFamilyTests\main.cpp') -Encoding UTF8
Require (([regex]::Matches($test,'reject\(p,')).Count -eq 40) 'CU4 test must contain exactly 40 Raw family attacks.'
foreach($token in @('Cross-set Sparse Family seal replay accepted','Wrong Sparse Family Resource accepted','Cross-role Sparse Family binding accepted','Cross-epoch Sparse Family Frozen Candidate accepted','Pairwise full-output equivalence: byte-identical','Runtime sparse-policy decision: None')){Require ($test -match [regex]::Escape($token)) "CU4 test gate missing: $token"}

Write-Host 'SGE4-5 Spiral 6 CU4 static Candidate-family boundaries passed.'
Write-Host 'Verifier -> Planner dependency: absent.'
Write-Host 'Candidate family: Dense Mask / Compact Index List / Active Block Local Mask.'
