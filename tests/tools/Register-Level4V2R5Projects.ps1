$ErrorActionPreference = 'Stop'
$toolsRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$testsRoot = Split-Path -Parent $toolsRoot
$root = Split-Path -Parent $testsRoot
$solutionPath = Join-Path $root 'SemanticGpuEngine4-5.sln'
if (-not (Test-Path -LiteralPath $solutionPath -PathType Leaf)) { throw 'SemanticGpuEngine4-5.sln is missing.' }

function Parse-ProjectLine([string]$Line) {
    $match = [regex]::Match($Line.Trim(), '^Project\("(?<type>\{[0-9A-Fa-f-]+\})"\) = "(?<name>[^"]+)", "(?<path>[^"]+)", "(?<guid>\{[0-9A-Fa-f-]+\})"$')
    if (-not $match.Success) { return $null }
    return [pscustomobject]@{Type=$match.Groups['type'].Value;Name=$match.Groups['name'].Value;Path=$match.Groups['path'].Value.Replace('/','\');Guid=$match.Groups['guid'].Value}
}

$projectTypeGuid='{BC8A1FFA-BEE3-4634-8014-F334798102B3}'
$projects=@(
[pscustomobject]@{Name='206_Level4V2RuntimeModel';Path='206_Level4V2RuntimeModel\206_Level4V2RuntimeModel.vcxproj';Guid='{000000CE-0000-5000-8000-0000000000CE}'},
[pscustomobject]@{Name='207_Level4V2RuntimeMaterialization';Path='207_Level4V2RuntimeMaterialization\207_Level4V2RuntimeMaterialization.vcxproj';Guid='{000000CF-0000-5000-8000-0000000000CF}'},
[pscustomobject]@{Name='208_Level4V2RuntimeSubmission';Path='208_Level4V2RuntimeSubmission\208_Level4V2RuntimeSubmission.vcxproj';Guid='{000000D0-0000-5000-8000-0000000000D0}'},
[pscustomobject]@{Name='209_Level4V2RuntimeRecovery';Path='209_Level4V2RuntimeRecovery\209_Level4V2RuntimeRecovery.vcxproj';Guid='{000000D1-0000-5000-8000-0000000000D1}'},
[pscustomobject]@{Name='210_Level4V2RuntimeRecoveryTests';Path='210_Level4V2RuntimeRecoveryTests\210_Level4V2RuntimeRecoveryTests.vcxproj';Guid='{000000D2-0000-5000-8000-0000000000D2}'},
[pscustomobject]@{Name='211_Level4V2RuntimeWindowsQualification';Path='211_Level4V2RuntimeWindowsQualification\211_Level4V2RuntimeWindowsQualification.vcxproj';Guid='{000000D3-0000-5000-8000-0000000000D3}'})
foreach($p in $projects){$path=Join-Path $root $p.Path;if(-not(Test-Path -LiteralPath $path -PathType Leaf)){throw "R5 Project missing: $($p.Path)"};[xml]$xml=Get-Content -Raw -LiteralPath $path -Encoding UTF8;$actual=[string](($xml.Project.PropertyGroup|Where-Object{$_.Label -eq 'Globals'}|Select-Object -First 1).ProjectGuid);if($actual.ToUpperInvariant() -ne $p.Guid.ToUpperInvariant()){throw "R5 ProjectGuid mismatch: $($p.Path)"}}
$lines=New-Object 'System.Collections.Generic.List[string]';foreach($line in [IO.File]::ReadAllLines($solutionPath)){[void]$lines.Add($line)}
$knownGuids=New-Object 'System.Collections.Generic.List[string]'
foreach($p in $projects){
    $matches=New-Object 'System.Collections.Generic.List[int]'
    for($i=0;$i -lt $lines.Count;++$i){$parsed=Parse-ProjectLine $lines[$i];if($null -ne $parsed -and ($parsed.Name -eq $p.Name -or $parsed.Path -eq $p.Path -or $parsed.Guid.ToUpperInvariant() -eq $p.Guid.ToUpperInvariant())){[void]$matches.Add($i)}}
    if($matches.Count -gt 1){throw "Duplicate R5 Solution registration: $($p.Name)"}
    $canonical='Project("'+$projectTypeGuid+'") = "'+$p.Name+'", "'+$p.Path+'", "'+$p.Guid+'"'
    if($matches.Count -eq 1){$old=Parse-ProjectLine $lines[$matches[0]];if($null -ne $old -and -not $knownGuids.Contains($old.Guid)){[void]$knownGuids.Add($old.Guid)};$lines[$matches[0]]=$canonical}
    else{$global=-1;for($i=0;$i -lt $lines.Count;++$i){if($lines[$i].Trim() -eq 'Global'){$global=$i;break}};if($global -lt 0){throw 'Solution Global marker missing.'};$lines.Insert($global,$canonical);$lines.Insert($global+1,'EndProject')}
    if(-not $knownGuids.Contains($p.Guid)){[void]$knownGuids.Add($p.Guid)}
}
$start=-1;$end=-1;for($i=0;$i -lt $lines.Count;++$i){if($lines[$i].Trim() -eq 'GlobalSection(ProjectConfigurationPlatforms) = postSolution'){$start=$i;for($j=$i+1;$j -lt $lines.Count;++$j){if($lines[$j].Trim() -eq 'EndGlobalSection'){$end=$j;break}};break}}
if($start -lt 0 -or $end -lt 0){throw 'ProjectConfigurationPlatforms section missing.'}
for($i=$end-1;$i -gt $start;--$i){foreach($guid in $knownGuids){if($lines[$i].IndexOf($guid,[StringComparison]::OrdinalIgnoreCase) -ge 0){$lines.RemoveAt($i);break}}}
$insert=$start+1;foreach($p in $projects){foreach($entry in @("$($p.Guid).Debug|x64.ActiveCfg = Debug|x64","$($p.Guid).Debug|x64.Build.0 = Debug|x64","$($p.Guid).Release|x64.ActiveCfg = Release|x64","$($p.Guid).Release|x64.Build.0 = Release|x64")){$lines.Insert($insert,"`t`t$entry");++$insert}}
foreach($p in $projects){
    $canonical='Project("'+$projectTypeGuid+'") = "'+$p.Name+'", "'+$p.Path+'", "'+$p.Guid+'"'
    if(@($lines|Where-Object{$_.Trim() -eq $canonical}).Count -ne 1){throw "R5 registration mismatch after normalization: $($p.Name)"}
    foreach($entry in @("$($p.Guid).Debug|x64.ActiveCfg = Debug|x64","$($p.Guid).Debug|x64.Build.0 = Debug|x64","$($p.Guid).Release|x64.ActiveCfg = Release|x64","$($p.Guid).Release|x64.Build.0 = Release|x64")){if(@($lines|Where-Object{$_.Trim() -eq $entry}).Count -ne 1){throw "R5 configuration mismatch after normalization: $entry"}}
}
$utf8=New-Object System.Text.UTF8Encoding($true);[IO.File]::WriteAllLines($solutionPath,$lines,$utf8)
Write-Host 'Level 4 v2 R5 Projects registered and normalized. Projects: 6.'
