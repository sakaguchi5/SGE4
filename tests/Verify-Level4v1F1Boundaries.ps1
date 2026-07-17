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
        throw "$ProjectName direct references differ from its frozen F1 boundary.`nExpected: $($expectedSorted -join ', ')`nActual: $($actual -join ', ')"
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

Assert-ExactReferences '10A_D3D12CompositionSchema18' @(
    '00_Foundation', '09_FrozenPackageCore', '10_D3D12PackageSchema')
Assert-ExactReferences '12A_CompositionLeafCompiler' @(
    '00_Foundation', '02_SemanticModel', '04_SemanticAnalysis', '05_TargetContract',
    '09_FrozenPackageCore', '10_D3D12PackageSchema', '10A_D3D12CompositionSchema18',
    '12_SGE4Compiler')

Assert-SourceDoesNotContain '10A_D3D12CompositionSchema18' @(
    'SemanticModel', 'SemanticAnalysis', 'CandidatePlanner', 'SGE4Compiler',
    'PackageRuntime', 'D3D12Backend', 'CompositionContract', 'LinkPlan',
    'PackageLinker', 'CompositionRuntime')
Assert-SourceDoesNotContain '12A_CompositionLeafCompiler' @(
    '13_PackageRuntime', '14_D3D12Backend', '16_CompositionContract',
    '17_LinkPlanModel', '18_LinkPlanVerifier', '19_PackageLinker', '29_CompositionRuntime')

$format = Get-Content -Raw -LiteralPath (Join-Path $root '09_FrozenPackageCore/PackageFormat.h') -Encoding UTF8
$reader = Get-Content -Raw -LiteralPath (Join-Path $root '09_FrozenPackageCore/PackageReader.cpp') -Encoding UTF8
if ($format -notmatch "D3D12CompositionEndpointTable\s*=\s*0x0000'1012") {
    throw 'Core Package Format does not freeze the Schema 18 Composition Endpoint section kind.'
}
if ($reader -notmatch 'case\s+SectionKind::D3D12CompositionEndpointTable') {
    throw 'Core PackageReader does not recognize the required Schema 18 Composition Endpoint section.'
}

$ordinaryCompiler = Get-Content -Raw -LiteralPath (Join-Path $root '12_SGE4Compiler/SGE4Compiler.cpp') -Encoding UTF8
$runtime = Get-Content -Raw -LiteralPath (Join-Path $root '13_PackageRuntime/PackageRuntime.cpp') -Encoding UTF8
if ($ordinaryCompiler -match 'd3d12_v18|CompileCompositionLeafCanonical') {
    throw 'Ordinary Schema 17 compiler path was modified to own Schema 18 authority.'
}
if ($runtime -match 'd3d12_v18|D3D12CompositionEndpointTable') {
    throw 'Runtime 17 must not accept or execute Schema 18 Leaves during F1.'
}

Write-Host 'L4V1-F1 dependency and authority boundaries passed.'
