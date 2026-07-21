$ErrorActionPreference='Stop'
$testsRoot=Split-Path -Parent $MyInvocation.MyCommand.Path
$root=Split-Path -Parent $testsRoot
. (Join-Path $testsRoot 'tools\Sha256.ps1')
function Invoke-Checked([string]$File,[string[]]$Arguments=@()){& $File @Arguments;if($LASTEXITCODE-ne 0){throw "Command failed: $File $Arguments"}}
function Get-MSBuild{ $vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe';if(-not(Test-Path $vswhere)){throw 'vswhere.exe missing'};$msbuild=& $vswhere -latest -products * -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe'|Select-Object -Last 1;if(-not$msbuild){throw 'MSBuild missing'};return $msbuild }
function Test-BytesEqual([string]$A,[string]$B){$x=[IO.File]::ReadAllBytes($A);$y=[IO.File]::ReadAllBytes($B);if($x.Length-ne$y.Length){return $false};for($i=0;$i-lt$x.Length;++$i){if($x[$i]-ne$y[$i]){return $false}};return $true}
function Invoke-Spiral3Checks([string[]]$Extra=@()){foreach($script in @('tools\Verify-SGE4_5Identity.ps1','tools\Verify-Spiral3ResearchContract.ps1')+$Extra+@('tools\Verify-SourceManifest.ps1')){Invoke-Checked powershell.exe @('-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',(Join-Path $testsRoot $script))};Invoke-Checked powershell.exe @('-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',(Join-Path $root 'verify_dependencies.ps1'))}
function Build-Spiral3{ $m=Get-MSBuild;foreach($c in @('Debug','Release')){Invoke-Checked $m @((Join-Path $root 'SemanticGpuEngine4-5.sln'),'/m','/nologo','/t:Build',"/p:Configuration=$c",'/p:Platform=x64','/v:minimal')} }
