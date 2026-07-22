$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)

function Require([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}

$manifestPath = Join-Path $root 'docs\spiral4\CU3_AUTHORITY_MANIFEST_V1.json'
Require (Test-Path -LiteralPath $manifestPath -PathType Leaf) 'Spiral 4 CU3 authority manifest missing.'
$m = Get-Content -Raw -LiteralPath $manifestPath -Encoding UTF8 | ConvertFrom-Json

Require ($m.schema -eq 'SGE4-5.Spiral4.CU3AuthorityManifest.V1') 'CU3 authority manifest schema mismatch.'
Require ($m.baselineCommit -eq 'ca29f228687691769355afe33f390adf04c1ed24') 'CU3 baseline mismatch.'
Require ($m.rawExecutionAuthority -eq 'None') 'Raw Candidate must have no execution authority.'
Require ($m.verifierPlannerDependency -eq 'Forbidden') 'Verifier/Planner dependency policy mismatch.'
Require ($m.runtimeDispatchDecision -eq 'None') 'CU3 Runtime dispatch decision must remain None.'
Require ([int]$m.authorityMutationCount -eq 27) 'CU3 mutation corpus mismatch.'

$required = @(
    '123_ActiveWorkLoweringCandidate\123_ActiveWorkLoweringCandidate.vcxproj',
    '123_ActiveWorkLoweringCandidate\ActiveWorkLoweringCandidate.h',
    '123_ActiveWorkLoweringCandidate\ActiveWorkLoweringCandidate.cpp',
    '124_ActiveWorkLoweringPlanner\124_ActiveWorkLoweringPlanner.vcxproj',
    '124_ActiveWorkLoweringPlanner\ActiveWorkLoweringPlanner.h',
    '124_ActiveWorkLoweringPlanner\ActiveWorkLoweringPlanner.cpp',
    '125_ActiveWorkLoweringVerifier\125_ActiveWorkLoweringVerifier.vcxproj',
    '125_ActiveWorkLoweringVerifier\ActiveWorkLoweringVerifier.h',
    '125_ActiveWorkLoweringVerifier\ActiveWorkLoweringVerifier.cpp',
    '126_Spiral4VerifiedExecution\126_Spiral4VerifiedExecution.vcxproj',
    '126_Spiral4VerifiedExecution\Spiral4VerifiedExecution.h',
    '126_Spiral4VerifiedExecution\Spiral4VerifiedExecution.cpp',
    '141_Spiral4AuthorityMutationTests\141_Spiral4AuthorityMutationTests.vcxproj',
    '141_Spiral4AuthorityMutationTests\main.cpp',
    'tests\tools\Register-Spiral4CU3Projects.ps1',
    'tests\tools\Verify-Spiral4CU3.ps1',
    'tests\Run-Spiral4CU3.ps1',
    'run_sge4_5_spiral4_cu3_prepare.bat',
    'run_sge4_5_spiral4_cu3_independent_authority.bat',
    'docs\spiral4\CU3_AUTHORITY_MANIFEST_V1.json',
    'docs\spiral4\CU3_INDEPENDENT_AUTHORITY.md',
    'docs\spiral4\CU3_MUTATION_MATRIX.md',
    'docs\spiral4\CU3_CHANGESET.md',
    'docs\spiral4\CU3_EVIDENCE_LEDGER.md'
)
foreach ($relative in $required) {
    Require (Test-Path -LiteralPath (Join-Path $root $relative) -PathType Leaf) "Missing Spiral 4 CU3 file: $relative"
}

$solution = Get-Content -Raw -LiteralPath (Join-Path $root 'SemanticGpuEngine4-5.sln') -Encoding UTF8
$projects = @(
    [pscustomobject]@{ Path='123_ActiveWorkLoweringCandidate\123_ActiveWorkLoweringCandidate.vcxproj'; Guid='{0000007B-0000-5000-8000-00000000007B}' },
    [pscustomobject]@{ Path='124_ActiveWorkLoweringPlanner\124_ActiveWorkLoweringPlanner.vcxproj'; Guid='{0000007C-0000-5000-8000-00000000007C}' },
    [pscustomobject]@{ Path='125_ActiveWorkLoweringVerifier\125_ActiveWorkLoweringVerifier.vcxproj'; Guid='{0000007D-0000-5000-8000-00000000007D}' },
    [pscustomobject]@{ Path='126_Spiral4VerifiedExecution\126_Spiral4VerifiedExecution.vcxproj'; Guid='{0000007E-0000-5000-8000-00000000007E}' },
    [pscustomobject]@{ Path='141_Spiral4AuthorityMutationTests\141_Spiral4AuthorityMutationTests.vcxproj'; Guid='{0000008D-0000-5000-8000-00000000008D}' }
)
foreach ($project in $projects) {
    Require ($solution.Contains($project.Path)) "CU3 project missing from Solution: $($project.Path)"
    foreach ($entry in @(
        "$($project.Guid).Debug|x64.ActiveCfg = Debug|x64",
        "$($project.Guid).Debug|x64.Build.0 = Debug|x64",
        "$($project.Guid).Release|x64.ActiveCfg = Release|x64",
        "$($project.Guid).Release|x64.Build.0 = Release|x64"
    )) {
        Require ($solution.Contains($entry)) "CU3 Solution configuration missing: $entry"
    }
}

$verifierProject = Get-Content -Raw -LiteralPath (Join-Path $root '125_ActiveWorkLoweringVerifier\125_ActiveWorkLoweringVerifier.vcxproj') -Encoding UTF8
$verifierSource = Get-Content -Raw -LiteralPath (Join-Path $root '125_ActiveWorkLoweringVerifier\ActiveWorkLoweringVerifier.cpp') -Encoding UTF8
Require ($verifierProject -notmatch '124_ActiveWorkLoweringPlanner') 'Independent Verifier references Planner project.'
Require ($verifierSource -notmatch 'ActiveWorkLoweringPlanner') 'Independent Verifier source references Planner.'
foreach ($token in @(
    'IndependentCanonicalContext',
    'DeriveExpectedPlan',
    'raw candidate identity mismatch',
    'candidate execution decision mismatch',
    'verified plan context replay mismatch',
    'ActiveWorkVerifierCertificate'
)) {
    Require ($verifierSource.Contains($token)) "CU3 Verifier token missing: $token"
}

$verifiedHeader = Get-Content -Raw -LiteralPath (Join-Path $root '125_ActiveWorkLoweringVerifier\ActiveWorkLoweringVerifier.h') -Encoding UTF8
Require ($verifiedHeader.Contains('VerifiedActiveWorkLoweringV1() = delete')) 'Verified type is default constructible.'
Require ($verifiedHeader.Contains('private:')) 'Verified type private construction boundary missing.'

$frozenHeader = Get-Content -Raw -LiteralPath (Join-Path $root '126_Spiral4VerifiedExecution\Spiral4VerifiedExecution.h') -Encoding UTF8
Require ($frozenHeader.Contains('FrozenVerifiedSingleIndirectExecutionV1() = delete')) 'Frozen Verified execution is default constructible.'
Require ($frozenHeader.Contains('const verification::VerifiedActiveWorkLoweringV1& verified')) 'Freeze entry does not require Verified authority.'

$executionProject = Get-Content -Raw -LiteralPath (Join-Path $root '126_Spiral4VerifiedExecution\126_Spiral4VerifiedExecution.vcxproj') -Encoding UTF8
Require ($executionProject -notmatch '124_ActiveWorkLoweringPlanner') 'Verified execution adapter references Planner.'
Require ($executionProject -match '125_ActiveWorkLoweringVerifier') 'Verified execution adapter does not reference Verifier.'

$lowLevelHeader = Get-Content -Raw -LiteralPath (Join-Path $root '122_Spiral4IndirectExecution\Spiral4IndirectExecution.h') -Encoding UTF8
$lowLevelSource = Get-Content -Raw -LiteralPath (Join-Path $root '122_Spiral4IndirectExecution\Spiral4IndirectExecution.cpp') -Encoding UTF8
Require ($lowLevelHeader.Contains('producerShaderBytecodeDigest')) 'Producer shader digest evidence missing.'
Require ($lowLevelHeader.Contains('consumerShaderBytecodeDigest')) 'Consumer shader digest evidence missing.'
Require ($lowLevelSource.Contains('producerShaderBytecodeDigest = base::Sha256')) 'Producer actual shader digest not generated.'
Require ($lowLevelSource.Contains('consumerShaderBytecodeDigest = base::Sha256')) 'Consumer actual shader digest not generated.'

$tests = Get-Content -Raw -LiteralPath (Join-Path $root '141_Spiral4AuthorityMutationTests\main.cpp') -Encoding UTF8
foreach ($token in @(
    'attempted == 27',
    'cross-target Verified seal replay was accepted',
    'cross-command-signature seal replay was accepted',
    'cross-observation Freeze replay was accepted',
    'RunVerifiedSingleIndirectOnWarpV1',
    'Runtime dispatch-dimension decision: None'
)) {
    Require ($tests.Contains($token)) "CU3 authority test token missing: $token"
}

Write-Host 'SGE4-5 Spiral 4 CU3 project, independence, type, mutation, and actual binding boundaries passed.'
