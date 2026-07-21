param([switch]$IncludeRealGpu)
$ErrorActionPreference = 'Stop'
$testsRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$root = Split-Path -Parent $testsRoot

function Invoke-Checked([string]$file,[string[]]$arguments=@()) {
    & $file @arguments
    if ($LASTEXITCODE -ne 0) { throw "Command failed: $file $arguments" }
}
function Equal-Files([string]$left,[string]$right) {
    $a=[IO.File]::ReadAllBytes($left);$b=[IO.File]::ReadAllBytes($right)
    if($a.Length-ne$b.Length){return $false}
    for($i=0;$i-lt$a.Length;++$i){if($a[$i]-ne$b[$i]){return $false}}
    return $true
}

Invoke-Checked powershell.exe @('-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',(Join-Path $testsRoot 'tools\Verify-Spiral2FinalAuthority.ps1'))
Invoke-Checked powershell.exe @('-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',(Join-Path $testsRoot 'tools\Verify-Spiral2CU6.ps1'))
Invoke-Checked powershell.exe @('-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',(Join-Path $testsRoot 'Run-Spiral2CU2.ps1'))
Invoke-Checked powershell.exe @('-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',(Join-Path $testsRoot 'Run-Spiral2CU3.ps1'))
Invoke-Checked powershell.exe @('-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',(Join-Path $testsRoot 'Run-Spiral2CU5.ps1'))

$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$msbuild=&$vswhere -latest -products * -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe'|Select-Object -Last 1
if(-not$msbuild){throw 'MSBuild missing'}
foreach($configuration in @('Debug','Release')) {
    Invoke-Checked $msbuild @((Join-Path $root '99_Spiral2Launcher\99_Spiral2Launcher.vcxproj'),'/m','/nr:false','/nologo','/t:Build',"/p:Configuration=$configuration",'/p:Platform=x64',"/p:SolutionDir=$($root.TrimEnd('\')+'\')",'/v:minimal')
}
$output=Join-Path $root 'build\tests\spiral2-final-qualification'
New-Item -ItemType Directory -Force -Path $output|Out-Null
$debugOutput=Join-Path $output 'debug'
$releaseOutput=Join-Path $output 'release'
$debugLauncher=Join-Path $root 'build\bin\x64\Debug\99_Spiral2Launcher.exe'
$releaseLauncher=Join-Path $root 'build\bin\x64\Release\99_Spiral2Launcher.exe'
Invoke-Checked $debugLauncher @('--self-test','--output',$debugOutput)
Invoke-Checked $releaseLauncher @('--self-test','--output',$releaseOutput)
$debugEvidence=Join-Path $debugOutput 'spiral2_measurement_evidence_v2.bin'
$releaseEvidence=Join-Path $releaseOutput 'spiral2_measurement_evidence_v2.bin'
if(-not(Equal-Files $debugEvidence $releaseEvidence)){throw 'Measurement Evidence V2 format is not Debug/Release deterministic.'}

if($IncludeRealGpu) {
    $hardware=Join-Path $output 'hardware'
    Invoke-Checked $debugLauncher @('--measure','--output',$hardware)
    Invoke-Checked $debugLauncher @('--evidence','--report','--input',(Join-Path $hardware 'spiral2_measurement_evidence_v2.bin'),'--output',$hardware)
}

Write-Host '============================================================'
Write-Host 'SGE4-5 SPIRAL 2 FINAL QUALIFICATION PASSED'
Write-Host 'Items 1-5: Debug/Release authority, Package, WARP and recovery suites passed.'
Write-Host 'Item 6: candidate-specific Measurement Evidence V2 format passed.'
if($IncludeRealGpu){Write-Host 'Item 6 hardware measurement and report also passed.'}
else{Write-Host 'Real GPU measurement was not requested. Re-run with -IncludeRealGpu when needed.'}
Write-Host '============================================================'
