param(
    [ValidateSet('R3','R4','R5','All')][string]$Stage = 'All',
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
$solution = Join-Path $root 'SemanticGpuEngine4_Level4v1_R3_R5.sln'
$outputRoot = Join-Path $root 'build/tests/level4v1-r3-r5'
$logRoot = Join-Path $root 'docs/test-logs'
New-Item -ItemType Directory -Force -Path $outputRoot, $logRoot | Out-Null
$timestamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$logPath = Join-Path $logRoot "$timestamp-Level4v1-Canonical-R3-R5-$Stage.log"

function Invoke-Native([string]$FilePath, [string[]]$Arguments = @()) {
    $display = '"' + $FilePath + '"'
    if ($Arguments.Count -gt 0) { $display += ' ' + ($Arguments -join ' ') }
    Write-Host "> $display"
    Add-Content -LiteralPath $logPath -Value "> $display" -Encoding UTF8
    & $FilePath @Arguments 2>&1 | ForEach-Object {
        $text = [string]$_; Write-Host $text; Add-Content -LiteralPath $logPath -Value $text -Encoding UTF8
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
function Build-All([string]$Configuration) {
    if ($NoBuild) { return }
    Invoke-Native (Get-MSBuild) @($solution,'/m','/nologo','/t:Build',"/p:Configuration=$Configuration",'/p:Platform=x64')
}
function Test-ByteIdentity([string]$Left, [string]$Right) {
    $a=[IO.File]::ReadAllBytes($Left); $b=[IO.File]::ReadAllBytes($Right)
    if ($a.Length -ne $b.Length) { return $false }
    for ($i=0; $i -lt $a.Length; ++$i) { if ($a[$i] -ne $b[$i]) { return $false } }
    return $true
}
function Run-Emit([string]$Project,[string]$Configuration,[string]$Name) {
    $exe=Join-Path $root "build/bin/x64/$Configuration/$Project.exe"
    if (-not (Test-Path -LiteralPath $exe)) { throw "Executable not found: $exe" }
    $path=Join-Path $outputRoot $Name
    Invoke-Native $exe @('--emit',$path)
    return $path
}
function Run-Plain([string]$Project,[string]$Configuration) {
    $exe=Join-Path $root "build/bin/x64/$Configuration/$Project.exe"
    if (-not (Test-Path -LiteralPath $exe)) { throw "Executable not found: $exe" }
    Invoke-Native $exe
}
function Run-R3 {
    $a=Run-Emit '48_CanonicalCompositionRuntimeTests' 'Debug' 'Canonical_R3_Debug_A.txt'
    $b=Run-Emit '48_CanonicalCompositionRuntimeTests' 'Debug' 'Canonical_R3_Debug_B.txt'
    $r=Run-Emit '48_CanonicalCompositionRuntimeTests' 'Release' 'Canonical_R3_Release.txt'
    if (-not (Test-ByteIdentity $a $b) -or -not (Test-ByteIdentity $a $r)) { throw 'R3 evidence is not Debug/Release deterministic.' }
    Write-Host ''
    Write-Host '============================================================'
    Write-Host 'SGE4 LEVEL 4 V1 CANONICAL R3 SHARED DEVICE DOMAIN RUNTIME PASSED'
    Write-Host 'Domain: all Schema 17 Leaves share one DeviceDomain and epoch'
    Write-Host 'Resources: one Composition-owned native Buffer and token chain per Resource Flow'
    Write-Host 'Authority: runtime follows only the verified Frozen schedule, bindings, handoffs, signals and waits'
    Write-Host 'WARP: independent, linear, fan-out, multi-input, diamond, headless and single-presenter DAGs'
    Write-Host '============================================================'
}
function Run-R4 {
    Run-Plain '49_CanonicalCompositionRecoveryTests' 'Debug'
    Run-Plain '49_CanonicalCompositionRecoveryTests' 'Release'
    Write-Host ''
    Write-Host '============================================================'
    Write-Host 'SGE4 LEVEL 4 V1 CANONICAL R4 WHOLE COMPOSITION RECOVERY PASSED'
    Write-Host 'Ownership: all Leaves, Buffers and tokens are released before DeviceDomain recovery'
    Write-Host 'Rematerialization: the independently verified Frozen Composition is re-read as sole authority'
    Write-Host 'Epoch: stale resources and completion tokens are rejected after controlled rebuild'
    Write-Host 'Actual removal: removed-LUID exclusion, AwaitingAdapter and retry state are qualified on WARP'
    Write-Host '============================================================'
}
function Run-R5 {
    $a=Run-Emit '49A_CanonicalLevel4v1FreezeTests' 'Debug' 'Canonical_R5_Debug_A.txt'
    $b=Run-Emit '49A_CanonicalLevel4v1FreezeTests' 'Debug' 'Canonical_R5_Debug_B.txt'
    $r=Run-Emit '49A_CanonicalLevel4v1FreezeTests' 'Release' 'Canonical_R5_Release.txt'
    if (-not (Test-ByteIdentity $a $b) -or -not (Test-ByteIdentity $a $r)) { throw 'R5 final evidence is not Debug/Release deterministic.' }
    Invoke-Native 'powershell.exe' @('-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',(Join-Path $testsRoot 'Run-Level4v1R2.ps1'),'-NoBuild','-SkipFoundationFreeze','-SkipR1Regression')
    Invoke-Native 'powershell.exe' @('-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',(Join-Path $testsRoot 'Run-Level4v1R1.ps1'),'-NoBuild','-SkipFoundationFreeze')
    Invoke-Native (Join-Path $testsRoot 'run_architecture.bat')
    if (-not $SkipFoundationFreeze) { Invoke-Native (Join-Path $testsRoot 'run_freeze.bat') }
    $digest=(Get-FileHash -Algorithm SHA256 -LiteralPath $a).Hash.ToUpperInvariant()
    Write-Host ''
    Write-Host '============================================================'
    Write-Host 'SGE4 LEVEL 4 V1 CANONICAL FREEZE PASSED'
    Write-Host 'Foundation: preserved Stage 0D / Schema 17 / Runtime 17'
    Write-Host 'Composition: verified static acyclic multi-Leaf Buffer graph'
    Write-Host 'Authority: independent verification and frozen-only runtime'
    Write-Host 'Device: one DeviceDomain and whole-composition recovery'
    Write-Host 'Determinism: fresh Debug and Release evidence is byte-identical'
    Write-Host "Evidence SHA-256: $digest"
    Write-Host '============================================================'
}

Set-Content -LiteralPath $logPath -Value "SGE4 Level 4 v1 Canonical R3-R5 qualification`nStage=$Stage`nStarted=$(Get-Date -Format o)" -Encoding UTF8
try {
    Invoke-Native 'powershell.exe' @('-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',(Join-Path $root 'verify_dependencies.ps1'))
    Invoke-Native 'powershell.exe' @('-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',(Join-Path $testsRoot 'Verify-Level4v1R3R5Boundaries.ps1'))
    Invoke-Native 'powershell.exe' @('-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',(Join-Path $testsRoot 'tools/Verify-ScriptContracts.ps1'))
    Invoke-Native 'powershell.exe' @('-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',(Join-Path $testsRoot 'tools/Verify-SourceManifest.ps1'))
    Build-All 'Debug'; Build-All 'Release'
    if ($Stage -eq 'R3') { Run-R3 }
    elseif ($Stage -eq 'R4') { Run-R3; Run-R4 }
    elseif ($Stage -eq 'R5' -or $Stage -eq 'All') { Run-R3; Run-R4; Run-R5 }
    Add-Content -LiteralPath $logPath -Value 'PASSED' -Encoding UTF8
    Write-Host "Log: $logPath"
}
catch {
    Add-Content -LiteralPath $logPath -Value "FAILED`n$($_.Exception.Message)" -Encoding UTF8
    Write-Host "Level 4 v1 Canonical R3-R5 failed. Log: $logPath" -ForegroundColor Red
    throw
}
