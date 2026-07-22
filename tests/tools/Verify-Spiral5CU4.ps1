$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
function Require([bool]$Condition, [string]$Message) { if (-not $Condition) { throw $Message } }

$manifestPath = Join-Path $root 'docs\spiral5\CU4_CANDIDATE_FAMILY_MANIFEST_V1.json'
Require (Test-Path -LiteralPath $manifestPath -PathType Leaf) 'Spiral 5 CU4 manifest missing.'
$m = Get-Content -Raw -LiteralPath $manifestPath -Encoding UTF8 | ConvertFrom-Json
Require ($m.schema -eq 'SGE4-5.Spiral5.CU4CandidateFamilyManifest.V1') 'CU4 manifest schema mismatch.'
Require ($m.completionUnit -eq 'CU4') 'CU4 completion unit mismatch.'
Require ($m.predecessor.cu3AuthorityEvidenceSha256 -eq '9C05F1EB8F73DD45577F0F97C92EC76E0DB60B2A6A0BA487CB998FC2359E84DE') 'CU4 predecessor evidence mismatch.'
Require (($m.candidateKinds -join ',') -eq 'EveryInvocationRecompute,GlobalMotorHistoryReuse,FinalOutputHistoryReuse') 'CU4 Candidate family mismatch.'
Require ([int]$m.candidateInvocationCountPerBuild -eq 1536) 'CU4 invocation count mismatch.'
Require ([int]$m.mutationProposalCount -eq 38) 'CU4 mutation count mismatch.'
Require ($m.runtimeTemporalPolicyDecision -eq 'None') 'CU4 Runtime policy must remain None.'
Require ($m.legacySchemaMutation -eq 'None' -and $m.legacyRuntimeMutation -eq 'None' -and $m.legacyBackendMutation -eq 'None') 'CU4 legacy mutation boundary mismatch.'

$projects = @(
    [pscustomobject]@{ Path = '147_TemporalCandidateFamily\147_TemporalCandidateFamily.vcxproj'; Guid = '{00000093-0000-5000-8000-000000000093}' },
    [pscustomobject]@{ Path = '148_TemporalCandidateFamilyPlanner\148_TemporalCandidateFamilyPlanner.vcxproj'; Guid = '{00000094-0000-5000-8000-000000000094}' },
    [pscustomobject]@{ Path = '149_TemporalCandidateFamilyVerifier\149_TemporalCandidateFamilyVerifier.vcxproj'; Guid = '{00000095-0000-5000-8000-000000000095}' },
    [pscustomobject]@{ Path = '150_Spiral5TemporalCandidateFamilyTests\150_Spiral5TemporalCandidateFamilyTests.vcxproj'; Guid = '{00000096-0000-5000-8000-000000000096}' }
)
$solution = Get-Content -Raw -LiteralPath (Join-Path $root 'SemanticGpuEngine4-5.sln') -Encoding UTF8
foreach ($project in $projects) {
    $projectPath = Join-Path $root $project.Path
    Require (Test-Path -LiteralPath $projectPath -PathType Leaf) "Missing CU4 project: $($project.Path)"
    [xml]$xml = Get-Content -Raw -LiteralPath $projectPath -Encoding UTF8
    $actualGuid = [string](
        $xml.Project.PropertyGroup |
        Where-Object { $_.Label -eq 'Globals' } |
        Select-Object -First 1
    ).ProjectGuid
    Require ($actualGuid.ToUpperInvariant() -eq $project.Guid.ToUpperInvariant()) "CU4 ProjectGuid mismatch: $($project.Path)"
    Require (([regex]::Matches($solution, [regex]::Escape($project.Path))).Count -eq 1) "CU4 Solution registration mismatch: $($project.Path)"
}

$plannerProject = Get-Content -Raw -LiteralPath (Join-Path $root '148_TemporalCandidateFamilyPlanner\148_TemporalCandidateFamilyPlanner.vcxproj') -Encoding UTF8
$verifierProject = Get-Content -Raw -LiteralPath (Join-Path $root '149_TemporalCandidateFamilyVerifier\149_TemporalCandidateFamilyVerifier.vcxproj') -Encoding UTF8
Require ($plannerProject -match '147_TemporalCandidateFamily') 'CU4 Planner must emit family Raw Candidates.'
Require ($verifierProject -match '147_TemporalCandidateFamily') 'CU4 Verifier must inspect family Raw Candidates.'
Require ($verifierProject -notmatch '148_TemporalCandidateFamilyPlanner') 'CU4 Verifier must be Planner-independent.'

$executionProject = Get-Content -Raw -LiteralPath (Join-Path $root '136_Spiral5TemporalExecution\136_Spiral5TemporalExecution.vcxproj') -Encoding UTF8
Require ($executionProject -match '149_TemporalCandidateFamilyVerifier') 'CU4 execution must consume opaque family Verified types.'
Require ($executionProject -notmatch '148_TemporalCandidateFamilyPlanner') 'CU4 execution must not depend on family Planner.'

$candidateHeader = Get-Content -Raw -LiteralPath (Join-Path $root '147_TemporalCandidateFamily\TemporalCandidateFamily.h') -Encoding UTF8
foreach ($token in @('EveryInvocationRecompute','GlobalMotorHistoryReuse','FinalOutputHistoryReuse','TemporalFamilyHistoryRoleV1','invocationAuthorityIdentity')) {
    Require ($candidateHeader -match [regex]::Escape($token)) "CU4 Candidate token missing: $token"
}
$verifierHeader = Get-Content -Raw -LiteralPath (Join-Path $root '149_TemporalCandidateFamilyVerifier\TemporalCandidateFamilyVerifier.h') -Encoding UTF8
foreach ($token in @('class VerifiedTemporalCandidateV1','VerifiedTemporalCandidateV1() = delete','CertificateIdentity','VerifiedTemporalCandidatePlanV1')) {
    Require ($verifierHeader -match [regex]::Escape($token)) "CU4 Verified type token missing: $token"
}
$testCpp = Get-Content -Raw -LiteralPath (Join-Path $root '150_Spiral5TemporalCandidateFamilyTests\main.cpp') -Encoding UTF8
foreach ($token in @('attempted==38','pairwiseOutputEquivalent','Cross-schedule Candidate seal replay accepted','Cross-role History binding accepted','4 x 3 x 128 = 1536','Runtime temporal-policy decision: None')) {
    Require ($testCpp -match [regex]::Escape($token)) "CU4 test token missing: $token"
}

$schema = Get-Content -Raw -LiteralPath (Join-Path $root '10_D3D12PackageSchema\D3D12Schema.h') -Encoding UTF8
Require ($schema -match 'WaitTemporal') 'Legacy Temporal operation disappeared.'
Write-Host 'SGE4-5 Spiral 5 CU4 Candidate family boundaries passed.'
Write-Host 'Verifier -> Planner dependency: absent. Runtime temporal-policy decision: None.'
