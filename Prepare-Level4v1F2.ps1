$ErrorActionPreference = 'Stop'

$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
[Console]::InputEncoding = $utf8NoBom
[Console]::OutputEncoding = $utf8NoBom
$OutputEncoding = $utf8NoBom

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$expectedCommit = '94c8cb8915437e25e3e0d6a3ae3c5277940114a0'

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
        throw "L4V1-F2 must be applied to Git commit $expectedCommit. Current HEAD: $head"
    }
}
else {
    Assert-FileSha256 '10A_D3D12CompositionSchema18/D3D12CompositionSchema18.h' '893bc4043907ede70532180dae6ed5788edad2e69305d601d1932c219168373d'
    Assert-FileSha256 '12A_CompositionLeafCompiler/CompositionLeafCompiler.cpp' 'edcbe3ea9966a6609c118c62d1f27328d3f8251be3c8b45d283ab15d47502b45'
    Assert-FileSha256 '46A_Schema18CompositionInterfaceTests/main.cpp' 'ecad1d5529ef1766671060209557c1ab155c3c651579da2180080beb77a0f017'
    Assert-FileSha256 '44_CanonicalFreezeTests/main.cpp' '08a836494cdc9e270fb97d9b6968e305b3ebe526605ada560cfea100754bab24'
    Assert-FileSha256 'tests/tools/Verify-ScriptContracts.ps1' '377d806f4768f6c65ad45e1439f1ac19fddc6ee37d10deecbf572d5bbdf19091'
}

$requiredPatchFiles = @(
    '27A_Schema18LeafCorpus/Schema18LeafCorpus.h',
    '27A_Schema18LeafCorpus/Schema18LeafCorpus.cpp',
    '27A_Schema18LeafCorpus/27A_Schema18LeafCorpus.vcxproj',
    '46B_Schema18LeafCorpusTests/main.cpp',
    '46B_Schema18LeafCorpusTests/46B_Schema18LeafCorpusTests.vcxproj',
    'SemanticGpuEngine4_Level4v1_F2.sln',
    'docs/L4V1-F2_SCHEMA18_LEAF_CORPUS_JA.md',
    'prepare_sge4_level4v1_f2.bat',
    'run_sge4_level4v1_f2.bat',
    'tests/Run-Level4v1F2.ps1',
    'tests/Verify-Level4v1F2Boundaries.ps1')
foreach ($relative in $requiredPatchFiles) {
    if (-not (Test-Path -LiteralPath (Join-Path $root $relative))) {
        throw "L4V1-F2 patch file is missing: $relative"
    }
}

& powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass `
    -File (Join-Path $root 'tests/tools/Update-SourceManifest.ps1')
if ($LASTEXITCODE -ne 0) { throw 'SOURCE_MANIFEST update failed.' }

& powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass `
    -File (Join-Path $root 'verify_dependencies.ps1')
if ($LASTEXITCODE -ne 0) { throw 'Dependency verification failed.' }

& powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass `
    -File (Join-Path $root 'tests/Verify-Level4v1F1Boundaries.ps1')
if ($LASTEXITCODE -ne 0) { throw 'L4V1-F1 boundary verification failed.' }

& powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass `
    -File (Join-Path $root 'tests/Verify-Level4v1F2Boundaries.ps1')
if ($LASTEXITCODE -ne 0) { throw 'L4V1-F2 boundary verification failed.' }

& powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass `
    -File (Join-Path $root 'tests/tools/Verify-ScriptContracts.ps1')
if ($LASTEXITCODE -ne 0) { throw 'Script contract verification failed.' }

& powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass `
    -File (Join-Path $root 'tests/tools/Verify-SourceManifest.ps1')
if ($LASTEXITCODE -ne 0) { throw 'SOURCE_MANIFEST verification failed.' }

Write-Host ''
Write-Host "L4V1-F2 files prepared for commit $expectedCommit."
Write-Host 'Next: run_sge4_level4v1_f2.bat'
