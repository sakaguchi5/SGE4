param(
    [Parameter(Mandatory=$true)]
    [ValidateSet('Dev','Gate','Regression','Freeze')]
    [string]$Suite,
    [ValidateSet('Debug','Release')]
    [string]$Configuration='Debug'
)
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path

function Invoke-Checked([string]$FilePath,[string[]]$Arguments=@()) {
    & $FilePath @Arguments
    if($LASTEXITCODE -ne 0){throw "Command failed with exit code $LASTEXITCODE`: $FilePath"}
}

foreach($verifier in @('Verify-Spiral2ResearchContract.ps1','Verify-Spiral2CU1.ps1','Verify-Spiral2CU2.ps1','Verify-Spiral2CU3.ps1','Verify-Spiral2CU4.ps1','Verify-Spiral2CU5.ps1','Verify-Spiral2CU6.ps1')) {
    Invoke-Checked 'powershell.exe' @('-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',(Join-Path $PSScriptRoot "tools\$verifier"))
}

$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$msbuild=&$vswhere -latest -products * -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe'|Select-Object -Last 1
if(-not$msbuild){throw 'MSBuild was not found.'}
Invoke-Checked $msbuild @((Join-Path $root 'SemanticGpuEngine4-5.sln'),'/m','/nr:false','/nologo','/t:Build',"/p:Configuration=$Configuration",'/p:Platform=x64','/v:minimal')
$bin=Join-Path $root "build\bin\x64\$Configuration"
$tests=if($Suite -eq 'Dev'){@('90_Spiral2SemanticTests','91_Spiral2DynamicInvocationTests','92_Spiral2CandidateGraphTests','93_Spiral2AuthorityMutationTests','94_Spiral2LeafPackageTests')}else{@('90_Spiral2SemanticTests','91_Spiral2DynamicInvocationTests','92_Spiral2CandidateGraphTests','93_Spiral2AuthorityMutationTests','94_Spiral2LeafPackageTests','95_Spiral2CompositionTests','98_Spiral2FreezeTests')}
foreach($test in $tests){Invoke-Checked (Join-Path $bin "$test.exe")}
Invoke-Checked (Join-Path $bin '99_Spiral2Launcher.exe') @('--self-test','--output',(Join-Path $root "build\tests\spiral2\$Suite-$Configuration-self-test"))

if($Suite -in @('Regression','Freeze')) {
    Invoke-Checked 'powershell.exe' @('-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',(Join-Path $PSScriptRoot 'Run-Spiral2CU5.ps1'))
}
Write-Host "SGE4-5 Spiral 2 suite passed: $Suite ($Configuration)"
