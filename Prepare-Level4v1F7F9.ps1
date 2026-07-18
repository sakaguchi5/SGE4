$ErrorActionPreference = 'Stop'

$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
[Console]::InputEncoding = $utf8NoBom
[Console]::OutputEncoding = $utf8NoBom
$OutputEncoding = $utf8NoBom

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$expectedCommit = 'c5fda52924da3de9ee552036908eb4e8a74aaebf'

function Assert-FileSha256([string]$RelativePath, [string]$Expected) {
    $path = Join-Path $root $RelativePath
    if (-not (Test-Path -LiteralPath $path)) { throw "Required base file is missing: $RelativePath" }
    $actual = (Get-FileHash -Algorithm SHA256 -LiteralPath $path).Hash.ToLowerInvariant()
    if ($actual -ne $Expected) {
        throw "Base file differs from commit ${expectedCommit}: $RelativePath`nExpected: $Expected`nActual:   $actual"
    }
}

$git = Get-Command git.exe -ErrorAction SilentlyContinue
$gitDirectory = Join-Path $root '.git'
if ($git -and (Test-Path -LiteralPath $gitDirectory)) {
    $head = (& $git.Source -C $root rev-parse HEAD).Trim()
    if ($LASTEXITCODE -ne 0 -or $head -ne $expectedCommit) {
        throw "L4V1-F7-F9 must be applied to Git commit $expectedCommit. Current HEAD: $head"
    }
}
else {
    Assert-FileSha256 '19A_Schema18CompositionLinker/Schema18CompositionLinker.cpp' '09952c82b9f455b327a662a91598eb4d809faa3d4827bd56d9bea276e22e4f35'
    Assert-FileSha256 '19A_Schema18CompositionLinker/Schema18CompositionLinker.h' 'c2a93724fd51b627b408a820880613af4ac47ef0222415f5b97d936295ef7302'
    Assert-FileSha256 'SemanticGpuEngine4_Level4v1_F4_F6.sln' '5ae9907faa7807802c9c29996186afe439e7928137b43a6a15bdf6252f82eb88'
}

$requiredPatchFiles = @(
    '14_D3D12Backend/D3D12Backend.h',
    'patches/D3D12Backend_F7F9_Transition.cpp.txt',
    'Apply-Level4v1F7F9BackendPatch.ps1',
    '13A_Schema18SharedDeviceDomain/Schema18SharedDeviceDomain.h',
    '13A_Schema18SharedDeviceDomain/Schema18SharedDeviceDomain.cpp',
    '13A_Schema18SharedDeviceDomain/13A_Schema18SharedDeviceDomain.vcxproj',
    '14A_Schema18SharedResources/Schema18SharedResources.h',
    '14A_Schema18SharedResources/Schema18SharedResources.cpp',
    '14A_Schema18SharedResources/14A_Schema18SharedResources.vcxproj',
    '29A_Schema18StaticDagRuntime/Schema18StaticDagRuntime.h',
    '29A_Schema18StaticDagRuntime/Schema18StaticDagRuntime.cpp',
    '29A_Schema18StaticDagRuntime/29A_Schema18StaticDagRuntime.vcxproj',
    '46G_Schema18SharedDeviceDomainTests/RuntimeFixture.h',
    '46G_Schema18SharedDeviceDomainTests/main.cpp',
    '46G_Schema18SharedDeviceDomainTests/46G_Schema18SharedDeviceDomainTests.vcxproj',
    '46H_Schema18SharedResourceTests/main.cpp',
    '46H_Schema18SharedResourceTests/46H_Schema18SharedResourceTests.vcxproj',
    '46I_Schema18StaticDagRuntimeTests/main.cpp',
    '46I_Schema18StaticDagRuntimeTests/46I_Schema18StaticDagRuntimeTests.vcxproj',
    'SemanticGpuEngine4_Level4v1_F7_F9.sln',
    'docs/L4V1-F7-F9_SHARED_DEVICE_DOMAIN_STATIC_DAG_RUNTIME_JA.md',
    'prepare_sge4_level4v1_f7_f9.bat',
    'run_sge4_level4v1_f7_f9.bat',
    'tests/Run-Level4v1F7F9.ps1',
    'tests/Verify-Level4v1F7F9Boundaries.ps1')
foreach ($relative in $requiredPatchFiles) {
    if (-not (Test-Path -LiteralPath (Join-Path $root $relative))) {
        throw "L4V1-F7-F9 patch file is missing: $relative"
    }
}

& powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass `
    -File (Join-Path $root 'Apply-Level4v1F7F9BackendPatch.ps1')
if ($LASTEXITCODE -ne 0) { throw 'F7-F9 D3D12Backend patch application failed.' }

& powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass `
    -File (Join-Path $root 'tests/tools/Update-SourceManifest.ps1')
if ($LASTEXITCODE -ne 0) { throw 'SOURCE_MANIFEST update failed.' }

& powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass `
    -File (Join-Path $root 'verify_dependencies.ps1')
if ($LASTEXITCODE -ne 0) { throw 'Dependency verification failed.' }

foreach ($gate in @('Verify-Level4v1F1Boundaries.ps1', 'Verify-Level4v1F2Boundaries.ps1',
    'Verify-Level4v1F3Boundaries.ps1', 'Verify-Level4v1F4F6Boundaries.ps1',
    'Verify-Level4v1F7F9Boundaries.ps1')) {
    & powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass `
        -File (Join-Path $root "tests/$gate")
    if ($LASTEXITCODE -ne 0) { throw "Boundary verification failed: $gate" }
}

& powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass `
    -File (Join-Path $root 'tests/tools/Verify-ScriptContracts.ps1')
if ($LASTEXITCODE -ne 0) { throw 'Script contract verification failed.' }

& powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass `
    -File (Join-Path $root 'tests/tools/Verify-SourceManifest.ps1')
if ($LASTEXITCODE -ne 0) { throw 'SOURCE_MANIFEST verification failed.' }

Write-Host ''
Write-Host "L4V1-F7-F9 files prepared for commit $expectedCommit."
Write-Host 'Next: run_sge4_level4v1_f7_f9.bat'
