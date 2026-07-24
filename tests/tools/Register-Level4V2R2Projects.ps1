$ErrorActionPreference = 'Stop'
$toolsRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$testsRoot = Split-Path -Parent $toolsRoot
$root = Split-Path -Parent $testsRoot
$solutionPath = Join-Path $root 'SemanticGpuEngine4-5.sln'
if (-not (Test-Path -LiteralPath $solutionPath -PathType Leaf)) { throw 'SemanticGpuEngine4-5.sln is missing.' }

$projects = @(
    [pscustomobject]@{Name='191_Level4V2AuthorityModel';Path='191_Level4V2AuthorityModel\191_Level4V2AuthorityModel.vcxproj';Guid='{000000BF-0000-5000-8000-0000000000BF}'},
    [pscustomobject]@{Name='192_Level4V2CandidatePlanner';Path='192_Level4V2CandidatePlanner\192_Level4V2CandidatePlanner.vcxproj';Guid='{000000C0-0000-5000-8000-0000000000C0}'},
    [pscustomobject]@{Name='193_Level4V2IndependentVerifier';Path='193_Level4V2IndependentVerifier\193_Level4V2IndependentVerifier.vcxproj';Guid='{000000C1-0000-5000-8000-0000000000C1}'},
    [pscustomobject]@{Name='194_Level4V2FrozenAuthority';Path='194_Level4V2FrozenAuthority\194_Level4V2FrozenAuthority.vcxproj';Guid='{000000C2-0000-5000-8000-0000000000C2}'},
    [pscustomobject]@{Name='195_Level4V2AuthorityTests';Path='195_Level4V2AuthorityTests\195_Level4V2AuthorityTests.vcxproj';Guid='{000000C3-0000-5000-8000-0000000000C3}'}
)
foreach ($project in $projects) {
    $projectPath = Join-Path $root $project.Path
    if (-not (Test-Path -LiteralPath $projectPath -PathType Leaf)) { throw "Level 4 v2 R2 project missing: $($project.Path)" }
    [xml]$xml = Get-Content -Raw -LiteralPath $projectPath -Encoding UTF8
    $actualGuid = [string](($xml.Project.PropertyGroup | Where-Object {$_.Label -eq 'Globals'} | Select-Object -First 1).ProjectGuid)
    if ($actualGuid.ToUpperInvariant() -ne $project.Guid.ToUpperInvariant()) { throw "ProjectGuid mismatch for $($project.Path): $actualGuid" }
}

$text = [IO.File]::ReadAllText($solutionPath)
$newLine = if ($text.Contains("`r`n")) { "`r`n" } else { "`n" }
$projectTypeGuid = '{BC8A1FFA-BEE3-4634-8014-F334798102B3}'
$missing = @()
foreach ($project in $projects) {
    $count = ([regex]::Matches($text,[regex]::Escape($project.Path))).Count
    if ($count -gt 1) { throw "Duplicate Solution project path: $($project.Path)" }
    if ($count -eq 0) { $missing += $project }
}
if ($missing.Count -gt 0) {
    $marker = "${newLine}Global${newLine}"
    $index = $text.IndexOf($marker,[StringComparison]::Ordinal)
    if ($index -lt 0) { throw 'Solution Global marker was not found.' }
    $block = ''
    foreach ($project in $missing) {
        $block += 'Project("' + $projectTypeGuid + '") = "' + $project.Name + '", "' + $project.Path + '", "' + $project.Guid + '"' + $newLine + 'EndProject' + $newLine
    }
    $text = $text.Substring(0,$index+$newLine.Length) + $block + 'Global' + $newLine + $text.Substring($index+$marker.Length)
}

$header = "`tGlobalSection(ProjectConfigurationPlatforms) = postSolution${newLine}"
$index = $text.IndexOf($header,[StringComparison]::Ordinal)
if ($index -lt 0) { throw 'ProjectConfigurationPlatforms section was not found.' }
$insert = $index + $header.Length
$configBlock = ''
foreach ($project in $projects) {
    $required = @(
        "$($project.Guid).Debug|x64.ActiveCfg = Debug|x64",
        "$($project.Guid).Debug|x64.Build.0 = Debug|x64",
        "$($project.Guid).Release|x64.ActiveCfg = Release|x64",
        "$($project.Guid).Release|x64.Build.0 = Release|x64")
    $present = @($required | Where-Object {$text.Contains($_)}).Count
    if ($present -ne 0 -and $present -ne 4) { throw "Partial Solution configuration exists for $($project.Name)." }
    if ($present -eq 0) { foreach ($entry in $required) { $configBlock += "`t`t$entry$newLine" } }
}
if ($configBlock.Length -gt 0) { $text = $text.Insert($insert,$configBlock) }

foreach ($project in $projects) {
    if (([regex]::Matches($text,[regex]::Escape($project.Path))).Count -ne 1) { throw "Solution registration mismatch: $($project.Path)" }
    foreach ($entry in @("$($project.Guid).Debug|x64.ActiveCfg = Debug|x64","$($project.Guid).Debug|x64.Build.0 = Debug|x64","$($project.Guid).Release|x64.ActiveCfg = Release|x64","$($project.Guid).Release|x64.Build.0 = Release|x64")) {
        if (([regex]::Matches($text,[regex]::Escape($entry))).Count -ne 1) { throw "Solution configuration mismatch: $entry" }
    }
}
$utf8WithBom = New-Object System.Text.UTF8Encoding($true)
[IO.File]::WriteAllText($solutionPath,$text,$utf8WithBom)
Write-Host 'Level 4 v2 R2 projects registered in SemanticGpuEngine4-5.sln.'
Write-Host 'Projects added/verified: 5'
