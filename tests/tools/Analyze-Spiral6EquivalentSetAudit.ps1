param(
    [Parameter(Mandatory=$true)][string]$BaselineCsv,
    [Parameter(Mandatory=$true)][string]$AuditRoot,
    [Parameter(Mandatory=$true)][string]$OutputPath
)
$ErrorActionPreference = 'Stop'
$culture = [Globalization.CultureInfo]::InvariantCulture

function To-Double([object]$value) {
    return [double]::Parse([string]$value, [Globalization.NumberStyles]::Float, $culture)
}
function Median([double[]]$values) {
    if ($values.Count -eq 0) { throw 'Median requires at least one value.' }
    $sorted = @($values | Sort-Object)
    $middle = [int][math]::Floor($sorted.Count / 2)
    if (($sorted.Count % 2) -eq 1) { return [double]$sorted[$middle] }
    return ([double]$sorted[$middle - 1] + [double]$sorted[$middle]) / 2.0
}
function Format-Ns([double]$value) { return $value.ToString('0.###', $culture) }
function Candidate-Median($rows, [string]$column) {
    return Median @($rows | ForEach-Object { To-Double $_.$column })
}
function Distinct-One($rows, [string]$column, [string]$label) {
    $values = @($rows | ForEach-Object { [string]$_.$column } | Sort-Object -Unique)
    if ($values.Count -ne 1) { throw "$label has $($values.Count) distinct $column values." }
    return $values[0]
}
function Find-ChangePoint($rows, [string]$column, [int]$window = 12) {
    $ordered = @($rows | Sort-Object {[int64]$_.ordinal})
    if ($ordered.Count -lt ($window * 2 + 1)) { return $null }
    $best = $null
    for ($i = $window; $i -le $ordered.Count - $window; ++$i) {
        $before = Median @($ordered[($i-$window)..($i-1)] | ForEach-Object { To-Double $_.$column })
        $after = Median @($ordered[$i..($i+$window-1)] | ForEach-Object { To-Double $_.$column })
        $small = [math]::Max([math]::Min($before,$after), 1.0)
        $ratio = [math]::Max($before,$after) / $small
        if ($null -eq $best -or $ratio -gt $best.Ratio) {
            $best = [pscustomobject]@{Ordinal=[int64]$ordered[$i].ordinal;Before=$before;After=$after;Ratio=$ratio}
        }
    }
    return $best
}
function Quartile-Medians($rows, [string]$column) {
    $ordered = @($rows | Sort-Object {[int64]$_.ordinal})
    $result = @()
    for ($q = 0; $q -lt 4; ++$q) {
        $start = [int][math]::Floor($ordered.Count * $q / 4.0)
        $endExclusive = [int][math]::Floor($ordered.Count * ($q + 1) / 4.0)
        $slice = if ($endExclusive -gt $start) { $ordered[$start..($endExclusive-1)] } else { @() }
        $result += Median @($slice | ForEach-Object { To-Double $_.$column })
    }
    return $result
}

if (-not (Test-Path -LiteralPath $BaselineCsv -PathType Leaf)) { throw "Baseline CSV missing: $BaselineCsv" }
$runFiles = [ordered]@{
    'Balanced A' = Join-Path $AuditRoot 'balanced-a\spiral6_equivalent_set_audit_samples_v1.csv'
    'Fixed Pattern-major' = Join-Path $AuditRoot 'fixed\spiral6_equivalent_set_audit_samples_v1.csv'
    'Balanced B' = Join-Path $AuditRoot 'balanced-b\spiral6_equivalent_set_audit_samples_v1.csv'
}
foreach ($entry in $runFiles.GetEnumerator()) {
    if (-not (Test-Path -LiteralPath $entry.Value -PathType Leaf)) { throw "Audit CSV missing: $($entry.Value)" }
}

