$ErrorActionPreference='Stop'
$testsRoot=Split-Path -Parent $MyInvocation.MyCommand.Path
$root=Split-Path -Parent $testsRoot
. (Join-Path $testsRoot 'tools\Sha256.ps1')
function Invoke-Checked([string]$file,[string[]]$arguments=@()){&$file @arguments;if($LASTEXITCODE-ne 0){throw "Command failed: $file"}}
function Equal-Files([string]$left,[string]$right){$a=[IO.File]::ReadAllBytes($left);$b=[IO.File]::ReadAllBytes($right);if($a.Length-ne$b.Length){return $false};for($i=0;$i-lt$a.Length;++$i){if($a[$i]-ne$b[$i]){return $false}};return $true}
foreach($script in @('tools\Verify-SGE4_5Identity.ps1','tools\Verify-Spiral2ResearchContract.ps1','tools\Verify-Spiral2CU4.ps1','tools\Verify-SourceManifest.ps1')){Invoke-Checked powershell.exe @('-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',(Join-Path $testsRoot $script))}
Invoke-Checked powershell.exe @('-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',(Join-Path $root 'verify_dependencies.ps1'))
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$msbuild=&$vswhere -latest -products * -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe'|Select-Object -Last 1
if(-not$msbuild){throw 'MSBuild missing'}
foreach($configuration in @('Debug','Release')){Invoke-Checked $msbuild @((Join-Path $root 'SemanticGpuEngine4-5.sln'),'/m','/nologo','/t:95_Spiral2CompositionTests',"/p:Configuration=$configuration",'/p:Platform=x64','/v:q')}
$debug=Join-Path $root 'build\bin\x64\Debug\95_Spiral2CompositionTests.exe'
$release=Join-Path $root 'build\bin\x64\Release\95_Spiral2CompositionTests.exe'
Invoke-Checked $debug
Invoke-Checked $release
$output=Join-Path $root 'build\tests\spiral2-cu4'
New-Item -ItemType Directory -Force -Path $output|Out-Null
$debugA=Join-Path $output 'debug-a.bin';$debugB=Join-Path $output 'debug-b.bin';$releaseFile=Join-Path $output 'release.bin'
Invoke-Checked $debug @('--emit',$debugA);Invoke-Checked $debug @('--emit',$debugB);Invoke-Checked $release @('--emit',$releaseFile)
if(-not(Equal-Files $debugA $debugB)-or-not(Equal-Files $debugA $releaseFile)){throw 'Frozen comparison Composition is not fresh-process Debug/Release deterministic.'}
Write-Host '============================================================'
Write-Host 'SGE4-5 SPIRAL 2 COMPLETION UNIT 4 PASSED'
Write-Host 'Ten-Leaf Frozen comparison Composition and Level 4 sufficiency'
Write-Host "Composition evidence SHA-256: $(Get-SGE4FileSha256 $debugA)"
Write-Host '============================================================'
