$ErrorActionPreference = 'Stop'
$toolsRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$testsRoot = Split-Path -Parent $toolsRoot
$root = Split-Path -Parent $testsRoot
$solutionPath = Join-Path $root 'SemanticGpuEngine4-5.sln'
if (-not (Test-Path -LiteralPath $solutionPath -PathType Leaf)) { throw 'SemanticGpuEngine4-5.sln is missing.' }

function Normalize-SolutionPath([string]$Path) { return $Path.Replace('/', '\').Trim() }
function Parse-ProjectLine([string]$Line) {
    $match=[regex]::Match($Line.Trim(),'^Project\("(?<type>\{[0-9A-Fa-f-]+\})"\) = "(?<name>[^"]+)", "(?<path>[^"]+)", "(?<guid>\{[0-9A-Fa-f-]+\})"$')
    if(-not $match.Success){return $null}
    return [pscustomobject]@{Type=$match.Groups['type'].Value;Name=$match.Groups['name'].Value;Path=$match.Groups['path'].Value;Guid=$match.Groups['guid'].Value}
}
$projectTypeGuid='{BC8A1FFA-BEE3-4634-8014-F334798102B3}'
$projects=@(
[pscustomobject]@{Name='201_Level4V2DynamicInvocationModel';Path='201_Level4V2DynamicInvocationModel\201_Level4V2DynamicInvocationModel.vcxproj';Guid='{000000C9-0000-5000-8000-0000000000C9}'},
[pscustomobject]@{Name='202_Level4V2DynamicInvocationPlanner';Path='202_Level4V2DynamicInvocationPlanner\202_Level4V2DynamicInvocationPlanner.vcxproj';Guid='{000000CA-0000-5000-8000-0000000000CA}'},
[pscustomobject]@{Name='203_Level4V2DynamicInvocationVerifier';Path='203_Level4V2DynamicInvocationVerifier\203_Level4V2DynamicInvocationVerifier.vcxproj';Guid='{000000CB-0000-5000-8000-0000000000CB}'},
[pscustomobject]@{Name='204_Level4V2FrozenInvocationHistory';Path='204_Level4V2FrozenInvocationHistory\204_Level4V2FrozenInvocationHistory.vcxproj';Guid='{000000CC-0000-5000-8000-0000000000CC}'},
[pscustomobject]@{Name='205_Level4V2DynamicInvocationTests';Path='205_Level4V2DynamicInvocationTests\205_Level4V2DynamicInvocationTests.vcxproj';Guid='{000000CD-0000-5000-8000-0000000000CD}'})
foreach($project in $projects){
    $path=Join-Path $root $project.Path
    if(-not(Test-Path -LiteralPath $path -PathType Leaf)){throw "R4 project missing: $($project.Path)"}
    [xml]$xml=Get-Content -Raw -LiteralPath $path -Encoding UTF8
    $actual=[string](($xml.Project.PropertyGroup|Where-Object{$_.Label -eq 'Globals'}|Select-Object -First 1).ProjectGuid)
    if($actual.ToUpperInvariant() -ne $project.Guid.ToUpperInvariant()){throw "ProjectGuid mismatch: $($project.Path)"}
}
$lines=New-Object 'System.Collections.Generic.List[string]';foreach($line in [IO.File]::ReadAllLines($solutionPath)){[void]$lines.Add($line)}
$priorGuids=@{};$missing=New-Object 'System.Collections.Generic.List[object]'
foreach($project in $projects){
    $expectedPath=Normalize-SolutionPath $project.Path;$matches=New-Object 'System.Collections.Generic.List[int]'
    for($index=0;$index -lt $lines.Count;++$index){
        $parsed=Parse-ProjectLine $lines[$index]
        if($null -ne $parsed){if($parsed.Name -eq $project.Name -or (Normalize-SolutionPath $parsed.Path) -eq $expectedPath -or $parsed.Guid.ToUpperInvariant() -eq $project.Guid.ToUpperInvariant()){[void]$matches.Add($index)}}
        elseif($lines[$index].Contains($project.Name) -or $lines[$index].Contains($project.Path)){[void]$matches.Add($index)}
    }
    if($matches.Count -gt 1){throw "Duplicate or conflicting Solution registration: $($project.Name)"}
    if($matches.Count -eq 0){[void]$missing.Add($project);continue}
    $lineIndex=$matches[0];$parsedExisting=Parse-ProjectLine $lines[$lineIndex];if($null -ne $parsedExisting){$priorGuids[$project.Name]=$parsedExisting.Guid}
    $lines[$lineIndex]='Project("'+$projectTypeGuid+'") = "'+$project.Name+'", "'+$project.Path+'", "'+$project.Guid+'"'
    if($lineIndex+1 -ge $lines.Count -or $lines[$lineIndex+1].Trim() -ne 'EndProject'){$lines.Insert($lineIndex+1,'EndProject')}
}
if($missing.Count -gt 0){
    $globalIndex=-1;for($index=0;$index -lt $lines.Count;++$index){if($lines[$index].Trim() -eq 'Global'){$globalIndex=$index;break}}
    if($globalIndex -lt 0){throw 'Solution Global marker missing.'}
    foreach($project in $missing){$lines.Insert($globalIndex,'Project("'+$projectTypeGuid+'") = "'+$project.Name+'", "'+$project.Path+'", "'+$project.Guid+'"');++$globalIndex;$lines.Insert($globalIndex,'EndProject');++$globalIndex}
}
$sectionStart=-1;$sectionEnd=-1
for($index=0;$index -lt $lines.Count;++$index){if($lines[$index].Trim() -eq 'GlobalSection(ProjectConfigurationPlatforms) = postSolution'){$sectionStart=$index;for($end=$index+1;$end -lt $lines.Count;++$end){if($lines[$end].Trim() -eq 'EndGlobalSection'){$sectionEnd=$end;break}};break}}
if($sectionStart -lt 0 -or $sectionEnd -lt 0){throw 'ProjectConfigurationPlatforms section missing.'}
$configurationGuids=@();foreach($project in $projects){if($configurationGuids -notcontains $project.Guid){$configurationGuids+=$project.Guid};if($priorGuids.ContainsKey($project.Name)){$prior=[string]$priorGuids[$project.Name];if($configurationGuids -notcontains $prior){$configurationGuids+=$prior}}}
for($index=$sectionEnd-1;$index -gt $sectionStart;--$index){foreach($guid in $configurationGuids){if($lines[$index].IndexOf($guid,[StringComparison]::OrdinalIgnoreCase) -ge 0){$lines.RemoveAt($index);break}}}
$insertIndex=$sectionStart+1
foreach($project in $projects){foreach($entry in @("$($project.Guid).Debug|x64.ActiveCfg = Debug|x64","$($project.Guid).Debug|x64.Build.0 = Debug|x64","$($project.Guid).Release|x64.ActiveCfg = Release|x64","$($project.Guid).Release|x64.Build.0 = Release|x64")){$lines.Insert($insertIndex,"`t`t$entry");++$insertIndex}}
foreach($project in $projects){$canonical='Project("'+$projectTypeGuid+'") = "'+$project.Name+'", "'+$project.Path+'", "'+$project.Guid+'"';if(@($lines|Where-Object{$_.Trim() -eq $canonical}).Count -ne 1){throw "Solution registration mismatch after normalization: $($project.Name)"}}
$utf8=New-Object System.Text.UTF8Encoding($true);[IO.File]::WriteAllLines($solutionPath,$lines,$utf8)
Write-Host 'Level 4 v2 R4 Projects registered and normalized. Projects: 5.'
