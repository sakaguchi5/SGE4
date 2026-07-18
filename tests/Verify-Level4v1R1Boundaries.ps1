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
        throw "$ProjectName direct references differ from the R1 boundary.`nExpected: $($expectedSorted -join ', ')`nActual: $($actual -join ', ')"
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

Assert-ExactReferences '16_FrozenCompositionArtifact' @(
    '00_Foundation', '09_FrozenPackageCore')
Assert-ExactReferences '46_CanonicalCompositionArtifactTests' @(
    '00_Foundation', '09_FrozenPackageCore', '16_FrozenCompositionArtifact')

Assert-SourceDoesNotContain '16_FrozenCompositionArtifact' @(
    'SemanticModel', 'SemanticAnalysis', 'ExecutionPlan', 'CandidatePlanner',
    'SGE4Compiler', 'PackageRuntime', 'D3D12Backend', 'CompositionRuntime',
    '<Windows.h>', '<d3d12.h>', '<dxgi', 'TextureShape', 'ConditionalRegion',
    'ResidencyPolicy', 'MultiDevice')

$formatPath = Join-Path $root '16_FrozenCompositionArtifact/FrozenCompositionFormat.h'
$writerPath = Join-Path $root '16_FrozenCompositionArtifact/FrozenCompositionWriter.cpp'
$readerPath = Join-Path $root '16_FrozenCompositionArtifact/FrozenCompositionReader.cpp'
$publicHeaders = @(
    Get-Content -Raw -LiteralPath (Join-Path $root '16_FrozenCompositionArtifact/FrozenComposition.h') -Encoding UTF8
    Get-Content -Raw -LiteralPath (Join-Path $root '16_FrozenCompositionArtifact/FrozenCompositionReader.h') -Encoding UTF8
    Get-Content -Raw -LiteralPath (Join-Path $root '16_FrozenCompositionArtifact/FrozenCompositionWriter.h') -Encoding UTF8
) -join "`n"
$format = Get-Content -Raw -LiteralPath $formatPath -Encoding UTF8
$writer = Get-Content -Raw -LiteralPath $writerPath -Encoding UTF8
$reader = Get-Content -Raw -LiteralPath $readerPath -Encoding UTF8
$tests = Get-Content -Raw -LiteralPath (Join-Path $root '46_CanonicalCompositionArtifactTests/main.cpp') -Encoding UTF8

foreach ($required in @(
    'FrozenCompositionMagic', 'FrozenCompositionHeaderBytes = 192',
    'FrozenCompositionSectionDescriptorBytes = 80', 'EmbeddedLeafRecordBytes = 128',
    'EmbeddedSchemaVersion = 17', 'EmbeddedRuntimeVersion = 17',
    'Manifest = 1', 'LeafTable = 2', 'LeafBytes = 3', 'ContractData = 4',
    'VerifiedDecisionData = 5', 'VerificationCertificate = 6')) {
    if ($format -notmatch [regex]::Escape($required)) {
        throw "R1 format does not freeze required item: $required"
    }
}

foreach ($required in @(
    'PackageReader::Read', 'targetSchemaVersion != EmbeddedSchemaVersion',
    'minimumRuntimeVersion != EmbeddedRuntimeVersion', 'ComputeSemanticDigest',
    'ComputeFrozenCompositionFileDigest')) {
    if (($writer + "`n" + $reader) -notmatch [regex]::Escape($required)) {
        throw "R1 implementation is missing required validation: $required"
    }
}

foreach ($forbidden in @('Execute(', 'Submit(', 'LoadPackage(', 'CreateDevice', 'ID3D12')) {
    if ($publicHeaders -match [regex]::Escape($forbidden)) {
        throw "R1 public artifact boundary leaked runtime authority: $forbidden"
    }
}

foreach ($negativeGate in @(
    'one-Leaf composition was accepted', 'duplicate stable key was accepted',
    'Schema 18 Leaf was accepted', 'bad magic was accepted',
    'non-canonical section order was accepted', 'out-of-range presenter was accepted',
    'non-canonical Leaf offset was accepted', 'corrupt embedded Leaf was accepted',
    'stale semantic digest was accepted', 'non-zero canonical padding was accepted')) {
    if ($tests -notmatch [regex]::Escape($negativeGate)) {
        throw "R1 test suite is missing negative gate: $negativeGate"
    }
}

$solution = Get-Content -Raw -LiteralPath (Join-Path $root 'SemanticGpuEngine4.sln') -Encoding UTF8
foreach ($project in @('00_Foundation', '09_FrozenPackageCore', '16_FrozenCompositionArtifact', '46_CanonicalCompositionArtifactTests')) {
    if ($solution -notmatch [regex]::Escape($project)) {
        throw "R1 solution is missing project: $project"
    }
}

Write-Host 'Level 4 v1 Canonical R1 dependency and artifact boundaries passed.'
