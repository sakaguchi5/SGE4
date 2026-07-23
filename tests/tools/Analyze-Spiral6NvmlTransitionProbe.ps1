param(
    [Parameter(Mandatory=$true)][string]$ProbeRoot,
    [Parameter(Mandatory=$true)][string]$OutputPath)
$ErrorActionPreference = 'Stop'

$directories = Get-ChildItem -LiteralPath $ProbeRoot -Directory | Where-Object {
    Test-Path -LiteralPath (Join-Path $_.FullName 'SPIRAL6_TRANSITION_PROBE_REPORT.md') -PathType Leaf
} | Sort-Object Name
if ($directories.Count -eq 0) { throw 'No NVML Transition Probe runs were found.' }

$rows = @()
foreach ($directory in $directories) {
    $reportPath = Join-Path $directory.FullName 'SPIRAL6_TRANSITION_PROBE_REPORT.md'
    $text = Get-Content -Raw -LiteralPath $reportPath -Encoding UTF8
    $value = {
        param([string]$Pattern)
        $match = [regex]::Match($text,$Pattern,[Text.RegularExpressions.RegexOptions]::Multiline)
        if (-not $match.Success) { return '' }
        return $match.Groups[1].Value.Trim()
    }
    $timelinePath = Join-Path $directory.FullName 'spiral6_transition_probe_nvml_timeline_v1.csv'
    $timeline = if (Test-Path -LiteralPath $timelinePath -PathType Leaf) {
        @(Import-Csv -LiteralPath $timelinePath)
    } else { @() }
    $validClocks = @($timeline | Where-Object {$_.graphicsResult -eq '0'} | ForEach-Object {[double]$_.graphicsClockMHz})
    $validMemory = @($timeline | Where-Object {$_.memoryResult -eq '0'} | ForEach-Object {[double]$_.memoryClockMHz})
    $pstates = @($timeline | Where-Object {$_.pStateResult -eq '0'} | ForEach-Object {$_.pState} | Sort-Object -Unique)
    $reasons = @($timeline | Where-Object {$_.eventReasonsResult -eq '0' -and $_.eventReasons -ne '0'} |
        ForEach-Object {$_.eventReasonNames} | Sort-Object -Unique)
    $sidecar = Join-Path $directory.FullName 'nvidia_smi_sidecar.csv'
    $rows += [pscustomobject]@{
        Run=$directory.Name
        Mode=(& $value '^- Mode: `([^`]+)`$')
        Observed=(& $value '^- Transition observed: (true|false)$')
        Ordinal=(& $value '^- First sustained slow ordinal: ([0-9]+)$')
        Window=(& $value '^- Located transition window: `([^`]+)`$')
        Diagnosis=(& $value '^- NVML transition diagnosis: `([^`]+)`$')
        Stable=(& $value '^- Stable Power State applied: (true|false)$')
        NvmlAvailable=(& $value '^- NVML available: (true|false)$')
        Device=(& $value '^- NVML device: `([^`]*)`$')
        Uuid=(& $value '^- NVML UUID: `([^`]*)`$')
        ClockMin=if($validClocks.Count){($validClocks|Measure-Object -Minimum).Minimum}else{''}
        ClockMax=if($validClocks.Count){($validClocks|Measure-Object -Maximum).Maximum}else{''}
        MemoryMin=if($validMemory.Count){($validMemory|Measure-Object -Minimum).Minimum}else{''}
        MemoryMax=if($validMemory.Count){($validMemory|Measure-Object -Maximum).Maximum}else{''}
        PStates=($pstates -join ',')
        Reasons=($reasons -join ';')
        NvmlRecords=$timeline.Count
        SidecarRows=if(Test-Path -LiteralPath $sidecar){[Math]::Max(0,@(Get-Content -LiteralPath $sidecar).Count-1)}else{0}
        ReportSha=(Get-FileHash -Algorithm SHA256 -LiteralPath $reportPath).Hash
    }
}

$diagnoses = @($rows | Where-Object {$_.Observed -eq 'true'} | ForEach-Object {$_.Diagnosis} | Sort-Object -Unique)
$primary = if ($diagnoses -match 'HwPowerBrakeSlowdown') {
    'NVML observed `HwPowerBrakeSlowdown`; the system or power supply asserted an external power brake.'
} elseif ($diagnoses -match 'HwThermalSlowdown|SwThermalSlowdown') {
    'NVML observed a thermal slowdown reason.'
} elseif ($diagnoses -match 'SwPowerCap') {
    'NVML observed software power capping.'
} elseif ($diagnoses -match 'PerformanceStateDegraded') {
    'NVML directly observed degradation to a lower-performance P-state.'
} elseif ($diagnoses -match 'GraphicsClockDropWithoutReportedLimiter|MemoryClockDropWithoutReportedLimiter') {
    'NVML directly observed a clock drop, but the captured clock-event reason did not name a limiter.'
} elseif ($diagnoses.Count -gt 0) {
    'A D3D12 timing transition was reproduced, but the checkpoint NVML snapshot did not identify a clock, P-state, or limiter change. Inspect the high-frequency NVML and nvidia-smi timelines.'
} else {
    'No sustained D3D12 transition was reproduced in these runs.'
}

$builder = New-Object Text.StringBuilder
[void]$builder.AppendLine('# Spiral 6 CU6 NVIDIA Performance-State Telemetry — Combined Report')
[void]$builder.AppendLine()
[void]$builder.AppendLine($primary)
[void]$builder.AppendLine()
[void]$builder.AppendLine('| Run | Mode | Stable | Transition | Ordinal | Window | NVML diagnosis | Graphics MHz min–max | Memory MHz min–max | P-states | Event reasons | NVML records | nvidia-smi rows |')
[void]$builder.AppendLine('|---|---|---:|---:|---:|---|---|---:|---:|---|---|---:|---:|')
foreach ($row in $rows) {
    $ordinal = if($row.Ordinal){$row.Ordinal}else{'—'}
    $window = if($row.Window){$row.Window}else{'—'}
    [void]$builder.AppendLine("| $($row.Run) | $($row.Mode) | $($row.Stable) | $($row.Observed) | $ordinal | $window | $($row.Diagnosis) | $($row.ClockMin)–$($row.ClockMax) | $($row.MemoryMin)–$($row.MemoryMax) | $($row.PStates) | $($row.Reasons) | $($row.NvmlRecords) | $($row.SidecarRows) |")
}
[void]$builder.AppendLine()
[void]$builder.AppendLine('## Interpretation boundary')
[void]$builder.AppendLine()
[void]$builder.AppendLine('In-process NVML snapshots and polling are primary evidence. Each unsupported GeForce query retains its NVML result code rather than being synthesized. The `nvidia-smi` sidecar is independent corroboration and may expose average power and clock-event cumulative counters not available through the minimal dynamically loaded ABI.')
[void]$builder.AppendLine()
[void]$builder.AppendLine('Stable Power State runs, when requested, are diagnostic causal controls only and do not authorize use of their timings in the Spiral 6 Candidate ranking.')
[void]$builder.AppendLine()
[void]$builder.AppendLine('## Run report identities')
[void]$builder.AppendLine()
foreach($row in $rows){[void]$builder.AppendLine(('- `{0}`: `{1}`' -f $row.Run,$row.ReportSha))}

$parent=Split-Path -Parent $OutputPath
if($parent){New-Item -ItemType Directory -Force $parent|Out-Null}
[IO.File]::WriteAllText($OutputPath,$builder.ToString(),(New-Object Text.UTF8Encoding($false)))
Write-Host "NVML Transition Probe combined report generated: $OutputPath"
