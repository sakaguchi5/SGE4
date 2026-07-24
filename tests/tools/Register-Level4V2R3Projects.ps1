$ErrorActionPreference = 'Stop'
$toolsRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$testsRoot = Split-Path -Parent $toolsRoot
$root = Split-Path -Parent $testsRoot
$solutionPath = Join-Path $root 'SemanticGpuEngine4-5.sln'
if (-not (Test-Path -LiteralPath $solutionPath -PathType Leaf)) { throw 'SemanticGpuEngine4-5.sln is missing.' }

function Normalize-SolutionPath([string]$Path) {
    return $Path.Replace('/', '\').Trim()
}

function Parse-ProjectLine([string]$Line) {
    $match = [regex]::Match(
        $Line.Trim(),
        '^Project\("(?<type>\{[0-9A-Fa-f-]+\})"\) = "(?<name>[^"]+)", "(?<path>[^"]+)", "(?<guid>\{[0-9A-Fa-f-]+\})"$')
    if (-not $match.Success) { return $null }
    return [pscustomobject]@{
        Type = $match.Groups['type'].Value
        Name = $match.Groups['name'].Value
        Path = $match.Groups['path'].Value
        Guid = $match.Groups['guid'].Value
    }
}

$projectTypeGuid = '{BC8A1FFA-BEE3-4634-8014-F334798102B3}'
$projects = @(
    [pscustomobject]@{Name='196_Level4V2CompositionModel';Path='196_Level4V2CompositionModel\196_Level4V2CompositionModel.vcxproj';Guid='{000000C4-0000-5000-8000-0000000000C4}'},
    [pscustomobject]@{Name='197_Level4V2CompositionPlanner';Path='197_Level4V2CompositionPlanner\197_Level4V2CompositionPlanner.vcxproj';Guid='{000000C5-0000-5000-8000-0000000000C5}'},
    [pscustomobject]@{Name='198_Level4V2CompositionVerifier';Path='198_Level4V2CompositionVerifier\198_Level4V2CompositionVerifier.vcxproj';Guid='{000000C6-0000-5000-8000-0000000000C6}'},
    [pscustomobject]@{Name='199_Level4V2FrozenComposition';Path='199_Level4V2FrozenComposition\199_Level4V2FrozenComposition.vcxproj';Guid='{000000C7-0000-5000-8000-0000000000C7}'},
    [pscustomobject]@{Name='200_Level4V2CompositionTests';Path='200_Level4V2CompositionTests\200_Level4V2CompositionTests.vcxproj';Guid='{000000C8-0000-5000-8000-0000000000C8}'})

foreach ($project in $projects) {
    $projectPath = Join-Path $root $project.Path
    if (-not (Test-Path -LiteralPath $projectPath -PathType Leaf)) { throw "R3 project missing: $($project.Path)" }
    [xml]$xml = Get-Content -Raw -LiteralPath $projectPath -Encoding UTF8
    $actual = [string](($xml.Project.PropertyGroup | Where-Object {$_.Label -eq 'Globals'} | Select-Object -First 1).ProjectGuid)
    if ($actual.ToUpperInvariant() -ne $project.Guid.ToUpperInvariant()) { throw "ProjectGuid mismatch: $($project.Path)" }
}

$lines = New-Object 'System.Collections.Generic.List[string]'
foreach ($line in [IO.File]::ReadAllLines($solutionPath)) { [void]$lines.Add($line) }
$priorGuids = @{}
$missing = New-Object 'System.Collections.Generic.List[object]'

foreach ($project in $projects) {
    $expectedPath = Normalize-SolutionPath $project.Path
    $matches = New-Object 'System.Collections.Generic.List[int]'
    for ($index = 0; $index -lt $lines.Count; ++$index) {
        $parsed = Parse-ProjectLine $lines[$index]
        if ($null -ne $parsed) {
            if ($parsed.Name -eq $project.Name -or
                (Normalize-SolutionPath $parsed.Path) -eq $expectedPath -or
                $parsed.Guid.ToUpperInvariant() -eq $project.Guid.ToUpperInvariant()) {
                [void]$matches.Add($index)
            }
        }
        elseif ($lines[$index].Contains($project.Name) -or $lines[$index].Contains($project.Path)) {
            [void]$matches.Add($index)
        }
    }

    if ($matches.Count -gt 1) { throw "Duplicate or conflicting Solution registration: $($project.Name)" }
    if ($matches.Count -eq 0) {
        [void]$missing.Add($project)
        continue
    }

    $lineIndex = $matches[0]
    $parsedExisting = Parse-ProjectLine $lines[$lineIndex]
    if ($null -ne $parsedExisting) { $priorGuids[$project.Name] = $parsedExisting.Guid }
    $lines[$lineIndex] = 'Project("' + $projectTypeGuid + '") = "' + $project.Name + '", "' + $project.Path + '", "' + $project.Guid + '"'
    if ($lineIndex + 1 -ge $lines.Count -or $lines[$lineIndex + 1].Trim() -ne 'EndProject') {
        $lines.Insert($lineIndex + 1, 'EndProject')
    }
}

if ($missing.Count -gt 0) {
    $globalIndex = -1
    for ($index = 0; $index -lt $lines.Count; ++$index) {
        if ($lines[$index].Trim() -eq 'Global') { $globalIndex = $index; break }
    }
    if ($globalIndex -lt 0) { throw 'Solution Global marker missing.' }
    foreach ($project in $missing) {
        $lines.Insert($globalIndex, 'Project("' + $projectTypeGuid + '") = "' + $project.Name + '", "' + $project.Path + '", "' + $project.Guid + '"')
        ++$globalIndex
        $lines.Insert($globalIndex, 'EndProject')
        ++$globalIndex
    }
}

$sectionStart = -1
$sectionEnd = -1
for ($index = 0; $index -lt $lines.Count; ++$index) {
    if ($lines[$index].Trim() -eq 'GlobalSection(ProjectConfigurationPlatforms) = postSolution') {
        $sectionStart = $index
        for ($end = $index + 1; $end -lt $lines.Count; ++$end) {
            if ($lines[$end].Trim() -eq 'EndGlobalSection') { $sectionEnd = $end; break }
        }
        break
    }
}
if ($sectionStart -lt 0 -or $sectionEnd -lt 0) { throw 'ProjectConfigurationPlatforms section missing.' }

$configurationGuids = @()
foreach ($project in $projects) {
    if ($configurationGuids -notcontains $project.Guid) { $configurationGuids += $project.Guid }
    if ($priorGuids.ContainsKey($project.Name)) {
        $priorGuid = [string]$priorGuids[$project.Name]
        if ($configurationGuids -notcontains $priorGuid) { $configurationGuids += $priorGuid }
    }
}
for ($index = $sectionEnd - 1; $index -gt $sectionStart; --$index) {
    foreach ($guid in $configurationGuids) {
        if ($lines[$index].IndexOf($guid, [StringComparison]::OrdinalIgnoreCase) -ge 0) {
            $lines.RemoveAt($index)
            break
        }
    }
}

$insertIndex = $sectionStart + 1
foreach ($project in $projects) {
    foreach ($entry in @(
        "$($project.Guid).Debug|x64.ActiveCfg = Debug|x64",
        "$($project.Guid).Debug|x64.Build.0 = Debug|x64",
        "$($project.Guid).Release|x64.ActiveCfg = Release|x64",
        "$($project.Guid).Release|x64.Build.0 = Release|x64")) {
        $lines.Insert($insertIndex, "`t`t$entry")
        ++$insertIndex
    }
}

foreach ($project in $projects) {
    $canonicalLine = 'Project("' + $projectTypeGuid + '") = "' + $project.Name + '", "' + $project.Path + '", "' + $project.Guid + '"'
    $projectCount = @($lines | Where-Object {$_.Trim() -eq $canonicalLine}).Count
    if ($projectCount -ne 1) { throw "Solution registration mismatch after normalization: $($project.Name)" }
    foreach ($entry in @(
        "$($project.Guid).Debug|x64.ActiveCfg = Debug|x64",
        "$($project.Guid).Debug|x64.Build.0 = Debug|x64",
        "$($project.Guid).Release|x64.ActiveCfg = Release|x64",
        "$($project.Guid).Release|x64.Build.0 = Release|x64")) {
        if (@($lines | Where-Object {$_.Trim() -eq $entry}).Count -ne 1) { throw "Solution configuration mismatch after normalization: $entry" }
    }
}

$utf8WithBom = New-Object System.Text.UTF8Encoding($true)
[IO.File]::WriteAllLines($solutionPath, $lines, $utf8WithBom)
Write-Host 'Level 4 v2 R3 Projects registered and normalized. Projects: 5.'
