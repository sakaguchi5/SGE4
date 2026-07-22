$ErrorActionPreference = 'Stop'
$toolsRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$testsRoot = Split-Path -Parent $toolsRoot
$root = Split-Path -Parent $testsRoot
$solutionPath = Join-Path $root 'SemanticGpuEngine4-5.sln'

$projects = @(
    [pscustomobject]@{ Name='152_Spiral5PerformanceExperiment'; Path='152_Spiral5PerformanceExperiment\152_Spiral5PerformanceExperiment.vcxproj'; Guid='{00000098-0000-5000-8000-000000000098}' },
    [pscustomobject]@{ Name='153_Spiral5PerformanceTests'; Path='153_Spiral5PerformanceTests\153_Spiral5PerformanceTests.vcxproj'; Guid='{00000099-0000-5000-8000-000000000099}' }
)

if (-not (Test-Path -LiteralPath $solutionPath -PathType Leaf)) { throw 'SemanticGpuEngine4-5.sln is missing.' }
foreach ($project in $projects) {
    $projectPath = Join-Path $root $project.Path
    if (-not (Test-Path -LiteralPath $projectPath -PathType Leaf)) { throw "Spiral 5 CU6 project is missing: $($project.Path)" }
    [xml]$xml = Get-Content -Raw -LiteralPath $projectPath -Encoding UTF8
    $actualGuid = [string](
        $xml.Project.PropertyGroup |
        Where-Object { $_.Label -eq 'Globals' } |
        Select-Object -First 1
    ).ProjectGuid
    if ($actualGuid.ToUpperInvariant() -ne $project.Guid.ToUpperInvariant()) {
        throw "ProjectGuid mismatch for $($project.Path): $actualGuid"
    }
}

$text = [IO.File]::ReadAllText($solutionPath)
$newLine = if ($text.Contains("`r`n")) { "`r`n" } else { "`n" }
$projectTypeGuid = '{BC8A1FFA-BEE3-4634-8014-F334798102B3}'
$missingProjects = @()
foreach ($project in $projects) {
    $count = ([regex]::Matches($text,[regex]::Escape($project.Path),[Text.RegularExpressions.RegexOptions]::CultureInvariant)).Count
    if ($count -gt 1) { throw "Duplicate Solution project path: $($project.Path)" }
    if ($count -eq 0) { $missingProjects += $project }
}
if ($missingProjects.Count -gt 0) {
    $globalMarker = "${newLine}Global${newLine}"
    $globalIndex = $text.IndexOf($globalMarker,[StringComparison]::Ordinal)
    if ($globalIndex -lt 0) { throw 'Solution Global marker was not found.' }
    $block = ''
    foreach ($project in $missingProjects) {
        $block += 'Project("' + $projectTypeGuid + '") = "' + $project.Name + '", "' + $project.Path + '", "' + $project.Guid + '"' + $newLine + 'EndProject' + $newLine
    }
    $text = $text.Substring(0,$globalIndex + $newLine.Length) + $block + 'Global' + $newLine + $text.Substring($globalIndex + $globalMarker.Length)
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
        "$($project.Guid).Release|x64.Build.0 = Release|x64"
    )
    $present = @($required | Where-Object { $text.Contains($_) }).Count
    if ($present -ne 0 -and $present -ne $required.Count) { throw "Partial Solution configuration exists for $($project.Name)." }
    if ($present -eq 0) { foreach ($entry in $required) { $configBlock += "`t`t$entry$newLine" } }
}
if ($configBlock.Length -gt 0) { $text = $text.Insert($insert,$configBlock) }

foreach ($project in $projects) {
    if (([regex]::Matches($text,[regex]::Escape($project.Path))).Count -ne 1) { throw "Solution registration count mismatch: $($project.Path)" }
}
$utf8WithBom = New-Object System.Text.UTF8Encoding($true)
[IO.File]::WriteAllText($solutionPath,$text,$utf8WithBom)
Write-Host 'Spiral 5 CU6 projects registered in SemanticGpuEngine4-5.sln.'
Write-Host 'Projects added/verified: 2'
Write-Host 'Configurations added/verified: Debug|x64, Release|x64'
