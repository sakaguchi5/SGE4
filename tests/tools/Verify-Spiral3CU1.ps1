$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
foreach ($name in @('100_ReuseSweepSemantic','101_Spiral3Contracts','102_Spiral3Corpus','110_Spiral3SemanticTests')) {
    if (-not (Test-Path (Join-Path $root "$name\$name.vcxproj"))) { throw "Missing CU1 project: $name" }
}
$corpus = Get-Content -Raw (Join-Path $root '102_Spiral3Corpus\Spiral3Corpus.cpp')
$contract = Get-Content -Raw (Join-Path $root '101_Spiral3Contracts\Spiral3Contracts.h')
$tests = Get-Content -Raw (Join-Path $root '110_Spiral3SemanticTests\main.cpp')
if ($contract -notmatch 'CanonicalReuseCountsV1\{1,\s*4,\s*16,\s*64,\s*256,\s*512\}') { throw 'Canonical reuse counts missing' }
foreach ($typeName in @('BoneCountV1','ReuseCountV1','PointCountV1','FixedReuseWorkloadShapeV1')) {
    if ($contract -notmatch $typeName) { throw "Strong workload type missing: $typeName" }
}
if ($corpus -notmatch 'BuildIndependentCpuReferenceV1') { throw 'Independent reference missing' }
if ($tests -notmatch 'is_convertible_v<contracts::BoneCountV1, contracts::ReuseCountV1>' -or
    $tests -notmatch 'BuildFixedReuseWorkloadShapeV1') { throw 'Compile-time workload type-boundary tests missing' }
Write-Host 'Spiral 3 CU1 semantic/corpus and strong workload-shape boundaries passed.'
