param([switch]$RequireClosure)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path

foreach ($relative in @(
    '89_Spiral2PerformanceExperiment/89_Spiral2PerformanceExperiment.vcxproj',
    '89_Spiral2PerformanceExperiment/Spiral2PerformanceExperiment.h',
    '89_Spiral2PerformanceExperiment/Spiral2PerformanceExperiment.cpp',
    '99_Spiral2Launcher/99_Spiral2Launcher.vcxproj',
    '99_Spiral2Launcher/main.cpp',
    'docs/spiral2/STAGE19_ARCHITECTURE_FINAL_FREEZE.md',
    'docs/spiral2/STAGE20_REAL_GPU_MEASUREMENT.md')) {
    if (-not (Test-Path -LiteralPath (Join-Path $root $relative))) { throw "Missing CU6 file: $relative" }
}

$backendHeader = Get-Content -Raw -LiteralPath (Join-Path $root '14_D3D12Backend/D3D12Backend.h') -Encoding UTF8
$backendSource = Get-Content -Raw -LiteralPath (Join-Path $root '14_D3D12Backend/D3D12Backend.cpp') -Encoding UTF8
$experiment = Get-Content -Raw -LiteralPath (Join-Path $root '89_Spiral2PerformanceExperiment/Spiral2PerformanceExperiment.cpp') -Encoding UTF8
$launcher = Get-Content -Raw -LiteralPath (Join-Path $root '99_Spiral2Launcher/main.cpp') -Encoding UTF8
$architecture = Get-Content -Raw -LiteralPath (Join-Path $root 'docs/spiral2/STAGE19_ARCHITECTURE_FINAL_FREEZE.md') -Encoding UTF8
$measurement = Get-Content -Raw -LiteralPath (Join-Path $root 'docs/spiral2/STAGE20_REAL_GPU_MEASUREMENT.md') -Encoding UTF8

foreach ($token in @('enableTimestampProfiling','ConsumeTimestampProfileSamples','EndQuery','ResolveQueryData','timestampFrequency')) {
    if (($backendHeader + $backendSource) -notmatch [regex]::Escape($token)) { throw "Missing passive timestamp evidence: $token" }
}
foreach ($token in @('M0','M1','M2','M3','M4','M5','ABC','ACB','BAC','BCA','CAB','CBA','DXGI_ADAPTER_FLAG_SOFTWARE','RecommendationAuthority = NonAuthoritative')) {
    if ($experiment -notmatch [regex]::Escape($token)) { throw "Missing fair measurement/report evidence: $token" }
}
foreach ($token in @('--self-test','--warp','--measure','--evidence','--report','DeferredByOwner','OWNER_DECISION_REQUIRED')) {
    if ($launcher -notmatch [regex]::Escape($token)) { throw "Missing launcher closure command: $token" }
}
foreach ($token in @('S2-I01-I04','S2-I05-I06','S2-I07-I09','S2-I10-I12','S2-I13-I14','S2-I15-I16','S2-I17-I18','S2-I19-I20','S2-I21-I22','HIERARCHICAL REPRESENTATION AUTHORITY PASSED')) {
    if ($architecture -notmatch [regex]::Escape($token)) { throw "Missing Architecture final-freeze evidence: $token" }
}
foreach ($token in @('HardwareEvidence = Qualified','MeasurementSamples = 576','ABC/ACB/BAC/BCA/CAB/CBA','RecommendationAuthority = NonAuthoritative')) {
    if ($measurement -notmatch [regex]::Escape($token)) { throw "Missing real-GPU measurement evidence: $token" }
}

if ($RequireClosure) {
    foreach ($relative in @('docs/spiral2/STAGE21_DECISION_EVIDENCE.md','docs/spiral2/STAGE22A_EVIDENCE_CLOSURE.md','build/tests/spiral2/measurement/SPIRAL2_DECISION_EVIDENCE_REPORT.md','build/tests/spiral2/measurement/spiral2_measurement_evidence_v1.bin')) {
        if (-not (Test-Path -LiteralPath (Join-Path $root $relative))) { throw "Missing CU6 closure evidence: $relative" }
    }
    $closure = (Get-Content -Raw -LiteralPath (Join-Path $root 'docs/spiral2/STAGE22A_EVIDENCE_CLOSURE.md') -Encoding UTF8) +
        (Get-Content -Raw -LiteralPath (Join-Path $root 'build/tests/spiral2/measurement/SPIRAL2_DECISION_EVIDENCE_REPORT.md') -Encoding UTF8)
    foreach ($token in @('NextCapabilitySelection = DeferredByOwner','SelectionStatus = OWNER_DECISION_REQUIRED','RecommendationAuthority = NonAuthoritative')) {
        if ($closure -notmatch [regex]::Escape($token)) { throw "Missing Owner decision boundary: $token" }
    }
}

Write-Host 'Spiral 2 CU6 source and decision-boundary verification passed.'
