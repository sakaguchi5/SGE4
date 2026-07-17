$ErrorActionPreference = 'Stop'

$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
[Console]::InputEncoding = $utf8NoBom
[Console]::OutputEncoding = $utf8NoBom
$OutputEncoding = $utf8NoBom

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$expectedCommit = 'eea6bfb3770f8879caf5e1d4000369a64ba6baf4'

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
        throw "L4V1-F3 must be applied to Git commit $expectedCommit. Current HEAD: $head"
    }
}
else {
    Assert-FileSha256 '10A_D3D12CompositionSchema18/D3D12CompositionSchema18.h' '893bc4043907ede70532180dae6ed5788edad2e69305d601d1932c219168373d'
    Assert-FileSha256 '12A_CompositionLeafCompiler/CompositionLeafCompiler.cpp' 'edcbe3ea9966a6609c118c62d1f27328d3f8251be3c8b45d283ab15d47502b45'
    Assert-FileSha256 '27A_Schema18LeafCorpus/Schema18LeafCorpus.h' '54a05d28dacb0f8715c3cce70d680123a963794cab5fb9d05417204606cf1225'
    Assert-FileSha256 '46B_Schema18LeafCorpusTests/main.cpp' '1fe3ef4af07d85a091028eddcb6130e2738fbeae2daa4bc6fec0a5ab3fb17d66'
    Assert-FileSha256 'SemanticGpuEngine4_Level4v1_F2.sln' 'b4dfa1876c8060c0f486f286b5595fe8e7a311d942758e16a89f3c42e3e840a9'
}

$requiredPatchFiles = @(
    '16A_Schema18CompositionContract/Schema18CompositionContract.h',
    '16A_Schema18CompositionContract/Schema18CompositionContract.cpp',
    '16A_Schema18CompositionContract/16A_Schema18CompositionContract.vcxproj',
    '46C_Schema18CompositionContractTests/main.cpp',
    '46C_Schema18CompositionContractTests/46C_Schema18CompositionContractTests.vcxproj',
    'SemanticGpuEngine4_Level4v1_F3.sln',
    'docs/L4V1-F3_SCHEMA18_COMPOSITION_CONTRACT_JA.md',
    'prepare_sge4_level4v1_f3.bat',
    'run_sge4_level4v1_f3.bat',
    'tests/Run-Level4v1F3.ps1',
    'tests/Verify-Level4v1F3Boundaries.ps1')
foreach ($relative in $requiredPatchFiles) {
    if (-not (Test-Path -LiteralPath (Join-Path $root $relative))) {
        throw "L4V1-F3 patch file is missing: $relative"
    }
}

& powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass `
    -File (Join-Path $root 'tests/tools/Update-SourceManifest.ps1')
if ($LASTEXITCODE -ne 0) { throw 'SOURCE_MANIFEST update failed.' }

& powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass `
    -File (Join-Path $root 'verify_dependencies.ps1')
if ($LASTEXITCODE -ne 0) { throw 'Dependency verification failed.' }

foreach ($gate in @('Verify-Level4v1F1Boundaries.ps1', 'Verify-Level4v1F2Boundaries.ps1',
    'Verify-Level4v1F3Boundaries.ps1')) {
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
Write-Host "L4V1-F3 files prepared for commit $expectedCommit."
Write-Host 'Next: run_sge4_level4v1_f3.bat'
