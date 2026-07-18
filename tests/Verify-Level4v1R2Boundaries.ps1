$ErrorActionPreference = 'Stop'

$testsRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$root = Split-Path -Parent $testsRoot

function Get-DirectReferenceNames([string]$ProjectName) {
    $projectPath = Join-Path $root "$ProjectName/$ProjectName.vcxproj"
    if (-not (Test-Path -LiteralPath $projectPath)) { throw "Project is missing: $ProjectName" }
    [xml]$xml = Get-Content -Raw -LiteralPath $projectPath -Encoding UTF8
    $names = foreach ($node in $xml.SelectNodes("//*[local-name()='ProjectReference']")) {
        Split-Path (Split-Path $node.Include -Parent) -Leaf
    }
    return [string[]]@($names | Sort-Object -Unique)
}

function Assert-ExactReferences([string]$ProjectName, [string[]]$Expected) {
    $actual = [string[]]@(Get-DirectReferenceNames $ProjectName)
    $expectedSorted = [string[]]@($Expected | Sort-Object -Unique)
    $difference = @(Compare-Object -ReferenceObject $expectedSorted -DifferenceObject $actual)
    if ($difference.Count -ne 0) {
        throw "$ProjectName direct references differ from the R2 boundary.`nExpected: $($expectedSorted -join ', ')`nActual: $($actual -join ', ')"
    }
}

function Assert-SourceDoesNotContain([string]$Directory, [string[]]$Patterns) {
    foreach ($file in Get-ChildItem -Path (Join-Path $root $Directory) -File -Recurse |
        Where-Object { $_.Extension -in '.h', '.cpp' }) {
        $text = Get-Content -Raw -LiteralPath $file.FullName -Encoding UTF8
        foreach ($pattern in $Patterns) {
            if ($text -match [regex]::Escape($pattern)) {
                throw "$Directory leaked forbidden authority '$pattern' through $($file.Name)"
            }
        }
    }
}

Assert-ExactReferences '17_CompositionContract' @(
    '00_Foundation', '09_FrozenPackageCore', '10_D3D12PackageSchema')
Assert-ExactReferences '18_CompositionPlan' @(
    '00_Foundation', '17_CompositionContract')
Assert-ExactReferences '19_CompositionVerifier' @(
    '00_Foundation', '16_FrozenCompositionArtifact',
    '17_CompositionContract', '18_CompositionPlan')
Assert-ExactReferences '47_CanonicalCompositionAuthorityTests' @(
    '00_Foundation', '02_SemanticModel', '03_SemanticBuilder',
    '04_SemanticAnalysis', '05A_CompilationInput', '05_TargetContract',
    '06_ExecutionPlanModel', '07_ExecutionPlanVerifier', '08_CandidatePlanner',
    '09_FrozenPackageCore', '10_D3D12PackageSchema', '11_D3D12PackageLowering',
    '12_SGE4Compiler', '16_FrozenCompositionArtifact', '17_CompositionContract',
    '18_CompositionPlan', '19_CompositionVerifier')

Assert-SourceDoesNotContain '17_CompositionContract' @(
    'SemanticModel', 'SemanticAnalysis', 'ExecutionPlan', 'CandidatePlanner',
    'SGE4Compiler', 'PackageRuntime', 'D3D12Backend', 'CompositionRuntime',
    '<Windows.h>', '<d3d12.h>', '<dxgi')
Assert-SourceDoesNotContain '18_CompositionPlan' @(
    'FrozenCompositionWriter', 'FrozenCompositionReader', 'PackageRuntime',
    'D3D12Backend', '<Windows.h>', '<d3d12.h>', '<dxgi')
Assert-SourceDoesNotContain '19_CompositionVerifier' @(
    'SemanticModel', 'SemanticAnalysis', 'CandidatePlanner', 'SGE4Compiler',
    'PackageRuntime', 'D3D12Backend', 'CompositionRuntime',
    '<Windows.h>', '<d3d12.h>', '<dxgi')

$contractHeader = Get-Content -Raw -LiteralPath (Join-Path $root '17_CompositionContract/CompositionContract.h') -Encoding UTF8
$contractSource = Get-Content -Raw -LiteralPath (Join-Path $root '17_CompositionContract/CompositionContract.cpp') -Encoding UTF8
$planHeader = Get-Content -Raw -LiteralPath (Join-Path $root '18_CompositionPlan/CompositionPlan.h') -Encoding UTF8
$verifierHeader = Get-Content -Raw -LiteralPath (Join-Path $root '19_CompositionVerifier/CompositionVerifier.h') -Encoding UTF8
$verifierSource = Get-Content -Raw -LiteralPath (Join-Path $root '19_CompositionVerifier/CompositionVerifier.cpp') -Encoding UTF8
$tests = Get-Content -Raw -LiteralPath (Join-Path $root '47_CanonicalCompositionAuthorityTests/main.cpp') -Encoding UTF8

foreach ($required in @(
    'ValidatedCompositionContract', 'BuildCompositionContract',
    'ValidateCompositionContractAgainstLeaves', 'DeserializeCompositionContract',
    'ExternalSlots()', 'Views()', 'EndpointAccess::ReadOnly',
    'EndpointAccess::WriteOnly', 'CompletionTokenRequired')) {
    if (($contractHeader + "`n" + $contractSource) -notmatch [regex]::Escape($required)) {
        throw "R2 Contract authority is missing required item: $required"
    }
}

