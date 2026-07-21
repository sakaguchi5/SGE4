$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
foreach ($name in @('103_ReuseRepresentationCandidate','104_ReuseRepresentationPlanner','105_ReuseRepresentationVerifier','111_Spiral3CandidateGraphTests','112_Spiral3AuthorityMutationTests')) {
    if (-not (Test-Path (Join-Path $root "$name\$name.vcxproj"))) { throw "Missing CU2 project: $name" }
}
[xml]$verifierProject = Get-Content -Raw (Join-Path $root '105_ReuseRepresentationVerifier\105_ReuseRepresentationVerifier.vcxproj')
$references = @($verifierProject.SelectNodes("//*[local-name()='ProjectReference']") | ForEach-Object { $_.Include })
if ($references -match '104_ReuseRepresentationPlanner') { throw 'Independent verifier references Planner' }
$verifier = Get-Content -Raw (Join-Path $root '105_ReuseRepresentationVerifier\ReuseRepresentationVerifier.cpp')
if ($verifier -match 'PlanCandidateGraphV1|ReuseRepresentationPlanner') { throw 'Verifier calls Planner' }
$raw = Get-Content -Raw (Join-Path $root '103_ReuseRepresentationCandidate\ReuseRepresentationCandidate.h')
if ($raw -match '\b(Freeze|Execute|Submit)\b') { throw 'Raw Candidate exposes execution authority' }
$typeTests = Get-Content -Raw (Join-Path $root '112_Spiral3AuthorityMutationTests\main.cpp')
foreach ($required in @('is_default_constructible_v','is_constructible_v','is_convertible_v','rejected == 24')) {
    if ($typeTests -notmatch [regex]::Escape($required)) { throw "Authority type/mutation gate missing: $required" }
}
Write-Host 'Spiral 3 CU2 independent authority, compile-time Raw/Verified, and 24-mutation boundaries passed.'
