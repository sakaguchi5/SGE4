$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
function Require([bool]$Condition,[string]$Message) { if (-not $Condition) { throw $Message } }

$required = @(
 '170_Spiral6PerformanceExperiment\Spiral6PerformanceExperiment.h',
 '170_Spiral6PerformanceExperiment\Spiral6RealGpuMeasurement.cpp',
 '173_Spiral6TransitionProbeTests\173_Spiral6TransitionProbeTests.vcxproj',
 '173_Spiral6TransitionProbeTests\main.cpp',
 'tests\tools\Register-Spiral6TransitionProbeProject.ps1',
 'tests\tools\Verify-Spiral6TransitionProbe.ps1',
 'tests\tools\Analyze-Spiral6TransitionProbe.ps1',
 'tests\Run-Spiral6TransitionProbe.ps1',
 'run_sge4_5_spiral6_cu6_transition_probe_prepare.bat',
 'run_sge4_5_spiral6_cu6_transition_probe.bat',
 'docs\spiral6\CU6_INTRA_SAMPLE_TRANSITION_PROBE.md',
 'docs\spiral6\CU6_INTRA_SAMPLE_TRANSITION_PROBE_CHANGESET.md',
 'docs\spiral6\CU6_INTRA_SAMPLE_TRANSITION_PROBE_MANIFEST_V1.json')
foreach ($relative in $required) { Require (Test-Path -LiteralPath (Join-Path $root $relative) -PathType Leaf) "Missing Transition Probe file: $relative" }

$solution = Get-Content -Raw -LiteralPath (Join-Path $root 'SemanticGpuEngine4-5.sln') -Encoding UTF8
$path = '173_Spiral6TransitionProbeTests\173_Spiral6TransitionProbeTests.vcxproj'
$guid = '{000000AD-0000-5000-8000-0000000000AD}'
Require ($solution.Contains($path)) 'Transition Probe project is missing from Solution.'
foreach ($entry in @(
 "$guid.Debug|x64.ActiveCfg = Debug|x64",
 "$guid.Debug|x64.Build.0 = Debug|x64",
 "$guid.Release|x64.ActiveCfg = Release|x64",
 "$guid.Release|x64.Build.0 = Release|x64")) {
 Require ($solution.Contains($entry)) "Transition Probe Solution configuration missing: $entry"
}

$header = Get-Content -Raw -LiteralPath (Join-Path $root '170_Spiral6PerformanceExperiment\Spiral6PerformanceExperiment.h') -Encoding UTF8
foreach ($token in @(
 'TransitionProbeModeV1',
 'CheckpointOnly',
 'BracketedCalibration',
 'TransitionProbeWindowV1',
 'RunTransitionProbeSelfTestV1',
 'RunTransitionProbeV1')) {
 Require ($header.Contains($token)) "Transition Probe header token missing: $token"
}

$source = Get-Content -Raw -LiteralPath (Join-Path $root '170_Spiral6PerformanceExperiment\Spiral6RealGpuMeasurement.cpp') -Encoding UTF8
foreach ($token in @(
 'Transition Probe must reproduce the A/C/B boundary',
 'PrefixControl K=1 exact set {0}',
 'preResourceCalibration',
 'postResourceCalibration',
 'preSubmitCalibration',
 'postMainCalibration',
 'gpuSentinelCopyNanoseconds',
 'gpuOutputTransitionNanoseconds',
 'afterExecuteCall',
 'afterSignal',
 'afterFence',
 'spiral6_transition_probe_samples_v1.csv')) {
 Require ($source.Contains($token)) "Transition Probe source token missing: $token"
}

$project = Get-Content -Raw -LiteralPath (Join-Path $root $path) -Encoding UTF8
Require ($project.Contains('170_Spiral6PerformanceExperiment')) 'Transition Probe must depend on Project 170.'
Require ($project.Contains('<TreatWarningAsError>true</TreatWarningAsError>')) 'Transition Probe must use warnings as errors.'

$runner = Get-Content -Raw -LiteralPath (Join-Path $root 'tests\Run-Spiral6TransitionProbe.ps1') -Encoding UTF8
foreach ($token in @('checkpoint-a','checkpoint-b','bracketed-a','bracketed-b','SPIRAL6_TRANSITION_PROBE_COMBINED_REPORT.md')) {
 Require ($runner.Contains($token)) "Transition Probe runner token missing: $token"
}
Write-Host 'Spiral 6 CU6 intra-sample Transition Probe structure, A/C/B control, checkpoints and calibration brackets passed.'
