$ErrorActionPreference='Stop'
$testsRoot=Split-Path -Parent $MyInvocation.MyCommand.Path
$root=Split-Path -Parent $testsRoot
function Invoke-Checked([string]$File,[string[]]$Arguments=@()){& $File @Arguments;if($LASTEXITCODE -ne 0){throw "Command failed: $File $Arguments"}}
function Invoke-Stage([string]$Name,[scriptblock]$Action){Write-Host "[L4V2-R3] START $Name";$w=[Diagnostics.Stopwatch]::StartNew();& $Action;$w.Stop();Write-Host ("[L4V2-R3] DONE  {0} elapsed={1}" -f $Name,$w.Elapsed)}
function Get-MSBuild{$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe';if(-not(Test-Path -LiteralPath $vswhere -PathType Leaf)){throw 'vswhere.exe missing.'};$msbuild=& $vswhere -latest -products * -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe'|Select-Object -Last 1;if(-not $msbuild){throw 'MSBuild missing.'};return $msbuild}
function Test-BytesEqual([string]$A,[string]$B){$x=[IO.File]::ReadAllBytes($A);$y=[IO.File]::ReadAllBytes($B);if($x.Length -ne $y.Length){return $false};for($i=0;$i -lt $x.Length;++$i){if($x[$i] -ne $y[$i]){return $false}};return $true}
$powershell='powershell.exe';$common=@('-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File')
Invoke-Stage 'Frozen reference gate' {Invoke-Checked $powershell ($common+@((Join-Path $testsRoot 'Run-Spiral7ReferenceGate.ps1')))}
Invoke-Stage 'R0 regression freeze' {Invoke-Checked $powershell ($common+@((Join-Path $testsRoot 'tools\Verify-Level4V2R0.ps1'),'-Mode','Regression'))}
Invoke-Stage 'R1 regression vocabulary' {Invoke-Checked $powershell ($common+@((Join-Path $testsRoot 'tools\Verify-Level4V2R1.ps1'),'-Mode','Regression'))}
Invoke-Stage 'R2 regression authority' {Invoke-Checked $powershell ($common+@((Join-Path $testsRoot 'tools\Verify-Level4V2R2.ps1'),'-Mode','Regression'))}
Invoke-Stage 'R3 static composition boundaries' {Invoke-Checked $powershell ($common+@((Join-Path $testsRoot 'tools\Verify-Level4V2R3.ps1')))}
$msbuild=Get-MSBuild;$project=Join-Path $root '200_Level4V2CompositionTests\200_Level4V2CompositionTests.vcxproj'
Invoke-Stage 'Build Debug and Release' {foreach($configuration in @('Debug','Release')){Invoke-Checked $msbuild @($project,'/m','/nr:false','/nologo',"/p:Configuration=$configuration",'/p:Platform=x64',"/p:SolutionDir=$root\",'/v:minimal')}}
$output=Join-Path $root 'build\tests\level4v2-r3';Remove-Item -Recurse -Force -ErrorAction SilentlyContinue $output;New-Item -ItemType Directory -Force $output|Out-Null
$debug=Join-Path $output 'composition-evidence-debug.bin';$release=Join-Path $output 'composition-evidence-release.bin'
Invoke-Stage 'Debug composition self-test' {Invoke-Checked (Join-Path $root 'build\bin\x64\Debug\200_Level4V2CompositionTests.exe') @('--emit',$debug)}
Invoke-Stage 'Release composition self-test' {Invoke-Checked (Join-Path $root 'build\bin\x64\Release\200_Level4V2CompositionTests.exe') @('--emit',$release)}
if(-not(Test-BytesEqual $debug $release)){throw 'R3 deterministic Composition Evidence differs across Debug and Release.'}
$hash=(Get-FileHash -Algorithm SHA256 -LiteralPath $debug).Hash.ToLowerInvariant();$manifest=Get-Content -Raw -LiteralPath (Join-Path $root 'docs\level4v2\R3_CANONICAL_COMPOSITION_MANIFEST_V1.json') -Encoding UTF8|ConvertFrom-Json
if($hash -ne $manifest.deterministicEvidence.expectedSha256){throw "R3 deterministic Evidence identity mismatch: $hash"};Write-Host "R3 deterministic Composition Evidence SHA-256: $hash"
Invoke-Stage 'Source Manifest' {Invoke-Checked $powershell ($common+@((Join-Path $testsRoot 'tools\Verify-SourceManifest.ps1')))}
Write-Host '============================================================'
Write-Host 'SGE4 LEVEL 4 V2 R3 CANONICAL COMPOSITION PASSED'
Write-Host 'R3 COMPLETE'
Write-Host 'Graph scenarios: 7'
Write-Host 'Contract/proposal rejections: 24'
Write-Host 'Verifier dependency on Composition Planner: None'
Write-Host 'Runtime capability added: None'
Write-Host 'Runtime Candidate-policy authorization: None'
Write-Host 'NEXT: R4 DYNAMIC INVOCATION AND HISTORY'
Write-Host '============================================================'
