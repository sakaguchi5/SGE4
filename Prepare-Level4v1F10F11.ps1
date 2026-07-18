$ErrorActionPreference = 'Stop'

$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
[Console]::InputEncoding = $utf8NoBom
[Console]::OutputEncoding = $utf8NoBom
$OutputEncoding = $utf8NoBom

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$expectedCommit = '54b2e54d6da96cb030dfe3ad626885b6c1f826f7'

$git = Get-Command git.exe -ErrorAction SilentlyContinue
$gitDirectory = Join-Path $root '.git'
if ($git -and (Test-Path -LiteralPath $gitDirectory)) {
    $head = (& $git.Source -C $root rev-parse HEAD).Trim()
    if ($LASTEXITCODE -ne 0 -or $head -ne $expectedCommit) {
        throw "L4V1-F10-F11 must be applied to Git commit $expectedCommit. Current HEAD: $head"
    }
}
else {
    throw 'Git metadata is required because F10-F11 intentionally patches the DeviceDomain recovery implementation.'
}

$requiredPatchFiles = @(
    'Apply-Level4v1F10F11BackendPatch.ps1',
    '13A_Schema18SharedDeviceDomain/Schema18SharedDeviceDomain.h',
    '13A_Schema18SharedDeviceDomain/Schema18SharedDeviceDomain.cpp',
    '29A_Schema18StaticDagRuntime/Schema18StaticDagRuntime.h',
    '29A_Schema18StaticDagRuntime/Schema18StaticDagRuntime.cpp',
    '29B_Schema18CompositionRecovery/Schema18CompositionRecovery.h',
    '29B_Schema18CompositionRecovery/Schema18CompositionRecovery.cpp',
    '29B_Schema18CompositionRecovery/29B_Schema18CompositionRecovery.vcxproj',
    '46J_Schema18CompositionRecoveryTests/main.cpp',
    '46J_Schema18CompositionRecoveryTests/46J_Schema18CompositionRecoveryTests.vcxproj',
    '46K_Schema18FinalQualificationTests/main.cpp',
    '46K_Schema18FinalQualificationTests/46K_Schema18FinalQualificationTests.vcxproj',
    'SemanticGpuEngine4_Level4v1_F10_F11.sln',
    'docs/L4V1-F10-F11_COMPOSITION_RECOVERY_FINAL_QUALIFICATION_JA.md',
    'prepare_sge4_level4v1_f10_f11.bat',
    'run_sge4_level4v1_f10_f11.bat',
    'run_sge4_level4v1_f10_f11_quick.bat',
    'tests/Run-Level4v1F10F11.ps1',
    'tests/Verify-Level4v1F10F11Boundaries.ps1')
foreach ($relative in $requiredPatchFiles) {
    if (-not (Test-Path -LiteralPath (Join-Path $root $relative))) {
        throw "L4V1-F10-F11 patch file is missing: $relative"
    }
}

& powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass `
    -File (Join-Path $root 'Apply-Level4v1F10F11BackendPatch.ps1')
if ($LASTEXITCODE -ne 0) { throw 'F10-F11 D3D12Backend recovery patch application failed.' }

& powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass `
    -File (Join-Path $root 'tests/tools/Update-SourceManifest.ps1')
if ($LASTEXITCODE -ne 0) { throw 'SOURCE_MANIFEST update failed.' }

& powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass `
    -File (Join-Path $root 'verify_dependencies.ps1')
if ($LASTEXITCODE -ne 0) { throw 'Dependency verification failed.' }

foreach ($gate in @('Verify-Level4v1F1Boundaries.ps1', 'Verify-Level4v1F2Boundaries.ps1',
    'Verify-Level4v1F3Boundaries.ps1', 'Verify-Level4v1F4F6Boundaries.ps1',
    'Verify-Level4v1F7F9Boundaries.ps1', 'Verify-Level4v1F10F11Boundaries.ps1')) {
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
Write-Host "L4V1-F10-F11 files prepared for commit $expectedCommit."
Write-Host 'Next: run_sge4_level4v1_f10_f11.bat'
