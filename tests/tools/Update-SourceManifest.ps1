$ErrorActionPreference = 'Stop'

$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
[Console]::InputEncoding = $utf8NoBom
[Console]::OutputEncoding = $utf8NoBom
$OutputEncoding = $utf8NoBom

$testsRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$root = Split-Path -Parent $testsRoot
$manifestPath = Join-Path $root 'SOURCE_MANIFEST.sha256'
. (Join-Path $PSScriptRoot 'Sha256.ps1')

function Normalize-RelativePath([string]$Path) {
    return $Path.Replace('\', '/')
}

function Get-RelativePath([string]$FullName) {
    return Normalize-RelativePath ($FullName.Substring($root.Length).TrimStart('\', '/'))
}

function Test-ManifestExcludedPath([string]$RelativePath) {
    return ($RelativePath -eq 'SOURCE_MANIFEST.sha256' -or
            $RelativePath -like 'build/*' -or
            $RelativePath -like '*/build/*' -or
            $RelativePath -like 'docs/test-logs/*.log' -or
            $RelativePath -like '.vs/*' -or
            $RelativePath -like '*/.vs/*' -or
            $RelativePath -like '.git/*' -or
            $RelativePath -like '*.user' -or
            $RelativePath -like '*.suo' -or
            $RelativePath -like '*.VC.db' -or
            $RelativePath -like '*.VC.opendb')
}

$manifestFiles = @(& git -C $root ls-files --cached --others --exclude-standard)
if ($LASTEXITCODE -ne 0) { throw 'git ls-files failed.' }
$digests = @{}
foreach ($listedPath in $manifestFiles) {
    $relative = Normalize-RelativePath $listedPath
    if (Test-ManifestExcludedPath $relative) {
        continue
    }

    if ($digests.ContainsKey($relative)) {
        throw "Duplicate normalized file path: $relative"
    }

    $fullName = Join-Path $root $listedPath
    if (-not (Test-Path -LiteralPath $fullName -PathType Leaf)) { continue }
    $digests[$relative] = (Get-SGE4FileSha256 $fullName).ToLowerInvariant()
}

$relativePaths = [string[]]@($digests.Keys)
[Array]::Sort($relativePaths, [StringComparer]::Ordinal)
$lines = foreach ($relative in $relativePaths) {
    "$($digests[$relative])  $relative"
}

[IO.File]::WriteAllLines($manifestPath, $lines, $utf8NoBom)
Write-Host "SOURCE_MANIFEST updated. Files: $($relativePaths.Count)."
Write-Host 'Run tests\run_architecture.bat immediately and review the manifest diff before committing.'
