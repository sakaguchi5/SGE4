$ErrorActionPreference='Stop'
$testsRoot=Split-Path -Parent $MyInvocation.MyCommand.Path
$root=Split-Path -Parent $testsRoot
function Invoke-Step([string]$Name,[scriptblock]$Action){Write-Host "[L4V2-R5] START $Name";$watch=[Diagnostics.Stopwatch]::StartNew();& $Action;$watch.Stop();Write-Host ("[L4V2-R5] DONE  {0} elapsed={1}" -f $Name,$watch.Elapsed)}
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
Invoke-Step 'R5 static Runtime and Recovery boundaries' {Invoke-PS 'tools\Verify-Level4V2R5.ps1' @('-Mode','Applied')}
$msbuild=Get-MSBuild
$canonicalProject=Join-Path $root '210_Level4V2RuntimeRecoveryTests\210_Level4V2RuntimeRecoveryTests.vcxproj'
$windowsProject=Join-Path $root '211_Level4V2RuntimeWindowsQualification\211_Level4V2RuntimeWindowsQualification.vcxproj'
Invoke-Step 'Build Canonical and Windows Debug/Release' {foreach($configuration in @('Debug','Release')){foreach($project in @($canonicalProject,$windowsProject)){Invoke-Checked $msbuild @($project,'/m','/nr:false','/nologo',"/p:Configuration=$configuration",'/p:Platform=x64',"/p:SolutionDir=$root\",'/v:minimal')}}}
$output=Join-Path $root 'build\tests\level4v2-r5';Remove-Item -Recurse -Force -ErrorAction SilentlyContinue $output;New-Item -ItemType Directory -Force $output|Out-Null
$canonicalDebug=Join-Path $root 'build\bin\x64\Debug\210_Level4V2RuntimeRecoveryTests.exe';$canonicalRelease=Join-Path $root 'build\bin\x64\Release\210_Level4V2RuntimeRecoveryTests.exe'
$windowsDebug=Join-Path $root 'build\bin\x64\Debug\211_Level4V2RuntimeWindowsQualification.exe';$windowsRelease=Join-Path $root 'build\bin\x64\Release\211_Level4V2RuntimeWindowsQualification.exe'
$cd=Join-Path $output 'runtime-recovery-evidence-debug.bin';$cr=Join-Path $output 'runtime-recovery-evidence-release.bin';$wd=Join-Path $output 'windows-runtime-recovery-evidence-debug.bin';$wr=Join-Path $output 'windows-runtime-recovery-evidence-release.bin'
Invoke-Step 'Canonical Debug self-test' {Invoke-Checked $canonicalDebug @('--emit',$cd)}
Invoke-Step 'Canonical Release self-test' {Invoke-Checked $canonicalRelease @('--emit',$cr)}
Invoke-Step 'Windows WARP Debug qualification' {Invoke-Checked $windowsDebug @('--emit',$wd)}
Invoke-Step 'Windows WARP Release qualification' {Invoke-Checked $windowsRelease @('--emit',$wr)}
Invoke-Step 'Deterministic Evidence' {
 if(-not(Test-BytesEqual $cd $cr)){throw 'R5 Canonical Debug/Release Evidence differs.'};$ch=(Get-FileHash -Algorithm SHA256 -LiteralPath $cd).Hash.ToLowerInvariant();if($ch -ne '40d78dc35894a55e50b314bd0afcbb60966f92e942ac70856155964cd8e66498'){throw "R5 Canonical Evidence SHA mismatch: $ch"}
 if(-not(Test-BytesEqual $wd $wr)){throw 'R5 Windows Debug/Release Evidence differs.'};$wh=(Get-FileHash -Algorithm SHA256 -LiteralPath $wd).Hash.ToLowerInvariant();if($wh -ne '1e4adaa04dc8d1d235f8f3438f4295b09b36480174f6a7d9fa0fbaba79ea7a9a'){throw "R5 Windows Evidence SHA mismatch: $wh"}
 Write-Host "R5 Canonical Evidence SHA-256: $ch";Write-Host "R5 Windows Evidence SHA-256: $wh"
}
Invoke-Step 'Source Manifest' {Invoke-PS 'tools\Verify-SourceManifest.ps1'}
Write-Host '============================================================'
Write-Host 'SGE4 LEVEL 4 V2 R5 RUNTIME AND RECOVERY PASSED'
Write-Host 'R5 COMPLETE'
Write-Host 'Canonical scenarios: 10'
Write-Host 'Runtime/recovery rejections: 12'
Write-Host 'Windows: WARP controlled rebuild and actual removal passed'
Write-Host 'Runtime Candidate-policy authorization: None'
Write-Host 'NEXT: R6 MIGRATION CORPUS'
Write-Host '============================================================'
