param(
    [switch]$NoBuild
)

$ErrorActionPreference = 'Stop'

$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
[Console]::InputEncoding = $utf8NoBom
[Console]::OutputEncoding = $utf8NoBom
$OutputEncoding = $utf8NoBom

$testsRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$root = Split-Path -Parent $testsRoot
$f7Project = Join-Path $root '46G_Schema18SharedDeviceDomainTests/46G_Schema18SharedDeviceDomainTests.vcxproj'
$f8Project = Join-Path $root '46H_Schema18SharedResourceTests/46H_Schema18SharedResourceTests.vcxproj'
$f9Project = Join-Path $root '46I_Schema18StaticDagRuntimeTests/46I_Schema18StaticDagRuntimeTests.vcxproj'
$outputRoot = Join-Path $root 'build/tests/level4v1-f7-f9'
$logRoot = Join-Path $root 'docs/test-logs'
New-Item -ItemType Directory -Force -Path $outputRoot, $logRoot | Out-Null
$timestamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$logPath = Join-Path $logRoot "$timestamp-Level4v1-F7-F9.log"

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
    if ($LASTEXITCODE -ne 0) {
        throw "Command failed with exit code ${LASTEXITCODE}: $display"
    }
}

function Get-MSBuild {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path -LiteralPath $vswhere)) { throw 'vswhere.exe was not found.' }
    $found = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe'
    $msbuild = $found | Select-Object -Last 1
    if (-not $msbuild) { throw 'MSBuild was not found.' }
    return $msbuild
}

function Build-Project([string]$ProjectPath, [string]$Configuration) {
    if ($NoBuild) { return }
    $msbuild = Get-MSBuild
    Invoke-Native $msbuild @(
        $ProjectPath, '/m', '/nologo', '/t:Build',
        "/p:Configuration=$Configuration", '/p:Platform=x64',
        "/p:SolutionDir=$root\")
}

function Run-Test([string]$Configuration, [string]$ExecutableName,
                  [string[]]$Arguments = @()) {
    $exe = Join-Path $root "build/bin/x64/$Configuration/$ExecutableName.exe"
    if (-not (Test-Path -LiteralPath $exe)) { throw "Test executable was not found: $exe" }
    Invoke-Native $exe $Arguments
}

function Test-ByteIdentity([string]$Left, [string]$Right) {
    $a = [IO.File]::ReadAllBytes($Left)
    $b = [IO.File]::ReadAllBytes($Right)
    if ($a.Length -ne $b.Length) { return $false }
    for ($i = 0; $i -lt $a.Length; ++$i) {
        if ($a[$i] -ne $b[$i]) { return $false }
    }
    return $true
}

Set-Content -LiteralPath $logPath -Value "SGE4 L4V1-F7-F9 qualification`nStarted=$(Get-Date -Format o)" -Encoding UTF8

try {
    Invoke-Native 'powershell.exe' @('-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',(Join-Path $root 'verify_dependencies.ps1'))
    foreach ($gate in @('Verify-Level4v1F1Boundaries.ps1', 'Verify-Level4v1F2Boundaries.ps1',
        'Verify-Level4v1F3Boundaries.ps1', 'Verify-Level4v1F4F6Boundaries.ps1',
        'Verify-Level4v1F7F9Boundaries.ps1', 'tools/Verify-ScriptContracts.ps1',
        'tools/Verify-SourceManifest.ps1')) {
        Invoke-Native 'powershell.exe' @('-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',(Join-Path $testsRoot $gate))
    }

    foreach ($configuration in @('Debug', 'Release')) {
        Build-Project $f7Project $configuration
        Build-Project $f8Project $configuration
        Build-Project $f9Project $configuration
    }

    Run-Test 'Debug' '46G_Schema18SharedDeviceDomainTests'
    Run-Test 'Release' '46G_Schema18SharedDeviceDomainTests'
    Run-Test 'Debug' '46H_Schema18SharedResourceTests'
    Run-Test 'Release' '46H_Schema18SharedResourceTests'

    $debugA = Join-Path $outputRoot 'StaticDagRuntime_Debug_A.manifest.txt'
    $debugB = Join-Path $outputRoot 'StaticDagRuntime_Debug_B.manifest.txt'
    $release = Join-Path $outputRoot 'StaticDagRuntime_Release.manifest.txt'
    Run-Test 'Debug' '46I_Schema18StaticDagRuntimeTests' @('--emit', $debugA)
    Run-Test 'Debug' '46I_Schema18StaticDagRuntimeTests' @('--emit', $debugB)
    Run-Test 'Release' '46I_Schema18StaticDagRuntimeTests' @('--emit', $release)
    if (-not (Test-ByteIdentity $debugA $debugB)) {
        throw 'Fresh Debug processes emitted different Static DAG Runtime evidence manifests.'
    }
    if (-not (Test-ByteIdentity $debugA $release)) {
        throw 'Debug and Release emitted different Static DAG Runtime evidence manifests.'
    }

    $manifestDigest = (Get-FileHash -Algorithm SHA256 -LiteralPath $debugA).Hash
    $manifestBytes = (Get-Item -LiteralPath $debugA).Length
    Write-Host ''
    Write-Host '============================================================'
    Write-Host 'SGE4 L4V1-F7-F9 SHARED DEVICE DOMAIN AND STATIC DAG RUNTIME PASSED'
    Write-Host 'F7 Domain: all Schema-18 Leaves execute as embedded Schema-17 Packages in one DeviceDomain'
    Write-Host 'F8 Shared Buffer: one Composition-owned native Buffer and token chain per Resource Flow'
    Write-Host 'F9 Runtime: executes only the Frozen canonical schedule; no Compiler, Planner, Verifier, or Freeze operation'
    Write-Host 'WARP DAGs: independent, linear, fan-out, multi-input, diamond, headless, single-presenter'
    Write-Host 'State authority: explicit cross-Leaf transitions plus completion-token retirement chain'
    Write-Host 'Determinism: fresh Debug and Debug/Release evidence-manifest byte identity'
    Write-Host "Evidence manifest bytes: $manifestBytes"
    Write-Host "Evidence manifest SHA-256: $manifestDigest"
    Write-Host '============================================================'
    Add-Content -LiteralPath $logPath -Value "PASSED`nEvidence manifest bytes: $manifestBytes`nEvidence manifest SHA-256: $manifestDigest" -Encoding UTF8
}
catch {
    Add-Content -LiteralPath $logPath -Value "FAILED`n$($_.Exception.Message)" -Encoding UTF8
    Write-Host "L4V1-F7-F9 failed. Log: $logPath" -ForegroundColor Red
    throw
}