$baseline = @(Import-Csv -LiteralPath $BaselineCsv)
foreach ($candidate in @('A','B','C')) {
    $k1Digests = @($baseline | Where-Object { $_.K -eq '1' -and $_.candidate -eq $candidate -and $_.pattern -in @('PrefixControl','UniformStride') } | ForEach-Object { $_.outputDigest } | Sort-Object -Unique)
    if ($k1Digests.Count -ne 1) { throw "Original CU6 K=1 Prefix/Uniform output digests differ for Candidate $candidate." }
    $fullDigests = @($baseline | Where-Object { $_.K -eq '4096' -and $_.candidate -eq $candidate } | ForEach-Object { $_.outputDigest } | Sort-Object -Unique)
    if ($fullDigests.Count -ne 1) { throw "Original CU6 K=4096 Pattern output digests differ for Candidate $candidate." }
}
$runs = [ordered]@{}
foreach ($entry in $runFiles.GetEnumerator()) { $runs[$entry.Key] = @(Import-Csv -LiteralPath $entry.Value) }

# Structural gates: labels in one equivalent-set group must bind identical authority and output per Candidate.
foreach ($entry in $runs.GetEnumerator()) {
    $rows = $entry.Value
    if ($rows.Count -ne 432) { throw "$($entry.Key) must contain 432 measured samples; found $($rows.Count)." }
    foreach ($group in @('K1PrefixUniform','K4096AllPatterns')) {
        foreach ($candidate in @('A','B','C')) {
            $subset = @($rows | Where-Object { $_.group -eq $group -and $_.candidate -eq $candidate })
            if ($subset.Count -eq 0) { throw "$($entry.Key) $group Candidate $candidate is empty." }
            [void](Distinct-One $subset 'setIdentity' "$($entry.Key) $group $candidate")
            [void](Distinct-One $subset 'artifactIdentity' "$($entry.Key) $group $candidate")
            [void](Distinct-One $subset 'representationResourceIdentity' "$($entry.Key) $group $candidate")
            [void](Distinct-One $subset 'frozenBindingIdentity' "$($entry.Key) $group $candidate")
            [void](Distinct-One $subset 'outputDigest' "$($entry.Key) $group $candidate")
            [void](Distinct-One $subset 'expectedDispatchGroupsX' "$($entry.Key) $group $candidate")
            [void](Distinct-One $subset 'observedDispatchGroupsX' "$($entry.Key) $group $candidate")
        }
    }
    $frequencies = @($rows | ForEach-Object { $_.timestampFrequencyBefore; $_.timestampFrequencyAfter } | Where-Object { $_ -ne '0' } | Sort-Object -Unique)
    if ($frequencies.Count -gt 1) { throw "$($entry.Key) recorded multiple D3D12 timestamp frequencies." }
}

$builder = [Text.StringBuilder]::new()
[void]$builder.AppendLine('# Spiral 6 CU6 Equivalent Set Audit')
[void]$builder.AppendLine()
[void]$builder.AppendLine('This audit examines only the two naturally equivalent-input controls:')
[void]$builder.AppendLine()
[void]$builder.AppendLine('- `K=1`: `PrefixControl = UniformStride = {0}`')
[void]$builder.AppendLine('- `K=4096`: all four Pattern constructors equal the full universe `[0,4095]`')
[void]$builder.AppendLine()
[void]$builder.AppendLine('Every audit run enforces identical Set identity, Candidate Artifact identity, Representation Resource identity, Dispatch and Output digest across the equivalent Pattern labels.')
[void]$builder.AppendLine()

