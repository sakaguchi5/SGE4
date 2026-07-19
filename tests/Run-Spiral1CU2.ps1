param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('Stage06','Stage07','CU2')]
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
function Build-Test([string]$Name, [string]$Configuration) {
    $msbuild = Get-MSBuild
    $project = Join-Path $root "$Name\$Name.vcxproj"
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
    'tools\Verify-Spiral1CU2.ps1')) {
    Invoke-Checked 'powershell.exe' @('-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',(Join-Path $testsRoot $script))
}
Invoke-Checked 'powershell.exe' @('-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',(Join-Path $root 'verify_dependencies.ps1'))

foreach ($configuration in @('Debug','Release')) {
    Build-Test '71_Spiral1RepresentationPlanningTests' $configuration
    Build-Test '72_Spiral1RepresentationAuthorityTests' $configuration
}

$debugBin = Join-Path $root 'build\bin\x64\Debug'
$releaseBin = Join-Path $root 'build\bin\x64\Release'
$output = Join-Path $root "build\tests\spiral1-$($Mode.ToLowerInvariant())"
Remove-Item -Recurse -Force -ErrorAction SilentlyContinue $output
New-Item -ItemType Directory -Force -Path $output | Out-Null

if ($Mode -eq 'Stage06') {
    $debugExe = Join-Path $debugBin '71_Spiral1RepresentationPlanningTests.exe'
    $releaseExe = Join-Path $releaseBin '71_Spiral1RepresentationPlanningTests.exe'
    $emit = '--emit-stage06'
}
else {
    $debugExe = Join-Path $debugBin '72_Spiral1RepresentationAuthorityTests.exe'
    $releaseExe = Join-Path $releaseBin '72_Spiral1RepresentationAuthorityTests.exe'
    if ($Mode -eq 'Stage07') { $emit = '--emit-stage07' }
    else { $emit = '--emit-cu2' }
}

Invoke-Checked $debugExe
Invoke-Checked $releaseExe
$a = Join-Path $output 'debug-a.bin'
$b = Join-Path $output 'debug-b.bin'
$r = Join-Path $output 'release.bin'
Invoke-Checked $debugExe @($emit,$a)
Invoke-Checked $debugExe @($emit,$b)
Invoke-Checked $releaseExe @($emit,$r)
if (-not (Test-BytesEqual $a $b)) { throw 'Fresh Debug artifacts differ.' }
if (-not (Test-BytesEqual $a $r)) { throw 'Debug and Release artifacts differ.' }
$digest = (Get-FileHash -Algorithm SHA256 -LiteralPath $a).Hash

Write-Host '============================================================'
if ($Mode -eq 'Stage06') {
    Write-Host 'SGE4-5 STAGE 06 RAW REPRESENTATION CANDIDATES PASSED'
    Write-Host 'Lowerings: MatrixExpandedComputeV1 / DirectPgaMotorComputeV1'
}
elseif ($Mode -eq 'Stage07') {
    Write-Host 'SGE4-5 STAGE 07 INDEPENDENT REPRESENTATION AUTHORITY PASSED'
    Write-Host 'Authority: independent constant rederivation and opaque verifier seal'
}
else {
    Write-Host 'SGE4-5 SPIRAL 1 COMPLETION UNIT 2 PASSED'
    Write-Host 'Stages: 06 Raw Representation Candidates + 07 Independent Representation Verifier'
    Write-Host 'Projects: 63, 64, 65, 71, 72'
    Write-Host 'Ready for Completion Unit 3: Dual Frozen Leaf Qualification'
}
Write-Host "Fresh-process artifact SHA-256: $digest"
Write-Host 'Debug A / Debug B / Release: byte-identical'
Write-Host '============================================================'
