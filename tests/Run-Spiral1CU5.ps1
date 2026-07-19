param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('Stage12','CU5')]
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
    for ($i = 0; $i -lt $aBytes.Length; ++$i) { if ($aBytes[$i] -ne $bBytes[$i]) { return $false } }
    return $true
}
function Assert-Triplet([string]$Label, [string]$A, [string]$B, [string]$R) {
    if (-not (Test-BytesEqual $A $B)) { throw "Fresh Debug $Label evidence differs." }
    if (-not (Test-BytesEqual $A $R)) { throw "Debug and Release $Label evidence differs." }
}

foreach ($script in @(
    'tools\Verify-SGE4_5Identity.ps1',
    'tools\Verify-ScriptContracts.ps1',
    'tools\Verify-SourceManifest.ps1',
    'tools\Verify-Spiral1ResearchContract.ps1',
    'tools\Verify-Spiral1CU1.ps1',
    'tools\Verify-Spiral1CU2.ps1',
    'tools\Verify-Spiral1CU3.ps1',
    'tools\Verify-Spiral1CU4.ps1',
    'tools\Verify-Spiral1CU5.ps1')) {
    Invoke-Checked 'powershell.exe' @('-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',(Join-Path $testsRoot $script))
}
Invoke-Checked 'powershell.exe' @('-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',(Join-Path $root 'verify_dependencies.ps1'))

foreach ($project in @(
    '72_Spiral1RepresentationAuthorityTests',
    '73_Spiral1LeafPackageTests',
    '74_Spiral1CompositionTests',
    '75_Spiral1WarpObservationTests',
    '76_Spiral1RecoveryTests',
    '77_Spiral1FreezeTests')) {
    foreach ($configuration in @('Debug','Release')) { Build-Test $project $configuration }
}

$output = Join-Path $root 'build\tests\spiral1-cu5'
Remove-Item -Recurse -Force -ErrorAction SilentlyContinue $output
New-Item -ItemType Directory -Force -Path $output | Out-Null

$debug = Join-Path $root 'build\bin\x64\Debug'
$release = Join-Path $root 'build\bin\x64\Release'
Invoke-Checked (Join-Path $debug '72_Spiral1RepresentationAuthorityTests.exe')
Invoke-Checked (Join-Path $release '72_Spiral1RepresentationAuthorityTests.exe')
Invoke-Checked (Join-Path $debug '73_Spiral1LeafPackageTests.exe')
Invoke-Checked (Join-Path $release '73_Spiral1LeafPackageTests.exe')

$archA = Join-Path $output 'architecture-debug-a.bin'
$archB = Join-Path $output 'architecture-debug-b.bin'
$archR = Join-Path $output 'architecture-release.bin'
Invoke-Checked (Join-Path $debug '74_Spiral1CompositionTests.exe') @('--emit',$archA)
Invoke-Checked (Join-Path $debug '74_Spiral1CompositionTests.exe') @('--emit',$archB)
Invoke-Checked (Join-Path $release '74_Spiral1CompositionTests.exe') @('--emit',$archR)
Assert-Triplet 'Frozen Comparison architecture' $archA $archB $archR

$warpA = Join-Path $output 'warp-debug-a.bin'
$warpB = Join-Path $output 'warp-debug-b.bin'
$warpR = Join-Path $output 'warp-release.bin'
Invoke-Checked (Join-Path $debug '75_Spiral1WarpObservationTests.exe') @('--emit',$warpA)
Invoke-Checked (Join-Path $debug '75_Spiral1WarpObservationTests.exe') @('--emit',$warpB)
Invoke-Checked (Join-Path $release '75_Spiral1WarpObservationTests.exe') @('--emit',$warpR)
Assert-Triplet 'WARP S00-S15 corpus' $warpA $warpB $warpR

