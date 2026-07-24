$ErrorActionPreference='Stop'
$testsRoot=Split-Path -Parent $MyInvocation.MyCommand.Path
$root=Split-Path -Parent $testsRoot
function Invoke-Step([string]$Name,[scriptblock]$Action){Write-Host "[L4V2-R6] START $Name";$watch=[Diagnostics.Stopwatch]::StartNew();& $Action;$watch.Stop();Write-Host ("[L4V2-R6] DONE  {0} elapsed={1}" -f $Name,$watch.Elapsed)}
function Invoke-Checked([string]$File,[string[]]$Arguments=@()){& $File @Arguments;if($LASTEXITCODE -ne 0){throw "Command failed: $File $Arguments"}}
function Invoke-PS([string]$Relative,[string[]]$Arguments=@()){Invoke-Checked 'powershell.exe' (@('-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',(Join-Path $testsRoot $Relative))+$Arguments)}
function Get-MSBuild{$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe';if(-not(Test-Path $vswhere)){throw 'vswhere.exe missing'};$msbuild=& $vswhere -latest -products * -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe'|Select-Object -Last 1;if(-not $msbuild){throw 'MSBuild missing'};return $msbuild}
function Test-BytesEqual([string]$A,[string]$B){$x=[IO.File]::ReadAllBytes($A);$y=[IO.File]::ReadAllBytes($B);if($x.Length -ne $y.Length){return $false};for($i=0;$i -lt $x.Length;++$i){if($x[$i] -ne $y[$i]){return $false}};return $true}

Invoke-Step 'Frozen Spiral 7 reference gate' {Invoke-PS 'Run-Spiral7ReferenceGate.ps1'}
Invoke-Step 'R0 regression freeze' {Invoke-PS 'tools\Verify-Level4V2R0.ps1' @('-Mode','Regression')}
Invoke-Step 'R1 regression vocabulary' {Invoke-PS 'tools\Verify-Level4V2R1.ps1' @('-Mode','Regression')}
Invoke-Step 'R2 regression authority' {Invoke-PS 'tools\Verify-Level4V2R2.ps1' @('-Mode','Regression')}
Invoke-Step 'R3 regression composition' {Invoke-PS 'tools\Verify-Level4V2R3.ps1' @('-Mode','Regression')}
Invoke-Step 'R4 regression Dynamic Invocation and History' {Invoke-PS 'tools\Verify-Level4V2R4.ps1' @('-Mode','Regression')}
Invoke-Step 'R5 regression Runtime and Recovery' {Invoke-PS 'tools\Verify-Level4V2R5.ps1' @('-Mode','Regression')}
Invoke-Step 'R6 static migration boundaries' {Invoke-PS 'tools\Verify-Level4V2R6.ps1' @('-Mode','Applied')}

$msbuild=Get-MSBuild
$project=Join-Path $root '213_Level4V2MigrationCorpusTests\213_Level4V2MigrationCorpusTests.vcxproj'
Invoke-Step 'Build R6 Debug/Release' {foreach($configuration in @('Debug','Release')){Invoke-Checked $msbuild @($project,'/m','/nr:false','/nologo',"/p:Configuration=$configuration",'/p:Platform=x64',"/p:SolutionDir=$root\",'/v:minimal')}}
$output=Join-Path $root 'build\tests\level4v2-r6';Remove-Item -Recurse -Force -ErrorAction SilentlyContinue $output;New-Item -ItemType Directory -Force $output|Out-Null
$debug=Join-Path $root 'build\bin\x64\Debug\213_Level4V2MigrationCorpusTests.exe';$release=Join-Path $root 'build\bin\x64\Release\213_Level4V2MigrationCorpusTests.exe'
$debugEvidence=Join-Path $output 'migration-corpus-evidence-debug.bin';$releaseEvidence=Join-Path $output 'migration-corpus-evidence-release.bin'
Invoke-Step 'R6 Debug migration corpus' {Invoke-Checked $debug @('--emit',$debugEvidence)}
Invoke-Step 'R6 Release migration corpus' {Invoke-Checked $release @('--emit',$releaseEvidence)}
Invoke-Step 'Deterministic Migration Evidence' {
    if(-not(Test-BytesEqual $debugEvidence $releaseEvidence)){throw 'R6 Debug/Release Migration Evidence differs.'}
    $hash=(Get-FileHash -Algorithm SHA256 -LiteralPath $debugEvidence).Hash.ToLowerInvariant()
    if($hash -ne '41d8432bd1019a09f9ee92b384b3ce4f11e52db031fc38862c10ffcec7a91724'){throw "R6 Evidence SHA mismatch: $hash"}
    Write-Host "R6 Migration Evidence SHA-256: $hash"
}
Invoke-Step 'Optional external Evidence identity' {
    $candidates=@((Join-Path $root 'spiral7-cu6-2.zip'),(Join-Path $root 'build\evidence\spiral7-cu6-2.zip'))
    $found=$candidates|Where-Object{Test-Path -LiteralPath $_ -PathType Leaf}|Select-Object -First 1
    if($found){$hash=(Get-FileHash -Algorithm SHA256 -LiteralPath $found).Hash.ToLowerInvariant();if($hash -ne '9ee852c2a2574a8cb9c163b6e94213b855317bba74319e6443e9aa98c10c24d9'){throw "External Evidence bundle SHA mismatch: $hash"};Write-Host "External Evidence bundle verified: $found"}
    else{Write-Host 'External performance Evidence bundle was not repeated; tracked index SHA remains authoritative.'}
}
Invoke-Step 'Source Manifest' {Invoke-PS 'tools\Verify-SourceManifest.ps1'}
Write-Host '============================================================'
Write-Host 'SGE4 LEVEL 4 V2 R6 MIGRATION CORPUS PASSED'
Write-Host 'R6 COMPLETE'
Write-Host 'Source lineages: 16'
Write-Host 'Terminal Invariant outcomes: 40'
Write-Host 'Migration cases: 26 (correctness=24 observational=2)'
Write-Host 'Migration-certificate rejections: 20'
Write-Host 'Runtime Candidate-policy authorization: None'
Write-Host 'Reference Project deletion: None'
Write-Host 'NEXT: R7 REFERENCE RETIREMENT DECISION'
Write-Host '============================================================'
