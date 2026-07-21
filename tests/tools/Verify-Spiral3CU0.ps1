$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$solution = Get-Content -Raw (Join-Path $root 'SemanticGpuEngine4-5.sln')
$required = @('100_ReuseSweepSemantic','101_Spiral3Contracts','102_Spiral3Corpus','110_Spiral3SemanticTests')
foreach ($name in $required) {
    if ($solution -notmatch [regex]::Escape($name)) { throw "Spiral 3 CU0 solution member missing: $name" }
}
if ($solution -match '103_ReuseRepresentationCandidate|111_Spiral3CandidateGraphTests|119_Spiral3Launcher') {
    throw 'Projects beyond CU1 are present in the CU0/CU1 delivery.'
}
foreach ($path in @('100_ReuseSweepSemantic','101_Spiral3Contracts','102_Spiral3Corpus')) {
    $project = Get-Content -Raw (Join-Path $root "$path\$path.vcxproj")
    if ($project -match '13_PackageRuntime|14_D3D12Backend|22_CompositionRuntime') {
        throw "Semantic-side project illegally references Runtime/Backend: $path"
    }
}
foreach ($runtimePath in @('13_PackageRuntime\13_PackageRuntime.vcxproj','14_D3D12Backend\14_D3D12Backend.vcxproj','22_CompositionRuntime\22_CompositionRuntime.vcxproj')) {
    $project = Get-Content -Raw (Join-Path $root $runtimePath)
    if ($project -match '100_ReuseSweepSemantic|101_Spiral3Contracts|102_Spiral3Corpus') {
        throw "Runtime/Backend illegally references Spiral 3 CU1: $runtimePath"
    }
}
Write-Host 'Spiral 3 CU0 project and dependency boundaries passed.'
