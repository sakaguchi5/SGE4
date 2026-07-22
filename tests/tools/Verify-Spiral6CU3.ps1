$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
function Require([bool]$Condition,[string]$Message){if(-not $Condition){throw $Message}}

$manifestPath = Join-Path $root 'docs\spiral6\CU3_AUTHORITY_MANIFEST_V1.json'
Require (Test-Path -LiteralPath $manifestPath -PathType Leaf) 'Spiral 6 CU3 authority manifest missing.'
$m = Get-Content -Raw -LiteralPath $manifestPath -Encoding UTF8 | ConvertFrom-Json
Require ($m.schema -eq 'SGE4-5.Spiral6.CU3AuthorityManifest.V1') 'CU3 manifest schema mismatch.'
Require ($m.completionUnit -eq 'CU3') 'CU3 completion unit mismatch.'
Require ($m.ownerBaselineCommit -eq '46554ab55e532c438c9c4214ff1df3e7cd68638e') 'CU3 Owner baseline mismatch.'
Require ($m.cu1AcceptedCommit -eq 'b74f820ba2c4504ab44aa1b954da4b0cfafff3d2') 'CU3 CU1 commit mismatch.'
Require ($m.cu2AcceptedCommit -eq '1b46e8090b2684e82022aff3f9a0f4e383d6e1fe') 'CU3 CU2 commit mismatch.'
Require ($m.qualifiedCandidate -eq 'CompactIndexListDispatch') 'CU3 Candidate mismatch.'
Require ($m.qualifiedPattern -eq 'HashScatterPermutation') 'CU3 qualified pattern mismatch.'
Require ([int]$m.qualifiedCardinality -eq 1024) 'CU3 qualified cardinality mismatch.'
Require ([int]$m.qualifiedDeviceEpoch -eq 1) 'CU3 qualified epoch mismatch.'
Require ([int]$m.rawMutationAttackCount -eq 36) 'CU3 Raw attack count mismatch.'
Require (-not [bool]$m.dependencyBoundaries.verifierDependsOnPlanner) 'CU3 Verifier must not depend on Planner.'
Require ([bool]$m.dependencyBoundaries.executionDependsOnVerifier) 'CU3 execution must consume Verified authority.'
Require (-not [bool]$m.dependencyBoundaries.executionDependsDirectlyOnRawCandidate) 'CU3 execution must not depend directly on Raw Candidate.'
Require (-not [bool]$m.dependencyBoundaries.executionDependsDirectlyOnPlanner) 'CU3 execution must not depend directly on Planner.'
Require ($m.runtimeSparsePolicyDecision -eq 'None') 'CU3 Runtime Sparse policy must remain None.'
Require ($m.legacySchemaMutation -eq 'None') 'CU3 must not mutate Schema 17.'
Require ($m.legacyRuntimeMutation -eq 'None') 'CU3 must not mutate Runtime 17.'
Require ($m.legacyBackendMutation -eq 'None') 'CU3 must not mutate canonical Backend.'
Require ($m.compositionContractMutation -eq 'None') 'CU3 must not mutate Composition Contract.'

$required = @(
'157_SparseLoweringCandidate\157_SparseLoweringCandidate.vcxproj','157_SparseLoweringCandidate\SparseLoweringCandidate.h','157_SparseLoweringCandidate\SparseLoweringCandidate.cpp',
'158_SparseLoweringPlanner\158_SparseLoweringPlanner.vcxproj','158_SparseLoweringPlanner\SparseLoweringPlanner.h','158_SparseLoweringPlanner\SparseLoweringPlanner.cpp',
'159_SparseLoweringVerifier\159_SparseLoweringVerifier.vcxproj','159_SparseLoweringVerifier\SparseLoweringVerifier.h','159_SparseLoweringVerifier\SparseLoweringVerifier.cpp',
'167_Spiral6AuthorityMutationTests\167_Spiral6AuthorityMutationTests.vcxproj','167_Spiral6AuthorityMutationTests\main.cpp',
'tests\Run-Spiral6CU3.ps1','tests\tools\Register-Spiral6CU3Projects.ps1','tests\tools\Verify-Spiral6CU3.ps1',
'run_sge4_5_spiral6_cu3_prepare.bat','run_sge4_5_spiral6_cu3_independent_authority.bat',
'docs\spiral6\CU3_AUTHORITY_MANIFEST_V1.json','docs\spiral6\CU3_INDEPENDENT_SPARSE_AUTHORITY.md','docs\spiral6\CU3_MUTATION_MATRIX.md','docs\spiral6\CU3_CHANGESET.md','docs\spiral6\CU3_EVIDENCE_LEDGER.md')
foreach($relative in $required){Require (Test-Path -LiteralPath (Join-Path $root $relative) -PathType Leaf) "Missing Spiral 6 CU3 file: $relative"}

