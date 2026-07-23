param(
    [int]$AdapterIndex = 0,
    [int]$NvmlIndex = 0,
    [int]$CheckpointSamples = 1024,
    [int]$BracketedSamples = 768,
    [int]$NvmlPollMs = 5,
    [int]$NvidiaSmiPollMs = 20,
    [switch]$RunStablePowerState)
$ErrorActionPreference = 'Stop'
$testsRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$root = Split-Path -Parent $testsRoot
. (Join-Path $testsRoot 'Spiral6Common.ps1')

Invoke-Spiral6Checks @(
    'tools\Verify-Spiral6CU6.ps1',
    'tools\Verify-Spiral6EquivalentSetAudit.ps1',
    'tools\Verify-Spiral6TransitionProbe.ps1',
    'tools\Verify-Spiral6NvmlTransitionProbe.ps1')
Build-Spiral6Project '173_Spiral6TransitionProbeTests\173_Spiral6TransitionProbeTests.vcxproj'

$debugExe = Join-Path $root 'build\bin\x64\Debug\173_Spiral6TransitionProbeTests.exe'
$releaseExe = Join-Path $root 'build\bin\x64\Release\173_Spiral6TransitionProbeTests.exe'
Invoke-Checked $debugExe @('--self-test')
Invoke-Checked $releaseExe @('--self-test')

function Resolve-NvidiaSmi {
    $command = Get-Command 'nvidia-smi.exe' -ErrorAction SilentlyContinue
    if ($command) { return $command.Source }
    $system = Join-Path $env:WINDIR 'System32\nvidia-smi.exe'
    if (Test-Path -LiteralPath $system -PathType Leaf) { return $system }
    return $null
}

function Select-NvidiaSmiFields([string]$SmiPath) {
    $help = (& $SmiPath '--help-query-gpu' 2>&1 | Out-String)
    $groups = @(
        @('timestamp'),
        @('index'),
        @('uuid'),
        @('name'),
        @('pstate'),
        @('clocks.current.graphics','clocks.gr'),
        @('clocks.current.sm','clocks.sm'),
        @('clocks.current.memory','clocks.mem'),
        @('clocks.current.video','clocks.video'),
        @('temperature.gpu'),
        @('power.draw.instant','power.draw'),
        @('power.draw.average'),
        @('power.limit'),
        @('utilization.gpu'),
        @('utilization.memory'),
        @('clocks_event_reasons.active','clocks_throttle_reasons.active'),
        @('clocks_event_reasons.gpu_idle','clocks_throttle_reasons.gpu_idle'),
        @('clocks_event_reasons.sw_power_cap','clocks_throttle_reasons.sw_power_cap'),
        @('clocks_event_reasons.sw_thermal_slowdown','clocks_throttle_reasons.sw_thermal_slowdown'),
        @('clocks_event_reasons.hw_thermal_slowdown','clocks_throttle_reasons.hw_thermal_slowdown'),
        @('clocks_event_reasons.hw_power_brake_slowdown','clocks_throttle_reasons.hw_power_brake_slowdown'),
        @('clocks_event_reasons_counters.sw_power_cap'),
        @('clocks_event_reasons_counters.sw_thermal_slowdown'),
        @('clocks_event_reasons_counters.hw_thermal_slowdown'),
        @('clocks_event_reasons_counters.hw_power_brake_slowdown'))
    $selected = New-Object Collections.Generic.List[string]
    foreach ($group in $groups) {
        foreach ($field in $group) {
            if ($help.Contains('"' + $field + '"')) {
                $selected.Add($field)
                break
            }
        }
    }
    return @($selected | ForEach-Object { $_ })
}