[void]$builder.AppendLine('## Original CU6 baseline')
[void]$builder.AppendLine()
[void]$builder.AppendLine('| Control | Candidate | Pattern | Median candidate path (ns) | Median sentinel (ns) | Median consumer (ns) | Output digest |')
[void]$builder.AppendLine('|---|---:|---|---:|---:|---:|---|')
foreach ($candidate in @('A','B','C')) {
    foreach ($pattern in @('PrefixControl','UniformStride')) {
        $rows = @($baseline | Where-Object { $_.K -eq '1' -and $_.pattern -eq $pattern -and $_.candidate -eq $candidate })
        $digest = Distinct-One $rows 'outputDigest' "Baseline K=1 $pattern $candidate"
        [void]$builder.AppendLine("| K=1 | $candidate | $pattern | $(Format-Ns (Candidate-Median $rows 'gpuCandidatePathNs')) | $(Format-Ns (Candidate-Median $rows 'gpuSentinelInitializationNs')) | $(Format-Ns (Candidate-Median $rows 'gpuSparseConsumerExecutionNs')) | ``$digest`` |")
    }
}
foreach ($candidate in @('A','B','C')) {
    foreach ($pattern in @('PrefixControl','UniformStride','BlockClusteredPermutation','HashScatterPermutation')) {
        $rows = @($baseline | Where-Object { $_.K -eq '4096' -and $_.pattern -eq $pattern -and $_.candidate -eq $candidate })
        $digest = Distinct-One $rows 'outputDigest' "Baseline K=4096 $pattern $candidate"
        [void]$builder.AppendLine("| K=4096 | $candidate | $pattern | $(Format-Ns (Candidate-Median $rows 'gpuCandidatePathNs')) | $(Format-Ns (Candidate-Median $rows 'gpuSentinelInitializationNs')) | $(Format-Ns (Candidate-Median $rows 'gpuSparseConsumerExecutionNs')) | ``$digest`` |")
    }
}
[void]$builder.AppendLine()

$globalMaxBalancedSpread = 1.0
$globalMaxFixedSpread = 1.0
foreach ($entry in $runs.GetEnumerator()) {
    $name = $entry.Key
    $rows = $entry.Value
    [void]$builder.AppendLine("## $name")
    [void]$builder.AppendLine()
    [void]$builder.AppendLine('| Control | Candidate | Pattern | Samples | Median candidate path (ns) | Median sentinel (ns) | Median consumer (ns) |')
    [void]$builder.AppendLine('|---|---:|---|---:|---:|---:|---:|')
    $runMaxSpread = 1.0
    foreach ($group in @('K1PrefixUniform','K4096AllPatterns')) {
        $patterns = if ($group -eq 'K1PrefixUniform') { @('PrefixControl','UniformStride') } else { @('PrefixControl','UniformStride','BlockClusteredPermutation','HashScatterPermutation') }
        foreach ($candidate in @('A','B','C')) {
            $candidateMedians = @()
            foreach ($pattern in $patterns) {
                $subset = @($rows | Where-Object { $_.group -eq $group -and $_.candidate -eq $candidate -and $_.pattern -eq $pattern })
                $candidateMedian = Candidate-Median $subset 'gpuCandidatePathNs'
                $candidateMedians += $candidateMedian
                [void]$builder.AppendLine("| $group | $candidate | $pattern | $($subset.Count) | $(Format-Ns $candidateMedian) | $(Format-Ns (Candidate-Median $subset 'gpuSentinelInitializationNs')) | $(Format-Ns (Candidate-Median $subset 'gpuSparseConsumerExecutionNs')) |")
            }
            $minimum = ($candidateMedians | Measure-Object -Minimum).Minimum
            $maximum = ($candidateMedians | Measure-Object -Maximum).Maximum
            $spread = [double]$maximum / [math]::Max([double]$minimum, 1.0)
            if ($spread -gt $runMaxSpread) { $runMaxSpread = $spread }
        }
    }
    [void]$builder.AppendLine()
    [void]$builder.AppendLine("Maximum equivalent-label median spread: **$($runMaxSpread.ToString('0.000', $culture))x**")
    [void]$builder.AppendLine()
    $quartileSentinel = Quartile-Medians $rows 'gpuSentinelInitializationNs'
    $quartilePath = Quartile-Medians $rows 'gpuCandidatePathNs'
    [void]$builder.AppendLine('| Time quartile | Median sentinel (ns) | Median candidate path (ns) |')
    [void]$builder.AppendLine('|---:|---:|---:|')
    for ($q=0; $q -lt 4; ++$q) {
        [void]$builder.AppendLine("| $($q+1) | $(Format-Ns $quartileSentinel[$q]) | $(Format-Ns $quartilePath[$q]) |")
    }
    [void]$builder.AppendLine()
    $change = Find-ChangePoint $rows 'gpuSentinelInitializationNs' 12
    if ($null -ne $change) {
        [void]$builder.AppendLine("Largest sustained 12-sample sentinel step: ordinal **$($change.Ordinal)**, $(Format-Ns $change.Before) ns -> $(Format-Ns $change.After) ns, ratio **$($change.Ratio.ToString('0.000',$culture))x**.")
        [void]$builder.AppendLine()
    }
    $localUsage = @($rows | ForEach-Object { [int64]$_.localUsageBefore; [int64]$_.localUsageAfter })
    $localMin = ($localUsage | Measure-Object -Minimum).Minimum
    $localMax = ($localUsage | Measure-Object -Maximum).Maximum
    [void]$builder.AppendLine("Observed DXGI local-memory CurrentUsage range: $localMin to $localMax bytes.")
    [void]$builder.AppendLine()
    if ($name -like 'Balanced*') {
        if ($runMaxSpread -gt $globalMaxBalancedSpread) { $globalMaxBalancedSpread = $runMaxSpread }
    } else {
        if ($runMaxSpread -gt $globalMaxFixedSpread) { $globalMaxFixedSpread = $runMaxSpread }
    }
}

