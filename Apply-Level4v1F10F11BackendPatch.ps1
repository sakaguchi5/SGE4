$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$sourcePath = Join-Path $root '14_D3D12Backend/D3D12Backend.cpp'
if (-not (Test-Path -LiteralPath $sourcePath)) {
    throw "Required D3D12Backend source is missing: $sourcePath"
}

$bytes = [IO.File]::ReadAllBytes($sourcePath)
$hasBom = $bytes.Length -ge 3 -and $bytes[0] -eq 0xEF -and $bytes[1] -eq 0xBB -and $bytes[2] -eq 0xBF
$offset = if ($hasBom) { 3 } else { 0 }
$source = [Text.Encoding]::UTF8.GetString($bytes, $offset, $bytes.Length - $offset)
$hadCrLf = $source.Contains("`r`n")
$source = $source.Replace("`r`n", "`n")

$oldMarker = '// SGE4 L4V1-F10-F11 DeviceDomain removed-adapter exclusion and explicit retry.'
$newMarker = '// SGE4 L4V1-F10-F11 DeviceDomain preserves removed-adapter exclusion across retry.'
$source = $source.Replace($oldMarker + "`n", '')
$source = $source.Replace($newMarker + "`n", '')

$classAnchor = @(
    'class DeviceDomain final : public runtime::IPackageDeviceDomain',
    '{'
) -join "`n"
$nextClassAnchor = 'class ExternalBufferResource final'
$classIndex = $source.IndexOf($classAnchor, [StringComparison]::Ordinal)
if ($classIndex -lt 0) { throw 'F10-F11 DeviceDomain class anchor is missing.' }
if ($source.IndexOf($classAnchor, $classIndex + $classAnchor.Length, [StringComparison]::Ordinal) -ge 0) {
    throw 'F10-F11 DeviceDomain class anchor is duplicated.'
}
$endIndex = $source.IndexOf($nextClassAnchor, $classIndex + $classAnchor.Length, [StringComparison]::Ordinal)
if ($endIndex -lt 0) { throw 'F10-F11 DeviceDomain end anchor is missing.' }

$prefix = $source.Substring(0, $classIndex)
$deviceDomain = $source.Substring($classIndex, $endIndex - $classIndex)
$suffix = $source.Substring($endIndex)

function Replace-ScopedOnce([string]$Text, [string]$Old, [string]$New, [string]$Label) {
    $first = $Text.IndexOf($Old, [StringComparison]::Ordinal)
    if ($first -lt 0) { throw "F10-F11 DeviceDomain repair anchor is missing: $Label" }
    if ($Text.IndexOf($Old, $first + $Old.Length, [StringComparison]::Ordinal) -ge 0) {
        throw "F10-F11 DeviceDomain repair anchor is duplicated: $Label"
    }
    return $Text.Substring(0, $first) + $New + $Text.Substring($first + $Old.Length)
}

$oldCandidate = @(
    '            candidateFailure = candidate->GetDesc1(&desc);',
    '            if (FAILED(candidateFailure)) return false;',
    '            candidateFailure = D3D12CreateDevice(candidate.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&candidateDevice));'
) -join "`n"
$newCandidate = @(
    '            candidateFailure = candidate->GetDesc1(&desc);',
    '            if (FAILED(candidateFailure)) return false;',
    '            if (hasExcludedAdapterLuid_ && SameLuid(desc.AdapterLuid, excludedAdapterLuid_))',
    '            {',
    '                candidateFailure = DXGI_ERROR_NOT_FOUND;',
    '                return false;',
    '            }',
    '            candidateFailure = D3D12CreateDevice(candidate.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&candidateDevice));'
) -join "`n"
if ($deviceDomain -notmatch 'if\s*\(hasExcludedAdapterLuid_\s*&&\s*SameLuid\(desc\.AdapterLuid,\s*excludedAdapterLuid_\)\)') {
    $deviceDomain = Replace-ScopedOnce $deviceDomain $oldCandidate $newCandidate 'exclude removed adapter during DeviceDomain Initialize'
}

# A Retry is a topology re-check, not permission to reuse the removed adapter.
# Remove the earlier F10 assumption that cleared the safety exclusion.
$unsafeRetry = @(
    '            hasExcludedAdapterLuid_=false;',
    '            auto rebuilt=Initialize();'
) -join "`n"
$safeRetry = '            auto rebuilt=Initialize();'
if ($deviceDomain.Contains($unsafeRetry)) {
    $deviceDomain = Replace-ScopedOnce $deviceDomain $unsafeRetry $safeRetry 'preserve removed-adapter exclusion during RetryAdapterReacquisition'
}

