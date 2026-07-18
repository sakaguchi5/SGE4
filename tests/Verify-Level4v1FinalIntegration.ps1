$ErrorActionPreference = 'Stop'

$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
[Console]::InputEncoding = $utf8NoBom
[Console]::OutputEncoding = $utf8NoBom
$OutputEncoding = $utf8NoBom

$testsRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$root = Split-Path -Parent $testsRoot
$solutionPath = Join-Path $root 'SemanticGpuEngine4.sln'
if (-not (Test-Path -LiteralPath $solutionPath)) { throw 'Main solution is missing: SemanticGpuEngine4.sln' }
$solutionText = Get-Content -Raw -LiteralPath $solutionPath -Encoding UTF8
$projectPattern = '(?m)^Project\("\{[^\"]+\}"\) = "([^"]+)", "([^"]+)", "(\{[^\"]+\})"\r?$'
$matches = [regex]::Matches($solutionText, $projectPattern)
if ($matches.Count -eq 0) { throw 'Main solution contains no projects.' }

$entries = foreach ($match in $matches) {
    [pscustomobject]@{
        Name = $match.Groups[1].Value
        Path = $match.Groups[2].Value.Replace('\','/')
        Guid = $match.Groups[3].Value.ToUpperInvariant()
    }
}
foreach ($property in @('Name','Path','Guid')) {
    $duplicates = @($entries | Group-Object -Property $property | Where-Object Count -gt 1)
    if ($duplicates.Count -gt 0) {
        throw "Main solution contains duplicate $property values: $((@($duplicates.Name) -join ', '))"
    }
}

$actualProjects = @(
    Get-ChildItem -Path $root -Recurse -File -Filter *.vcxproj |
        Where-Object {
            $_.FullName -notlike (Join-Path $root 'build\*') -and
            $_.FullName -notlike (Join-Path $root '.vs\*') -and
            $_.FullName -notlike (Join-Path $root '.git\*')
        } |
        ForEach-Object { $_.FullName.Substring($root.Length).TrimStart('\','/').Replace('\','/') } |
        Sort-Object
)
$solutionProjects = @($entries.Path | Sort-Object)
$missingFromSolution = @($actualProjects | Where-Object { $solutionProjects -notcontains $_ })
$missingFromTree = @($solutionProjects | Where-Object { $actualProjects -notcontains $_ })
if ($missingFromSolution.Count -gt 0) { throw "Projects missing from main solution: $($missingFromSolution -join ', ')" }
if ($missingFromTree.Count -gt 0) { throw "Main solution references missing projects: $($missingFromTree -join ', ')" }
if ($actualProjects.Count -ne $solutionProjects.Count) { throw 'Main solution/project inventory count mismatch.' }

$canonicalProjects = @(
    '16_FrozenCompositionArtifact','17_CompositionContract','18_CompositionPlan','19_CompositionVerifier',
    '20_CompositionDeviceDomain','21_CompositionSharedResources','22_CompositionRuntime','23_CompositionRecovery',
    '46_CanonicalCompositionArtifactTests','47_CanonicalCompositionAuthorityTests',
    '48_CanonicalCompositionRuntimeTests','49_CanonicalCompositionRecoveryTests','49A_CanonicalLevel4v1FreezeTests'
)
foreach ($name in $canonicalProjects) {
    $entry = @($entries | Where-Object Name -eq $name)
    if ($entry.Count -ne 1) { throw "Canonical project is not integrated exactly once: $name" }
}

foreach ($entry in $entries) {
    foreach ($suffix in @(
        'Debug|x64.ActiveCfg = Debug|x64',
        'Debug|x64.Build.0 = Debug|x64',
        'Release|x64.ActiveCfg = Release|x64',
        'Release|x64.Build.0 = Release|x64')) {
        $needle = "$($entry.Guid).$suffix"
        $count = ([regex]::Matches($solutionText, [regex]::Escape($needle))).Count
        if ($count -ne 1) { throw "Main solution configuration mapping is missing or duplicated: $needle" }
    }
}

$canonicalRunners = @(
    'tests/Run-Level4v1R1.ps1',
    'tests/Run-Level4v1R2.ps1',
    'tests/Run-Level4v1R3R5.ps1',
    'tests/Verify-Level4v1R1Boundaries.ps1',
    'tests/Verify-Level4v1R2Boundaries.ps1',
    'tests/Verify-Level4v1R3R5Boundaries.ps1'
)
foreach ($relative in $canonicalRunners) {
    $path = Join-Path $root ($relative.Replace('/','\'))
    if (-not (Test-Path -LiteralPath $path)) { throw "Canonical runner is missing: $relative" }
    $text = Get-Content -Raw -LiteralPath $path -Encoding UTF8
    if ($text -notmatch "SemanticGpuEngine4\.sln") { throw "Canonical runner does not use the main solution: $relative" }
    if ($text -match 'SemanticGpuEngine4_Level4v1_') { throw "Canonical runner still uses a stage-specific solution: $relative" }
}

foreach ($relative in @(
    'tests/Run-Level4v1FinalIntegration.ps1',
    'tests/Verify-Level4v1FinalIntegration.ps1')) {
    $path = Join-Path $root ($relative.Replace('/','\'))
    if (-not (Test-Path -LiteralPath $path)) { throw "Final Integration script is missing: $relative" }
    $text = Get-Content -Raw -LiteralPath $path -Encoding UTF8
    if ($text -match '(?im)^\s*git(?:\.exe)?\s') { throw "Final Integration script must not execute Git: $relative" }
}

$obsoleteFiles = @(
    'ARTIFACT_INFO.txt',
    'DELETIONS.txt',
    'FINAL_INTEGRATION_PATCH_FILE_LIST.txt',
    'FINAL_INTEGRATION_PATCH_INFO.txt',
    'FINAL_INTEGRATION_VALIDATION_NOTES.txt',
    'PATCH_FILE_LIST.txt',
    'PATCH_INFO.txt',
    'R3_R5_PATCH_FILE_LIST.txt',
    'R3_R5_PATCH_INFO.txt',
    'R3_R5_VALIDATION_NOTES.txt',
    'VALIDATION_NOTES.txt',
    'Prepare-Level4v1FinalIntegration.ps1',
    'Prepare-Level4v1R1.ps1',
    'Prepare-Level4v1R2.ps1',
    'Prepare-Level4v1R3R5.ps1',
    'prepare_sge4_level4v1_final_integration.bat',
    'prepare_sge4_level4v1_r1.bat',
    'prepare_sge4_level4v1_r2.bat',
    'prepare_sge4_level4v1_r3_r5.bat',
    'SemanticGpuEngine4_Level4v1_R1.sln',
    'SemanticGpuEngine4_Level4v1_R2.sln',
    'SemanticGpuEngine4_Level4v1_R3_R5.sln'
)
foreach ($relative in $obsoleteFiles) {
    if (Test-Path -LiteralPath (Join-Path $root $relative)) {
        throw "Obsolete Level 4 v1 delivery or stage file remains in the canonical repository: $relative"
    }
}

Write-Host "Level 4 v1 Final Integration boundary verification passed. Main solution projects: $($entries.Count); canonical projects: $($canonicalProjects.Count); obsolete delivery files: none."
