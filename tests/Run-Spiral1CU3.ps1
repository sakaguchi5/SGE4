param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('Stage08','Stage09','CU3')]
    [string]$Mode
)

$ErrorActionPreference = 'Stop'
$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
[Console]::InputEncoding = $utf8NoBom
[Console]::OutputEncoding = $utf8NoBom
$OutputEncoding = $utf8NoBom

$testsRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$root = Split-Path -Parent $testsRoot

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
    $project = Join-Path $root '73_Spiral1LeafPackageTests\73_Spiral1LeafPackageTests.vcxproj'
    $solutionDir = $root.TrimEnd('\','/') + '\'
    Invoke-Checked $msbuild @($project,'/m','/nologo','/t:Build',"/p:Configuration=$Configuration",'/p:Platform=x64',"/p:SolutionDir=$solutionDir")
}
function Test-BytesEqual([string]$A, [string]$B) {
    $aBytes = [IO.File]::ReadAllBytes($A)
    $bBytes = [IO.File]::ReadAllBytes($B)
    if ($aBytes.Length -ne $bBytes.Length) { return $false }
    for ($i = 0; $i -lt $aBytes.Length; ++$i) {
        if ($aBytes[$i] -ne $bBytes[$i]) { return $false }
    }
    return $true
}

foreach ($script in @(
    'tools\Verify-SGE4_5Identity.ps1',
    'tools\Verify-ScriptContracts.ps1',
    'tools\Verify-SourceManifest.ps1',
    'tools\Verify-Spiral1ResearchContract.ps1',
    'tools\Verify-Spiral1CU1.ps1',
    'tools\Verify-Spiral1CU2.ps1',
    'tools\Verify-Spiral1CU3.ps1')) {
    Invoke-Checked 'powershell.exe' @('-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',(Join-Path $testsRoot $script))
}
Invoke-Checked 'powershell.exe' @('-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',(Join-Path $root 'verify_dependencies.ps1'))

foreach ($configuration in @('Debug','Release')) { Build-Test $configuration }
$debugExe = Join-Path $root 'build\bin\x64\Debug\73_Spiral1LeafPackageTests.exe'
$releaseExe = Join-Path $root 'build\bin\x64\Release\73_Spiral1LeafPackageTests.exe'
if ($Mode -eq 'Stage08') { $run = '--stage08'; $emit = '--emit-stage08' }
elseif ($Mode -eq 'Stage09') { $run = '--stage09'; $emit = '--emit-stage09' }
else { $run = '--cu3'; $emit = '--emit-cu3' }

Invoke-Checked $debugExe @($run)
Invoke-Checked $releaseExe @($run)
$output = Join-Path $root "build\tests\spiral1-$($Mode.ToLowerInvariant())"
Remove-Item -Recurse -Force -ErrorAction SilentlyContinue $output
New-Item -ItemType Directory -Force -Path $output | Out-Null
$a = Join-Path $output 'debug-a.bin'
$b = Join-Path $output 'debug-b.bin'
$r = Join-Path $output 'release.bin'
Invoke-Checked $debugExe @($emit,$a)
Invoke-Checked $debugExe @($emit,$b)
Invoke-Checked $releaseExe @($emit,$r)
if (-not (Test-BytesEqual $a $b)) { throw 'Fresh Debug Frozen Leaf evidence differs.' }
if (-not (Test-BytesEqual $a $r)) { throw 'Debug and Release Frozen Leaf evidence differs.' }
$digest = (Get-FileHash -Algorithm SHA256 -LiteralPath $a).Hash

Write-Host '============================================================'
if ($Mode -eq 'Stage08') {
    Write-Host 'SGE4-5 STAGE 08 MATRIX FROZEN LEAF QUALIFICATION PASSED'
    Write-Host 'Lowering: MatrixExpandedComputeV1'
}
elseif ($Mode -eq 'Stage09') {
    Write-Host 'SGE4-5 STAGE 09 DIRECT PGA FROZEN LEAF QUALIFICATION PASSED'
    Write-Host 'Lowering: DirectPgaMotorComputeV1'
}
else {
    Write-Host 'SGE4-5 SPIRAL 1 COMPLETION UNIT 3 PASSED'
    Write-Host 'Stages: 08 Matrix Leaf + 09 Direct PGA Leaf'
    Write-Host 'Authority: verified Representation Plans lower through existing ExecutionPlan authority'
    Write-Host 'Observation: standalone WARP representative cases satisfy one CPU reference contract'
    Write-Host 'Ready for Completion Unit 4: Frozen Comparison Architecture'
}
Write-Host "Fresh-process Frozen Leaf evidence SHA-256: $digest"
Write-Host 'Debug A / Debug B / Release: byte-identical'
Write-Host '============================================================'
