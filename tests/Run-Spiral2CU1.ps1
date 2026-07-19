$ErrorActionPreference = 'Stop'
$testsRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$root = Split-Path -Parent $testsRoot
. (Join-Path $testsRoot 'tools\Sha256.ps1')

function Invoke-Checked([string]$FilePath,[string[]]$Arguments=@()) { & $FilePath @Arguments; if($LASTEXITCODE-ne 0){throw "Command failed: $FilePath"} }
function Get-MSBuild {
    $vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    $found=& $vswhere -latest -products * -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe'
    $value=$found|Select-Object -Last 1; if(-not $value){throw 'MSBuild was not found.'}; return $value
}
function Equal([string]$a,[string]$b) {
    $x=[IO.File]::ReadAllBytes($a);$y=[IO.File]::ReadAllBytes($b);if($x.Length-ne$y.Length){return $false}
    for($i=0;$i-lt$x.Length;++$i){if($x[$i]-ne$y[$i]){return $false}};return $true
}

Invoke-Checked 'powershell.exe' @('-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',(Join-Path $testsRoot 'tools\Verify-SGE4_5Identity.ps1'))
Invoke-Checked 'powershell.exe' @('-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',(Join-Path $root 'verify_dependencies.ps1'))
Invoke-Checked 'powershell.exe' @('-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',(Join-Path $testsRoot 'tools\Verify-Spiral2ResearchContract.ps1'))
Invoke-Checked 'powershell.exe' @('-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',(Join-Path $testsRoot 'tools\Verify-Spiral2CU1.ps1'))
Invoke-Checked 'powershell.exe' @('-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',(Join-Path $testsRoot 'tools\Verify-SourceManifest.ps1'))
$msbuild=Get-MSBuild;$solutionDir=$root.TrimEnd('\','/')+'\'
foreach($configuration in @('Debug','Release')){Invoke-Checked $msbuild @((Join-Path $root 'SemanticGpuEngine4-5.sln'),'/m','/nologo','/t:Build',"/p:Configuration=$configuration",'/p:Platform=x64','/v:minimal')}
$out=Join-Path $root 'build\tests\spiral2-cu1';New-Item -ItemType Directory -Force -Path $out|Out-Null
$debug90=Join-Path $root 'build\bin\x64\Debug\90_Spiral2SemanticTests.exe';$release90=Join-Path $root 'build\bin\x64\Release\90_Spiral2SemanticTests.exe'
$debug91=Join-Path $root 'build\bin\x64\Debug\91_Spiral2DynamicInvocationTests.exe';$release91=Join-Path $root 'build\bin\x64\Release\91_Spiral2DynamicInvocationTests.exe'
foreach($exe in @($debug90,$release90,$debug91,$release91)){Invoke-Checked $exe}
$d90a=Join-Path $out 'semantic-debug-a.bin';$d90b=Join-Path $out 'semantic-debug-b.bin';$r90=Join-Path $out 'semantic-release.bin'
$d91a=Join-Path $out 'corpus-debug-a.bin';$d91b=Join-Path $out 'corpus-debug-b.bin';$r91=Join-Path $out 'corpus-release.bin'
Invoke-Checked $debug90 @('--emit',$d90a);Invoke-Checked $debug90 @('--emit',$d90b);Invoke-Checked $release90 @('--emit',$r90)
Invoke-Checked $debug91 @('--emit',$d91a);Invoke-Checked $debug91 @('--emit',$d91b);Invoke-Checked $release91 @('--emit',$r91)
if(-not(Equal $d90a $d90b)-or-not(Equal $d90a $r90)){throw 'Hierarchy semantic is not fresh-process Debug/Release deterministic.'}
if(-not(Equal $d91a $d91b)-or-not(Equal $d91a $r91)){throw 'Hierarchy corpus is not fresh-process Debug/Release deterministic.'}
Write-Host '============================================================'
Write-Host 'SGE4-5 SPIRAL 2 COMPLETION UNIT 1 PASSED'
Write-Host 'Stages: S2-03 canonical hierarchy / S2-04 dynamic authority / S2-05 corpus'
Write-Host 'Corpus: H00-H08 x F00-F11 = 108 deterministic combinations'
Write-Host "Semantic SHA-256: $(Get-SGE4FileSha256 $d90a)"
Write-Host "Corpus SHA-256: $(Get-SGE4FileSha256 $d91a)"
Write-Host '============================================================'
