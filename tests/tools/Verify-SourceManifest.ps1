$ErrorActionPreference = 'Stop'

$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
[Console]::InputEncoding = $utf8NoBom
[Console]::OutputEncoding = $utf8NoBom
$OutputEncoding = $utf8NoBom

$testsRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$root = Split-Path -Parent $testsRoot
$manifestPath = Join-Path $root 'SOURCE_MANIFEST.sha256'
if (-not (Test-Path -LiteralPath $manifestPath)) {
    throw 'SOURCE_MANIFEST.sha256 was not found.'
}

function Normalize-RelativePath([string]$Path) {
    return $Path.Replace('\', '/')
}

function Get-RelativePath([string]$FullName) {
    return Normalize-RelativePath ($FullName.Substring($root.Length).TrimStart('\', '/'))
}

$expected = @{}
$lineNumber = 0
foreach ($line in Get-Content -LiteralPath $manifestPath -Encoding UTF8) {
    $lineNumber++
    if ([string]::IsNullOrWhiteSpace($line)) { continue }
    if ($line -notmatch '^([0-9a-fA-F]{64})  (.+)$') {
        throw "Invalid SOURCE_MANIFEST line ${lineNumber}: $line"
    }

    $relative = Normalize-RelativePath $Matches[2]
    if ($expected.ContainsKey($relative)) {
        throw "Duplicate SOURCE_MANIFEST entry: $relative"
    }
    $expected[$relative] = $Matches[1].ToLowerInvariant()
}

function Test-ManifestExcludedPath([string]$RelativePath) {
    return ($RelativePath -eq 'SOURCE_MANIFEST.sha256' -or
            $RelativePath -like 'build/*' -or
            $RelativePath -like 'docs/test-logs/*.log' -or
            $RelativePath -like '.vs/*' -or
            $RelativePath -like '.git/*' -or
            $RelativePath -like '*.user' -or
            $RelativePath -like '*.suo' -or
            $RelativePath -like '*.VC.db' -or
            $RelativePath -like '*.VC.opendb')
}

$actualFiles = @(
    Get-ChildItem -Path $root -Recurse -File | Where-Object {
        $relative = Get-RelativePath $_.FullName
        -not (Test-ManifestExcludedPath $relative)
    }
)

$actual = @{}
foreach ($file in $actualFiles) {
    $relative = Get-RelativePath $file.FullName
    if ($actual.ContainsKey($relative)) {
        throw "Duplicate normalized file path: $relative"
    }
    $actual[$relative] = (
        Get-FileHash -Algorithm SHA256 -LiteralPath $file.FullName
    ).Hash.ToLowerInvariant()
}

$missing = @($expected.Keys | Where-Object { -not $actual.ContainsKey($_) } | Sort-Object)
$extra = @($actual.Keys | Where-Object { -not $expected.ContainsKey($_) } | Sort-Object)
$changed = @(
    $expected.Keys | Where-Object {
        $actual.ContainsKey($_) -and $actual[$_] -ne $expected[$_]
    } | Sort-Object
)

if ($missing.Count -gt 0 -or $extra.Count -gt 0 -or $changed.Count -gt 0) {
    if ($missing.Count -gt 0) {
        Write-Host 'Missing files:' -ForegroundColor Red
        $missing | ForEach-Object { Write-Host "  $_" }
    }
    if ($extra.Count -gt 0) {
        Write-Host 'Untracked files:' -ForegroundColor Red
        $extra | ForEach-Object { Write-Host "  $_" }
    }
    if ($changed.Count -gt 0) {
        Write-Host 'Digest mismatches:' -ForegroundColor Red
        $changed | ForEach-Object {
            Write-Host "  $_"
            Write-Host "    expected: $($expected[$_])"
            Write-Host "    actual:   $($actual[$_])"
        }
    }
    throw 'SOURCE_MANIFEST verification failed.'
}

Write-Host "SOURCE_MANIFEST verification passed. Files: $($actual.Count)."
