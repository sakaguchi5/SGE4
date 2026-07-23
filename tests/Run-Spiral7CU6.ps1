param(
    [int]$AdapterIndex = 0,
    [int]$CanonicalRuns = 2,
    [int]$CanonicalCycles = 1,
    [int]$CanonicalIterations = 128,
    [int]$CanonicalWarmup = 1024,
    [int]$CanonicalAttempts = 3,
    [int]$RefinementRuns = 3,
    [int]$RefinementCycles = 3,
    [int]$RefinementIterations = 256,
    [int]$RefinementWarmup = 2048,
    [int]$RefinementAttempts = 4
)
$ErrorActionPreference = 'Stop'
$testsRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$root = Split-Path -Parent $testsRoot
. (Join-Path $testsRoot 'Spiral7Common.ps1')

function Invoke-CU6Stage([string]$Name,[scriptblock]$Action){
    Write-Host "[CU6-2] START $Name"
    $watch=[Diagnostics.Stopwatch]::StartNew()
    & $Action
    $watch.Stop()
    Write-Host ("[CU6-2] DONE  {0} elapsed={1}" -f $Name,$watch.Elapsed)
}

$total=[Diagnostics.Stopwatch]::StartNew()
Invoke-CU6Stage 'Static boundaries and Source Manifest' {
    Invoke-Spiral7Checks @('tools\Verify-Spiral7CU2.ps1','tools\Verify-Spiral7CU3.ps1','tools\Verify-Spiral7CU4.ps1','tools\Verify-Spiral7CU5.ps1','tools\Verify-Spiral7CU6.ps1')
}
Invoke-CU6Stage 'Build Debug and Release' {
    Build-Spiral7Project '188_Spiral7PerformanceTests\188_Spiral7PerformanceTests.vcxproj'
}

$output=Join-Path $root 'build\tests\spiral7-cu6-2'
Remove-Item -Recurse -Force -ErrorAction SilentlyContinue $output
New-Item -ItemType Directory -Force $output | Out-Null
$debugExe=Join-Path $root 'build\bin\x64\Debug\188_Spiral7PerformanceTests.exe'
$releaseExe=Join-Path $root 'build\bin\x64\Release\188_Spiral7PerformanceTests.exe'
$debugSelf=Join-Path $output 'self-test-debug-s7m3.bin'
$releaseSelf=Join-Path $output 'self-test-release-s7m3.bin'

Invoke-CU6Stage 'Debug deterministic S7M3 paired-decision self-test' {
    Invoke-Checked $debugExe @('--self-test','--emit',$debugSelf)
}
Invoke-CU6Stage 'Release deterministic S7M3 paired-decision self-test' {
    Invoke-Checked $releaseExe @('--self-test','--emit',$releaseSelf)
}
if (-not (Test-BytesEqual $debugSelf $releaseSelf)) {
    throw 'Spiral 7 CU6-2 deterministic S7M3 self-test bundle differs across Debug/Release.'
}

$canonicalEvidence=Join-Path $output 'canonical_measurement_evidence_s7m3.bin'
$canonicalCsv=Join-Path $output 'canonical_decision_cases_v3.csv'
$canonicalReport=Join-Path $output 'canonical_decision_report_v3.txt'
$canonicalReload=Join-Path $output 'canonical_decision_report_reloaded_v3.txt'
$canonicalArguments=@(
    '--measure','--pass','canonical','--evidence',$canonicalEvidence,
    '--csv',$canonicalCsv,'--report',$canonicalReport,
    '--adapter',[string]$AdapterIndex,
    '--runs',[string]$CanonicalRuns,'--cycles',[string]$CanonicalCycles,
    '--iterations',[string]$CanonicalIterations,'--warmup',[string]$CanonicalWarmup,
    '--attempts',[string]$CanonicalAttempts)
Invoke-CU6Stage 'Release canonical 4x5x8 measurement' {
    Invoke-Checked $releaseExe $canonicalArguments
}
Invoke-CU6Stage 'Canonical evidence reload and independent paired analysis' {
    Invoke-Checked $releaseExe @('--verify','--evidence',$canonicalEvidence,'--report',$canonicalReload)
}
if (-not (Test-BytesEqual $canonicalReport $canonicalReload)) {
    throw 'Spiral 7 CU6-2 canonical direct and reloaded reports differ.'
}

$refinementEvidence=Join-Path $output 'high_transition_refinement_evidence_s7m3.bin'
$refinementCsv=Join-Path $output 'high_transition_refinement_cases_v3.csv'
$refinementReport=Join-Path $output 'high_transition_refinement_report_v3.txt'
$refinementReload=Join-Path $output 'high_transition_refinement_report_reloaded_v3.txt'
$refinementArguments=@(
    '--measure','--pass','refinement','--evidence',$refinementEvidence,
    '--csv',$refinementCsv,'--report',$refinementReport,
    '--adapter',[string]$AdapterIndex,
    '--runs',[string]$RefinementRuns,'--cycles',[string]$RefinementCycles,
    '--iterations',[string]$RefinementIterations,'--warmup',[string]$RefinementWarmup,
    '--attempts',[string]$RefinementAttempts)
Invoke-CU6Stage 'Release high-Transition 4x5x5 refinement measurement' {
    Invoke-Checked $releaseExe $refinementArguments
}
Invoke-CU6Stage 'Refinement evidence reload and independent paired analysis' {
    Invoke-Checked $releaseExe @('--verify','--evidence',$refinementEvidence,'--report',$refinementReload)
}
if (-not (Test-BytesEqual $refinementReport $refinementReload)) {
    throw 'Spiral 7 CU6-2 refinement direct and reloaded reports differ.'
}

$combinedCsv=Join-Path $output 'combined_paired_decision_cases_v3.csv'
$combinedReport=Join-Path $output 'combined_paired_decision_report_v3.txt'
Invoke-CU6Stage 'Canonical plus refinement paired Decision Map' {
    Invoke-Checked $releaseExe @(
        '--combine','--base-evidence',$canonicalEvidence,
        '--refinement-evidence',$refinementEvidence,
        '--csv',$combinedCsv,'--report',$combinedReport)
}
$total.Stop()

Write-Host '============================================================'
Write-Host 'SGE4-5 SPIRAL 7 COMPLETION UNIT 6-2 PASSED'
Write-Host 'SGE4-5 SPIRAL 7 EXPERIMENT COMPLETE'
Write-Host 'Decision authority: balanced paired agreement against both alternatives'
Write-Host 'Median ranking: descriptive only; it cannot authorize a winner or crossover'
Write-Host 'T=0: B/C ZeroDispatchEquivalent; excluded from crossover analysis'
Write-Host 'Canonical pass: 4 patterns x 5 Active x 8 Transition counts'
Write-Host 'Refinement pass: 4 patterns x 5 Active x T={1024,1536,2048,3072,4096}'
Write-Host 'Combined map: refinement replaces duplicate T=1024 and T=4096 coordinates'
Write-Host 'Runtime Candidate-policy decision: None'
Write-Host 'SPIRAL 7 CLOSURE: OWNER REQUIRED'
Write-Host 'Next program after Owner closure: Level 4 v2 Canonical reconstruction'
Write-Host "CU6-2 total elapsed: $($total.Elapsed)"
Write-Host "Canonical evidence SHA-256: $(Get-SGE4FileSha256 $canonicalEvidence)"
Write-Host "Refinement evidence SHA-256: $(Get-SGE4FileSha256 $refinementEvidence)"
Write-Host "Combined CSV: $combinedCsv"
Write-Host "Combined report: $combinedReport"
Write-Host '============================================================'
