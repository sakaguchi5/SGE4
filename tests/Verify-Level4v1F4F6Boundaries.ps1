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
        throw "$ProjectName direct references differ from its F4-F6 boundary.`nExpected: $($expectedSorted -join ', ')`nActual: $($actual -join ', ')"
    }
}

function Assert-Sha256([string]$RelativePath, [string]$Expected) {
    $path = Join-Path $root $RelativePath
    if (-not (Test-Path -LiteralPath $path)) { throw "Frozen file is missing: $RelativePath" }
    $actual = (Get-FileHash -Algorithm SHA256 -LiteralPath $path).Hash.ToLowerInvariant()
    if ($actual -ne $Expected) {
        throw "F4-F6 changed a frozen pre-F4 file: $RelativePath`nExpected: $Expected`nActual:   $actual"
    }
}

Assert-ExactReferences '17A_Schema18ResourceFlowPlan' @(
    '00_Foundation', '16A_Schema18CompositionContract')
Assert-ExactReferences '18A_Schema18ResourceFlowVerifier' @(
    '00_Foundation', '16A_Schema18CompositionContract',
    '17A_Schema18ResourceFlowPlan')
Assert-ExactReferences '19A_Schema18CompositionLinker' @(
    '00_Foundation', '16A_Schema18CompositionContract',
    '17A_Schema18ResourceFlowPlan', '18A_Schema18ResourceFlowVerifier')
$testReferences = @('00_Foundation', '02_SemanticModel', '03_SemanticBuilder', '04_SemanticAnalysis', '05_TargetContract', '05A_CompilationInput', '06_ExecutionPlanModel', '07_ExecutionPlanVerifier', '08_CandidatePlanner', '09_FrozenPackageCore', '10_D3D12PackageSchema', '10A_D3D12CompositionSchema18', '11_D3D12PackageLowering', '12_SGE4Compiler', '12A_CompositionLeafCompiler', '16A_Schema18CompositionContract', '20_ExperimentDomain', '21_ClassicalFrontend', '22_SdfFrontend', '23_PgaFrontend', '24_SliceScenarios', '25_GeneralGraphScenarios', '26_GeneratedGraphCorpus', '27_RuntimeScenarios', '27A_Schema18LeafCorpus', '17A_Schema18ResourceFlowPlan', '18A_Schema18ResourceFlowVerifier', '19A_Schema18CompositionLinker')
foreach ($project in @('46D_Schema18ResourceFlowPlanningTests',
    '46E_Schema18ResourceFlowAuthorityTests',
    '46F_Schema18FrozenCompositionTests')) {
    Assert-ExactReferences $project $testReferences
}

$coreFiles = @(
    '17A_Schema18ResourceFlowPlan/Schema18ResourceFlowPlan.h',
    '17A_Schema18ResourceFlowPlan/Schema18ResourceFlowPlan.cpp',
    '18A_Schema18ResourceFlowVerifier/Schema18ResourceFlowVerifier.h',
    '18A_Schema18ResourceFlowVerifier/Schema18ResourceFlowVerifier.cpp',
    '19A_Schema18CompositionLinker/Schema18CompositionLinker.h',
    '19A_Schema18CompositionLinker/Schema18CompositionLinker.cpp')
$coreText = ($coreFiles | ForEach-Object {
    Get-Content -Raw -LiteralPath (Join-Path $root $_) -Encoding UTF8
}) -join "`n"
foreach ($forbidden in @('13_PackageRuntime', '14_D3D12Backend',
    '16_CompositionContract', '17_LinkPlanModel', '18_LinkPlanVerifier',
    '19_PackageLinker', '29_CompositionRuntime')) {
    if ($coreText.Contains($forbidden)) {
        throw "F4-F6 core leaked Runtime, Backend, or Prototype-A dependency: $forbidden"
    }
}

$f4Header = Get-Content -Raw -LiteralPath (Join-Path $root '17A_Schema18ResourceFlowPlan/Schema18ResourceFlowPlan.h') -Encoding UTF8
foreach ($required in @('RawResourceFlowPlan', 'ResourceAllocationPlan',
    'LeafScheduleEntry', 'EndpointBindingPlan', 'StateHandoffPlan',
    'SignalPointPlan', 'WaitPointPlan', 'RecoveryPlan',
    'ProposeResourceFlowPlan', 'SerializeRawResourceFlowPlan')) {
    if (-not $f4Header.Contains($required)) { throw "F4 Plan Model is missing: $required" }
}
foreach ($forbidden in @('Execute', 'Submit', 'FreezeVerifiedComposition',
    'VerifiedResourceFlowPlan')) {
    if ($f4Header.Contains($forbidden)) { throw "F4 Raw Plan pre-empts later authority: $forbidden" }
}

$f5Header = Get-Content -Raw -LiteralPath (Join-Path $root '18A_Schema18ResourceFlowVerifier/Schema18ResourceFlowVerifier.h') -Encoding UTF8
foreach ($required in @('class VerifiedResourceFlowPlan final', 'ConstructionToken',
    'VerifyAndSeal', 'ValidateVerifiedPlan', 'ComputeVerifierSeal')) {
    if (-not $f5Header.Contains($required)) { throw "F5 Verifier boundary is missing: $required" }
}
$privateIndex = $f5Header.IndexOf('private:')
$authorityConstructorIndex = $f5Header.IndexOf(
    'VerifiedResourceFlowPlan(planning::RawResourceFlowPlan')
