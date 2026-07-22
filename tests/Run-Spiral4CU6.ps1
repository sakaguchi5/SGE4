param(
    [ValidateSet('SelfTest','Measurement','Report','CU6')]
    [string]$Mode = 'CU6',
    [uint32]$WarmupPerOrder = 1,
    [uint32]$MeasurementPerOrder = 2,
    [uint32]$Runs = 2,
    [uint32]$AdapterIndex = 0,
    [string]$ClockPolicy = 'uncontrolled'
)

$ErrorActionPreference = 'Stop'

$testsRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$root = Split-Path -Parent $testsRoot
. (Join-Path $testsRoot 'Spiral4Common.ps1')

Invoke-Spiral4Checks @(
    'tools\Verify-Spiral4CU2.ps1',
    'tools\Verify-Spiral4CU3.ps1',
    'tools\Verify-Spiral4CU4.ps1',
    'tools\Verify-Spiral4CU5.ps1',
    'tools\Verify-Spiral4CU6.ps1'
)

Build-Spiral4Project '144_Spiral4PerformanceTests\144_Spiral4PerformanceTests.vcxproj'

$output = Join-Path $root 'build\tests\spiral4-cu6\measurement'
$selfTestRoot = Join-Path $root 'build\tests\spiral4-cu6\self-test'
$debugSelfTest = Join-Path $selfTestRoot 'debug'
$releaseSelfTest = Join-Path $selfTestRoot 'release'
$debugExe = Join-Path $root 'build\bin\x64\Debug\144_Spiral4PerformanceTests.exe'
$releaseExe = Join-Path $root 'build\bin\x64\Release\144_Spiral4PerformanceTests.exe'

if ($Mode -in @('SelfTest','CU6')) {
    Remove-Item -Recurse -Force -ErrorAction SilentlyContinue $selfTestRoot
    New-Item -ItemType Directory -Force $debugSelfTest | Out-Null
    New-Item -ItemType Directory -Force $releaseSelfTest | Out-Null

    Invoke-Checked $debugExe @('--self-test','--output',$debugSelfTest)
    Invoke-Checked $releaseExe @('--self-test','--output',$releaseSelfTest)

    $debugEvidence = Join-Path $debugSelfTest 'spiral4_measurement_evidence_v1.bin'
    $releaseEvidence = Join-Path $releaseSelfTest 'spiral4_measurement_evidence_v1.bin'
    if (-not (Test-BytesEqual $debugEvidence $releaseEvidence)) {
        throw 'CU6 synthetic evidence differs across Debug and Release.'
    }
}

if ($Mode -in @('Measurement','CU6')) {
    Remove-Item -Recurse -Force -ErrorAction SilentlyContinue $output
    New-Item -ItemType Directory -Force $output | Out-Null
    Invoke-Checked $releaseExe @(
        '--measure',
        '--output',$output,
        '--warmup-per-order',"$WarmupPerOrder",
        '--measurement-per-order',"$MeasurementPerOrder",
        '--runs',"$Runs",
        '--adapter-index',"$AdapterIndex",
        '--clock-policy',$ClockPolicy
    )
}

$evidence = Join-Path $output 'spiral4_measurement_evidence_v1.bin'
if ($Mode -in @('Report','CU6')) {
    if (-not (Test-Path -LiteralPath $evidence -PathType Leaf)) {
        throw 'CU6 report requires spiral4_measurement_evidence_v1.bin. Run Measurement first.'
    }
    Invoke-Checked $releaseExe @('--report','--input',$evidence,'--output',$output)

    $reportPath = Join-Path $output 'SPIRAL4_DECISION_EVIDENCE_REPORT.md'
    if (-not (Test-Path -LiteralPath $reportPath -PathType Leaf)) {
        throw 'CU6 Decision Evidence Report was not generated.'
    }
    $reportText = Get-Content -Raw -LiteralPath $reportPath -Encoding UTF8
    foreach ($token in @(
        'RuntimePolicyAuthorization = None',
        'NextCapabilitySelection = DeferredByOwner',
        'SelectionStatus = OWNER_DECISION_REQUIRED',
        'Performance superiority and crossover are not completion gates'
    )) {
        if (-not $reportText.Contains($token)) {
            throw "CU6 Decision Report token missing: $token"
        }
    }
}

$evidenceDigest = if (Test-Path -LiteralPath $evidence) {
    Get-SGE4FileSha256 $evidence
} else {
    'NOT-RUN'
}
$reportFile = Join-Path $output 'SPIRAL4_DECISION_EVIDENCE_REPORT.md'
$reportDigest = if (Test-Path -LiteralPath $reportFile) {
    Get-SGE4FileSha256 $reportFile
} else {
    'NOT-RUN'
}
$summaryFile = Join-Path $output 'spiral4_measurement_summary_v1.json'
$summaryDigest = if (Test-Path -LiteralPath $summaryFile) {
    Get-SGE4FileSha256 $summaryFile
} else {
    'NOT-RUN'
}

Write-Host '============================================================'
if ($Mode -eq 'SelfTest') {
    Write-Host 'SGE4-5 SPIRAL 4 CU6 SELF-TEST PASSED'
} elseif ($Mode -eq 'Measurement') {
    Write-Host 'SGE4-5 SPIRAL 4 CU6 REAL GPU MEASUREMENT PASSED'
} elseif ($Mode -eq 'Report') {
    Write-Host 'SGE4-5 SPIRAL 4 CU6 DECISION EVIDENCE REPORT PASSED'
} else {
    Write-Host 'SGE4-5 SPIRAL 4 EXPERIMENT COMPLETE'
    Write-Host 'Completion Unit 6: PASSED'
    Write-Host 'Architecture Complete: retained from CU5'
    Write-Host 'Candidates: A.FixedMaximum, B.SingleIndirect, C.Batched B64/B128/B256/B512/B1024'
    Write-Host 'Active Count corpus: 19'
    Write-Host 'Order balance: 14 cyclic/reverse orders; every Candidate appears twice per position'
    Write-Host "Samples per Candidate and Nf: $(14 * $MeasurementPerOrder * $Runs)"
    Write-Host 'Primary metric: GPU Argument + state transition + execution + output synchronization'
    Write-Host 'Decision Report: Fixed/Single, Single/BestBatch, best Batch size, per-Nf winner set'
    Write-Host 'RuntimePolicyAuthorization: None'
    Write-Host 'NextCapabilitySelection: DeferredByOwner'
    Write-Host 'SelectionStatus: OWNER_DECISION_REQUIRED'
    Write-Host 'Spiral 4 Closed banner remains withheld until the Owner decision'
}
Write-Host "Measurement evidence SHA-256: $evidenceDigest"
Write-Host "Decision report SHA-256: $reportDigest"
Write-Host "Measurement summary SHA-256: $summaryDigest"
Write-Host '============================================================'
