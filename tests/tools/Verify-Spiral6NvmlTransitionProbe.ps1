$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
function Require([bool]$Condition,[string]$Message){if(-not $Condition){throw $Message}}

$required=@(
 '170_Spiral6PerformanceExperiment\Spiral6PerformanceExperiment.h',
 '170_Spiral6PerformanceExperiment\Spiral6RealGpuMeasurement.cpp',
 '173_Spiral6TransitionProbeTests\main.cpp',
 'tests\Run-Spiral6NvmlTransitionProbe.ps1',
 'tests\tools\Analyze-Spiral6NvmlTransitionProbe.ps1',
 'tests\tools\Verify-Spiral6NvmlTransitionProbe.ps1',
 'run_sge4_5_spiral6_cu6_nvml_transition_probe_prepare.bat',
 'run_sge4_5_spiral6_cu6_nvml_transition_probe.bat',
 'docs\spiral6\CU6_NVIDIA_PERFORMANCE_STATE_TELEMETRY.md',
 'docs\spiral6\CU6_NVIDIA_PERFORMANCE_STATE_TELEMETRY_CHANGESET.md',
 'docs\spiral6\CU6_NVIDIA_PERFORMANCE_STATE_TELEMETRY_MANIFEST_V1.json')
foreach($relative in $required){Require (Test-Path -LiteralPath (Join-Path $root $relative) -PathType Leaf) "Missing NVML telemetry file: $relative"}

$header=Get-Content -Raw -LiteralPath (Join-Path $root '170_Spiral6PerformanceExperiment\Spiral6PerformanceExperiment.h') -Encoding UTF8
foreach($token in @('enableNvmlTelemetry','requireNvmlTelemetry','nvmlPollingIntervalMilliseconds','requestStablePowerState','nvmlTransitionDiagnosis')){
 Require ($header.Contains($token)) "NVML header token missing: $token"
}
$source=Get-Content -Raw -LiteralPath (Join-Path $root '170_Spiral6PerformanceExperiment\Spiral6RealGpuMeasurement.cpp') -Encoding UTF8
foreach($token in @('LoadLibraryExW(L"nvml.dll"','nvmlInit_v2','nvmlDeviceGetClockInfo','nvmlDeviceGetPerformanceState','nvmlDeviceGetCurrentClocksEventReasons','nvmlDeviceGetPowerUsage','nvmlDeviceGetUtilizationRates','SetStablePowerState(TRUE)','spiral6_transition_probe_nvml_timeline_v1.csv','HwPowerBrakeSlowdown','GraphicsClockDropWithoutReportedLimiter')){
 Require ($source.Contains($token)) "NVML source token missing: $token"
}
Require (-not $source.Contains('#include <nvml.h>')) 'NVML telemetry must remain dynamically loaded and Toolkit-independent.'
$main=Get-Content -Raw -LiteralPath (Join-Path $root '173_Spiral6TransitionProbeTests\main.cpp') -Encoding UTF8
foreach($token in @('--nvml','--require-nvml','--nvml-index','--nvml-poll-ms','--stable-power-state')){
 Require ($main.Contains($token)) "NVML command-line token missing: $token"
}
$runner=Get-Content -Raw -LiteralPath (Join-Path $root 'tests\Run-Spiral6NvmlTransitionProbe.ps1') -Encoding UTF8
foreach($token in @('nvidia-smi.exe','--help-query-gpu','clocks_event_reasons_counters.hw_power_brake_slowdown','nvml-only-checkpoint','sidecar-bracketed','RunStablePowerState')){
 Require ($runner.Contains($token)) "NVML runner token missing: $token"
}
Write-Host 'Spiral 6 CU6 NVML dynamic loading, high-frequency polling, nvidia-smi sidecar and Stable Power State diagnostic boundaries passed.'
