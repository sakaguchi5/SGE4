$ErrorActionPreference = 'Stop'

$toolsRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$testsRoot = Split-Path -Parent $toolsRoot
$root = Split-Path -Parent $testsRoot
$solutionPath = Join-Path $root 'SemanticGpuEngine4-5.sln'

$projects = @(
    [pscustomobject]@{ Name = '147_TemporalCandidateFamily'; Path = '147_TemporalCandidateFamily\147_TemporalCandidateFamily.vcxproj'; Guid = '{00000093-0000-5000-8000-000000000093}' },
    [pscustomobject]@{ Name = '148_TemporalCandidateFamilyPlanner'; Path = '148_TemporalCandidateFamilyPlanner\148_TemporalCandidateFamilyPlanner.vcxproj'; Guid = '{00000094-0000-5000-8000-000000000094}' },
    [pscustomobject]@{ Name = '149_TemporalCandidateFamilyVerifier'; Path = '149_TemporalCandidateFamilyVerifier\149_TemporalCandidateFamilyVerifier.vcxproj'; Guid = '{00000095-0000-5000-8000-000000000095}' },
    [pscustomobject]@{ Name = '150_Spiral5TemporalCandidateFamilyTests'; Path = '150_Spiral5TemporalCandidateFamilyTests\150_Spiral5TemporalCandidateFamilyTests.vcxproj'; Guid = '{00000096-0000-5000-8000-000000000096}' }
)

foreach ($project in $projects) {
    $projectPath = Join-Path $root $project.Path
    if (-not (Test-Path -LiteralPath $projectPath -PathType Leaf)) {
        throw "Missing CU4 project: $($project.Path)"
    }
    [xml]$xml = Get-Content -Raw -LiteralPath $projectPath -Encoding UTF8
    $actualGuid = [string](
        $xml.Project.PropertyGroup |
        Where-Object { $_.Label -eq 'Globals' } |
        Select-Object -First 1
    ).ProjectGuid
    if ($actualGuid.ToUpperInvariant() -ne $project.Guid.ToUpperInvariant()) {
        throw "ProjectGuid mismatch: $($project.Path)"
    }
}

$text = [IO.File]::ReadAllText($solutionPath)
$newLine = if ($text.Contains("`r`n")) { "`r`n" } else { "`n" }
$projectTypeGuid = '{BC8A1FFA-BEE3-4634-8014-F334798102B3}'
$missingProjects = @()
foreach ($project in $projects) {
    $pathCount = ([regex]::Matches($text, [regex]::Escape($project.Path))).Count
    if ($pathCount -gt 1) { throw "Duplicate Solution path: $($project.Path)" }
    if ($pathCount -eq 0) { $missingProjects += $project }
}

if ($missingProjects.Count -gt 0) {
    $globalMarker = "${newLine}Global${newLine}"
    $globalIndex = $text.IndexOf($globalMarker, [StringComparison]::Ordinal)
    if ($globalIndex -lt 0) { throw 'Solution Global marker missing.' }
    $projectBlock = ''
    foreach ($project in $missingProjects) {
        $projectBlock += (
            'Project("' + $projectTypeGuid + '") = "' + $project.Name +
            '", "' + $project.Path + '", "' + $project.Guid + '"' + $newLine +
            'EndProject' + $newLine
        )
    }
    $text = (
        $text.Substring(0, $globalIndex + $newLine.Length) +
        $projectBlock + 'Global' + $newLine +
        $text.Substring($globalIndex + $globalMarker.Length)
    )
}

$configurationHeader = "`tGlobalSection(ProjectConfigurationPlatforms) = postSolution${newLine}"
$configurationIndex = $text.IndexOf($configurationHeader, [StringComparison]::Ordinal)
if ($configurationIndex -lt 0) { throw 'ProjectConfigurationPlatforms missing.' }
$insertPosition = $configurationIndex + $configurationHeader.Length
$configurationBlock = ''
foreach ($project in $projects) {
    $required = @(
        "$($project.Guid).Debug|x64.ActiveCfg = Debug|x64",
        "$($project.Guid).Debug|x64.Build.0 = Debug|x64",
        "$($project.Guid).Release|x64.ActiveCfg = Release|x64",
        "$($project.Guid).Release|x64.Build.0 = Release|x64"
    )
    $presentCount = @($required | Where-Object { $text.Contains($_) }).Count
    if ($presentCount -ne 0 -and $presentCount -ne $required.Count) {
        throw "Partial Solution configuration: $($project.Name)"
    }
    if ($presentCount -eq 0) {
        foreach ($entry in $required) { $configurationBlock += "`t`t$entry$newLine" }
    }
}
if ($configurationBlock.Length -gt 0) {
    $text = $text.Insert($insertPosition, $configurationBlock)
}

$utf8WithBom = New-Object System.Text.UTF8Encoding($true)
[IO.File]::WriteAllText($solutionPath, $text, $utf8WithBom)
Write-Host 'Spiral 5 CU4 projects registered. Projects: 4.'
