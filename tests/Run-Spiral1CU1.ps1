param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('Stage04','Stage05','CU1')]
    [string]$Mode
)

$ErrorActionPreference = 'Stop'
$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
[Console]::InputEncoding = $utf8NoBom
[Console]::OutputEncoding = $utf8NoBom
$OutputEncoding = $utf8NoBom

$testsRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$root = Split-Path -Parent $testsRoot
. (Join-Path $testsRoot 'tools\Sha256.ps1')

function Invoke-Checked([string]$FilePath, [string[]]$Arguments = @()) {
    & $FilePath @Arguments
    if ($LASTEXITCODE -ne 0) { throw "Command failed: $FilePath" }
}
function Get-MSBuild {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path -LiteralPath $vswhere)) { throw 'vswhere.exe was not found.' }
    $found = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe'
    $msbuild = $found | Select-Object -Last 1
    if (-not $msbuild) { throw 'MSBuild was not found.' }
    return $msbuild
}
function Build-Test([string]$Configuration) {
    $msbuild = Get-MSBuild
    $project = Join-Path $root '70_Spiral1SemanticTests\70_Spiral1SemanticTests.vcxproj'
    $solutionDir = $root.TrimEnd('\','/') + '\'
    Invoke-Checked $msbuild @($project,'/m','/nologo','/t:Build',"/p:Configuration=$Configuration",'/p:Platform=x64',"/p:SolutionDir=$solutionDir")
}
function Test-BytesEqual([string]$A,[string]$B) {
    $aBytes = [IO.File]::ReadAllBytes($A)
    $bBytes = [IO.File]::ReadAllBytes($B)
    if ($aBytes.Length -ne $bBytes.Length) { return $false }
    for ($i=0; $i -lt $aBytes.Length; ++$i) { if ($aBytes[$i] -ne $bBytes[$i]) { return $false } }
    return $true
}

Invoke-Checked 'powershell.exe' @('-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',(Join-Path $testsRoot 'tools\Verify-SGE4_5Identity.ps1'))
Invoke-Checked 'powershell.exe' @('-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',(Join-Path $root 'verify_dependencies.ps1'))
Invoke-Checked 'powershell.exe' @('-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',(Join-Path $testsRoot 'tools\Verify-ScriptContracts.ps1'))
Invoke-Checked 'powershell.exe' @('-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',(Join-Path $testsRoot 'tools\Verify-SourceManifest.ps1'))
Invoke-Checked 'powershell.exe' @('-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',(Join-Path $testsRoot 'tools\Verify-Spiral1ResearchContract.ps1'))
Invoke-Checked 'powershell.exe' @('-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',(Join-Path $testsRoot 'tools\Verify-Spiral1CU1.ps1'))

Build-Test 'Debug'
Build-Test 'Release'
$debugExe = Join-Path $root 'build\bin\x64\Debug\70_Spiral1SemanticTests.exe'
$releaseExe = Join-Path $root 'build\bin\x64\Release\70_Spiral1SemanticTests.exe'
$output = Join-Path $root "build\tests\spiral1-$($Mode.ToLowerInvariant())"
Remove-Item -Recurse -Force -ErrorAction SilentlyContinue $output
New-Item -ItemType Directory -Force -Path $output | Out-Null

if ($Mode -eq 'Stage04') {
    Invoke-Checked $debugExe @('--stage04')
    Invoke-Checked $releaseExe @('--stage04')
    $emit = '--emit-stage04'
}
elseif ($Mode -eq 'Stage05') {
    Invoke-Checked $debugExe @('--stage05')
    Invoke-Checked $releaseExe @('--stage05')
    $emit = '--emit-stage05'
}
else {
    Invoke-Checked $debugExe
    Invoke-Checked $releaseExe
    $emit = '--emit-cu1'
}

$a = Join-Path $output 'debug-a.bin'
$b = Join-Path $output 'debug-b.bin'
$r = Join-Path $output 'release.bin'
Invoke-Checked $debugExe @($emit,$a)
Invoke-Checked $debugExe @($emit,$b)
Invoke-Checked $releaseExe @($emit,$r)
if (-not (Test-BytesEqual $a $b)) { throw 'Fresh Debug process artifacts are not byte-identical.' }
if (-not (Test-BytesEqual $a $r)) { throw 'Debug and Release artifacts are not byte-identical.' }
$digest = Get-SGE4FileSha256 $a

Write-Host '============================================================'
if ($Mode -eq 'Stage04') {
    Write-Host 'SGE4-5 STAGE 04 PGA CANONICAL SEMANTIC PASSED'
    Write-Host 'Canonical meaning: binary64 PGA Motor, no Cartesian Matrix'
    Write-Host 'Authority: builder, validation, serialization and identity qualified'
}
elseif ($Mode -eq 'Stage05') {
    Write-Host 'SGE4-5 STAGE 05 CORPUS AND OBSERVATION FOUNDATION PASSED'
    Write-Host 'Corpus: S00-S15 / 31 cases / 25921 ordered points'
    Write-Host 'Reference: independent CPU binary64 path qualified'
}
else {
    Write-Host 'SGE4-5 SPIRAL 1 COMPLETION UNIT 1 PASSED'
    Write-Host 'Stages: 04 PGA Canonical Semantic + 05 Corpus/Reference/Observation'
    Write-Host 'Projects: 60, 61, 62, 67, 70'
    Write-Host 'Scope: CPU-only semantic and observation foundation'
    Write-Host 'Ready for Completion Unit 2: Representation Authority'
}
Write-Host "Fresh-process artifact SHA-256: $digest"
Write-Host 'Debug A / Debug B / Release: byte-identical'
Write-Host '============================================================'
