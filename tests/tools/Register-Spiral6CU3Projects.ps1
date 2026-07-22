$ErrorActionPreference = 'Stop'
$toolsRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$testsRoot = Split-Path -Parent $toolsRoot
$root = Split-Path -Parent $testsRoot
$solutionPath = Join-Path $root 'SemanticGpuEngine4-5.sln'
if (-not (Test-Path -LiteralPath $solutionPath -PathType Leaf)) { throw 'SemanticGpuEngine4-5.sln is missing.' }

$projects = @(
    [pscustomobject]@{Name='157_SparseLoweringCandidate';Path='157_SparseLoweringCandidate\157_SparseLoweringCandidate.vcxproj';Guid='{0000009D-0000-5000-8000-00000000009D}'},
    [pscustomobject]@{Name='158_SparseLoweringPlanner';Path='158_SparseLoweringPlanner\158_SparseLoweringPlanner.vcxproj';Guid='{0000009E-0000-5000-8000-00000000009E}'},
    [pscustomobject]@{Name='159_SparseLoweringVerifier';Path='159_SparseLoweringVerifier\159_SparseLoweringVerifier.vcxproj';Guid='{0000009F-0000-5000-8000-00000000009F}'},
    [pscustomobject]@{Name='167_Spiral6AuthorityMutationTests';Path='167_Spiral6AuthorityMutationTests\167_Spiral6AuthorityMutationTests.vcxproj';Guid='{000000A7-0000-5000-8000-0000000000A7}'}
)
foreach ($project in $projects) {
    $projectPath = Join-Path $root $project.Path
    if (-not (Test-Path -LiteralPath $projectPath -PathType Leaf)) { throw "Spiral 6 CU3 project missing: $($project.Path)" }
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
}
$utf8WithBom = New-Object System.Text.UTF8Encoding($true)
[IO.File]::WriteAllText($solutionPath,$text,$utf8WithBom)
Write-Host 'Spiral 6 CU3 projects registered in SemanticGpuEngine4-5.sln.'
Write-Host 'Projects added/verified: 4'