$recoveryA = Join-Path $output 'controlled-recovery-debug-a.bin'
$recoveryB = Join-Path $output 'controlled-recovery-debug-b.bin'
$recoveryR = Join-Path $output 'controlled-recovery-release.bin'
Invoke-Checked (Join-Path $debug '76_Spiral1RecoveryTests.exe') @('--controlled','--emit',$recoveryA)
Invoke-Checked (Join-Path $debug '76_Spiral1RecoveryTests.exe') @('--controlled','--emit',$recoveryB)
Invoke-Checked (Join-Path $release '76_Spiral1RecoveryTests.exe') @('--controlled','--emit',$recoveryR)
Assert-Triplet 'controlled recovery' $recoveryA $recoveryB $recoveryR
$freshBefore = Join-Path $output 'fresh-rematerialization-before-removal.bin'
$freshAfter = Join-Path $output 'fresh-rematerialization-after-removal.bin'
$freshRelease = Join-Path $output 'fresh-rematerialization-release.bin'
Invoke-Checked (Join-Path $debug '76_Spiral1RecoveryTests.exe') @('--fresh-rematerialization','--emit',$freshBefore)
Invoke-Checked (Join-Path $debug '76_Spiral1RecoveryTests.exe') @('--actual-removal')
Invoke-Checked (Join-Path $debug '76_Spiral1RecoveryTests.exe') @('--fresh-rematerialization','--emit',$freshAfter)
Invoke-Checked (Join-Path $release '76_Spiral1RecoveryTests.exe') @('--fresh-rematerialization','--emit',$freshRelease)
Assert-Triplet 'fresh-process rematerialization around actual removal' $freshBefore $freshAfter $freshRelease

$freezeA = Join-Path $output 'final-freeze-debug-a.bin'
$freezeB = Join-Path $output 'final-freeze-debug-b.bin'
$freezeR = Join-Path $output 'final-freeze-release.bin'
Invoke-Checked (Join-Path $debug '77_Spiral1FreezeTests.exe') @('--emit',$freezeA)
Invoke-Checked (Join-Path $debug '77_Spiral1FreezeTests.exe') @('--emit',$freezeB)
Invoke-Checked (Join-Path $release '77_Spiral1FreezeTests.exe') @('--emit',$freezeR)
Assert-Triplet 'architecture final freeze' $freezeA $freezeB $freezeR

$architectureDigest = (Get-FileHash -Algorithm SHA256 -LiteralPath $archA).Hash
$warpDigest = (Get-FileHash -Algorithm SHA256 -LiteralPath $warpA).Hash
$recoveryDigest = (Get-FileHash -Algorithm SHA256 -LiteralPath $recoveryA).Hash
$freshRematerializationDigest = (Get-FileHash -Algorithm SHA256 -LiteralPath $freshBefore).Hash
$freezeDigest = (Get-FileHash -Algorithm SHA256 -LiteralPath $freezeA).Hash

Write-Host '============================================================'
if ($Mode -eq 'Stage12') {
    Write-Host 'SGE4-5 STAGE 12 RECOVERY / AUTHORITY / DETERMINISM FREEZE PASSED'
} else {
    Write-Host 'SGE4-5 SPIRAL 1 ARCHITECTURE COMPLETE'
    Write-Host 'Completion Unit 5: Architecture Final Freeze'
}
Write-Host 'Recovery: controlled rebuild, actual RemoveDevice quarantine, and separate fresh-process rematerialization'
Write-Host 'Authority: Representation seal, Frozen Leaf and Frozen Composition mutations rejected'
Write-Host 'Determinism: Debug A / Debug B / Release evidence is byte-identical'
Write-Host 'Qualified invariants: S1-I01 through S1-I16'
Write-Host 'Remaining: CU6 real GPU measurement and evidence-based Decision Report'
Write-Host "Architecture evidence SHA-256: $architectureDigest"
Write-Host "WARP corpus evidence SHA-256: $warpDigest"
Write-Host "Controlled recovery evidence SHA-256: $recoveryDigest"
Write-Host "Fresh-process rematerialization evidence SHA-256: $freshRematerializationDigest"
Write-Host "Architecture final-freeze evidence SHA-256: $freezeDigest"
Write-Host '============================================================'
