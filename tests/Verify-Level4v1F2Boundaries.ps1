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
        throw "$ProjectName direct references differ from its frozen F2 boundary.`nExpected: $($expectedSorted -join ', ')`nActual: $($actual -join ', ')"
    }
}

function Assert-SourceDoesNotContain([string]$Directory, [string[]]$Patterns) {
    foreach ($file in Get-ChildItem -Path (Join-Path $root $Directory) -File -Recurse |
        Where-Object { $_.Extension -in '.h', '.cpp' }) {
        $text = Get-Content -Raw -LiteralPath $file.FullName -Encoding UTF8
        foreach ($pattern in $Patterns) {
            if ($text -match [regex]::Escape($pattern)) {
                throw "$Directory leaked forbidden dependency '$pattern' through $($file.Name)"
            }
        }
    }
}

function Assert-Sha256([string]$RelativePath, [string]$Expected) {
    $path = Join-Path $root $RelativePath
    if (-not (Test-Path -LiteralPath $path)) { throw "Frozen file is missing: $RelativePath" }
    $actual = (Get-FileHash -Algorithm SHA256 -LiteralPath $path).Hash.ToLowerInvariant()
    if ($actual -ne $Expected) {
        throw "F2 changed a frozen pre-F2 file: $RelativePath`nExpected: $Expected`nActual:   $actual"
    }
}

Assert-ExactReferences '27A_Schema18LeafCorpus' @(
    '00_Foundation', '02_SemanticModel', '04_SemanticAnalysis', '05_TargetContract',
    '24_SliceScenarios', '25_GeneralGraphScenarios', '26_GeneratedGraphCorpus',
    '27_RuntimeScenarios')
Assert-ExactReferences '46B_Schema18LeafCorpusTests' @(
    '00_Foundation', '02_SemanticModel', '03_SemanticBuilder', '04_SemanticAnalysis',
    '05_TargetContract', '05A_CompilationInput', '06_ExecutionPlanModel',
    '07_ExecutionPlanVerifier', '08_CandidatePlanner', '09_FrozenPackageCore',
    '10_D3D12PackageSchema', '10A_D3D12CompositionSchema18',
    '11_D3D12PackageLowering', '12_SGE4Compiler', '12A_CompositionLeafCompiler',
    '20_ExperimentDomain', '21_ClassicalFrontend', '22_SdfFrontend', '23_PgaFrontend',
    '24_SliceScenarios', '25_GeneralGraphScenarios', '26_GeneratedGraphCorpus',
    '27_RuntimeScenarios', '27A_Schema18LeafCorpus')

Assert-SourceDoesNotContain '27A_Schema18LeafCorpus' @(
    '10_D3D12PackageSchema', '10A_D3D12CompositionSchema18', '11_D3D12PackageLowering',
    '12_SGE4Compiler', '12A_CompositionLeafCompiler', '13_PackageRuntime',
    '14_D3D12Backend', '16_CompositionContract', '17_LinkPlanModel',
    '18_LinkPlanVerifier', '19_PackageLinker', '29_CompositionRuntime')

$header = Get-Content -Raw -LiteralPath (Join-Path $root '27A_Schema18LeafCorpus/Schema18LeafCorpus.h') -Encoding UTF8
foreach ($required in @(
    'FrozenCaseCount = 54',
    'FrozenNormalizedCaseCount = 3',
    'FrozenEndpointCount = 12',
    'FrozenReadOnlyEndpointCount = 9',
    'FrozenWriteOnlyEndpointCount = 3',
    '43322a7c33d2ae82bfa577c0fee79ad7b915bdd8d38655eac1631a4e81c89d9b',
    '6ecafb273230ccf3e0ea0afe020fea440e5e26f9d4c7d330e1ef7922c5268d34',
    '1455e539474d5433d4e827013279b3ca7e8f7a4561a8c56fb71b1d451fff057a')) {
    if (-not $header.Contains($required)) { throw "F2 frozen corpus header is missing: $required" }
}

# F2 must not silently rewrite the accepted Schema-17 corpus or the main solution.
Assert-Sha256 '44_CanonicalFreezeTests/main.cpp' '08a836494cdc9e270fb97d9b6968e305b3ebe526605ada560cfea100754bab24'
Assert-Sha256 'SemanticGpuEngine4.sln' '2bf10c62de202d0a7d195f9784210145261514a16b0881bf20a8a7ea95e725df'

$mainSolution = Get-Content -Raw -LiteralPath (Join-Path $root 'SemanticGpuEngine4.sln') -Encoding UTF8
if ($mainSolution -match '27A_Schema18LeafCorpus|46B_Schema18LeafCorpusTests') {
    throw 'F2 projects leaked into the frozen main solution before formal integration.'
}

$f2Solution = Get-Content -Raw -LiteralPath (Join-Path $root 'SemanticGpuEngine4_Level4v1_F2.sln') -Encoding UTF8
$expectedF2Projects = @(
    '01_MathVocabulary',
    '10A_D3D12CompositionSchema18',
    '12A_CompositionLeafCompiler',
    '27A_Schema18LeafCorpus',
    '46A_Schema18CompositionInterfaceTests',
    '46B_Schema18LeafCorpusTests')
foreach ($project in $expectedF2Projects) {
    if ($f2Solution -notmatch ('"' + [regex]::Escape($project) + '"')) {
        throw "F2 solution is missing project: $project"
    }
}
$projectDeclarationCount = ([regex]::Matches($f2Solution, '(?m)^Project\(')).Count
if ($projectDeclarationCount -ne $expectedF2Projects.Count) {
    throw "F2 solution project count changed: $projectDeclarationCount"
}

Write-Host 'L4V1-F2 dependency, corpus, and freeze boundaries passed.'
