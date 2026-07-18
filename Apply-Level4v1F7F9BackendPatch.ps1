$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$sourcePath = Join-Path $root '14_D3D12Backend/D3D12Backend.cpp'
$headerPath = Join-Path $root '14_D3D12Backend/D3D12Backend.h'
$patchPath = Join-Path $root 'patches/D3D12Backend_F7F9_Transition.cpp.txt'

foreach ($path in @($sourcePath, $headerPath, $patchPath)) {
    if (-not (Test-Path -LiteralPath $path)) { throw "Required F7-F9 Backend patch input is missing: $path" }
}

$header = [IO.File]::ReadAllText($headerPath)
if (-not $header.Contains('TransitionSharedBuffer(')) {
    throw 'D3D12Backend.h does not expose the F8 shared-Buffer transition boundary.'
}

$source = [IO.File]::ReadAllText($sourcePath)
if ($source.Contains('D3D12Backend::TransitionSharedBuffer(')) {
    Write-Host 'F7-F9 D3D12Backend transition patch is already applied.'
    exit 0
}

$anchor = 'base::Result<ExternalBufferReadback, runtime::RuntimeError> D3D12Backend::ReadSharedBuffer('
$first = $source.IndexOf($anchor, [StringComparison]::Ordinal)
if ($first -lt 0 -or $source.IndexOf($anchor, $first + $anchor.Length, [StringComparison]::Ordinal) -ge 0) {
    throw 'D3D12Backend.cpp does not contain exactly one expected ReadSharedBuffer anchor.'
}
foreach ($required in @('class DeviceDomain final', 'class ExternalBufferResource final',
    'class CompletionToken final', 'D3D12Backend::CreateSharedBuffer(')) {
    if (-not $source.Contains($required)) {
        throw "D3D12Backend.cpp is not the expected Shared DeviceDomain baseline: $required"
    }
}

$newline = if ($source.Contains("`r`n")) { "`r`n" } else { "`n" }
$patch = [IO.File]::ReadAllText($patchPath).Replace("`r`n", "`n").Replace("`n", $newline)
$updated = $source.Insert($first, $patch)
$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
[IO.File]::WriteAllText($sourcePath, $updated, $utf8NoBom)
Write-Host 'Applied F7-F9 shared-Buffer state-transition patch to D3D12Backend.cpp.'
