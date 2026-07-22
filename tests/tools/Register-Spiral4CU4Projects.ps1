$ErrorActionPreference = 'Stop'
$toolsRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$testsRoot = Split-Path -Parent $toolsRoot
$root = Split-Path -Parent $testsRoot
$solutionPath = Join-Path $root 'SemanticGpuEngine4-5.sln'
if (-not (Test-Path -LiteralPath $solutionPath -PathType Leaf)) {
    throw 'SemanticGpuEngine4-5.sln is missing.'
}
$projects = @(
    [pscustomobject]@{
        Name = '127_Spiral4CandidateFamilyContracts'
        Path = '127_Spiral4CandidateFamilyContracts\127_Spiral4CandidateFamilyContracts.vcxproj'
        Guid = '{0000007F-0000-5000-8000-00000000007F}'
    },
    [pscustomobject]@{
        Name = '128_ActiveWorkCandidateFamily'
        Path = '128_ActiveWorkCandidateFamily\128_ActiveWorkCandidateFamily.vcxproj'
        Guid = '{00000080-0000-5000-8000-000000000080}'
    },
    [pscustomobject]@{
        Name = '129_ActiveWorkCandidateFamilyPlanner'
        Path = '129_ActiveWorkCandidateFamilyPlanner\129_ActiveWorkCandidateFamilyPlanner.vcxproj'
        Guid = '{00000081-0000-5000-8000-000000000081}'
    },
    [pscustomobject]@{
        Name = '130_ActiveWorkCandidateFamilyVerifier'
        Path = '130_ActiveWorkCandidateFamilyVerifier\130_ActiveWorkCandidateFamilyVerifier.vcxproj'
        Guid = '{00000082-0000-5000-8000-000000000082}'
    },
    [pscustomobject]@{
        Name = '131_Spiral4CandidateFamilyExecution'
        Path = '131_Spiral4CandidateFamilyExecution\131_Spiral4CandidateFamilyExecution.vcxproj'
        Guid = '{00000083-0000-5000-8000-000000000083}'
    },
    [pscustomobject]@{
        Name = '142_Spiral4CandidateFamilyTests'
        Path = '142_Spiral4CandidateFamilyTests\142_Spiral4CandidateFamilyTests.vcxproj'
        Guid = '{0000008E-0000-5000-8000-00000000008E}'
    }
)
foreach ($project in $projects) {
    $projectPath = Join-Path $root $project.Path
    if (-not (Test-Path -LiteralPath $projectPath -PathType Leaf)) {
        throw "CU4 project is missing: $($project.Path)"
    }
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
$missing = @()
foreach ($project in $projects) {
    $count = ([regex]::Matches($text, [regex]::Escape($project.Path))).Count
    if ($count -gt 1) { throw "Duplicate Solution path: $($project.Path)" }
    if ($count -eq 0) { $missing += $project }
}
if ($missing.Count -gt 0) {
    $marker = "${newLine}Global${newLine}"
    $index = $text.IndexOf($marker, [StringComparison]::Ordinal)
    if ($index -lt 0) { throw 'Solution Global marker missing.' }
    $block = ''
    foreach ($project in $missing) {
        $block += (
            'Project("' + $projectTypeGuid + '") = "' +
            $project.Name + '", "' + $project.Path + '", "' +
            $project.Guid + '"' + $newLine +
            'EndProject' + $newLine
        )
    }
    $text = (
        $text.Substring(0, $index + $newLine.Length) +
        $block + 'Global' + $newLine +
        $text.Substring($index + $marker.Length)
    )
}
$header = "`tGlobalSection(ProjectConfigurationPlatforms) = postSolution${newLine}"
$index = $text.IndexOf($header, [StringComparison]::Ordinal)
if ($index -lt 0) { throw 'ProjectConfigurationPlatforms missing.' }
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
    if ($present -ne 0 -and $present -ne 4) {
        throw "Partial Solution configuration: $($project.Name)"
    }
    if ($present -eq 0) {
        foreach ($entry in $required) {
            $configBlock += "`t`t$entry$newLine"
        }
    }
}
if ($configBlock.Length -gt 0) {
    $text = $text.Insert($insert, $configBlock)
}
$utf8 = New-Object System.Text.UTF8Encoding($true)
[IO.File]::WriteAllText($solutionPath, $text, $utf8)
Write-Host 'Spiral 4 CU4 projects registered in SemanticGpuEngine4-5.sln.'
Write-Host 'Projects added/verified: 6'
Write-Host 'Configurations added/verified: Debug|x64, Release|x64'
