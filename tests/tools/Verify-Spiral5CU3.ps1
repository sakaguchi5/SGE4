$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
function Require([bool]$Condition,[string]$Message){if(-not $Condition){throw $Message}}

$manifestPath = Join-Path $root 'docs\spiral5\CU3_AUTHORITY_MANIFEST_V1.json'
Require (Test-Path -LiteralPath $manifestPath -PathType Leaf) 'Spiral 5 CU3 authority manifest missing.'
$m = Get-Content -Raw -LiteralPath $manifestPath -Encoding UTF8 | ConvertFrom-Json
Require ($m.schema -eq 'SGE4-5.Spiral5.CU3AuthorityManifest.V1') 'CU3 manifest schema mismatch.'
Require ($m.completionUnit -eq 'CU3') 'CU3 completion unit mismatch.'
Require ($m.ownerBaselineCommit -eq 'f29d6597ec370d963c7b7dfbbc9af9590e8bd58f') 'CU3 Owner baseline mismatch.'
Require ($m.predecessor.architectureEvidenceSha256 -eq 'C7599F52F0D5D7AA9288568274F8E8663A023C52859F1E992D18BF454BF1EC92') 'CU3 predecessor evidence mismatch.'
Require ([bool]$m.plannerIndependentVerifier) 'CU3 Verifier must be Planner-independent.'
Require (-not [bool]$m.verifierDependsOnPlanner) 'CU3 manifest must not claim a Verifier-to-Planner dependency.'
Require ($m.rawToVerified -eq 'CompileTimeUnavailable') 'Raw-to-Verified type boundary mismatch.'
Require ($m.rawToFrozen -eq 'CompileTimeUnavailable') 'Raw-to-Frozen type boundary mismatch.'
Require ($m.verifiedSchedule -eq 'P4') 'CU3 verified schedule mismatch.'
Require ([int]$m.verifiedInvocationCount -eq 128) 'CU3 verified invocation count mismatch.'
Require ([int]$m.verifiedDeviceEpoch -eq 1) 'CU3 verified device epoch mismatch.'
Require ([int]$m.mutationProposalCount -eq 40) 'CU3 mutation proposal count mismatch.'
Require ($m.runtimeTemporalPolicyDecision -eq 'None') 'CU3 Runtime temporal decision must remain None.'
Require ($m.legacySchemaMutation -eq 'None' -and $m.legacyRuntimeMutation -eq 'None' -and $m.legacyBackendMutation -eq 'None') 'CU3 legacy mutation boundary mismatch.'

$required = @(
'137_TemporalLoweringCandidate\137_TemporalLoweringCandidate.vcxproj','137_TemporalLoweringCandidate\TemporalLoweringCandidate.h','137_TemporalLoweringCandidate\TemporalLoweringCandidate.cpp',
'138_TemporalLoweringPlanner\138_TemporalLoweringPlanner.vcxproj','138_TemporalLoweringPlanner\TemporalLoweringPlanner.h','138_TemporalLoweringPlanner\TemporalLoweringPlanner.cpp',
'139_TemporalLoweringVerifier\139_TemporalLoweringVerifier.vcxproj','139_TemporalLoweringVerifier\TemporalLoweringVerifier.h','139_TemporalLoweringVerifier\TemporalLoweringVerifier.cpp',
'136_Spiral5TemporalExecution\136_Spiral5TemporalExecution.vcxproj','136_Spiral5TemporalExecution\Spiral5TemporalExecution.h','136_Spiral5TemporalExecution\Spiral5TemporalExecution.cpp',
'146_Spiral5AuthorityMutationTests\146_Spiral5AuthorityMutationTests.vcxproj','146_Spiral5AuthorityMutationTests\main.cpp',
'tests\Run-Spiral5CU3.ps1','tests\tools\Register-Spiral5CU3Projects.ps1','tests\tools\Verify-Spiral5CU3.ps1',
'run_sge4_5_spiral5_cu3_prepare.bat','run_sge4_5_spiral5_cu3_independent_authority.bat',
'docs\spiral5\CU3_AUTHORITY_MANIFEST_V1.json','docs\spiral5\CU3_INDEPENDENT_TEMPORAL_AUTHORITY.md','docs\spiral5\CU3_MUTATION_MATRIX.md','docs\spiral5\CU3_CHANGESET.md','docs\spiral5\CU3_EVIDENCE_LEDGER.md')
foreach($relative in $required){Require (Test-Path -LiteralPath (Join-Path $root $relative) -PathType Leaf) "Missing Spiral 5 CU3 file: $relative"}

$projects = @(
    [pscustomobject]@{Path='137_TemporalLoweringCandidate\137_TemporalLoweringCandidate.vcxproj';Guid='{00000089-0000-5000-8000-000000000089}'},
    [pscustomobject]@{Path='138_TemporalLoweringPlanner\138_TemporalLoweringPlanner.vcxproj';Guid='{0000008A-0000-5000-8000-00000000008A}'},
    [pscustomobject]@{Path='139_TemporalLoweringVerifier\139_TemporalLoweringVerifier.vcxproj';Guid='{0000008B-0000-5000-8000-00000000008B}'},
    [pscustomobject]@{Path='146_Spiral5AuthorityMutationTests\146_Spiral5AuthorityMutationTests.vcxproj';Guid='{00000092-0000-5000-8000-000000000092}'}
)
$solution = Get-Content -Raw -LiteralPath (Join-Path $root 'SemanticGpuEngine4-5.sln') -Encoding UTF8
foreach($project in $projects){
    [xml]$xml = Get-Content -Raw -LiteralPath (Join-Path $root $project.Path) -Encoding UTF8
    $guid=[string](($xml.Project.PropertyGroup|Where-Object{$_.Label -eq 'Globals'}|Select-Object -First 1).ProjectGuid)
    Require ($guid.ToUpperInvariant() -eq $project.Guid.ToUpperInvariant()) "Project GUID mismatch: $($project.Path)"
    Require (([regex]::Matches($solution,[regex]::Escape($project.Path))).Count -eq 1) "Solution registration mismatch: $($project.Path)"
}

