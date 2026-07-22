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
        Name = '123_ActiveWorkLoweringCandidate'
        Path = '123_ActiveWorkLoweringCandidate\123_ActiveWorkLoweringCandidate.vcxproj'
        Guid = '{0000007B-0000-5000-8000-00000000007B}'
    },
    [pscustomobject]@{
        Name = '124_ActiveWorkLoweringPlanner'
        Path = '124_ActiveWorkLoweringPlanner\124_ActiveWorkLoweringPlanner.vcxproj'
        Guid = '{0000007C-0000-5000-8000-00000000007C}'
    },
    [pscustomobject]@{
        Name = '125_ActiveWorkLoweringVerifier'
        Path = '125_ActiveWorkLoweringVerifier\125_ActiveWorkLoweringVerifier.vcxproj'
        Guid = '{0000007D-0000-5000-8000-00000000007D}'
    },
    [pscustomobject]@{
        Name = '126_Spiral4VerifiedExecution'
        Path = '126_Spiral4VerifiedExecution\126_Spiral4VerifiedExecution.vcxproj'
        Guid = '{0000007E-0000-5000-8000-00000000007E}'
    },
    [pscustomobject]@{
        Name = '141_Spiral4AuthorityMutationTests'
        Path = '141_Spiral4AuthorityMutationTests\141_Spiral4AuthorityMutationTests.vcxproj'
        Guid = '{0000008D-0000-5000-8000-00000000008D}'
    }
)

foreach ($project in $projects) {
    $projectPath = Join-Path $root $project.Path
    if (-not (Test-Path -LiteralPath $projectPath -PathType Leaf)) {
        throw "Spiral 4 CU3 project is missing: $($project.Path)"
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

$missingProjects = @()
foreach ($project in $projects) {
    $pathCount = ([regex]::Matches(
        $text,
        [regex]::Escape($project.Path),
        [Text.RegularExpressions.RegexOptions]::CultureInvariant
    )).Count

    if ($pathCount -gt 1) {
        throw "Duplicate Solution project path: $($project.Path)"
    }
    if ($pathCount -eq 0) {
        $missingProjects += $project
    }
}

if ($missingProjects.Count -gt 0) {
    $globalMarker = "${newLine}Global${newLine}"
    $globalIndex = $text.IndexOf(
        $globalMarker,
        [StringComparison]::Ordinal
    )
    if ($globalIndex -lt 0) {
        throw 'Solution Global marker was not found.'
    }

    $projectBlock = ''
    foreach ($project in $missingProjects) {
        $projectBlock += (
            'Project("' + $projectTypeGuid + '") = "' +
            $project.Name + '", "' + $project.Path + '", "' +
            $project.Guid + '"' + $newLine +
            'EndProject' + $newLine
        )
    }

    $text = (
        $text.Substring(0, $globalIndex + $newLine.Length) +
        $projectBlock +
        'Global' + $newLine +
        $text.Substring($globalIndex + $globalMarker.Length)
    )
}

$configurationHeader =
    "`tGlobalSection(ProjectConfigurationPlatforms) = postSolution${newLine}"
$configurationIndex = $text.IndexOf(
    $configurationHeader,
    [StringComparison]::Ordinal
)
if ($configurationIndex -lt 0) {
    throw 'ProjectConfigurationPlatforms section was not found.'
}
$configurationInsertPosition =
    $configurationIndex + $configurationHeader.Length

$configurationBlock = ''
foreach ($project in $projects) {
    $required = @(
        "$($project.Guid).Debug|x64.ActiveCfg = Debug|x64",
        "$($project.Guid).Debug|x64.Build.0 = Debug|x64",
        "$($project.Guid).Release|x64.ActiveCfg = Release|x64",
        "$($project.Guid).Release|x64.Build.0 = Release|x64"
    )

    $presentCount = 0
    foreach ($entry in $required) {
        if ($text.Contains($entry)) {
            ++$presentCount
        }
    }

    if ($presentCount -ne 0 -and $presentCount -ne $required.Count) {
        throw "Partial Solution configuration exists for $($project.Name)."
    }

    if ($presentCount -eq 0) {
        foreach ($entry in $required) {
            $configurationBlock += "`t`t$entry$newLine"
        }
    }
}

if ($configurationBlock.Length -gt 0) {
    $text = $text.Insert(
        $configurationInsertPosition,
        $configurationBlock
    )
}

foreach ($project in $projects) {
    $pathCount = ([regex]::Matches(
        $text,
        [regex]::Escape($project.Path),
        [Text.RegularExpressions.RegexOptions]::CultureInvariant
    )).Count
    if ($pathCount -ne 1) {
        throw "Solution registration count mismatch for $($project.Path): $pathCount"
    }

    foreach ($entry in @(
        "$($project.Guid).Debug|x64.ActiveCfg = Debug|x64",
        "$($project.Guid).Debug|x64.Build.0 = Debug|x64",
        "$($project.Guid).Release|x64.ActiveCfg = Release|x64",
        "$($project.Guid).Release|x64.Build.0 = Release|x64"
    )) {
        $entryCount = ([regex]::Matches(
            $text,
            [regex]::Escape($entry),
            [Text.RegularExpressions.RegexOptions]::CultureInvariant
        )).Count
        if ($entryCount -ne 1) {
            throw "Solution configuration count mismatch: $entry"
        }
    }
}

$utf8WithBom = New-Object System.Text.UTF8Encoding($true)
[IO.File]::WriteAllText($solutionPath, $text, $utf8WithBom)

Write-Host 'Spiral 4 CU3 projects registered in SemanticGpuEngine4-5.sln.'
Write-Host 'Projects added/verified: 5'
Write-Host 'Configurations added/verified: Debug|x64, Release|x64'
