$ErrorActionPreference = 'Stop'
$testsRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$root = Split-Path -Parent $testsRoot

function Invoke-Checked([string]$File, [string[]]$Arguments = @()) {
    & $File @Arguments
    if ($LASTEXITCODE -ne 0) { throw "Command failed: $File $Arguments" }
}

function Invoke-Stage([string]$Name, [scriptblock]$Action) {
    Write-Host "[L4V2-R2] START $Name"
    $watch = [Diagnostics.Stopwatch]::StartNew()
    & $Action
    $watch.Stop()
    Write-Host ("[L4V2-R2] DONE  {0} elapsed={1}" -f $Name, $watch.Elapsed)
}

function Get-MSBuild {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path -LiteralPath $vswhere -PathType Leaf)) { throw 'vswhere.exe missing.' }
    $msbuild = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -Last 1
    if (-not $msbuild) { throw 'MSBuild missing.' }
    return $msbuild
}

function Test-BytesEqual([string]$A, [string]$B) {
    $x = [IO.File]::ReadAllBytes($A)
    $y = [IO.File]::ReadAllBytes($B)
    if ($x.Length -ne $y.Length) { return $false }
    for ($i = 0; $i -lt $x.Length; ++$i) {
        if ($x[$i] -ne $y[$i]) { return $false }
    }
    return $true
}

$powershell = 'powershell.exe'
$common = @('-NoLogo', '-NoProfile', '-NonInteractive', '-ExecutionPolicy', 'Bypass', '-File')

Invoke-Stage 'Frozen Spiral 7 reference gate' {
    Invoke-Checked $powershell ($common + @((Join-Path $testsRoot 'Run-Spiral7ReferenceGate.ps1')))
}
Invoke-Stage 'R0 regression freeze' {
    Invoke-Checked $powershell ($common + @((Join-Path $testsRoot 'tools\Verify-Level4V2R0.ps1'), '-Mode', 'Regression'))
}
Invoke-Stage 'R1 regression vocabulary' {
    Invoke-Checked $powershell ($common + @((Join-Path $testsRoot 'tools\Verify-Level4V2R1.ps1'), '-Mode', 'Regression'))
}
Invoke-Stage 'R2 static authority boundaries' {
    Invoke-Checked $powershell ($common + @((Join-Path $testsRoot 'tools\Verify-Level4V2R2.ps1')))
}

$msbuild = Get-MSBuild
$project = Join-Path $root '195_Level4V2AuthorityTests\195_Level4V2AuthorityTests.vcxproj'
Invoke-Stage 'Build Debug and Release' {
    foreach ($configuration in @('Debug', 'Release')) {
        Invoke-Checked $msbuild @(
            $project,
            '/m',
            '/nr:false',
            '/nologo',
            "/p:Configuration=$configuration",
            '/p:Platform=x64',
            "/p:SolutionDir=$root\",
            '/v:minimal')
    }
}

$output = Join-Path $root 'build\tests\level4v2-r2'
Remove-Item -Recurse -Force -ErrorAction SilentlyContinue $output
New-Item -ItemType Directory -Force $output | Out-Null
$debugEvidence = Join-Path $output 'authority-evidence-debug.bin'
$releaseEvidence = Join-Path $output 'authority-evidence-release.bin'
$debugExe = Join-Path $root 'build\bin\x64\Debug\195_Level4V2AuthorityTests.exe'
$releaseExe = Join-Path $root 'build\bin\x64\Release\195_Level4V2AuthorityTests.exe'

Invoke-Stage 'Debug authority self-test' {
    Invoke-Checked $debugExe @('--self-test', '--emit', $debugEvidence)
}
Invoke-Stage 'Release authority self-test' {
    Invoke-Checked $releaseExe @('--self-test', '--emit', $releaseEvidence)
}
if (-not (Test-BytesEqual $debugEvidence $releaseEvidence)) {
    throw 'R2 deterministic authority Evidence differs across Debug and Release.'
}
$evidenceHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $debugEvidence).Hash.ToLowerInvariant()
$manifest = Get-Content -Raw -LiteralPath (Join-Path $root 'docs\level4v2\R2_UNIFIED_AUTHORITY_MANIFEST_V1.json') -Encoding UTF8 | ConvertFrom-Json
if ($evidenceHash -ne $manifest.deterministicEvidence.expectedSha256) {
    throw "R2 deterministic authority Evidence identity mismatch: $evidenceHash"
}
Write-Host "R2 deterministic authority Evidence SHA-256: $evidenceHash"

Invoke-Stage 'Source Manifest' {
    Invoke-Checked $powershell ($common + @((Join-Path $testsRoot 'tools\Verify-SourceManifest.ps1')))
}

Write-Host '============================================================'
Write-Host 'SGE4 LEVEL 4 V2 R2 UNIFIED AUTHORITY CHAIN PASSED'
Write-Host 'R2 COMPLETE'
Write-Host 'Authority operations: 6'
Write-Host 'Mutation/replay rejections: 18'
Write-Host 'Verifier dependency on Planner project: None'
Write-Host 'Runtime capability added: None'
Write-Host 'Runtime Candidate-policy authorization: None'
Write-Host 'NEXT: R3 CANONICAL COMPOSITION'
Write-Host '============================================================'
