param(
    [Parameter(Mandatory=$true)][string]$ProbeRoot,
    [Parameter(Mandatory=$true)][string]$OutputPath)
$ErrorActionPreference = 'Stop'

$runs = @('checkpoint-a','bracketed-a','checkpoint-b','bracketed-b')
$rows = @()
foreach ($run in $runs) {
    $reportPath = Join-Path (Join-Path $ProbeRoot $run) 'SPIRAL6_TRANSITION_PROBE_REPORT.md'
    if (-not (Test-Path -LiteralPath $reportPath -PathType Leaf)) { throw "Missing Transition Probe report: $reportPath" }
    $text = Get-Content -Raw -LiteralPath $reportPath -Encoding UTF8
    $matchValue = {
        param([string]$Pattern)
        $match = [regex]::Match($text,$Pattern,[Text.RegularExpressions.RegexOptions]::Multiline)
        if (-not $match.Success) { return '' }
        return $match.Groups[1].Value.Trim()
    }
    $rows += [pscustomobject]@{
        Run=$run
        Mode=(& $matchValue '^- Mode: `([^`]+)`$')
        Observed=(& $matchValue '^- Transition observed: (true|false)$')
        Ordinal=(& $matchValue '^- First sustained slow ordinal: ([0-9]+)$')
        Window=(& $matchValue '^- Located transition window: `([^`]+)`$')
        Baseline=(& $matchValue '^- Baseline sentinel path: ([0-9.eE+-]+) ns$')
        Slow=(& $matchValue '^- First slow sentinel path: ([0-9.eE+-]+) ns$')
        ReportSha=(Get-FileHash -Algorithm SHA256 -LiteralPath $reportPath).Hash
    }
}

$bracketed = @($rows | Where-Object {$_.Mode -eq 'BracketedCalibration' -and $_.Observed -eq 'true'})
$checkpoint = @($rows | Where-Object {$_.Mode -eq 'CheckpointOnly' -and $_.Observed -eq 'true'})
$classification = if ($bracketed.Count -gt 0) {
    'A sustained transition was reproduced with GPU calibration brackets; use the located window as the primary diagnosis.'
} elseif ($checkpoint.Count -gt 0) {
    'A sustained transition was reproduced in the minimally intrusive flow, but not inside a bracketed run; the exact intra-sample window remains unresolved.'
} else {
    'No sustained transition was reproduced in these fresh processes. The probe completed and retained all checkpoint evidence.'
}

$builder = New-Object Text.StringBuilder
[void]$builder.AppendLine('# Spiral 6 CU6 Intra-Sample Transition Probe — Combined Report')
[void]$builder.AppendLine()
[void]$builder.AppendLine('The probe fixes `PrefixControl K=1 = {0}` and repeats Candidate order `A, C, B`.')
[void]$builder.AppendLine('Checkpoint runs preserve the original one-main-submit flow. Bracketed runs add fixed 64 KiB GPU calibration copies before resource creation, after resource creation, before the main submit, and after the main GPU work.')
[void]$builder.AppendLine()
[void]$builder.AppendLine('| Run | Mode | Transition | First slow ordinal | Located window | Baseline sentinel ns | First slow ns |')
[void]$builder.AppendLine('|---|---|---:|---:|---|---:|---:|')
foreach ($row in $rows) {
    $ordinal = if ($row.Ordinal) {$row.Ordinal} else {'—'}
    $window = if ($row.Window) {$row.Window} else {'—'}
    $slow = if ($row.Slow) {$row.Slow} else {'—'}
    [void]$builder.AppendLine("| $($row.Run) | $($row.Mode) | $($row.Observed) | $ordinal | $window | $($row.Baseline) | $slow |")
}
[void]$builder.AppendLine()
[void]$builder.AppendLine('## Classification')
[void]$builder.AppendLine()
[void]$builder.AppendLine($classification)
[void]$builder.AppendLine()
[void]$builder.AppendLine('The test locates the earliest measured interval where the slowdown appears. It does not name a GPU hardware or driver event without clock, power, temperature, or vendor telemetry.')
[void]$builder.AppendLine()
[void]$builder.AppendLine('## Run report identities')
[void]$builder.AppendLine()
foreach ($row in $rows) { [void]$builder.AppendLine(('- `{0}`: `{1}`' -f $row.Run,$row.ReportSha)) }

$parent = Split-Path -Parent $OutputPath
if ($parent) { New-Item -ItemType Directory -Force $parent | Out-Null }
$utf8 = New-Object Text.UTF8Encoding($false)
[IO.File]::WriteAllText($OutputPath,$builder.ToString(),$utf8)
Write-Host "Transition Probe combined report generated: $OutputPath"
