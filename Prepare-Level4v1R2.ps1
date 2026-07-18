$ErrorActionPreference = 'Stop'

$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
[Console]::InputEncoding = $utf8NoBom
[Console]::OutputEncoding = $utf8NoBom
$OutputEncoding = $utf8NoBom

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$expectedCommit = 'ab76dd59c750249d2459a7e1123699651c466494'

$git = Get-Command git.exe -ErrorAction SilentlyContinue
$gitDirectory = Join-Path $root '.git'
if ($git -and (Test-Path -LiteralPath $gitDirectory)) {
    $head = (& $git.Source -C $root rev-parse HEAD).Trim()
    if ($LASTEXITCODE -ne 0 -or $head -ne $expectedCommit) {
        throw "Canonical R2 must be applied to Git commit $expectedCommit. Current HEAD: $head"
    }
}

foreach ($required in @(
    '16_FrozenCompositionArtifact/FrozenCompositionWriter.cpp',
    '17_CompositionContract/CompositionContract.h',
    '17_CompositionContract/CompositionContract.cpp',
    '18_CompositionPlan/CompositionPlan.h',
    '18_CompositionPlan/CompositionPlan.cpp',
    '19_CompositionVerifier/CompositionVerifier.h',
    '19_CompositionVerifier/CompositionVerifier.cpp',
    '47_CanonicalCompositionAuthorityTests/main.cpp',
    'docs/level4v1-canonical/R2_COMPOSITION_AUTHORITY.md',
    'docs/level4v1-canonical/R2_EVIDENCE_MAP.md',
    'tests/Verify-Level4v1R2Boundaries.ps1',
    'tests/Run-Level4v1R2.ps1',
    'SemanticGpuEngine4_Level4v1_R2.sln')) {
    if (-not (Test-Path -LiteralPath (Join-Path $root $required))) {
        throw "Canonical R2 required file is missing: $required"
    }
}

& powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass `
    -File (Join-Path $root 'tests/tools/Update-SourceManifest.ps1')
if ($LASTEXITCODE -ne 0) { throw 'SOURCE_MANIFEST update failed.' }

& powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass `
    -File (Join-Path $root 'verify_dependencies.ps1')
if ($LASTEXITCODE -ne 0) { throw 'Dependency verification failed.' }

& powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass `
    -File (Join-Path $root 'tests/Verify-Level4v1R2Boundaries.ps1')
if ($LASTEXITCODE -ne 0) { throw 'Canonical R2 boundary verification failed.' }

& powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass `
    -File (Join-Path $root 'tests/tools/Verify-ScriptContracts.ps1')
if ($LASTEXITCODE -ne 0) { throw 'Script contract verification failed.' }

& powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass `
    -File (Join-Path $root 'tests/tools/Verify-SourceManifest.ps1')
if ($LASTEXITCODE -ne 0) { throw 'SOURCE_MANIFEST verification failed.' }

Write-Host ''
Write-Host "Canonical R2 files prepared for base commit $expectedCommit."
Write-Host 'Next: run_sge4_level4v1_r2.bat'
