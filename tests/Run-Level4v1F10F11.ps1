param(
    [switch]$NoBuild,
    [switch]$Quick
)

$ErrorActionPreference = 'Stop'

$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
[Console]::InputEncoding = $utf8NoBom
[Console]::OutputEncoding = $utf8NoBom
$OutputEncoding = $utf8NoBom

$testsRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$root = Split-Path -Parent $testsRoot
$f10Project = Join-Path $root '46J_Schema18CompositionRecoveryTests/46J_Schema18CompositionRecoveryTests.vcxproj'
$f11Project = Join-Path $root '46K_Schema18FinalQualificationTests/46K_Schema18FinalQualificationTests.vcxproj'
$outputRoot = Join-Path $root 'build/tests/level4v1-f10-f11'
$logRoot = Join-Path $root 'docs/test-logs'
New-Item -ItemType Directory -Force -Path $outputRoot, $logRoot | Out-Null
$timestamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$logPath = Join-Path $logRoot "$timestamp-Level4v1-F10-F11.log"

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

Set-Content -LiteralPath $logPath -Value "SGE4 L4V1-F10-F11 qualification`nStarted=$(Get-Date -Format o)`nQuick=$Quick" -Encoding UTF8

try {
    Invoke-Native 'powershell.exe' @('-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',(Join-Path $root 'verify_dependencies.ps1'))
    foreach ($gate in @('Verify-Level4v1F1Boundaries.ps1', 'Verify-Level4v1F2Boundaries.ps1',
        'Verify-Level4v1F3Boundaries.ps1', 'Verify-Level4v1F4F6Boundaries.ps1',
        'Verify-Level4v1F7F9Boundaries.ps1', 'Verify-Level4v1F10F11Boundaries.ps1',
        'tools/Verify-ScriptContracts.ps1', 'tools/Verify-SourceManifest.ps1')) {
        Invoke-Native 'powershell.exe' @('-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',(Join-Path $testsRoot $gate))
    }

    foreach ($configuration in @('Debug', 'Release')) {
        Build-Project $f10Project $configuration
        Build-Project $f11Project $configuration
    }
    Run-Test 'Debug' '46J_Schema18CompositionRecoveryTests'
    Run-Test 'Release' '46J_Schema18CompositionRecoveryTests'

    $debugA = Join-Path $outputRoot 'RecoveryQualification_Debug_A.manifest.txt'
    $debugB = Join-Path $outputRoot 'RecoveryQualification_Debug_B.manifest.txt'
    $release = Join-Path $outputRoot 'RecoveryQualification_Release.manifest.txt'
    Run-Test 'Debug' '46K_Schema18FinalQualificationTests' @('--emit', $debugA)
    Run-Test 'Debug' '46K_Schema18FinalQualificationTests' @('--emit', $debugB)
    Run-Test 'Release' '46K_Schema18FinalQualificationTests' @('--emit', $release)
    if (-not (Test-ByteIdentity $debugA $debugB)) {
        throw 'Fresh Debug processes emitted different F10-F11 evidence manifests.'
    }
    if (-not (Test-ByteIdentity $debugA $release)) {
        throw 'Debug and Release emitted different F10-F11 evidence manifests.'
    }

    if (-not $Quick) {
        foreach ($runner in @('run_sge4_level4v1_f1.bat', 'run_sge4_level4v1_f2.bat',
            'run_sge4_level4v1_f3.bat', 'run_sge4_level4v1_f4_f6.bat',
            'run_sge4_level4v1_f7_f9.bat', 'tests/run_architecture.bat')) {
            Invoke-Native (Join-Path $root $runner)
        }
    }

    $manifestDigest = (Get-FileHash -Algorithm SHA256 -LiteralPath $debugA).Hash
    $manifestBytes = (Get-Item -LiteralPath $debugA).Length
    Write-Host ''
    Write-Host '============================================================'
    Write-Host 'SGE4 L4V1-F10-F11 COMPOSITION RECOVERY AND FINAL QUALIFICATION PASSED'
    Write-Host 'F10 Recovery: one whole-domain unit destroys old Leaves, Resources, tokens, and state before recovery'
    Write-Host 'Rematerialization: Frozen Composition only; Schema-18 authority and Schema-17 execution ABI are preserved'
    Write-Host 'Actual loss: RemoveDevice -> removed-adapter exclusion -> AwaitingAdapter; same-topology WARP retry remains Awaiting'
    Write-Host 'Negative gates: retry while Active, submit/read while AwaitingAdapter, stale resource/token epoch'
    Write-Host 'Post-recovery: diamond and single-presenter DAGs execute after controlled whole-domain rematerialization'
    Write-Host 'Fresh-process rematerialization: each Debug/Release evidence process independently loads the Frozen Composition'
    if ($Quick) {
        Write-Host 'F11 Regression: quick mode; prior stage runners and Architecture were intentionally skipped'
    }
    else {
        Write-Host 'F11 Regression: F1-F9 stage runners plus Architecture all passed'
    }
    Write-Host 'Determinism: fresh Debug and Debug/Release recovery-evidence byte identity'
    Write-Host "Evidence manifest bytes: $manifestBytes"
    Write-Host "Evidence manifest SHA-256: $manifestDigest"
    Write-Host '============================================================'
    Add-Content -LiteralPath $logPath -Value "PASSED`nEvidence manifest bytes: $manifestBytes`nEvidence manifest SHA-256: $manifestDigest" -Encoding UTF8
}
catch {
    Add-Content -LiteralPath $logPath -Value "FAILED`n$($_.Exception.Message)" -Encoding UTF8
    Write-Host "L4V1-F10-F11 failed. Log: $logPath" -ForegroundColor Red
    throw
}
