param(
    [int]$AdapterIndex = 0,
    [int]$CheckpointSamples = 768,
    [int]$BracketedSamples = 512)
$ErrorActionPreference = 'Stop'
$testsRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$root = Split-Path -Parent $testsRoot
. (Join-Path $testsRoot 'Spiral6Common.ps1')

Invoke-Spiral6Checks @(
    'tools\Verify-Spiral6CU6.ps1',
    'tools\Verify-Spiral6EquivalentSetAudit.ps1',
    'tools\Verify-Spiral6TransitionProbe.ps1')
Build-Spiral6Project '173_Spiral6TransitionProbeTests\173_Spiral6TransitionProbeTests.vcxproj'

$debugExe = Join-Path $root 'build\bin\x64\Debug\173_Spiral6TransitionProbeTests.exe'
$releaseExe = Join-Path $root 'build\bin\x64\Release\173_Spiral6TransitionProbeTests.exe'
Invoke-Checked $debugExe @('--self-test')
Invoke-Checked $releaseExe @('--self-test')

$output = Join-Path $root 'build\tests\spiral6-cu6\transition-probe'
Remove-Item -Recurse -Force -ErrorAction SilentlyContinue $output
$checkpointA = Join-Path $output 'checkpoint-a'
$bracketedA = Join-Path $output 'bracketed-a'
$checkpointB = Join-Path $output 'checkpoint-b'
$bracketedB = Join-Path $output 'bracketed-b'
New-Item -ItemType Directory -Force $checkpointA,$bracketedA,$checkpointB,$bracketedB | Out-Null

$common = @('--probe','--adapter-index',"$AdapterIndex",'--baseline-samples','32','--sustained-samples','8','--transition-ratio','2.5')
Invoke-Checked $releaseExe ($common + @('--mode','checkpoint','--samples',"$CheckpointSamples",'--output',$checkpointA))
Invoke-Checked $releaseExe ($common + @('--mode','bracketed','--samples',"$BracketedSamples",'--output',$bracketedA))
Invoke-Checked $releaseExe ($common + @('--mode','checkpoint','--samples',"$CheckpointSamples",'--output',$checkpointB))
Invoke-Checked $releaseExe ($common + @('--mode','bracketed','--samples',"$BracketedSamples",'--output',$bracketedB))

$report = Join-Path $output 'SPIRAL6_TRANSITION_PROBE_COMBINED_REPORT.md'
& (Join-Path $testsRoot 'tools\Analyze-Spiral6TransitionProbe.ps1') -ProbeRoot $output -OutputPath $report
if ($LASTEXITCODE -ne 0) { throw "Transition Probe analyzer failed with exit code $LASTEXITCODE." }
if (-not (Test-Path -LiteralPath $report -PathType Leaf)) { throw 'Transition Probe combined report was not generated.' }
$reportDigest = (Get-FileHash -Algorithm SHA256 -LiteralPath $report).Hash
Write-Host '============================================================'
Write-Host 'SGE4-5 SPIRAL 6 CU6 INTRA-SAMPLE TRANSITION PROBE COMPLETE'
Write-Host 'Control: PrefixControl K=1 exact set {0}'
Write-Host 'Candidate sequence: A.Dense / C.ActiveBlock / B.Compact repeated'
Write-Host "Checkpoint fresh processes: 2 x $CheckpointSamples samples"
Write-Host "Bracketed fresh processes: 2 x $BracketedSamples samples"
Write-Host 'CPU checkpoints: validation / Artifact / Resource / recording / Execute / Signal / Fence / Readback'
Write-Host 'GPU phases: sentinel copy / transition / argument / consumer / barriers / readback copies'
Write-Host 'Calibration brackets: pre-resource / post-resource / pre-submit / post-main'
Write-Host "Report: $report"
Write-Host "Report SHA-256: $reportDigest"
Write-Host '============================================================'
