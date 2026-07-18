$ErrorActionPreference = 'Stop'

$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
[Console]::InputEncoding = $utf8NoBom
[Console]::OutputEncoding = $utf8NoBom
$OutputEncoding = $utf8NoBom

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$expectedCommit = 'bd686bde95eb357c87003fa807eac9438abb413b'

$git = Get-Command git.exe -ErrorAction SilentlyContinue
$gitDirectory = Join-Path $root '.git'
if ($git -and (Test-Path -LiteralPath $gitDirectory)) {
    $head = (& $git.Source -C $root rev-parse HEAD).Trim()
    if ($LASTEXITCODE -ne 0 -or $head -ne $expectedCommit) {
        throw "Canonical R1 must be applied to Git commit $expectedCommit. Current HEAD: $head"
    }
}

foreach ($required in @(
    'docs/level4v1-canonical/RECONSTRUCTION_POLICY.md',
    'docs/level4v1-canonical/PROOF_LEDGER.md',
    'docs/level4v1-canonical/COMPLETION_SPECIFICATION.md',
    'docs/level4v1-canonical/QUALIFICATION_MATRIX.md',
    'docs/level4v1-canonical/R1_EVIDENCE_MAP.md',
    '16_FrozenCompositionArtifact/FrozenCompositionFormat.h',
    '16_FrozenCompositionArtifact/FrozenCompositionWriter.cpp',
    '16_FrozenCompositionArtifact/FrozenCompositionReader.cpp',
    '46_CanonicalCompositionArtifactTests/main.cpp',
    'tests/Verify-Level4v1R1Boundaries.ps1',
    'tests/Run-Level4v1R1.ps1')) {
    if (-not (Test-Path -LiteralPath (Join-Path $root $required))) {
        throw "Canonical R1 required file is missing: $required"
    }
}

& powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass `
    -File (Join-Path $root 'tests/tools/Update-SourceManifest.ps1')
if ($LASTEXITCODE -ne 0) { throw 'SOURCE_MANIFEST update failed.' }

& powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass `
    -File (Join-Path $root 'verify_dependencies.ps1')
if ($LASTEXITCODE -ne 0) { throw 'Dependency verification failed.' }

& powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass `
    -File (Join-Path $root 'tests/Verify-Level4v1R1Boundaries.ps1')
if ($LASTEXITCODE -ne 0) { throw 'Canonical R1 boundary verification failed.' }

& powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass `
    -File (Join-Path $root 'tests/tools/Verify-ScriptContracts.ps1')
if ($LASTEXITCODE -ne 0) { throw 'Script contract verification failed.' }

& powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass `
    -File (Join-Path $root 'tests/tools/Verify-SourceManifest.ps1')
if ($LASTEXITCODE -ne 0) { throw 'SOURCE_MANIFEST verification failed.' }

Write-Host ''
Write-Host "Canonical R1 files prepared for base commit $expectedCommit."
Write-Host 'Next: run_sge4_level4v1_r1.bat'
