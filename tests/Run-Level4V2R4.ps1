$ErrorActionPreference='Stop'
$testsRoot=Split-Path -Parent $MyInvocation.MyCommand.Path
$root=Split-Path -Parent $testsRoot
function Invoke-Step([string]$Name,[scriptblock]$Action){Write-Host "[L4V2-R4] START $Name";$watch=[Diagnostics.Stopwatch]::StartNew();& $Action;$watch.Stop();Write-Host ("[L4V2-R4] DONE  {0} elapsed={1}" -f $Name,$watch.Elapsed)}
function Invoke-Checked([string]$File,[string[]]$Arguments=@()){& $File @Arguments;if($LASTEXITCODE -ne 0){throw "Command failed: $File $Arguments"}}
function Invoke-PS([string]$Relative,[string[]]$Arguments=@()){Invoke-Checked 'powershell.exe' (@('-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',(Join-Path $testsRoot $Relative))+$Arguments)}
function Get-MSBuild{$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe';if(-not(Test-Path $vswhere)){throw 'vswhere.exe missing'};$msbuild=& $vswhere -latest -products * -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe'|Select-Object -Last 1;if(-not $msbuild){throw 'MSBuild missing'};return $msbuild}
function Test-BytesEqual([string]$A,[string]$B){$x=[IO.File]::ReadAllBytes($A);$y=[IO.File]::ReadAllBytes($B);if($x.Length -ne $y.Length){return $false};for($i=0;$i -lt $x.Length;++$i){if($x[$i] -ne $y[$i]){return $false}};return $true}
Invoke-Step 'Frozen Spiral 7 reference gate' {Invoke-PS 'Run-Spiral7ReferenceGate.ps1'}
Invoke-Step 'R0 regression freeze' {Invoke-PS 'tools\Verify-Level4V2R0.ps1' @('-Mode','Regression')}
Invoke-Step 'R1 regression vocabulary' {Invoke-PS 'tools\Verify-Level4V2R1.ps1' @('-Mode','Regression')}
Invoke-Step 'R2 regression authority' {Invoke-PS 'tools\Verify-Level4V2R2.ps1' @('-Mode','Regression')}
Invoke-Step 'R3 regression composition' {Invoke-PS 'tools\Verify-Level4V2R3.ps1' @('-Mode','Regression')}
Invoke-Step 'R4 static Dynamic Invocation and History boundaries' {Invoke-PS 'tools\Verify-Level4V2R4.ps1' @('-Mode','Applied')}
$msbuild=Get-MSBuild;$project=Join-Path $root '205_Level4V2DynamicInvocationTests\205_Level4V2DynamicInvocationTests.vcxproj'
Invoke-Step 'Build Debug and Release' {foreach($configuration in @('Debug','Release')){Invoke-Checked $msbuild @($project,'/m','/nr:false','/nologo',"/p:Configuration=$configuration",'/p:Platform=x64',"/p:SolutionDir=$root\",'/v:minimal')}}
$output=Join-Path $root 'build\tests\level4v2-r4';Remove-Item -Recurse -Force -ErrorAction SilentlyContinue $output;New-Item -ItemType Directory -Force $output|Out-Null
$debugExe=Join-Path $root 'build\bin\x64\Debug\205_Level4V2DynamicInvocationTests.exe';$releaseExe=Join-Path $root 'build\bin\x64\Release\205_Level4V2DynamicInvocationTests.exe'
$debugEvidence=Join-Path $output 'dynamic-invocation-history-evidence-debug.bin';$releaseEvidence=Join-Path $output 'dynamic-invocation-history-evidence-release.bin'
Invoke-Step 'Debug exact dynamic authority self-test' {Invoke-Checked $debugExe @('--emit',$debugEvidence)}
Invoke-Step 'Release exact dynamic authority self-test' {Invoke-Checked $releaseExe @('--emit',$releaseEvidence)}
Invoke-Step 'Debug/Release deterministic Evidence' {
    if(-not(Test-BytesEqual $debugEvidence $releaseEvidence)){throw 'R4 Debug/Release Evidence differs.'}
    $actual=(Get-FileHash -Algorithm SHA256 -LiteralPath $debugEvidence).Hash.ToLowerInvariant()
    $expected='78f16d2fa4185412144205dc54f842f2929e6e269711dfca9ced9967f37cfbe1'
    if($actual -ne $expected){throw "R4 Evidence SHA-256 mismatch: $actual"}
    Write-Host "R4 deterministic Evidence SHA-256: $actual"
}
Invoke-Step 'Source Manifest' {Invoke-PS 'tools\Verify-SourceManifest.ps1'}
Write-Host '============================================================'
Write-Host 'SGE4 LEVEL 4 V2 R4 DYNAMIC INVOCATION AND HISTORY PASSED'
Write-Host 'R4 COMPLETE'
Write-Host 'Accepted scenarios: 8'
Write-Host 'Exact-set/request/proposal rejections: 27'
Write-Host 'Runtime capability added: None'
Write-Host 'Runtime Candidate-policy authorization: None'
Write-Host 'NEXT: R5 RUNTIME AND RECOVERY'
Write-Host '============================================================'
