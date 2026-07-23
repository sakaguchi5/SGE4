param(
    [ValidateSet('SelfTest','Measurement','Report','CU6')]
    [string]$Mode = 'CU6',
    [int]$Runs = 2,
    [int]$CyclesPerOrder = 2,
    [int]$IterationsPerBatch = 512,
    [int]$WarmupIterations = 4096,
    [int]$MaximumBlockAttempts = 4,
    [int]$AdapterIndex = 0,
    [int]$NvmlDeviceIndex = 0,
    [int]$NvmlPollMs = 2,
    [double]$MaximumControlRatio = 1.20,
    [double]$RelativeNoiseFloor = 0.02,
    [double]$MinimumPairAgreement = 0.70,
    [switch]$DisableNvmlDiagnostic,
    [switch]$StablePowerStateDiagnostic
)
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

$output = Join-Path $root 'build\tests\spiral6-cu6\relative-map-v2'
$selfTestRoot = Join-Path $root 'build\tests\spiral6-cu6\self-test-relative-v2'
$debugExe = Join-Path $root 'build\bin\x64\Debug\171_Spiral6PerformanceTests.exe'
$releaseExe = Join-Path $root 'build\bin\x64\Release\171_Spiral6PerformanceTests.exe'

if ($Mode -in @('SelfTest','CU6')) {
    Remove-Item -Recurse -Force -ErrorAction SilentlyContinue $selfTestRoot
    $debugOutput = Join-Path $selfTestRoot 'debug'
    $releaseOutput = Join-Path $selfTestRoot 'release'
    New-Item -ItemType Directory -Force $debugOutput,$releaseOutput | Out-Null
    Invoke-Checked $debugExe @('--self-test','--output',$debugOutput)
    Invoke-Checked $releaseExe @('--self-test','--output',$releaseOutput)
}

if ($Mode -in @('Measurement','CU6')) {
    Remove-Item -Recurse -Force -ErrorAction SilentlyContinue $output
    New-Item -ItemType Directory -Force $output | Out-Null
    $arguments = @(
        '--measure','--output',$output,
        '--runs',"$Runs",
        '--cycles-per-order',"$CyclesPerOrder",
        '--iterations-per-batch',"$IterationsPerBatch",
        '--warmup-iterations',"$WarmupIterations",
        '--maximum-block-attempts',"$MaximumBlockAttempts",
        '--adapter-index',"$AdapterIndex",
        '--nvml-device-index',"$NvmlDeviceIndex",
        '--nvml-poll-ms',"$NvmlPollMs",
        '--maximum-control-ratio',"$MaximumControlRatio",
        '--relative-noise-floor',"$RelativeNoiseFloor",
        '--minimum-pair-agreement',"$MinimumPairAgreement")
    if ($DisableNvmlDiagnostic) { $arguments += '--disable-nvml-diagnostic' }
    if ($StablePowerStateDiagnostic) { $arguments += '--stable-power-state-diagnostic' }
    Invoke-Checked $releaseExe $arguments
}

$evidence = Join-Path $output 'spiral6_measurement_evidence_v2.bin'
if ($Mode -in @('Report','CU6')) {
    if (-not (Test-Path -LiteralPath $evidence -PathType Leaf)) {
        throw 'CU6 V2 report requires spiral6_measurement_evidence_v2.bin. Run Measurement first.'
    }
    Invoke-Checked $releaseExe @('--report','--input',$evidence,'--output',$output)
    $reportPath = Join-Path $output 'SPIRAL6_RELATIVE_DECISION_MAP_V2.md'
    if (-not (Test-Path -LiteralPath $reportPath -PathType Leaf)) {
        throw 'CU6 V2 Relative Decision Map was not generated.'
    }
    $reportText = Get-Content -Raw -LiteralPath $reportPath -Encoding UTF8
    foreach ($token in @(
        'Primary evidence: A/B/C paired ratios',
        'Absolute nanoseconds are descriptive only',
        'Fixed control authority:',
        'NVML/P-state/clock telemetry: diagnostic only',
        'RuntimePolicyAuthorization: None',
        'NextCapabilitySelection: DeferredByOwner',
        'SelectionStatus: OWNER_DECISION_REQUIRED')) {
        if (-not $reportText.Contains($token)) { throw "CU6 V2 Relative Map token missing: $token" }
    }
}

$evidenceDigest = if (Test-Path -LiteralPath $evidence) { Get-SGE4FileSha256 $evidence } else { 'NOT-RUN' }
$reportFile = Join-Path $output 'SPIRAL6_RELATIVE_DECISION_MAP_V2.md'
$reportDigest = if (Test-Path -LiteralPath $reportFile) { Get-SGE4FileSha256 $reportFile } else { 'NOT-RUN' }
$manifestFile = Join-Path $output 'spiral6_measurement_manifest_v2.json'
$manifestDigest = if (Test-Path -LiteralPath $manifestFile) { Get-SGE4FileSha256 $manifestFile } else { 'NOT-RUN' }
$pairedObservations = 4 * 11 * 6 * $CyclesPerOrder * $Runs
$candidateBatches = $pairedObservations * 3

Write-Host '============================================================'
if ($Mode -eq 'SelfTest') {
    Write-Host 'SGE4-5 SPIRAL 6 CU6 V2 RELATIVE-MAP SELF-TEST PASSED'
} elseif ($Mode -eq 'Measurement') {
    Write-Host 'SGE4-5 SPIRAL 6 CU6 V2 PAIRED MEASUREMENT PASSED'
} elseif ($Mode -eq 'Report') {
    Write-Host 'SGE4-5 SPIRAL 6 CU6 V2 RELATIVE DECISION MAP PASSED'
} else {
    Write-Host 'SGE4-5 SPIRAL 6 EXPERIMENT COMPLETE'
    Write-Host 'Completion Unit 6 V2: PASSED'
    Write-Host 'Architecture Complete: retained from CU5'
    Write-Host 'Primary evidence: block-local paired A/B/C ratios'
    Write-Host 'Absolute nanoseconds: descriptive only; no cross-block absolute ranking'
    Write-Host 'Block authority: all six Candidate orders plus fixed-control drift gate'
    Write-Host 'Telemetry: optional diagnostic only; no P-state or clock acceptance condition'
    Write-Host "Paired A/B/C observations: $pairedObservations"
    Write-Host "Candidate batches: $candidateBatches"
    Write-Host 'RuntimePolicyAuthorization: None'
    Write-Host 'NextCapabilitySelection: DeferredByOwner'
    Write-Host 'SelectionStatus: OWNER_DECISION_REQUIRED'
    Write-Host 'Spiral 6 Closed banner remains withheld until the Owner decision'
}
Write-Host "Relative evidence SHA-256: $evidenceDigest"
Write-Host "Relative Decision Map SHA-256: $reportDigest"
Write-Host "Measurement manifest SHA-256: $manifestDigest"
Write-Host '============================================================'