$oldRemoved = @(
    '        report.removedAdapterLuidLow=adapterLuid_.LowPart; report.removedAdapterLuidHigh=adapterLuid_.HighPart;',
    '        state_=runtime::DeviceRuntimeState::Lost; device_.Reset(); factory_.Reset();'
) -join "`n"
$newRemoved = @(
    '        report.removedAdapterLuidLow=adapterLuid_.LowPart; report.removedAdapterLuidHigh=adapterLuid_.HighPart;',
    '        if (mode!=runtime::DeviceRecoveryMode::ControlledRebuild)',
    '        {',
    '            excludedAdapterLuid_=adapterLuid_;',
    '            hasExcludedAdapterLuid_=true;',
    '        }',
    '        state_=runtime::DeviceRuntimeState::Lost; device_.Reset(); factory_.Reset();'
) -join "`n"
if ($deviceDomain -notmatch 'excludedAdapterLuid_\s*=\s*adapterLuid_\s*;[\s\S]*?hasExcludedAdapterLuid_\s*=\s*true\s*;') {
    $deviceDomain = Replace-ScopedOnce $deviceDomain $oldRemoved $newRemoved 'record removed adapter in DeviceDomain'
}

$oldFields = '    LUID adapterLuid_{}; ComPtr<IDXGIFactory6> factory_; ComPtr<ID3D12Device> device_;'
$newFields = @(
    '    LUID adapterLuid_{}; LUID excludedAdapterLuid_{}; bool hasExcludedAdapterLuid_=false;',
    '    ComPtr<IDXGIFactory6> factory_; ComPtr<ID3D12Device> device_;'
) -join "`n"
if ($deviceDomain -notmatch 'LUID\s+adapterLuid_\{\}\s*;\s*LUID\s+excludedAdapterLuid_\{\}\s*;\s*bool\s+hasExcludedAdapterLuid_\s*=\s*false\s*;') {
    $deviceDomain = Replace-ScopedOnce $deviceDomain $oldFields $newFields 'DeviceDomain exclusion fields'
}

$retryStartText = 'if (mode==runtime::DeviceRecoveryMode::RetryAdapterReacquisition)'
$retryStart = $deviceDomain.IndexOf($retryStartText, [StringComparison]::Ordinal)
if ($retryStart -lt 0) { throw 'F10-F11 DeviceDomain RetryAdapterReacquisition branch is missing.' }
$retryEnd = $deviceDomain.IndexOf('        if (state_!=runtime::DeviceRuntimeState::Active', $retryStart, [StringComparison]::Ordinal)
if ($retryEnd -lt 0) { throw 'F10-F11 DeviceDomain RetryAdapterReacquisition branch end is missing.' }
$retryText = $deviceDomain.Substring($retryStart, $retryEnd - $retryStart)
if ($retryText -match 'hasExcludedAdapterLuid_\s*=\s*false\s*;') {
    throw 'F10-F11 RetryAdapterReacquisition still clears the removed-adapter safety exclusion.'
}
if ($retryText -notmatch 'auto\s+rebuilt\s*=\s*Initialize\(\)\s*;') {
    throw 'F10-F11 RetryAdapterReacquisition no longer checks current adapter topology.'
}

$requiredPatterns = @(
    'if\s*\(hasExcludedAdapterLuid_\s*&&\s*SameLuid\(desc\.AdapterLuid,\s*excludedAdapterLuid_\)\)',
    'excludedAdapterLuid_\s*=\s*adapterLuid_\s*;[\s\S]*?hasExcludedAdapterLuid_\s*=\s*true\s*;',
    'LUID\s+adapterLuid_\{\}\s*;\s*LUID\s+excludedAdapterLuid_\{\}\s*;\s*bool\s+hasExcludedAdapterLuid_\s*=\s*false\s*;',
    'state_\s*=\s*runtime::DeviceRuntimeState::AwaitingAdapter'
)
foreach ($pattern in $requiredPatterns) {
    if ($deviceDomain -notmatch $pattern) {
        throw "F10-F11 DeviceDomain repair postcondition is missing: $pattern"
    }
}

$source = $prefix + $newMarker + "`n" + $deviceDomain + $suffix
if ($hadCrLf) { $source = $source.Replace("`n", "`r`n") }
$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
[IO.File]::WriteAllText($sourcePath, $source, $utf8NoBom)
Write-Host 'Verified F10-F11 removed-adapter exclusion and same-topology AwaitingAdapter retry contract inside DeviceDomain.'
