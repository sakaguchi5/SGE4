$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)

$required = @(
    '80_HierarchySemantic\HierarchySemantic.h',
    '80_HierarchySemantic\HierarchySemantic.cpp',
    '81_Spiral2Contracts\Spiral2Contracts.h',
    '81_Spiral2Contracts\Spiral2Contracts.cpp',
    '82_Spiral2Corpus\Spiral2Corpus.h',
    '82_Spiral2Corpus\Spiral2Corpus.cpp',
    '90_Spiral2SemanticTests\main.cpp',
    '91_Spiral2DynamicInvocationTests\main.cpp'
)
foreach ($relative in $required) {
    if (-not (Test-Path -LiteralPath (Join-Path $root $relative))) { throw "Missing CU1 source: $relative" }
}
$solution = Get-Content -Raw -LiteralPath (Join-Path $root 'SemanticGpuEngine4-5.sln')
foreach ($name in @('80_HierarchySemantic','81_Spiral2Contracts','82_Spiral2Corpus','90_Spiral2SemanticTests','91_Spiral2DynamicInvocationTests')) {
    if (($solution.Split("`n") | Where-Object { $_ -match ('= "' + [regex]::Escape($name) + '"') }).Count -ne 1) {
        throw "CU1 project is not registered exactly once: $name"
    }
}
$hierarchyText = Get-Content -Raw -LiteralPath (Join-Path $root '80_HierarchySemantic\HierarchySemantic.h')
foreach ($forbidden in @('Matrix','Shader','Queue','Descriptor','ResourceState','Allocation','DynamicLocalMotorPalette')) {
    if ($hierarchyText -match $forbidden) { throw "Static hierarchy leaked forbidden representation/runtime concept: $forbidden" }
}
$contractText = Get-Content -Raw -LiteralPath (Join-Path $root '81_Spiral2Contracts\Spiral2Contracts.h')
foreach ($requiredText in @('DynamicLocalMotorPaletteV1','DynamicMotorStrideBytesV1 = 32','requiredBytes','hierarchySemanticIdentity')) {
    if ($contractText -notmatch [regex]::Escape($requiredText)) { throw "Dynamic contract is missing: $requiredText" }
}
Write-Host 'SGE4-5 Spiral 2 CU1 source and authority boundaries passed.'
