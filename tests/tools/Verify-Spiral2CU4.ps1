$ErrorActionPreference='Stop'
$root=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
foreach($name in @('88_Spiral2Scenarios','95_Spiral2CompositionTests')){
    if(-not(Test-Path -LiteralPath (Join-Path $root "$name\$name.vcxproj"))){throw "Missing CU4 project: $name"}
}
[xml]$scenario=Get-Content -Raw -LiteralPath (Join-Path $root '88_Spiral2Scenarios\88_Spiral2Scenarios.vcxproj')
$references=@($scenario.SelectNodes("//*[local-name()='ProjectReference']")|ForEach-Object{$_.Include}) -join '|'
if($references-notmatch'19_CompositionVerifier' -or $references-notmatch'22_CompositionRuntime'){throw 'Scenario does not use canonical verification and runtime authority.'}
$source=Get-Content -Raw -LiteralPath (Join-Path $root '88_Spiral2Scenarios\Spiral2Scenarios.cpp')
foreach($required in @('s2/motors','s2/reference','s2/a/observations','s2/b/observations','s2/c/observations','s2/records','FreezeVerifiedComposition','SubmitStaticComposition')){if($source-notmatch[regex]::Escape($required)){throw "Missing CU4 composition authority: $required"}}
foreach($project in @('13_PackageRuntime','14_D3D12Backend','20_CompositionDeviceDomain','21_CompositionSharedResources','22_CompositionRuntime')){
    [xml]$xml=Get-Content -Raw -LiteralPath (Join-Path $root "$project\$project.vcxproj")
    $text=@($xml.SelectNodes("//*[local-name()='ProjectReference']")|ForEach-Object{$_.Include}) -join '|'
    if($text-match'(80_Hierarchy|81_Spiral2|82_Spiral2|83_Hierarchy|84_Hierarchy|85_Hierarchy|86_Spiral2|87_Spiral2|88_Spiral2)'){throw "$project depends on Spiral 2 authority."}
}
Write-Host 'SGE4-5 Spiral 2 CU4 ten-Leaf Composition and Level 4 boundary verification passed.'