[void]$builder.AppendLine('## Diagnostic classification')
[void]$builder.AppendLine()
[void]$builder.AppendLine("- Maximum balanced equivalent-label spread: **$($globalMaxBalancedSpread.ToString('0.000',$culture))x**")
[void]$builder.AppendLine("- Maximum fixed Pattern-major equivalent-label spread: **$($globalMaxFixedSpread.ToString('0.000',$culture))x**")
[void]$builder.AppendLine()
if ($globalMaxBalancedSpread -le 1.10 -and $globalMaxFixedSpread -gt 1.25) {
    [void]$builder.AppendLine('**Classification: fixed Pattern-major order confounding reproduced.** Equivalent labels converge when interleaved and diverge when grouped by label.')
} elseif ($globalMaxBalancedSpread -gt 1.25) {
    [void]$builder.AppendLine('**Classification: a large timing-state effect remains even under balanced equivalent-label interleaving.** Inspect the reported time-quartile and change-point sections; the effect is not attributable to different Exact Sets or Candidate Artifacts.')
} else {
    [void]$builder.AppendLine('**Classification: equivalent labels are timing-consistent in both schedules within the 10% diagnostic band.** The original CU6 discrepancy did not reproduce in this audit process.')
}
[void]$builder.AppendLine()
[void]$builder.AppendLine('This classification is diagnostic. It does not authorize Runtime Candidate selection and does not alter CU1-CU5 Architecture evidence.')

$parent = Split-Path -Parent $OutputPath
New-Item -ItemType Directory -Force $parent | Out-Null
$utf8NoBom = New-Object Text.UTF8Encoding($false)
[IO.File]::WriteAllText($OutputPath, $builder.ToString(), $utf8NoBom)
$sha = (Get-FileHash -Algorithm SHA256 -LiteralPath $OutputPath).Hash
Write-Host "Equivalent Set Audit combined report: $OutputPath"
Write-Host "Equivalent Set Audit report SHA-256: $sha"
Write-Host "Balanced max spread: $($globalMaxBalancedSpread.ToString('0.000',$culture))x"
Write-Host "Fixed max spread: $($globalMaxFixedSpread.ToString('0.000',$culture))x"
