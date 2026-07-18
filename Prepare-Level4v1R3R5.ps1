$ErrorActionPreference = 'Stop'
$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
[Console]::InputEncoding = $utf8NoBom
[Console]::OutputEncoding = $utf8NoBom
$OutputEncoding = $utf8NoBom

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$expectedBase = '18152c574a85383cf55630d5b3fe2dd851fbff92'
$sentinels = @{
    '19_CompositionVerifier/CompositionVerifier.cpp' = 'CBA7BDA75AA755BE74F2B2DE3E57B7AFD1E85562BF02305DF24105E05598EB93'
    '47_CanonicalCompositionAuthorityTests/main.cpp' = 'E8A648B21EE275DC5AB8ECC64BCA406DD888C481AA8112A866D0120B89CB70E0'
    'SemanticGpuEngine4_Level4v1_R2.sln' = 'CD784179359450B49FA04F38DEC46252403FB943FF47514302CB243DD86E6E32'
    'docs/level4v1-canonical/R2_COMPOSITION_AUTHORITY.md' = '6615AA37C5D2F2688440C627D288C75E3DE3023099BE698D75817AC603F26F60'
}
foreach ($entry in $sentinels.GetEnumerator()) {
    $path = Join-Path $root $entry.Key
    if (-not (Test-Path -LiteralPath $path)) { throw "R2 base sentinel is missing: $($entry.Key)" }
    $actual = (Get-FileHash -Algorithm SHA256 -LiteralPath $path).Hash.ToUpperInvariant()
    if ($actual -ne $entry.Value) {
        throw "R3-R5 patch expects base $expectedBase. Sentinel mismatch: $($entry.Key)"
    }
}

foreach ($required in @(
    '13_PackageRuntime/PackageRuntime.h','14_D3D12Backend/D3D12Backend.h','14_D3D12Backend/D3D12Backend.cpp',
    '20_CompositionDeviceDomain/20_CompositionDeviceDomain.vcxproj',
    '21_CompositionSharedResources/21_CompositionSharedResources.vcxproj',
    '22_CompositionRuntime/22_CompositionRuntime.vcxproj',
    '23_CompositionRecovery/23_CompositionRecovery.vcxproj',
    '48_CanonicalCompositionRuntimeTests/48_CanonicalCompositionRuntimeTests.vcxproj',
    '49_CanonicalCompositionRecoveryTests/49_CanonicalCompositionRecoveryTests.vcxproj',
    '49A_CanonicalLevel4v1FreezeTests/49A_CanonicalLevel4v1FreezeTests.vcxproj',
    'SemanticGpuEngine4_Level4v1_R3_R5.sln',
    'tests/Verify-Level4v1R3R5Boundaries.ps1','tests/Run-Level4v1R3R5.ps1')) {
    if (-not (Test-Path -LiteralPath (Join-Path $root $required))) { throw "Canonical R3-R5 required file is missing: $required" }
}

& powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File (Join-Path $root 'tests/tools/Update-SourceManifest.ps1')
if ($LASTEXITCODE -ne 0) { throw 'SOURCE_MANIFEST update failed.' }
& powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File (Join-Path $root 'verify_dependencies.ps1')
if ($LASTEXITCODE -ne 0) { throw 'Dependency verification failed.' }
& powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File (Join-Path $root 'tests/Verify-Level4v1R3R5Boundaries.ps1')
if ($LASTEXITCODE -ne 0) { throw 'Canonical R3-R5 boundary verification failed.' }
& powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File (Join-Path $root 'tests/Verify-Level4v1R2Boundaries.ps1')
if ($LASTEXITCODE -ne 0) { throw 'Canonical R2 boundary regression failed.' }
& powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File (Join-Path $root 'tests/tools/Verify-ScriptContracts.ps1')
if ($LASTEXITCODE -ne 0) { throw 'Script contract verification failed.' }
& powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File (Join-Path $root 'tests/tools/Verify-SourceManifest.ps1')
if ($LASTEXITCODE -ne 0) { throw 'SOURCE_MANIFEST verification failed.' }

Write-Host ''
Write-Host "Canonical R3-R5 files prepared for base $expectedBase."
Write-Host 'No repository-history command or hidden source extraction was used.'
Write-Host 'Next: run_sge4_level4v1_r3_r5.bat'