$endpointAuthorBlock = [regex]::Match(
    $contractHeader, '(?s)struct LeafEndpointDeclaration final\s*\{.*?\};').Value
if (-not $endpointAuthorBlock) { throw 'LeafEndpointDeclaration was not found.' }
foreach ($forbidden in @('EndpointAccess', 'ResourceState', 'minimumBytes', 'synchronization')) {
    if ($endpointAuthorBlock -match [regex]::Escape($forbidden)) {
        throw "Composition author can forge Leaf-derived endpoint authority: $forbidden"
    }
}

# Inspect executable declarations rather than comments or descriptive text.
# PowerShell -match is case-insensitive by default, so the former plain-text
# search falsely treated the comment "does not grant ... freeze authority"
# as a leaked Freeze API.
$planCode = [regex]::Replace(
    $planHeader,
    '(?s)/\*.*?\*/|//[^\r\n]*',
    '')
$rawPlanBlock = [regex]::Match(
    $planCode,
    '(?s)struct\s+RawCompositionPlan\s+final\s*\{.*?\};').Value
if (-not $rawPlanBlock) {
    throw 'RawCompositionPlan declaration was not found.'
}

$authorityFunctionPattern =
    '(?s)\b(?:[A-Za-z_]\w*)?(?:Execute|Submit|Freeze)(?:[A-Za-z_]\w*)?\s*\('
if ($rawPlanBlock -match $authorityFunctionPattern) {
    throw 'RawCompositionPlan contains an execution, submission or freeze member API.'
}

$rawPlanAuthorityApiPattern =
    '(?s)\b(?:[A-Za-z_]\w*)?(?:Execute|Submit|Freeze)(?:[A-Za-z_]\w*)?\s*\([^;{}]*\bRawCompositionPlan\b[^;{}]*\)\s*;'
if ($planCode -match $rawPlanAuthorityApiPattern) {
    throw 'CompositionPlan exposes an execution, submission or freeze API that accepts RawCompositionPlan.'
}
if ($verifierSource -match [regex]::Escape('ProposeCompositionPlan(')) {
    throw 'Independent verifier calls the Planner proposal implementation.'
}
if ($verifierHeader -match '(?s)FreezeVerifiedComposition\s*\([^)]*RawCompositionPlan') {
    throw 'A RawCompositionPlan freeze overload exists.'
}
foreach ($required in @(
    'class VerifiedCompositionPlan final', 'struct ConstructionToken final',
    'VerifyAndSeal', 'FreezeVerifiedComposition', 'ReadVerifiedFrozenComposition',
    'ValidateCompositionContractAgainstLeaves', 'ComputeVerifierSeal')) {
    if (($verifierHeader + "`n" + $verifierSource) -notmatch [regex]::Escape($required)) {
        throw "R2 independent verification is missing required item: $required"
    }
}

foreach ($negativeGate in @(
    'missing Leaf endpoint declaration was accepted',
    'duplicate Leaf endpoint key was accepted',
    'ReadOnly endpoint was accepted as Resource Flow producer',
    'WriteOnly endpoint was accepted as Resource Flow consumer',
    'one Endpoint was accepted in two Resource Flows',
    'unbound required Endpoint was accepted',
    'CompositionInput with a producer was accepted',
    'Leaf-derived endpoint access forgery was accepted after Contract resigning',
    'Leaf-derived endpoint size forgery was accepted after Contract resigning',
    'Leaf-derived endpoint state forgery was accepted after Contract resigning',
    'two presenting Leaves were accepted',
    'cyclic same-frame Composition produced a raw Plan',
    'independent verifier accepted a cyclic same-frame Composition',
    'raw Plan with corrupt identity was verified',
    'resigned Contract identity mutation was verified',
    'resigned allocation mutation was verified',
    'resigned schedule mutation was verified',
    'resigned binding mutation was verified',
    'resigned binding access mutation was verified',
    'resigned state handoff mutation was verified',
    'resigned signal mutation was verified',
    'resigned wait mutation was verified',
    'resigned recovery mutation was verified',
    'resigned forged Plan inside a digest-valid Artifact was accepted',
    'forged verification certificate inside a digest-valid Artifact was accepted',
    'resigned forged Contract inside a digest-valid Artifact was accepted')) {
    if ($tests -notmatch [regex]::Escape($negativeGate)) {
        throw "R2 test suite is missing negative gate: $negativeGate"
    }
}

foreach ($constructionGate in @(
    '!std::is_default_constructible_v<canonical::ValidatedCompositionContract>',
    '!std::is_default_constructible_v<verification::VerifiedCompositionPlan>',
    '!std::is_default_constructible_v<verification::VerifiedFrozenComposition>')) {
    if ($tests -notmatch [regex]::Escape($constructionGate)) {
        throw "R2 type construction gate is missing: $constructionGate"
    }
}

$solution = Get-Content -Raw -LiteralPath (Join-Path $root 'SemanticGpuEngine4.sln') -Encoding UTF8
foreach ($project in @(
    '00_Foundation', '09_FrozenPackageCore', '10_D3D12PackageSchema',
    '16_FrozenCompositionArtifact', '17_CompositionContract',
    '18_CompositionPlan', '19_CompositionVerifier',
    '47_CanonicalCompositionAuthorityTests')) {
    if ($solution -notmatch [regex]::Escape($project)) {
        throw "R2 solution is missing project: $project"
    }
}

Write-Host 'Level 4 v1 Canonical R2 Contract, Plan and independent verifier boundaries passed.'