$plannerProject = Get-Content -Raw -LiteralPath (Join-Path $root '138_TemporalLoweringPlanner\138_TemporalLoweringPlanner.vcxproj') -Encoding UTF8
Require ($plannerProject -match '137_TemporalLoweringCandidate') 'Temporal Planner must emit a Raw Temporal Candidate.'
$verifierProject = Get-Content -Raw -LiteralPath (Join-Path $root '139_TemporalLoweringVerifier\139_TemporalLoweringVerifier.vcxproj') -Encoding UTF8
Require ($verifierProject -match '137_TemporalLoweringCandidate') 'Temporal Verifier must inspect Raw Temporal Candidates.'
Require ($verifierProject -notmatch '138_TemporalLoweringPlanner') 'Temporal Verifier must not depend on the Planner.'
$executionProject = Get-Content -Raw -LiteralPath (Join-Path $root '136_Spiral5TemporalExecution\136_Spiral5TemporalExecution.vcxproj') -Encoding UTF8
Require ($executionProject -match '139_TemporalLoweringVerifier') 'Verified Temporal execution must depend on the opaque Verifier type.'
foreach($forbidden in @('137_TemporalLoweringCandidate','138_TemporalLoweringPlanner','22_CompositionRuntime','14_D3D12Backend')){Require ($executionProject -notmatch [regex]::Escape($forbidden)) "Verified execution has forbidden direct dependency: $forbidden"}

$candidateHeader = Get-Content -Raw -LiteralPath (Join-Path $root '137_TemporalLoweringCandidate\TemporalLoweringCandidate.h') -Encoding UTF8
foreach($token in @('RawTemporalLoweringCandidateV1','invocationAuthorityIdentity','proposedHistoryResourceIdentity','deviceEpochPolicyIdentity')){Require ($candidateHeader -match [regex]::Escape($token)) "Raw Candidate authority token missing: $token"}
$verifierHeader = Get-Content -Raw -LiteralPath (Join-Path $root '139_TemporalLoweringVerifier\TemporalLoweringVerifier.h') -Encoding UTF8
foreach($token in @('class VerifiedTemporalLoweringV1','VerifiedTemporalLoweringV1() = delete','CertificateIdentity','VerifiedTemporalExecutionPlanV1')){Require ($verifierHeader -match [regex]::Escape($token)) "Verified type token missing: $token"}
$verifierCpp = Get-Content -Raw -LiteralPath (Join-Path $root '139_TemporalLoweringVerifier\TemporalLoweringVerifier.cpp') -Encoding UTF8
foreach($token in @('IndependentCanonicalContext','IndependentHistoryResourceIdentity','IndependentInvocationAuthorityIdentity','SameProposal','raw temporal candidate identity mismatch')){Require ($verifierCpp -match [regex]::Escape($token)) "Independent Verifier token missing: $token"}
Require ($verifierCpp -notmatch 'TemporalLoweringPlanner') 'Verifier source must not call the Planner.'
$executionHeader = Get-Content -Raw -LiteralPath (Join-Path $root '136_Spiral5TemporalExecution\Spiral5TemporalExecution.h') -Encoding UTF8
foreach($token in @('FrozenVerifiedGlobalMotorHistoryExecutionV1','TemporalHistoryResourceBindingInputV1','expectedDeviceEpoch')){Require ($executionHeader -match [regex]::Escape($token)) "Frozen binding token missing: $token"}
$testCpp = Get-Content -Raw -LiteralPath (Join-Path $root '146_Spiral5AuthorityMutationTests\main.cpp') -Encoding UTF8
foreach($token in @('attempted == 40','cross-schedule Verified seal replay','cross-device-epoch Frozen History replay','wrong actual History Resource identity','Runtime temporal-policy decision: None')){Require ($testCpp -match [regex]::Escape($token)) "CU3 executable gate token missing: $token"}

$schema = Get-Content -Raw -LiteralPath (Join-Path $root '10_D3D12PackageSchema\D3D12Schema.h') -Encoding UTF8
Require ($schema -match 'WaitTemporal') 'Legacy Temporal physical support disappeared.'
$backend = Get-Content -Raw -LiteralPath (Join-Path $root '14_D3D12Backend\D3D12Backend.cpp') -Encoding UTF8
Require ($backend -notmatch 'TemporalLoweringPlanner') 'Backend must not depend on the Temporal Planner.'
Require ($backend -notmatch 'RawTemporalLoweringCandidateV1') 'Backend must not depend on Raw Temporal Candidates.'

Write-Host 'SGE4-5 Spiral 5 CU3 independent Temporal authority boundaries passed.'
Write-Host 'Verifier -> Planner dependency: absent.'
Write-Host 'Raw -> Verified/Frozen construction: compile-time unavailable.'
Write-Host 'Schedule, generation, role, artifact, Program, Resource, and epoch bindings: present.'
