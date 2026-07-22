param(
    [ValidateSet('SelfTest','Measurement','Report','CU6')]
    [string]$Mode = 'CU6',
    [int]$WarmupPerOrder = 1,
    [int]$MeasurementPerOrder = 2,
    [int]$Runs = 2,
    [int]$AdapterIndex = 0,
    [string]$ClockPolicy = 'uncontrolled'
)
$ErrorActionPreference = 'Stop'
$testsRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$root = Split-Path -Parent $testsRoot
. (Join-Path $testsRoot 'Spiral5Common.ps1')

Invoke-Spiral5Checks @(
 'tools\Verify-Spiral5CU2.ps1',
 'tools\Verify-Spiral5CU3.ps1',
 'tools\Verify-Spiral5CU4.ps1',
 'tools\Verify-Spiral5CU5.ps1',
 'tools\Verify-Spiral5CU6.ps1'
)
Build-Spiral5Project '153_Spiral5PerformanceTests\153_Spiral5PerformanceTests.vcxproj'

$output = Join-Path $root 'build\tests\spiral5-cu6\measurement'
$selfTestRoot = Join-Path $root 'build\tests\spiral5-cu6\self-test'
$debugExe = Join-Path $root 'build\bin\x64\Debug\153_Spiral5PerformanceTests.exe'
$releaseExe = Join-Path $root 'build\bin\x64\Release\153_Spiral5PerformanceTests.exe'

if ($Mode -in @('SelfTest','CU6')) {
 Remove-Item -Recurse -Force -ErrorAction SilentlyContinue $selfTestRoot
 $debugOutput = Join-Path $selfTestRoot 'debug'
 $releaseOutput = Join-Path $selfTestRoot 'release'
 New-Item -ItemType Directory -Force $debugOutput,$releaseOutput | Out-Null
 Invoke-Checked $debugExe @('--self-test','--output',$debugOutput)
 Invoke-Checked $releaseExe @('--self-test','--output',$releaseOutput)
 foreach ($name in @('spiral5_measurement_evidence_v1.bin','spiral5_measurement_samples_v1.csv','spiral5_measurement_summary_v1.json','SPIRAL5_DECISION_EVIDENCE_REPORT.md')) {
  if (-not (Test-BytesEqual (Join-Path $debugOutput $name) (Join-Path $releaseOutput $name))) {
   throw "CU6 synthetic artifact differs across Debug and Release: $name"
  }
 }
}

if ($Mode -in @('Measurement','CU6')) {
 Remove-Item -Recurse -Force -ErrorAction SilentlyContinue $output
 New-Item -ItemType Directory -Force $output | Out-Null
 Invoke-Checked $releaseExe @('--measure','--output',$output,'--warmup-per-order',"$WarmupPerOrder",'--measurement-per-order',"$MeasurementPerOrder",'--runs',"$Runs",'--adapter-index',"$AdapterIndex",'--clock-policy',$ClockPolicy)
}

$evidence = Join-Path $output 'spiral5_measurement_evidence_v1.bin'
if ($Mode -in @('Report','CU6')) {
 if (-not (Test-Path -LiteralPath $evidence -PathType Leaf)) { throw 'CU6 report requires spiral5_measurement_evidence_v1.bin. Run Measurement first.' }
 Invoke-Checked $releaseExe @('--report','--input',$evidence,'--output',$output)
 $reportPath = Join-Path $output 'SPIRAL5_DECISION_EVIDENCE_REPORT.md'
 if (-not (Test-Path -LiteralPath $reportPath -PathType Leaf)) { throw 'CU6 Decision Evidence Report was not generated.' }
 $reportText = Get-Content -Raw -LiteralPath $reportPath -Encoding UTF8
 foreach ($token in @('RuntimePolicyAuthorization = None','NextCapabilitySelection = DeferredByOwner','SelectionStatus = OWNER_DECISION_REQUIRED','Performance superiority and crossover are not completion gates','No observed crossover is a valid result')) {
  if (-not $reportText.Contains($token)) { throw "CU6 Decision Report token missing: $token" }
 }
}

$evidenceDigest = if (Test-Path -LiteralPath $evidence) { Get-SGE4FileSha256 $evidence } else { 'NOT-RUN' }
$reportFile = Join-Path $output 'SPIRAL5_DECISION_EVIDENCE_REPORT.md'
$reportDigest = if (Test-Path -LiteralPath $reportFile) { Get-SGE4FileSha256 $reportFile } else { 'NOT-RUN' }
$summaryFile = Join-Path $output 'spiral5_measurement_summary_v1.json'
$summaryDigest = if (Test-Path -LiteralPath $summaryFile) { Get-SGE4FileSha256 $summaryFile } else { 'NOT-RUN' }

Write-Host '============================================================'
if ($Mode -eq 'SelfTest') {
 Write-Host 'SGE4-5 SPIRAL 5 CU6 SELF-TEST PASSED'
} elseif ($Mode -eq 'Measurement') {
 Write-Host 'SGE4-5 SPIRAL 5 CU6 REAL GPU MEASUREMENT PASSED'
} elseif ($Mode -eq 'Report') {
 Write-Host 'SGE4-5 SPIRAL 5 CU6 DECISION EVIDENCE REPORT PASSED'
} else {
 Write-Host 'SGE4-5 SPIRAL 5 EXPERIMENT COMPLETE'
 Write-Host 'Completion Unit 6: PASSED'
 Write-Host 'Architecture Complete: retained from CU5'
 Write-Host 'Candidates: A.EveryInvocationRecompute / B.GlobalMotorHistoryReuse / C.FinalOutputHistoryReuse'
 Write-Host 'Update interval corpus: U=1/2/4/8/16/32/64'
 Write-Host 'Order balance: all 6 Candidate permutations; every Candidate appears twice per position'
 Write-Host "Samples per Candidate and U: $(6 * $MeasurementPerOrder * $Runs)"
 Write-Host "Measured timeline samples: $(7 * 3 * 6 * $MeasurementPerOrder * $Runs)"
 Write-Host 'Primary metric: GPU Argument + Hierarchy + Consumer + retained completion timeline'
 Write-Host 'Update/Hold costs: independently accumulated from per-invocation timestamp phases'
 Write-Host 'Decision Report: per-U winner set, A/B crossover, B/C crossover, history materialization position'
 Write-Host 'RuntimePolicyAuthorization: None'
 Write-Host 'NextCapabilitySelection: DeferredByOwner'
 Write-Host 'SelectionStatus: OWNER_DECISION_REQUIRED'
 Write-Host 'Spiral 5 Closed banner remains withheld until the Owner decision'
}
Write-Host "Measurement evidence SHA-256: $evidenceDigest"
Write-Host "Decision report SHA-256: $reportDigest"
Write-Host "Measurement summary SHA-256: $summaryDigest"
Write-Host '============================================================'
