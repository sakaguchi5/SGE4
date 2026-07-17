$ErrorActionPreference = 'Stop'

$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
[Console]::InputEncoding = $utf8NoBom
[Console]::OutputEncoding = $utf8NoBom
$OutputEncoding = $utf8NoBom

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$expectedCommit = '547ed45695e8f6b15f0e0d5d5be5992d20bb8134'

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
        throw "L4V1-F4-F6 must be applied to Git commit $expectedCommit. Current HEAD: $head"
    }
}
else {
    Assert-FileSha256 '16A_Schema18CompositionContract/Schema18CompositionContract.cpp' '0c44eb8c7f7ec78956e980db89b657babc681e04748aa7be4ae7d935bb333579'
    Assert-FileSha256 '16A_Schema18CompositionContract/Schema18CompositionContract.h' '56093b4bc4e5a79e591b5c2b6dabea3f5c365d1e6dfe8ca428e376d1d25fef1d'
    Assert-FileSha256 'SemanticGpuEngine4_Level4v1_F3.sln' 'dbf312d5f7091bafeea3a1756dddf870159a7755adb7653785289aa7a08426b0'
}

$requiredPatchFiles = @(
    '17A_Schema18ResourceFlowPlan/Schema18ResourceFlowPlan.h',
    '17A_Schema18ResourceFlowPlan/Schema18ResourceFlowPlan.cpp',
    '17A_Schema18ResourceFlowPlan/17A_Schema18ResourceFlowPlan.vcxproj',
    '18A_Schema18ResourceFlowVerifier/Schema18ResourceFlowVerifier.h',
    '18A_Schema18ResourceFlowVerifier/Schema18ResourceFlowVerifier.cpp',
    '18A_Schema18ResourceFlowVerifier/18A_Schema18ResourceFlowVerifier.vcxproj',
    '19A_Schema18CompositionLinker/Schema18CompositionLinker.h',
    '19A_Schema18CompositionLinker/Schema18CompositionLinker.cpp',
    '19A_Schema18CompositionLinker/19A_Schema18CompositionLinker.vcxproj',
    '46D_Schema18ResourceFlowPlanningTests/main.cpp',
    '46D_Schema18ResourceFlowPlanningTests/TestFixture.h',
    '46D_Schema18ResourceFlowPlanningTests/46D_Schema18ResourceFlowPlanningTests.vcxproj',
    '46E_Schema18ResourceFlowAuthorityTests/main.cpp',
    '46E_Schema18ResourceFlowAuthorityTests/46E_Schema18ResourceFlowAuthorityTests.vcxproj',
    '46F_Schema18FrozenCompositionTests/main.cpp',
    '46F_Schema18FrozenCompositionTests/46F_Schema18FrozenCompositionTests.vcxproj',
    'SemanticGpuEngine4_Level4v1_F4_F6.sln',
    'docs/L4V1-F4-F6_VERIFIED_RESOURCE_FLOW_AND_FROZEN_COMPOSITION_JA.md',
    'prepare_sge4_level4v1_f4_f6.bat',
    'run_sge4_level4v1_f4_f6.bat',
    'tests/Run-Level4v1F4F6.ps1',
    'tests/Verify-Level4v1F4F6Boundaries.ps1')
foreach ($relative in $requiredPatchFiles) {
    if (-not (Test-Path -LiteralPath (Join-Path $root $relative))) {
        throw "L4V1-F4-F6 patch file is missing: $relative"
    }
}

& powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass `
    -File (Join-Path $root 'tests/tools/Update-SourceManifest.ps1')
if ($LASTEXITCODE -ne 0) { throw 'SOURCE_MANIFEST update failed.' }

& powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass `
    -File (Join-Path $root 'verify_dependencies.ps1')
if ($LASTEXITCODE -ne 0) { throw 'Dependency verification failed.' }

foreach ($gate in @('Verify-Level4v1F1Boundaries.ps1', 'Verify-Level4v1F2Boundaries.ps1',
    'Verify-Level4v1F3Boundaries.ps1', 'Verify-Level4v1F4F6Boundaries.ps1')) {
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
Write-Host "L4V1-F4-F6 files prepared for commit $expectedCommit."
Write-Host 'Next: run_sge4_level4v1_f4_f6.bat'
