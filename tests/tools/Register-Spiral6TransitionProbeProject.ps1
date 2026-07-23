$ErrorActionPreference = 'Stop'
$toolsRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$testsRoot = Split-Path -Parent $toolsRoot
$root = Split-Path -Parent $testsRoot
$solutionPath = Join-Path $root 'SemanticGpuEngine4-5.sln'
if (-not (Test-Path -LiteralPath $solutionPath -PathType Leaf)) { throw 'SemanticGpuEngine4-5.sln is missing.' }

$project = [pscustomobject]@{
    Name='173_Spiral6TransitionProbeTests'
    Path='173_Spiral6TransitionProbeTests\173_Spiral6TransitionProbeTests.vcxproj'
    Guid='{000000AD-0000-5000-8000-0000000000AD}'
}
$projectPath = Join-Path $root $project.Path
if (-not (Test-Path -LiteralPath $projectPath -PathType Leaf)) { throw "Transition Probe project missing: $($project.Path)" }
[xml]$xml = Get-Content -Raw -LiteralPath $projectPath -Encoding UTF8
$actualGuid = [string](($xml.Project.PropertyGroup | Where-Object {$_.Label -eq 'Globals'} | Select-Object -First 1).ProjectGuid)
if ($actualGuid.ToUpperInvariant() -ne $project.Guid.ToUpperInvariant()) { throw "ProjectGuid mismatch: $actualGuid" }

$text = [IO.File]::ReadAllText($solutionPath)
$newLine = if ($text.Contains("`r`n")) { "`r`n" } else { "`n" }
$projectTypeGuid = '{BC8A1FFA-BEE3-4634-8014-F334798102B3}'
$count = ([regex]::Matches($text,[regex]::Escape($project.Path))).Count
if ($count -gt 1) { throw "Duplicate Solution project path: $($project.Path)" }
if ($count -eq 0) {
    $marker = "${newLine}Global${newLine}"
    $index = $text.IndexOf($marker,[StringComparison]::Ordinal)
    if ($index -lt 0) { throw 'Solution Global marker was not found.' }
    $block = 'Project("' + $projectTypeGuid + '") = "' + $project.Name + '", "' + $project.Path + '", "' + $project.Guid + '"' + $newLine + 'EndProject' + $newLine
    $text = $text.Substring(0,$index+$newLine.Length) + $block + 'Global' + $newLine + $text.Substring($index+$marker.Length)
}

$header = "`tGlobalSection(ProjectConfigurationPlatforms) = postSolution${newLine}"
$index = $text.IndexOf($header,[StringComparison]::Ordinal)
if ($index -lt 0) { throw 'ProjectConfigurationPlatforms section was not found.' }
$insert = $index + $header.Length
$required = @(
    "$($project.Guid).Debug|x64.ActiveCfg = Debug|x64",
    "$($project.Guid).Debug|x64.Build.0 = Debug|x64",
    "$($project.Guid).Release|x64.ActiveCfg = Release|x64",
    "$($project.Guid).Release|x64.Build.0 = Release|x64")
$present = @($required | Where-Object {$text.Contains($_)}).Count
if ($present -ne 0 -and $present -ne 4) { throw 'Partial Solution configuration exists for Transition Probe.' }
if ($present -eq 0) {
    $block = ''
    foreach ($entry in $required) { $block += "`t`t$entry$newLine" }
    $text = $text.Insert($insert,$block)
}
if (([regex]::Matches($text,[regex]::Escape($project.Path))).Count -ne 1) { throw 'Transition Probe Solution registration mismatch.' }
$utf8WithBom = New-Object System.Text.UTF8Encoding($true)
[IO.File]::WriteAllText($solutionPath,$text,$utf8WithBom)
Write-Host 'Spiral 6 Transition Probe project registered in SemanticGpuEngine4-5.sln.'