$projects = @(
    [pscustomobject]@{Path='157_SparseLoweringCandidate\157_SparseLoweringCandidate.vcxproj';Guid='{0000009D-0000-5000-8000-00000000009D}'},
    [pscustomobject]@{Path='158_SparseLoweringPlanner\158_SparseLoweringPlanner.vcxproj';Guid='{0000009E-0000-5000-8000-00000000009E}'},
    [pscustomobject]@{Path='159_SparseLoweringVerifier\159_SparseLoweringVerifier.vcxproj';Guid='{0000009F-0000-5000-8000-00000000009F}'},
    [pscustomobject]@{Path='167_Spiral6AuthorityMutationTests\167_Spiral6AuthorityMutationTests.vcxproj';Guid='{000000A7-0000-5000-8000-0000000000A7}'}
)
$solution = Get-Content -Raw -LiteralPath (Join-Path $root 'SemanticGpuEngine4-5.sln') -Encoding UTF8
foreach($project in $projects){
    [xml]$xml=Get-Content -Raw -LiteralPath (Join-Path $root $project.Path) -Encoding UTF8
    $guid=[string](($xml.Project.PropertyGroup|Where-Object{$_.Label -eq 'Globals'}|Select-Object -First 1).ProjectGuid)
    Require ($guid.ToUpperInvariant() -eq $project.Guid.ToUpperInvariant()) "Project GUID mismatch: $($project.Path)"
    Require ($solution -match [regex]::Escape($project.Path)) "Solution registration missing: $($project.Path)"
}

$verifierProject = Get-Content -Raw -LiteralPath (Join-Path $root '159_SparseLoweringVerifier\159_SparseLoweringVerifier.vcxproj') -Encoding UTF8
Require ($verifierProject -notmatch '158_SparseLoweringPlanner') 'Sparse Verifier must not depend on Planner.'
$executionProject = Get-Content -Raw -LiteralPath (Join-Path $root '156_Spiral6SparseExecution\156_Spiral6SparseExecution.vcxproj') -Encoding UTF8
Require ($executionProject -match '159_SparseLoweringVerifier') 'Sparse execution must depend on the opaque Verifier boundary.'
Require ($executionProject -notmatch '157_SparseLoweringCandidate') 'Sparse execution must not depend directly on Raw Candidate.'
Require ($executionProject -notmatch '158_SparseLoweringPlanner') 'Sparse execution must not depend directly on Planner.'

$candidateHeader = Get-Content -Raw -LiteralPath (Join-Path $root '157_SparseLoweringCandidate\SparseLoweringCandidate.h') -Encoding UTF8
foreach($token in @('RawSparseLoweringCandidateV1','writeSetAuthorityIdentity','proposedIndexResourceIdentity','rawCandidateIdentity')){Require ($candidateHeader -match [regex]::Escape($token)) "CU3 Raw Candidate token missing: $token"}
$verifierHeader = Get-Content -Raw -LiteralPath (Join-Path $root '159_SparseLoweringVerifier\SparseLoweringVerifier.h') -Encoding UTF8
foreach($token in @('VerifiedSparseLoweringV1() = delete','VerifiedSparseExecutionPlanV1','CertificateIdentity','ValidateVerifiedSparseLoweringContextV1')){Require ($verifierHeader -match [regex]::Escape($token)) "CU3 Verifier token missing: $token"}
$executionHeader = Get-Content -Raw -LiteralPath (Join-Path $root '156_Spiral6SparseExecution\Spiral6SparseExecution.h') -Encoding UTF8
foreach($token in @('FrozenVerifiedCompactIndexExecutionV1() = delete','actualIndexResourceIdentity','deviceEpoch','ValidateFrozenCompactIndexExecutionEpochV1','ExecuteVerifiedCompactIndexListV1')){Require ($executionHeader -match [regex]::Escape($token)) "CU3 execution authority token missing: $token"}
$test = Get-Content -Raw -LiteralPath (Join-Path $root '167_Spiral6AuthorityMutationTests\main.cpp') -Encoding UTF8
Require (([regex]::Matches($test,'reject\(p,')).Count -eq 36) 'CU3 test must contain exactly 36 Raw proposal attacks.'
foreach($token in @('Cross-set Verified seal replay accepted','Cross-device-epoch Frozen replay accepted','Wrong actual Resource accepted','HashScatterPermutation','Runtime sparse-policy decision: None')){Require ($test -match [regex]::Escape($token)) "CU3 test gate missing: $token"}

Write-Host 'SGE4-5 Spiral 6 CU3 static independent-authority boundaries passed.'
Write-Host 'Verifier -> Planner dependency: absent.'
Write-Host 'Raw proposal attacks: 36; set/context/resource/epoch replay gates present.'
