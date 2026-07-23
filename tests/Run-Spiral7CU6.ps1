param(
    [int]$AdapterIndex = 0,
    [int]$Runs = 2,
    [int]$Cycles = 1,
    [int]$Iterations = 128,
    [int]$Warmup = 1024,
    [int]$Attempts = 3
)
$ErrorActionPreference = 'Stop'
$testsRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$root = Split-Path -Parent $testsRoot
. (Join-Path $testsRoot 'Spiral7Common.ps1')

function Invoke-CU6Stage([string]$Name,[scriptblock]$Action){
    Write-Host "[CU6] START $Name"
    $watch=[Diagnostics.Stopwatch]::StartNew()
    & $Action
    $watch.Stop()
    Write-Host ("[CU6] DONE  {0} elapsed={1}" -f $Name,$watch.Elapsed)
}

$total=[Diagnostics.Stopwatch]::StartNew()
Invoke-CU6Stage 'Static boundaries and Source Manifest' {
    Invoke-Spiral7Checks @('tools\Verify-Spiral7CU2.ps1','tools\Verify-Spiral7CU3.ps1','tools\Verify-Spiral7CU4.ps1','tools\Verify-Spiral7CU5.ps1','tools\Verify-Spiral7CU6.ps1')
}
Invoke-CU6Stage 'Build Debug and Release' {
    Build-Spiral7Project '188_Spiral7PerformanceTests\188_Spiral7PerformanceTests.vcxproj'
}

$output=Join-Path $root 'build\tests\spiral7-cu6'
Remove-Item -Recurse -Force -ErrorAction SilentlyContinue $output
New-Item -ItemType Directory -Force $output | Out-Null
$debugExe=Join-Path $root 'build\bin\x64\Debug\188_Spiral7PerformanceTests.exe'
$releaseExe=Join-Path $root 'build\bin\x64\Release\188_Spiral7PerformanceTests.exe'
$debugSelf=Join-Path $output 'self-test-debug.bin'
$releaseSelf=Join-Path $output 'self-test-release.bin'

Invoke-CU6Stage 'Debug deterministic format self-test' {
    Invoke-Checked $debugExe @('--self-test','--emit',$debugSelf)
}
Invoke-CU6Stage 'Release deterministic format self-test' {
    Invoke-Checked $releaseExe @('--self-test','--emit',$releaseSelf)
}
if (-not (Test-BytesEqual $debugSelf $releaseSelf)) {
    throw 'Spiral 7 CU6 deterministic self-test evidence differs across Debug/Release.'
}

$evidence=Join-Path $output 'measurement_evidence_v2.bin'
$csv=Join-Path $output 'decision_cases_v2.csv'
$report=Join-Path $output 'decision_report_v2.txt'
$reloadReport=Join-Path $output 'decision_report_reloaded_v2.txt'
$measureArguments=@(
    '--measure','--evidence',$evidence,'--csv',$csv,'--report',$report,
    '--adapter',[string]$AdapterIndex,'--runs',[string]$Runs,'--cycles',[string]$Cycles,
    '--iterations',[string]$Iterations,'--warmup',[string]$Warmup,'--attempts',[string]$Attempts)
Invoke-CU6Stage 'Release real-GPU 4x5x8 Candidate-surface measurement' {
    Invoke-Checked $releaseExe $measureArguments
}
Invoke-CU6Stage 'Evidence reload and independent decision analysis' {
    Invoke-Checked $releaseExe @('--verify','--evidence',$evidence,'--report',$reloadReport)
}
if (-not (Test-BytesEqual $report $reloadReport)) {
    throw 'Spiral 7 CU6 direct and reloaded decision reports differ.'
}
$total.Stop()

Write-Host '============================================================'
Write-Host 'SGE4-5 SPIRAL 7 COMPLETION UNIT 6 PASSED'
Write-Host 'SGE4-5 SPIRAL 7 EXPERIMENT COMPLETE'
Write-Host 'Measurement grid: 4 patterns x 5 Active counts x 8 Transition counts = 160 cases per run'
Write-Host 'Candidates: A=FullActiveDenseRecompute, B=CompactDeltaIndexHistoryReuse, C=AffectedBlockDeltaHistoryReuse'
Write-Host 'Primary metric: GPU Candidate batch nanoseconds per effective iteration with six balanced A-B-C orders'
Write-Host 'Timestamp policy: zero-dispatch zero-tick intervals are one-tick conservative upper bounds; nonzero dispatch adapts iterations'
Write-Host 'Decision scope: adapter/driver-specific observed winner surface; no universal winner claim'
Write-Host 'Runtime Candidate-policy decision: None'
Write-Host 'SPIRAL 7 CLOSURE: OWNER REQUIRED'
Write-Host 'Next program after Owner closure: Level 4 v2 Canonical reconstruction'
Write-Host "CU6 total elapsed: $($total.Elapsed)"
Write-Host "CU6 Measurement evidence SHA-256: $(Get-SGE4FileSha256 $evidence)"
Write-Host "Evidence: $evidence"
Write-Host "Case CSV: $csv"
Write-Host "Decision report: $report"
Write-Host '============================================================'
