param(
    [switch]$NoBuild,
    [switch]$SkipFoundationFreeze
)

$ErrorActionPreference = 'Stop'

$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
[Console]::InputEncoding = $utf8NoBom
[Console]::OutputEncoding = $utf8NoBom
$OutputEncoding = $utf8NoBom

$testsRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$root = Split-Path -Parent $testsRoot
. (Join-Path $testsRoot 'tools\Sha256.ps1')
$solution = Join-Path $root 'SemanticGpuEngine4-5.sln'
$outputRoot = Join-Path $root 'build/tests/level4v1-r1'
$logRoot = Join-Path $root 'docs/test-logs'
New-Item -ItemType Directory -Force -Path $outputRoot, $logRoot | Out-Null
$timestamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$logPath = Join-Path $logRoot "$timestamp-Level4v1-Canonical-R1.log"

function Invoke-Native([string]$FilePath, [string[]]$Arguments = @()) {
    $display = '"' + $FilePath + '"'
    if ($Arguments.Count -gt 0) { $display += ' ' + ($Arguments -join ' ') }
    Write-Host "> $display"
    Add-Content -LiteralPath $logPath -Value "> $display" -Encoding UTF8
    & $FilePath @Arguments 2>&1 | ForEach-Object {
        $text = [string]$_
        Write-Host $text
        Add-Content -LiteralPath $logPath -Value $text -Encoding UTF8
    }
    if ($LASTEXITCODE -ne 0) { throw "Command failed with exit code ${LASTEXITCODE}: $display" }
}

function Get-MSBuild {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path -LiteralPath $vswhere)) { throw 'vswhere.exe was not found.' }
    $found = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe'
    $msbuild = $found | Select-Object -Last 1
    if (-not $msbuild) { throw 'MSBuild was not found.' }
    return $msbuild
}

function Build-R1([string]$Configuration) {
    if ($NoBuild) { return }
    $msbuild = Get-MSBuild
    Invoke-Native $msbuild @(
        $solution, '/m', '/nologo', '/t:Build',
        "/p:Configuration=$Configuration", '/p:Platform=x64')
}

function Run-R1([string]$Configuration, [string]$OutputName) {
    $exe = Join-Path $root "build/bin/x64/$Configuration/46_CanonicalCompositionArtifactTests.exe"
    if (-not (Test-Path -LiteralPath $exe)) { throw "R1 test executable was not found: $exe" }
    $artifact = Join-Path $outputRoot $OutputName
    Invoke-Native $exe @('--emit', $artifact)
    return $artifact
}

function Test-ByteIdentity([string]$Left, [string]$Right) {
    $a = [IO.File]::ReadAllBytes($Left)
    $b = [IO.File]::ReadAllBytes($Right)
    if ($a.Length -ne $b.Length) { return $false }
    for ($index = 0; $index -lt $a.Length; ++$index) {
        if ($a[$index] -ne $b[$index]) { return $false }
    }
    return $true
}

Set-Content -LiteralPath $logPath -Value "SGE4-5 Level 4 v1 Canonical R1 qualification`nStarted=$(Get-Date -Format o)" -Encoding UTF8

try {
    Invoke-Native 'powershell.exe' @('-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',(Join-Path $root 'verify_dependencies.ps1'))
    Invoke-Native 'powershell.exe' @('-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',(Join-Path $testsRoot 'Verify-Level4v1R1Boundaries.ps1'))
    Invoke-Native 'powershell.exe' @('-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',(Join-Path $testsRoot 'tools/Verify-ScriptContracts.ps1'))
    Invoke-Native 'powershell.exe' @('-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',(Join-Path $testsRoot 'tools/Verify-SourceManifest.ps1'))

    Build-R1 'Debug'
    Build-R1 'Release'
    $debugA = Run-R1 'Debug' 'Canonical_R1_Debug_A.sgec'
    $debugB = Run-R1 'Debug' 'Canonical_R1_Debug_B.sgec'
    $release = Run-R1 'Release' 'Canonical_R1_Release.sgec'

    if (-not (Test-ByteIdentity $debugA $debugB)) { throw 'Fresh Debug processes emitted different Frozen Composition bytes.' }
    if (-not (Test-ByteIdentity $debugA $release)) { throw 'Debug and Release emitted different Frozen Composition bytes.' }

    if (-not $SkipFoundationFreeze) {
        Invoke-Native (Join-Path $testsRoot 'run_freeze.bat')
    }

    $fileInfo = Get-Item -LiteralPath $debugA
    $digest = (Get-SGE4FileSha256 $debugA).ToUpperInvariant()
    Write-Host ''
    Write-Host '============================================================'
    Write-Host 'SGE4 LEVEL 4 V1 CANONICAL R1 FROZEN COMPOSITION BOUNDARY PASSED'
    Write-Host 'Container: fixed-width, canonical, sectioned, digest-protected'
    Write-Host 'Leaves: at least two embedded D3D12 Schema 17 / Runtime 17 Packages'
    Write-Host 'Authority: contract, verified decisions and certificate are immutable opaque sections'
    Write-Host 'Negative gates: layout, identity, embedded Leaf, presenter and digest corruption'
    Write-Host 'Determinism: fresh Debug and Debug/Release byte identity'
    Write-Host 'Non-claim: R1 does not yet prove Contract semantics or independent verification authority'
    Write-Host "Artifact bytes: $($fileInfo.Length)"
    Write-Host "Artifact SHA-256: $digest"
    Write-Host '============================================================'
    Add-Content -LiteralPath $logPath -Value "PASSED`nArtifact bytes: $($fileInfo.Length)`nArtifact SHA-256: $digest" -Encoding UTF8
}
catch {
    Add-Content -LiteralPath $logPath -Value "FAILED`n$($_.Exception.Message)" -Encoding UTF8
    Write-Host "Level 4 v1 Canonical R1 failed. Log: $logPath" -ForegroundColor Red
    throw
}
