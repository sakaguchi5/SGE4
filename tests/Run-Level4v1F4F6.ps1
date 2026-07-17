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
$f3Project = Join-Path $root '46C_Schema18CompositionContractTests/46C_Schema18CompositionContractTests.vcxproj'
$f4Project = Join-Path $root '46D_Schema18ResourceFlowPlanningTests/46D_Schema18ResourceFlowPlanningTests.vcxproj'
$f5Project = Join-Path $root '46E_Schema18ResourceFlowAuthorityTests/46E_Schema18ResourceFlowAuthorityTests.vcxproj'
$f6Project = Join-Path $root '46F_Schema18FrozenCompositionTests/46F_Schema18FrozenCompositionTests.vcxproj'
$outputRoot = Join-Path $root 'build/tests/level4v1-f4-f6'
$logRoot = Join-Path $root 'docs/test-logs'
New-Item -ItemType Directory -Force -Path $outputRoot, $logRoot | Out-Null
$timestamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$logPath = Join-Path $logRoot "$timestamp-Level4v1-F4-F6.log"

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

function Build-Project([string]$ProjectPath, [string]$Configuration) {
    if ($NoBuild) { return }
    $msbuild = Get-MSBuild
    Invoke-Native $msbuild @(
        $ProjectPath, '/m', '/nologo', '/t:Build',
        "/p:Configuration=$Configuration", '/p:Platform=x64',
        "/p:SolutionDir=$root\")
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

function Run-Emit([string]$Configuration, [string]$ExecutableName,
                  [string]$ArtifactName) {
    $exe = Join-Path $root "build/bin/x64/$Configuration/$ExecutableName.exe"
    if (-not (Test-Path -LiteralPath $exe)) { throw "Test executable was not found: $exe" }
    $artifact = Join-Path $outputRoot $ArtifactName
    Invoke-Native $exe @('--emit', $artifact)
    return $artifact
}

function Run-Test([string]$Configuration, [string]$ExecutableName) {
    $exe = Join-Path $root "build/bin/x64/$Configuration/$ExecutableName.exe"
    if (-not (Test-Path -LiteralPath $exe)) { throw "Test executable was not found: $exe" }
    Invoke-Native $exe
}

Set-Content -LiteralPath $logPath -Value "SGE4 L4V1-F4-F6 qualification`nStarted=$(Get-Date -Format o)" -Encoding UTF8

try {
    Invoke-Native 'powershell.exe' @('-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',(Join-Path $root 'verify_dependencies.ps1'))
    foreach ($gate in @('Verify-Level4v1F1Boundaries.ps1', 'Verify-Level4v1F2Boundaries.ps1',
        'Verify-Level4v1F3Boundaries.ps1', 'Verify-Level4v1F4F6Boundaries.ps1',
        'tools/Verify-ScriptContracts.ps1', 'tools/Verify-SourceManifest.ps1')) {
        Invoke-Native 'powershell.exe' @('-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',(Join-Path $testsRoot $gate))
    }

    Build-Project $f3Project 'Debug'
    Build-Project $f4Project 'Debug'
    Build-Project $f4Project 'Release'
    Build-Project $f5Project 'Debug'
    Build-Project $f5Project 'Release'
    Build-Project $f6Project 'Debug'
    Build-Project $f6Project 'Release'

    Run-Test 'Debug' '46C_Schema18CompositionContractTests'

    $rawDebugA = Run-Emit 'Debug' '46D_Schema18ResourceFlowPlanningTests' 'ResourceFlowPlan_Debug_A.sgrp'
    $rawDebugB = Run-Emit 'Debug' '46D_Schema18ResourceFlowPlanningTests' 'ResourceFlowPlan_Debug_B.sgrp'
    $rawRelease = Run-Emit 'Release' '46D_Schema18ResourceFlowPlanningTests' 'ResourceFlowPlan_Release.sgrp'
    if (-not (Test-ByteIdentity $rawDebugA $rawDebugB)) {
        throw 'Fresh Debug processes emitted different Raw Resource Flow Plan bytes.'
    }
    if (-not (Test-ByteIdentity $rawDebugA $rawRelease)) {
        throw 'Debug and Release emitted different Raw Resource Flow Plan bytes.'
    }

    Run-Test 'Debug' '46E_Schema18ResourceFlowAuthorityTests'
    Run-Test 'Release' '46E_Schema18ResourceFlowAuthorityTests'

    $frozenDebugA = Run-Emit 'Debug' '46F_Schema18FrozenCompositionTests' 'FrozenComposition_Debug_A.sgec'
    $frozenDebugB = Run-Emit 'Debug' '46F_Schema18FrozenCompositionTests' 'FrozenComposition_Debug_B.sgec'
    $frozenRelease = Run-Emit 'Release' '46F_Schema18FrozenCompositionTests' 'FrozenComposition_Release.sgec'
    if (-not (Test-ByteIdentity $frozenDebugA $frozenDebugB)) {
        throw 'Fresh Debug processes emitted different Frozen Composition bytes.'
    }
    if (-not (Test-ByteIdentity $frozenDebugA $frozenRelease)) {
        throw 'Debug and Release emitted different Frozen Composition bytes.'
    }

    $rawDigest = (Get-FileHash -Algorithm SHA256 -LiteralPath $rawDebugA).Hash.ToUpperInvariant()
    $rawBytes = (Get-Item -LiteralPath $rawDebugA).Length
    $frozenDigest = (Get-FileHash -Algorithm SHA256 -LiteralPath $frozenDebugA).Hash.ToUpperInvariant()
    $frozenBytes = (Get-Item -LiteralPath $frozenDebugA).Length

    Write-Host ''
    Write-Host '============================================================'
    Write-Host 'SGE4 L4V1-F4-F6 VERIFIED RESOURCE FLOW AND FROZEN COMPOSITION PASSED'
    Write-Host 'F4 Plan: canonical allocation, Leaf schedule, endpoint binding, handoff, signal/wait, recovery'
    Write-Host 'F5 Authority: only the independent Verifier can construct a sealed Verified Plan'
    Write-Host 'F6 Artifact: Frozen Composition embeds Contract, Plan, Leaf Packages, and Verifier seal'
    Write-Host 'Negative gates: cycle, contract/plan identity, allocation, schedule, binding, state, signal/wait, recovery, seal'
    Write-Host 'Determinism: fresh Debug and Debug/Release byte identity for Raw Plan and Frozen Composition'
    Write-Host "Raw Plan bytes: $rawBytes"
    Write-Host "Raw Plan SHA-256: $rawDigest"
    Write-Host "Frozen Composition bytes: $frozenBytes"
    Write-Host "Frozen Composition SHA-256: $frozenDigest"
    Write-Host '============================================================'
    Add-Content -LiteralPath $logPath -Value "PASSED`nRaw Plan bytes: $rawBytes`nRaw Plan SHA-256: $rawDigest`nFrozen Composition bytes: $frozenBytes`nFrozen Composition SHA-256: $frozenDigest" -Encoding UTF8
}
catch {
    Add-Content -LiteralPath $logPath -Value "FAILED`n$($_.Exception.Message)" -Encoding UTF8
    Write-Host "L4V1-F4-F6 failed. Log: $logPath" -ForegroundColor Red
    throw
}
