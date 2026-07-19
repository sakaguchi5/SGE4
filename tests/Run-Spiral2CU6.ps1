param(
    [ValidateSet('SelfTest','Measurement','Report','All')]
    [string]$Mode = 'All',
    [string]$OutputDirectory = 'build/tests/spiral2/measurement'
)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$output = Join-Path $root $OutputDirectory

function Invoke-Checked([string]$FilePath,[string[]]$Arguments) {
    & $FilePath @Arguments
    if ($LASTEXITCODE -ne 0) { throw "Command failed with exit code $LASTEXITCODE`: $FilePath" }
}

Invoke-Checked 'powershell.exe' @('-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',(Join-Path $PSScriptRoot 'tools\Verify-Spiral2CU6.ps1'))
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$msbuild = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -Last 1
if (-not $msbuild) { throw 'MSBuild was not found.' }
$project = Join-Path $root '99_Spiral2Launcher\99_Spiral2Launcher.vcxproj'
$solutionDir = $root.TrimEnd('\','/') + '\'
Invoke-Checked $msbuild @($project,'/m','/nr:false','/nologo','/t:Build','/p:Configuration=Release','/p:Platform=x64',"/p:SolutionDir=$solutionDir",'/v:minimal')
$launcher = Join-Path $root 'build\bin\x64\Release\99_Spiral2Launcher.exe'

if ($Mode -in @('SelfTest','All')) {
    Invoke-Checked $launcher @('--self-test','--warp','--output',(Join-Path $root 'build\tests\spiral2\cu6-self-test'))
}
if ($Mode -in @('Measurement','All')) {
    Invoke-Checked $launcher @('--measure','--output',$output,'--clock-policy','uncontrolled')
}
if ($Mode -in @('Report','All')) {
    Invoke-Checked $launcher @('--evidence','--report','--input',(Join-Path $output 'spiral2_measurement_evidence_v1.bin'),'--output',$output)
}
if ($Mode -eq 'Report') {
    Invoke-Checked 'powershell.exe' @('-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',(Join-Path $PSScriptRoot 'tools\Verify-Spiral2CU6.ps1'),'-RequireClosure')
}
Write-Host "Spiral 2 CU6 runner passed: $Mode"
