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
        throw "$ProjectName direct references differ from its frozen F3 boundary.`nExpected: $($expectedSorted -join ', ')`nActual: $($actual -join ', ')"
    }
}

function Assert-Sha256([string]$RelativePath, [string]$Expected) {
    $path = Join-Path $root $RelativePath
    if (-not (Test-Path -LiteralPath $path)) { throw "Frozen file is missing: $RelativePath" }
    $actual = (Get-FileHash -Algorithm SHA256 -LiteralPath $path).Hash.ToLowerInvariant()
    if ($actual -ne $Expected) {
        throw "F3 changed a frozen pre-F3 file: $RelativePath`nExpected: $Expected`nActual:   $actual"
    }
}

Assert-ExactReferences '16A_Schema18CompositionContract' @(
    '00_Foundation', '09_FrozenPackageCore', '10_D3D12PackageSchema',
    '10A_D3D12CompositionSchema18')
Assert-ExactReferences '46C_Schema18CompositionContractTests' @(
    '00_Foundation', '02_SemanticModel', '03_SemanticBuilder', '04_SemanticAnalysis',
    '05_TargetContract', '05A_CompilationInput', '06_ExecutionPlanModel',
    '07_ExecutionPlanVerifier', '08_CandidatePlanner', '09_FrozenPackageCore',
    '10_D3D12PackageSchema', '10A_D3D12CompositionSchema18',
    '11_D3D12PackageLowering', '12_SGE4Compiler', '12A_CompositionLeafCompiler',
    '16A_Schema18CompositionContract',
    '20_ExperimentDomain', '21_ClassicalFrontend', '22_SdfFrontend', '23_PgaFrontend',
    '24_SliceScenarios', '25_GeneralGraphScenarios', '26_GeneratedGraphCorpus',
    '27_RuntimeScenarios', '27A_Schema18LeafCorpus')

$headerPath = Join-Path $root '16A_Schema18CompositionContract/Schema18CompositionContract.h'
$sourcePath = Join-Path $root '16A_Schema18CompositionContract/Schema18CompositionContract.cpp'
$header = Get-Content -Raw -LiteralPath $headerPath -Encoding UTF8
$source = Get-Content -Raw -LiteralPath $sourcePath -Encoding UTF8
foreach ($required in @(
    'ResourceBoundary', 'CompositionInput', 'CompositionOutput',
    'BuildSchema18CompositionContract', 'ValidateCompositionContract',
    'SerializeCompositionContract', 'ComputeContractIdentity',
    'StableKey ComputeStableLeafKey', 'StableKey ComputeStableResourceKey')) {
    if (-not $header.Contains($required)) { throw "F3 contract header is missing: $required" }
}
foreach ($forbidden in @('EndpointDirection', 'ImportExport', '16_CompositionContract',
    '17_LinkPlanModel', '18_LinkPlanVerifier', '19_PackageLinker', '29_CompositionRuntime')) {
    if ($header.Contains($forbidden) -or $source.Contains($forbidden)) {
        throw "F3 leaked Prototype-A authority or dependency: $forbidden"
    }
}

$flowDeclaration = [regex]::Match($header,
    'struct ResourceFlowDeclaration final\s*\{(?<body>.*?)\};',
    [Text.RegularExpressions.RegexOptions]::Singleline)
if (-not $flowDeclaration.Success) { throw 'F3 ResourceFlowDeclaration was not found.' }
foreach ($forbidden in @('sizeBytes', 'minimumBytes', 'format', 'kind', 'access',
    'requiredIncomingState', 'guaranteedOutgoingState', 'synchronization', 'direction')) {
    if ($flowDeclaration.Groups['body'].Value -match [regex]::Escape($forbidden)) {
        throw "Composition author can still forge derived Resource Flow field: $forbidden"
    }
}
foreach ($required in @('CompositionLeafView::Decode', 'EndpointAccess::WriteOnly',
    'EndpointAccess::ReadOnly', 'exactly one Resource Flow', 'at most one presenter Leaf')) {
    if (-not $source.Contains($required)) { throw "F3 implementation is missing authority gate: $required" }
}

# Freeze the main and F2 solutions. F3 remains isolated until its own gate passes.
Assert-Sha256 'SemanticGpuEngine4.sln' '2bf10c62de202d0a7d195f9784210145261514a16b0881bf20a8a7ea95e725df'
Assert-Sha256 '27A_Schema18LeafCorpus/Schema18LeafCorpus.h' '54a05d28dacb0f8715c3cce70d680123a963794cab5fb9d05417204606cf1225'
Assert-Sha256 '46B_Schema18LeafCorpusTests/main.cpp' '1fe3ef4af07d85a091028eddcb6130e2738fbeae2daa4bc6fec0a5ab3fb17d66'
Assert-Sha256 'SemanticGpuEngine4_Level4v1_F2.sln' 'b4dfa1876c8060c0f486f286b5595fe8e7a311d942758e16a89f3c42e3e840a9'

foreach ($solutionName in @('SemanticGpuEngine4.sln', 'SemanticGpuEngine4_Level4v1_F2.sln')) {
    $text = Get-Content -Raw -LiteralPath (Join-Path $root $solutionName) -Encoding UTF8
    if ($text -match '16A_Schema18CompositionContract|46C_Schema18CompositionContractTests') {
        throw "F3 projects leaked into frozen solution: $solutionName"
    }
}

$f3SolutionPath = Join-Path $root 'SemanticGpuEngine4_Level4v1_F3.sln'
$f3Solution = Get-Content -Raw -LiteralPath $f3SolutionPath -Encoding UTF8
if ($f3Solution -match 'Win32|16_CompositionContract|17_LinkPlanModel|18_LinkPlanVerifier|19_PackageLinker|29_CompositionRuntime') {
    throw 'F3 solution contains an invalid platform or Prototype-A project.'
}
foreach ($project in @('10A_D3D12CompositionSchema18', '12A_CompositionLeafCompiler',
    '16A_Schema18CompositionContract', '27A_Schema18LeafCorpus',
    '46A_Schema18CompositionInterfaceTests', '46B_Schema18LeafCorpusTests',
    '46C_Schema18CompositionContractTests')) {
    if ($f3Solution -notmatch ('"' + [regex]::Escape($project) + '"')) {
        throw "F3 solution is missing project: $project"
    }
}
$projectNames = @([regex]::Matches($f3Solution, '(?m)^Project\([^\r\n]+\) = "(?<name>[^"]+)"') |
    ForEach-Object { $_.Groups['name'].Value })
if ($projectNames.Count -ne 29) { throw "F3 solution project count changed: $($projectNames.Count)" }
$projectSet = @{}
foreach ($name in $projectNames) { $projectSet[$name] = $true }
foreach ($name in $projectNames) {
    foreach ($reference in Get-DirectReferenceNames $name) {
        if (-not $projectSet.ContainsKey($reference)) {
            throw "F3 solution omits transitive dependency $reference referenced by $name"
        }
    }
}

Write-Host 'L4V1-F3 endpoint authority, contract, dependency, and freeze boundaries passed.'
