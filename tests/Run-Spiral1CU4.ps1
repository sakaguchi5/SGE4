param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('Stage10','Stage11','CU4')]
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
function Build-Test([string]$ProjectName, [string]$Configuration) {
    $msbuild = Get-MSBuild
    $project = Join-Path $root "$ProjectName\$ProjectName.vcxproj"
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
    'tools\Verify-Spiral1CU3.ps1',
    'tools\Verify-Spiral1CU4.ps1')) {
    Invoke-Checked 'powershell.exe' @('-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',(Join-Path $testsRoot $script))
}
Invoke-Checked 'powershell.exe' @('-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',(Join-Path $root 'verify_dependencies.ps1'))

if ($Mode -eq 'Stage10' -or $Mode -eq 'CU4') {
    foreach ($configuration in @('Debug','Release')) { Build-Test '74_Spiral1CompositionTests' $configuration }
}
if ($Mode -eq 'Stage11' -or $Mode -eq 'CU4') {
    foreach ($configuration in @('Debug','Release')) { Build-Test '75_Spiral1WarpObservationTests' $configuration }
}

$output = Join-Path $root "build\tests\spiral1-$($Mode.ToLowerInvariant())"
Remove-Item -Recurse -Force -ErrorAction SilentlyContinue $output
New-Item -ItemType Directory -Force -Path $output | Out-Null

$architectureDigest = $null
$warpDigest = $null
if ($Mode -eq 'Stage10' -or $Mode -eq 'CU4') {
    $debugExe = Join-Path $root 'build\bin\x64\Debug\74_Spiral1CompositionTests.exe'
    $releaseExe = Join-Path $root 'build\bin\x64\Release\74_Spiral1CompositionTests.exe'
    Invoke-Checked $debugExe
    Invoke-Checked $releaseExe
    $a = Join-Path $output 'architecture-debug-a.bin'
    $b = Join-Path $output 'architecture-debug-b.bin'
    $r = Join-Path $output 'architecture-release.bin'
    Invoke-Checked $debugExe @('--emit',$a)
    Invoke-Checked $debugExe @('--emit',$b)
    Invoke-Checked $releaseExe @('--emit',$r)
    if (-not (Test-BytesEqual $a $b)) { throw 'Fresh Debug Frozen Comparison architecture evidence differs.' }
    if (-not (Test-BytesEqual $a $r)) { throw 'Debug and Release Frozen Comparison architecture evidence differs.' }
    $architectureDigest = Get-SGE4FileSha256 $a
}
if ($Mode -eq 'Stage11' -or $Mode -eq 'CU4') {
    $debugExe = Join-Path $root 'build\bin\x64\Debug\75_Spiral1WarpObservationTests.exe'
    $releaseExe = Join-Path $root 'build\bin\x64\Release\75_Spiral1WarpObservationTests.exe'
    $a = Join-Path $output 'warp-debug-a.bin'
    $b = Join-Path $output 'warp-debug-b.bin'
    $r = Join-Path $output 'warp-release.bin'
    Invoke-Checked $debugExe @('--emit',$a)
    Invoke-Checked $debugExe @('--emit',$b)
    Invoke-Checked $releaseExe @('--emit',$r)
    if (-not (Test-BytesEqual $a $b)) { throw 'Fresh Debug WARP corpus evidence differs.' }
    if (-not (Test-BytesEqual $a $r)) { throw 'Debug and Release WARP corpus evidence differs.' }
    $warpDigest = Get-SGE4FileSha256 $a
}

Write-Host '============================================================'
if ($Mode -eq 'Stage10') {
    Write-Host 'SGE4-5 STAGE 10 FOUR-LEAF FROZEN COMPARISON PASSED'
    Write-Host 'Composition: L0 Source / L1 Matrix / L2 Direct PGA / L3 Comparison'
    Write-Host "Fresh-process architecture evidence SHA-256: $architectureDigest"
}
elseif ($Mode -eq 'Stage11') {
    Write-Host 'SGE4-5 STAGE 11 WARP OBSERVATION CORPUS PASSED'
    Write-Host 'Corpus: S00-S15 under one Frozen Comparison architecture'
    Write-Host "Fresh-process WARP evidence SHA-256: $warpDigest"
}
else {
    Write-Host 'SGE4-5 SPIRAL 1 COMPLETION UNIT 4 PASSED'
    Write-Host 'Stages: 10 Four-Leaf Frozen Composition + 11 WARP S00-S15 Corpus'
    Write-Host 'Authority: role wiring, schedule, resources, state and synchronization are frozen'
    Write-Host 'Observation: GPU Comparison Records equal deterministic CPU Observation V2 records'
    Write-Host 'Ready for Completion Unit 5: Architecture Final Freeze'
    Write-Host "Architecture evidence SHA-256: $architectureDigest"
    Write-Host "WARP corpus evidence SHA-256: $warpDigest"
}
Write-Host 'Debug A / Debug B / Release: byte-identical'
Write-Host '============================================================'
