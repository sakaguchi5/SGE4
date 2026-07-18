$ErrorActionPreference = 'Stop'
$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
[Console]::InputEncoding = $utf8NoBom
[Console]::OutputEncoding = $utf8NoBom
$OutputEncoding = $utf8NoBom

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$expectedBase = '783700cbd49e384e4250363c2e9ed380bdca34fb'
$sentinels = @{
    '23_CompositionRecovery/CompositionRecovery.cpp' = 'E33A53FEE2B8AEBF00942252BD473B4446FC136F4D7A5964FC65385584389AA1'
    '49A_CanonicalLevel4v1FreezeTests/main.cpp' = 'DA03A7F4E3976761F2615552E54D79C211CB8C536D9AF87FA99184E473591777'
    'docs/level4v1-canonical/R3_R5_RUNTIME_RECOVERY_FINAL_QUALIFICATION.md' = '3D83ADEABB73B0E496FBBB78EA9C646ABAD6F98C36FCFFCA36DA23A53E829A96'
}
foreach ($entry in $sentinels.GetEnumerator()) {
    $path = Join-Path $root $entry.Key
    if (-not (Test-Path -LiteralPath $path)) { throw "Final Integration base sentinel is missing: $($entry.Key)" }
    $actual = (Get-FileHash -Algorithm SHA256 -LiteralPath $path).Hash.ToUpperInvariant()
    if ($actual -ne $entry.Value) { throw "Final Integration expects base $expectedBase. Sentinel mismatch: $($entry.Key)" }
}

foreach ($required in @(
    'SemanticGpuEngine4.sln',
    'tests/Run-Level4v1R1.ps1','tests/Run-Level4v1R2.ps1','tests/Run-Level4v1R3R5.ps1',
    'tests/Verify-Level4v1FinalIntegration.ps1','tests/Run-Level4v1FinalIntegration.ps1',
    'prepare_sge4_level4v1_final_integration.bat','run_sge4_level4v1_final.bat',
    'docs/level4v1-canonical/FINAL_INTEGRATION_FREEZE.md')) {
    if (-not (Test-Path -LiteralPath (Join-Path $root $required))) { throw "Final Integration required file is missing: $required" }
}

& powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File (Join-Path $root 'tests/tools/Update-SourceManifest.ps1')
if ($LASTEXITCODE -ne 0) { throw 'SOURCE_MANIFEST update failed.' }
& powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File (Join-Path $root 'tests/Verify-Level4v1FinalIntegration.ps1')
if ($LASTEXITCODE -ne 0) { throw 'Final Integration boundary verification failed.' }
& powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File (Join-Path $root 'verify_dependencies.ps1')
if ($LASTEXITCODE -ne 0) { throw 'Dependency verification failed.' }
& powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File (Join-Path $root 'tests/Verify-Level4v1R3R5Boundaries.ps1')
if ($LASTEXITCODE -ne 0) { throw 'R3-R5 boundary regression failed.' }
& powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File (Join-Path $root 'tests/Verify-Level4v1R2Boundaries.ps1')
if ($LASTEXITCODE -ne 0) { throw 'R2 boundary regression failed.' }
& powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File (Join-Path $root 'tests/tools/Verify-ScriptContracts.ps1')
if ($LASTEXITCODE -ne 0) { throw 'Script contract verification failed.' }
& powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File (Join-Path $root 'tests/tools/Verify-SourceManifest.ps1')
if ($LASTEXITCODE -ne 0) { throw 'SOURCE_MANIFEST verification failed.' }

Write-Host ''
Write-Host "Level 4 v1 Final Integration files prepared for base $expectedBase."
Write-Host 'No Git command, repository-history lookup, or hidden source extraction was used.'
Write-Host 'Next: run_sge4_level4v1_final.bat'