if ($privateIndex -lt 0 -or $authorityConstructorIndex -lt $privateIndex) {
    throw 'VerifiedResourceFlowPlan authority constructor is not private.'
}

$f6Header = Get-Content -Raw -LiteralPath (Join-Path $root '19A_Schema18CompositionLinker/Schema18CompositionLinker.h') -Encoding UTF8
$f6Source = Get-Content -Raw -LiteralPath (Join-Path $root '19A_Schema18CompositionLinker/Schema18CompositionLinker.cpp') -Encoding UTF8
foreach ($required in @('FreezeVerifiedComposition', 'VerifiedResourceFlowPlan',
    'ReadFrozenComposition', 'FrozenCompositionArtifact')) {
    if (-not $f6Header.Contains($required)) { throw "F6 Linker boundary is missing: $required" }
}
if ($f6Header -match 'FreezeVerifiedComposition[\s\S]*RawResourceFlowPlan') {
    throw 'F6 exposes a raw-plan freezing path.'
}
if ($f6Source.Contains('ProposeResourceFlowPlan')) {
    throw 'F6 Runtime artifact reader or Linker calls the Planner.'
}

# Freeze all accepted F1-F3 authority surfaces.
Assert-Sha256 'SemanticGpuEngine4.sln' '2bf10c62de202d0a7d195f9784210145261514a16b0881bf20a8a7ea95e725df'
Assert-Sha256 'SemanticGpuEngine4_Level4v1_F3.sln' 'dbf312d5f7091bafeea3a1756dddf870159a7755adb7653785289aa7a08426b0'
Assert-Sha256 '16A_Schema18CompositionContract/16A_Schema18CompositionContract.vcxproj' '9fb1a2010118f77e6f2b9f773d874b37b4e9c8e8a724f0be523559deeb6ed611'
Assert-Sha256 '16A_Schema18CompositionContract/Schema18CompositionContract.cpp' '0c44eb8c7f7ec78956e980db89b657babc681e04748aa7be4ae7d935bb333579'
Assert-Sha256 '16A_Schema18CompositionContract/Schema18CompositionContract.h' '56093b4bc4e5a79e591b5c2b6dabea3f5c365d1e6dfe8ca428e376d1d25fef1d'
Assert-Sha256 '46C_Schema18CompositionContractTests/46C_Schema18CompositionContractTests.vcxproj' 'b861d41fe73abaa3409b32477fdaaaceb0d889ce08579431b1d9c0a3c8b75e93'
Assert-Sha256 '46C_Schema18CompositionContractTests/main.cpp' 'd07fe16560c5deb50b21236dfa1b18dd529435fa200eaaaaf108c93f60a1b36f'

foreach ($solutionName in @('SemanticGpuEngine4.sln',
    'SemanticGpuEngine4_Level4v1_F1.sln',
    'SemanticGpuEngine4_Level4v1_F2.sln',
    'SemanticGpuEngine4_Level4v1_F3.sln')) {
    $text = Get-Content -Raw -LiteralPath (Join-Path $root $solutionName) -Encoding UTF8
    if ($text -match '17A_Schema18ResourceFlowPlan|18A_Schema18ResourceFlowVerifier|19A_Schema18CompositionLinker|46D_|46E_|46F_') {
        throw "F4-F6 projects leaked into frozen solution: $solutionName"
    }
}

$solutionPath = Join-Path $root 'SemanticGpuEngine4_Level4v1_F4_F6.sln'
$solution = Get-Content -Raw -LiteralPath $solutionPath -Encoding UTF8
if ($solution -match 'Win32|16_CompositionContract|17_LinkPlanModel|18_LinkPlanVerifier|19_PackageLinker|29_CompositionRuntime') {
    throw 'F4-F6 solution contains an invalid platform or Prototype-A project.'
}
foreach ($project in @('16A_Schema18CompositionContract',
    '17A_Schema18ResourceFlowPlan', '18A_Schema18ResourceFlowVerifier',
    '19A_Schema18CompositionLinker', '46C_Schema18CompositionContractTests',
    '46D_Schema18ResourceFlowPlanningTests',
    '46E_Schema18ResourceFlowAuthorityTests',
    '46F_Schema18FrozenCompositionTests')) {
    if ($solution -notmatch ('"' + [regex]::Escape($project) + '"')) {
        throw "F4-F6 solution is missing project: $project"
    }
}
$projectNames = @([regex]::Matches($solution, '(?m)^Project\([^\x0A]+\) = "(?<name>[^"]+)"') |
    ForEach-Object { $_.Groups['name'].Value })
if ($projectNames.Count -ne 35) { throw "F4-F6 solution project count changed: $($projectNames.Count)" }
$projectSet = @{}
foreach ($name in $projectNames) { $projectSet[$name] = $true }
foreach ($name in $projectNames) {
    foreach ($reference in Get-DirectReferenceNames $name) {
        if (-not $projectSet.ContainsKey($reference)) {
            throw "F4-F6 solution omits transitive dependency $reference referenced by $name"
        }
    }
}

Write-Host 'L4V1-F4-F6 Plan, Verifier, Frozen Composition, dependency, and authority boundaries passed.'
