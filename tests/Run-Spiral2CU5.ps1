$ErrorActionPreference='Stop'
$testsRoot=Split-Path -Parent $MyInvocation.MyCommand.Path
$root=Split-Path -Parent $testsRoot
. (Join-Path $testsRoot 'tools\Sha256.ps1')
function Invoke-Checked([string]$file,[string[]]$arguments=@()){&$file @arguments;if($LASTEXITCODE-ne 0){throw "Command failed: $file $arguments"}}
function Equal-Files([string]$left,[string]$right){$a=[IO.File]::ReadAllBytes($left);$b=[IO.File]::ReadAllBytes($right);if($a.Length-ne$b.Length){return $false};for($i=0;$i-lt$a.Length;++$i){if($a[$i]-ne$b[$i]){return $false}};return $true}
foreach($script in @('tools\Verify-SGE4_5Identity.ps1','tools\Verify-Spiral2ResearchContract.ps1','tools\Verify-Spiral2CU5.ps1','tools\Verify-SourceManifest.ps1')){Invoke-Checked powershell.exe @('-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',(Join-Path $testsRoot $script))}
Invoke-Checked powershell.exe @('-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',(Join-Path $root 'verify_dependencies.ps1'))
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe';$msbuild=&$vswhere -latest -products * -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe'|Select-Object -Last 1;if(-not$msbuild){throw 'MSBuild missing'}
foreach($configuration in @('Debug','Release')){Invoke-Checked $msbuild @((Join-Path $root 'SemanticGpuEngine4-5.sln'),'/m','/nr:false','/nologo','/t:96_Spiral2WarpObservationTests;97_Spiral2RecoveryTests;98_Spiral2FreezeTests',"/p:Configuration=$configuration",'/p:Platform=x64','/v:q')}
$output=Join-Path $root 'build\tests\spiral2-cu5';New-Item -ItemType Directory -Force -Path $output|Out-Null
foreach($configuration in @('Debug','Release')){
    $bin=Join-Path $root "build\bin\x64\$configuration"
    $warp=Join-Path $bin '96_Spiral2WarpObservationTests.exe';for($index=0;$index-lt9;++$index){Invoke-Checked $warp @('--warp-index',[string]$index)};Invoke-Checked $warp @('--reorder')
    $recovery=Join-Path $bin '97_Spiral2RecoveryTests.exe';Invoke-Checked $recovery @('--controlled','--emit',(Join-Path $output "$configuration-controlled.bin"));$actual=Join-Path $output "$configuration-actual.bin";$fresh=Join-Path $output "$configuration-fresh.bin";Invoke-Checked $recovery @('--actual-removal','--emit',$actual);Invoke-Checked $recovery @('--fresh-rematerialize','--emit',$fresh);if(-not(Equal-Files $actual $fresh)){throw "$configuration actual-removal/fresh-process evidence differs."}
}
$debugFreeze=Join-Path $root 'build\bin\x64\Debug\98_Spiral2FreezeTests.exe';$releaseFreeze=Join-Path $root 'build\bin\x64\Release\98_Spiral2FreezeTests.exe';$freezeA=Join-Path $output 'freeze-debug-a.bin';$freezeB=Join-Path $output 'freeze-debug-b.bin';$freezeRelease=Join-Path $output 'freeze-release.bin';Invoke-Checked $debugFreeze @('--emit',$freezeA);Invoke-Checked $debugFreeze @('--emit',$freezeB);Invoke-Checked $releaseFreeze @('--emit',$freezeRelease);if(-not(Equal-Files $freezeA $freezeB)-or-not(Equal-Files $freezeA $freezeRelease)){throw 'Spiral 2 Freeze bundle is not Debug/Release/fresh-process deterministic.'}
if(-not(Equal-Files (Join-Path $output 'Debug-controlled.bin') (Join-Path $output 'Release-controlled.bin'))){throw 'Controlled recovery readback differs across Debug/Release.'}
Write-Host '============================================================';Write-Host 'SGE4-5 SPIRAL 2 COMPLETION UNIT 5 PASSED';Write-Host '216 WARP H/F executions, order permutation, determinism, controlled/actual recovery';Write-Host "Freeze evidence SHA-256: $(Get-SGE4FileSha256 $freezeA)";Write-Host "F07 rematerialization SHA-256: $(Get-SGE4FileSha256 (Join-Path $output 'Debug-fresh.bin'))";Write-Host '============================================================'
