$ErrorActionPreference = 'Stop'

$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
[Console]::InputEncoding = $utf8NoBom
[Console]::OutputEncoding = $utf8NoBom
$OutputEncoding = $utf8NoBom

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$expectedCommit = 'e3730feb90be8690e1cc4e9d937c7c3b11899f89'

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
        throw "L4V1-F1 must be applied to Git commit $expectedCommit. Current HEAD: $head"
    }
}
else {
    Assert-FileSha256 '04_SemanticAnalysis/SemanticAnalysis.cpp' '3d5d016da2af25565c532eed2b13e266fc398e2128b2e4321025d7773d72e622'
    Assert-FileSha256 '10_D3D12PackageSchema/D3D12Encoding.cpp' '2641dcdacfc4b6a707f06d2acefdf8fca21f65b58412e60987fd036d73b1d998'
    Assert-FileSha256 '10_D3D12PackageSchema/D3D12Schema.h' 'b9c772e0ca84e9d47edd72f2a4207fa72668c923819eb984b53865642edd93f5'
    Assert-FileSha256 '12_SGE4Compiler/SGE4Compiler.cpp' 'cddd428342f21fe7f3b69e6ea8dad76291f1c8c62972be426ef6f1c355d8afd0'
    foreach ($required in @(
        '16_CompositionContract/CompositionContract.cpp',
        '17_LinkPlanModel/LinkPlanModel.cpp',
        '18_LinkPlanVerifier/LinkPlanVerifier.cpp',
        '19_PackageLinker/FrozenComposition.cpp',
        '29_CompositionRuntime/CompositionRuntime.cpp')) {
        if (-not (Test-Path -LiteralPath (Join-Path $root $required))) {
            throw "The extracted tree is not commit $expectedCommit; Prototype-A file is missing: $required"
        }
    }
}

$requiredPatchFiles = @(
    '09_FrozenPackageCore/PackageFormat.h',
    '09_FrozenPackageCore/PackageReader.cpp',
    '10A_D3D12CompositionSchema18/D3D12CompositionSchema18.h',
    '10A_D3D12CompositionSchema18/D3D12CompositionEncoding18.cpp',
    '12A_CompositionLeafCompiler/CompositionLeafCompiler.cpp',
    '46A_Schema18CompositionInterfaceTests/main.cpp',
    'tests/Run-Level4v1F1.ps1',
    'tests/Verify-Level4v1F1Boundaries.ps1')
foreach ($relative in $requiredPatchFiles) {
    if (-not (Test-Path -LiteralPath (Join-Path $root $relative))) {
        throw "L4V1-F1 patch file is missing: $relative"
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
    -File (Join-Path $root 'tests/tools/Verify-ScriptContracts.ps1')
if ($LASTEXITCODE -ne 0) { throw 'Script contract verification failed.' }

& powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass `
    -File (Join-Path $root 'tests/tools/Verify-SourceManifest.ps1')
if ($LASTEXITCODE -ne 0) { throw 'SOURCE_MANIFEST verification failed.' }

Write-Host ''
Write-Host 'L4V1-F1 files prepared for commit e3730feb90be8690e1cc4e9d937c7c3b11899f89.'
Write-Host 'Next: run_sge4_level4v1_f1.bat'
