$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$obsolete = @(
    'docs\spiral7\CU5_EXECUTION_OPTIMIZATION_V2.md'
)
$deleted = 0
foreach ($relative in $obsolete) {
    $path = Join-Path $root $relative
    if (Test-Path -LiteralPath $path -PathType Leaf) {
        Remove-Item -Force -LiteralPath $path
        Write-Host "Deleted obsolete CU5 file: $relative"
        ++$deleted
    }
}
Write-Host "Spiral 7 CU5 final layout applied. Obsolete files deleted: $deleted"
