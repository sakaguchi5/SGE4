param(
    [ValidateSet('SelfTest','Measurement','Report','CU6')]
    [string]$Mode = 'CU6',
    [int]$WarmupPerOrder = 1,
    [int]$MeasurementPerOrder = 2,
    [int]$Runs = 2,
    [int]$AdapterIndex = 0,
    [string]$ClockPolicy = 'uncontrolled'
)
# Canonical defaults produce 3168 measured Candidate samples.
$ErrorActionPreference = 'Stop'
$testsRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$root = Split-Path -Parent $testsRoot
. (Join-Path $testsRoot 'Spiral6Common.ps1')

Invoke-Spiral6Checks @(
    'tools\Verify-Spiral6CU2.ps1',
    'tools\Verify-Spiral6CU3.ps1',
    'tools\Verify-Spiral6CU4.ps1',
    'tools\Verify-Spiral6CU5.ps1',
    'tools\Verify-Spiral6CU6.ps1')
Build-Spiral6Project '171_Spiral6PerformanceTests\171_Spiral6PerformanceTests.vcxproj'

$output = Join-Path $root 'build\tests\spiral6-cu6\measurement'
$selfTestRoot = Join-Path $root 'build\tests\spiral6-cu6\self-test'
$debugExe = Join-Path $root 'build\bin\x64\Debug\171_Spiral6PerformanceTests.exe'
$releaseExe = Join-Path $root 'build\bin\x64\Release\171_Spiral6PerformanceTests.exe'

if ($Mode -in @('SelfTest','CU6')) {
    Remove-Item -Recurse -Force -ErrorAction SilentlyContinue $selfTestRoot
    $debugOutput = Join-Path $selfTestRoot 'debug'
    $releaseOutput = Join-Path $selfTestRoot 'release'
    New-Item -ItemType Directory -Force $debugOutput,$releaseOutput | Out-Null
    Invoke-Checked $debugExe @('--self-test','--output',$debugOutput)
    Invoke-Checked $releaseExe @('--self-test','--output',$releaseOutput)
    foreach ($name in @(
        'spiral6_measurement_evidence_v1.bin',
        'spiral6_measurement_samples_v1.csv',
        'spiral6_measurement_summary_v1.json',
        'SPIRAL6_DECISION_EVIDENCE_REPORT.md')) {
        if (-not (Test-BytesEqual (Join-Path $debugOutput $name) (Join-Path $releaseOutput $name))) {
            throw "CU6 synthetic artifact differs across Debug and Release: $name"
        }
    }
}

if ($Mode -in @('Measurement','CU6')) {
    Remove-Item -Recurse -Force -ErrorAction SilentlyContinue $output
    New-Item -ItemType Directory -Force $output | Out-Null
    Invoke-Checked $releaseExe @(
        '--measure','--output',$output,
        '--warmup-per-order',"$WarmupPerOrder",
        '--measurement-per-order',"$MeasurementPerOrder",
        '--runs',"$Runs",
        '--adapter-index',"$AdapterIndex",
        '--clock-policy',$ClockPolicy)
}

$evidence = Join-Path $output 'spiral6_measurement_evidence_v1.bin'
if ($Mode -in @('Report','CU6')) {
    if (-not (Test-Path -LiteralPath $evidence -PathType Leaf)) {
        throw 'CU6 report requires spiral6_measurement_evidence_v1.bin. Run Measurement first.'
    }
    Invoke-Checked $releaseExe @('--report','--input',$evidence,'--output',$output)
    $reportPath = Join-Path $output 'SPIRAL6_DECISION_EVIDENCE_REPORT.md'
    if (-not (Test-Path -LiteralPath $reportPath -PathType Leaf)) {
        throw 'CU6 Decision Evidence Report was not generated.'
    }
    $reportText = Get-Content -Raw -LiteralPath $reportPath -Encoding UTF8
    foreach ($token in @(
        'RuntimePolicyAuthorization = None',
        'NextCapabilitySelection = DeferredByOwner',
        'SelectionStatus = OWNER_DECISION_REQUIRED',
        'Performance superiority and crossover are not completion gates',
        'No observed crossover is a valid result',
        'Same-cardinality winner depends on Pattern')) {
        if (-not $reportText.Contains($token)) { throw "CU6 Decision Report token missing: $token" }
    }
}

$evidenceDigest = if (Test-Path -LiteralPath $evidence) { Get-SGE4FileSha256 $evidence } else { 'NOT-RUN' }
$reportFile = Join-Path $output 'SPIRAL6_DECISION_EVIDENCE_REPORT.md'
$reportDigest = if (Test-Path -LiteralPath $reportFile) { Get-SGE4FileSha256 $reportFile } else { 'NOT-RUN' }
$summaryFile = Join-Path $output 'spiral6_measurement_summary_v1.json'
$summaryDigest = if (Test-Path -LiteralPath $summaryFile) { Get-SGE4FileSha256 $summaryFile } else { 'NOT-RUN' }

Write-Host '============================================================'
if ($Mode -eq 'SelfTest') {
    Write-Host 'SGE4-5 SPIRAL 6 CU6 SELF-TEST PASSED'
} elseif ($Mode -eq 'Measurement') {
    Write-Host 'SGE4-5 SPIRAL 6 CU6 REAL GPU MEASUREMENT PASSED'
} elseif ($Mode -eq 'Report') {
    Write-Host 'SGE4-5 SPIRAL 6 CU6 DECISION EVIDENCE REPORT PASSED'
} else {
    Write-Host 'SGE4-5 SPIRAL 6 EXPERIMENT COMPLETE'
    Write-Host 'Completion Unit 6: PASSED'
    Write-Host 'Architecture Complete: retained from CU5'
    Write-Host 'Candidates: A.DenseMembershipMaskPredicate / B.CompactIndexListDispatch / C.ActiveBlockListLocalMask'
    Write-Host 'Pattern corpus: PrefixControl / UniformStride / BlockClusteredPermutation / HashScatterPermutation'
    Write-Host 'K corpus: 1/8/32/64/128/256/512/1024/2048/3072/4096'
    Write-Host 'Order balance: all 6 Candidate permutations; every Candidate appears twice per position'
    Write-Host "Samples per Candidate and (Pattern,K): $(6 * $MeasurementPerOrder * $Runs)"
    Write-Host "Measured Candidate samples: $(4 * 11 * 3 * 6 * $MeasurementPerOrder * $Runs)"
    Write-Host 'Primary metric: GPU sentinel + argument generation + Sparse Consumer + completion path'
    Write-Host 'Construction/upload, observation copy and CPU costs: independently retained'
    Write-Host 'Decision Report: per-(Pattern,K) winner set, per-Pattern A/B and B/C crossover, same-K Pattern sensitivity'
    Write-Host 'RuntimePolicyAuthorization: None'
    Write-Host 'NextCapabilitySelection: DeferredByOwner'
    Write-Host 'SelectionStatus: OWNER_DECISION_REQUIRED'
    Write-Host 'Spiral 6 Closed banner remains withheld until the Owner decision'
}
Write-Host "Measurement evidence SHA-256: $evidenceDigest"
Write-Host "Decision report SHA-256: $reportDigest"
Write-Host "Measurement summary SHA-256: $summaryDigest"
Write-Host '============================================================'