function Start-NvidiaSmiSidecar(
    [string]$RunDirectory,
    [int]$DeviceIndex,
    [int]$PollMilliseconds) {
    $smi = $script:NvidiaSmiPath
    if (-not $smi) {
        [IO.File]::WriteAllText((Join-Path $RunDirectory 'nvidia_smi_unavailable.txt'),
            'nvidia-smi.exe was not found.', (New-Object Text.UTF8Encoding($false)))
        return $null
    }
    $fields = $script:NvidiaSmiFields
    if ($fields.Count -eq 0) {
        [IO.File]::WriteAllText((Join-Path $RunDirectory 'nvidia_smi_unavailable.txt'),
            'No supported nvidia-smi selective query fields were discovered.',
            (New-Object Text.UTF8Encoding($false)))
        return $null
    }
    $utf8 = New-Object Text.UTF8Encoding($false)
    [IO.File]::WriteAllLines((Join-Path $RunDirectory 'nvidia_smi_sidecar_fields.txt'),$fields,$utf8)
    [IO.File]::WriteAllText((Join-Path $RunDirectory 'nvidia_smi_sidecar_start_utc.txt'),
        [DateTime]::UtcNow.ToString('o'),$utf8)
    $raw = Join-Path $RunDirectory 'nvidia_smi_sidecar_raw.csv'
    $stderr = Join-Path $RunDirectory 'nvidia_smi_sidecar_stderr.txt'
    $query = $fields -join ','
    $arguments = @(
        "--query-gpu=$query",
        '--format=csv,noheader,nounits',
        "--loop-ms=$PollMilliseconds",
        "--id=$DeviceIndex")
    $process = Start-Process -FilePath $smi -ArgumentList $arguments -PassThru -WindowStyle Hidden `
        -RedirectStandardOutput $raw -RedirectStandardError $stderr
    Start-Sleep -Milliseconds ([Math]::Max(100,2*$PollMilliseconds))
    return [pscustomobject]@{Process=$process;Raw=$raw;Fields=$fields;Directory=$RunDirectory}
}

function Stop-NvidiaSmiSidecar($Sidecar) {
    if (-not $Sidecar) { return }
    if (-not $Sidecar.Process.HasExited) {
        Stop-Process -Id $Sidecar.Process.Id -Force -ErrorAction SilentlyContinue
    }
    $Sidecar.Process.WaitForExit()
    $output = Join-Path $Sidecar.Directory 'nvidia_smi_sidecar.csv'
    $builder = New-Object Text.StringBuilder
    [void]$builder.AppendLine(($Sidecar.Fields -join ','))
    if (Test-Path -LiteralPath $Sidecar.Raw -PathType Leaf) {
        foreach ($line in Get-Content -LiteralPath $Sidecar.Raw -Encoding UTF8) {
            if ($line.Trim().Length -gt 0) { [void]$builder.AppendLine($line) }
        }
    }
    $utf8 = New-Object Text.UTF8Encoding($false)
    [IO.File]::WriteAllText($output,$builder.ToString(),$utf8)
    [IO.File]::WriteAllText((Join-Path $Sidecar.Directory 'nvidia_smi_sidecar_end_utc.txt'),
        [DateTime]::UtcNow.ToString('o'),$utf8)
}

function Invoke-TelemetryProbe(
    [string]$Name,
    [string]$Mode,
    [int]$Samples,
    [bool]$StablePower,
    [bool]$UseSidecar) {
    $directory = Join-Path $output $Name
    New-Item -ItemType Directory -Force $directory | Out-Null
    $sidecar = if ($UseSidecar) {
        Start-NvidiaSmiSidecar $directory $NvmlIndex $NvidiaSmiPollMs
    } else { $null }
    $arguments = @(
        '--probe','--adapter-index',"$AdapterIndex",'--mode',$Mode,
        '--samples',"$Samples",'--baseline-samples','32','--sustained-samples','8',
        '--transition-ratio','2.5','--nvml','--require-nvml',
        '--nvml-index',"$NvmlIndex",'--nvml-poll-ms',"$NvmlPollMs",'--output',$directory)
    if ($StablePower) { $arguments += '--stable-power-state' }
    try {
        & $releaseExe @arguments
        if ($LASTEXITCODE -ne 0) { throw "NVML Transition Probe $Name failed with exit code $LASTEXITCODE." }
    }
    finally {
        Stop-NvidiaSmiSidecar $sidecar
    }
}

$script:NvidiaSmiPath = Resolve-NvidiaSmi
$script:NvidiaSmiFields = if ($script:NvidiaSmiPath) {
    @(Select-NvidiaSmiFields $script:NvidiaSmiPath)
} else { @() }

$output = Join-Path $root 'build\tests\spiral6-cu6\nvml-transition-probe'
Remove-Item -Recurse -Force -ErrorAction SilentlyContinue $output
New-Item -ItemType Directory -Force $output | Out-Null

# First pair: in-process NVML only. Second pair: independent nvidia-smi sidecar
# added, so any observer effect from the sidecar remains visible.
Invoke-TelemetryProbe 'nvml-only-checkpoint' 'checkpoint' $CheckpointSamples $false $false
Invoke-TelemetryProbe 'nvml-only-bracketed' 'bracketed' $BracketedSamples $false $false
Invoke-TelemetryProbe 'sidecar-checkpoint' 'checkpoint' $CheckpointSamples $false $true
Invoke-TelemetryProbe 'sidecar-bracketed' 'bracketed' $BracketedSamples $false $true
if ($RunStablePowerState) {
    Invoke-TelemetryProbe 'stable-checkpoint' 'checkpoint' $CheckpointSamples $true $false
    Invoke-TelemetryProbe 'stable-bracketed' 'bracketed' $BracketedSamples $true $false
}

$report = Join-Path $output 'SPIRAL6_NVML_TRANSITION_PROBE_COMBINED_REPORT.md'
& (Join-Path $testsRoot 'tools\Analyze-Spiral6NvmlTransitionProbe.ps1') `
    -ProbeRoot $output -OutputPath $report
if ($LASTEXITCODE -ne 0) { throw "NVML Transition Probe analyzer failed with exit code $LASTEXITCODE." }
if (-not (Test-Path -LiteralPath $report -PathType Leaf)) {
    throw 'NVML Transition Probe combined report was not generated.'
}
$reportDigest = (Get-FileHash -Algorithm SHA256 -LiteralPath $report).Hash
Write-Host '============================================================'
Write-Host 'SGE4-5 SPIRAL 6 CU6 NVIDIA PERFORMANCE-STATE TELEMETRY COMPLETE'
Write-Host 'Control: PrefixControl K=1 exact set {0}; A/C/B repeated'
Write-Host "In-process NVML polling: ${NvmlPollMs}ms"
Write-Host "Independent nvidia-smi polling: ${NvidiaSmiPollMs}ms in the sidecar pair when available"
Write-Host 'Telemetry: clocks / P-state / temperature / power / utilization / clock-event reasons'
Write-Host "Stable Power State diagnostic included: $($RunStablePowerState.IsPresent)"
Write-Host "Report: $report"
Write-Host "Report SHA-256: $reportDigest"
Write-Host '============================================================'
