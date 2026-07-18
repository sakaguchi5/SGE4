$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)

function Read-Text([string]$Relative) {
    $path = Join-Path $root $Relative
    if (-not (Test-Path -LiteralPath $path)) { throw "Required file is missing: $Relative" }
    return [IO.File]::ReadAllText($path)
}
function Strip-CppComments([string]$Text) {
    $withoutBlocks = [regex]::Replace($Text, '/\*.*?\*/', '', 'Singleline')
    return [regex]::Replace($withoutBlocks, '//.*$', '', 'Multiline')
}

$required = @(
    '13_PackageRuntime/PackageRuntime.h',
    '14_D3D12Backend/D3D12Backend.h',
    '14_D3D12Backend/D3D12Backend.cpp',
    '20_CompositionDeviceDomain/CompositionDeviceDomain.h',
    '20_CompositionDeviceDomain/CompositionDeviceDomain.cpp',
    '21_CompositionSharedResources/CompositionSharedResources.h',
    '22_CompositionRuntime/CompositionRuntime.h',
    '22_CompositionRuntime/CompositionRuntime.cpp',
    '23_CompositionRecovery/CompositionRecovery.h',
    '23_CompositionRecovery/CompositionRecovery.cpp',
    '48_CanonicalCompositionRuntimeTests/main.cpp',
    '49_CanonicalCompositionRecoveryTests/main.cpp',
    '49A_CanonicalLevel4v1FreezeTests/main.cpp',
    'SemanticGpuEngine4_Level4v1_R3_R5.sln')
foreach ($file in $required) { [void](Read-Text $file) }

# Preparation and qualification must be self-contained. No history extraction or
# repository command is permitted in the R3-R5 scripts.
$scriptFiles = Get-ChildItem -LiteralPath $root -Recurse -File -Include '*.ps1','*.bat','*.cmd' |
    Where-Object { $_.Name -ne 'Verify-Level4v1R3R5Boundaries.ps1' -and $_.FullName -match 'Level4v1R3R5|level4v1_r[345]|level4v1_r3_r5' }
foreach ($script in $scriptFiles) {
    $text = [IO.File]::ReadAllText($script.FullName)
    if ($text -match '(?im)^\s*git(?:\.exe)?\s' -or
        $text -match '(?i)rev-parse|cat-file|git\s+show|bootstrap.*backend') {
        throw "R3-R5 script depends on repository history: $($script.FullName)"
    }
}

$backendHeader = Strip-CppComments (Read-Text '14_D3D12Backend/D3D12Backend.h')
$backendSource = Strip-CppComments (Read-Text '14_D3D12Backend/D3D12Backend.cpp')
foreach ($symbol in @('CreateDeviceDomain','LoadIntoDomain','CreateSharedBuffer','TransitionSharedBuffer','ReadSharedBuffer','RecoverDeviceDomain')) {
    if ($backendHeader -notmatch [regex]::Escape($symbol) -or $backendSource -notmatch [regex]::Escape($symbol)) {
        throw "D3D12 Backend is missing the explicit R3-R4 boundary: $symbol"
    }
}
if ($backendSource -match 'CompositionRuntime|CompositionRecovery|ProposeCompositionPlan|VerifyAndSeal') {
    throw 'D3D12 Backend depends on Composition planning or runtime authority.'
}
foreach ($symbol in @('ID3D12Device5','RemoveDevice','excludedAdapterLuid_','AwaitingAdapter','DeviceEpoch')) {
    if ($backendSource -notmatch [regex]::Escape($symbol)) { throw "Recovery backend proof boundary is missing: $symbol" }
}

$runtimeSource = Strip-CppComments (Read-Text '22_CompositionRuntime/CompositionRuntime.cpp')
foreach ($forbidden in @('ProposeCompositionPlan','VerifyAndSeal','FreezeVerifiedComposition','RawCompositionPlan')) {
    if ($runtimeSource -match [regex]::Escape($forbidden)) { throw "Runtime regained planning or freeze authority: $forbidden" }
}
if ($runtimeSource -notmatch 'ReadVerifiedFrozenComposition') {
    throw 'Runtime does not enter through independently verified Frozen Composition bytes.'
}

$recoverySource = Strip-CppComments (Read-Text '23_CompositionRecovery/CompositionRecovery.cpp')
$releaseAt = $recoverySource.IndexOf('loaded.resources_.reset()')
$recoverAt = $recoverySource.IndexOf('RecoverNativeDomain')
$leavesAt = $recoverySource.IndexOf('RematerializeLeaves')
$resourcesAt = $recoverySource.IndexOf('MaterializeSharedResources')
if ($releaseAt -lt 0 -or $recoverAt -lt 0 -or $releaseAt -gt $recoverAt) {
    throw 'Whole-composition runtime objects are not released before native recovery.'
}
if ($leavesAt -lt $recoverAt -or $resourcesAt -lt $leavesAt) {
    throw 'Recovery does not rematerialize Leaves and shared resources after native recovery.'
}
foreach ($symbol in @('allRuntimeObjectsReleased','allRuntimeObjectsRematerialized','frozenArtifactRevalidated','RetryAdapterReacquisition')) {
    if ($recoverySource -notmatch [regex]::Escape($symbol)) { throw "Recovery evidence is missing: $symbol" }
}

$allCpp = ($required | Where-Object { $_ -like '*.cpp' -or $_ -like '*.h' } | ForEach-Object { Read-Text $_ }) -join "`n"
foreach ($future in @('TextureFlow','MultipleWriter','PartialRecovery','MultipleDeviceDomain','StreamingResidency')) {
    if ($allCpp -match [regex]::Escape($future)) { throw "Speculative post-v1 abstraction was introduced: $future" }
}

Write-Host 'Canonical R3-R5 boundary verification passed.'
Write-Host 'Runtime input: independently verified Frozen Composition only.'
Write-Host 'Device ownership: one DeviceDomain and one epoch for all Leaves and shared Buffers.'
Write-Host 'Recovery ownership: entire Composition released and rematerialized as one unit.'
Write-Host 'History dependency: none.'
