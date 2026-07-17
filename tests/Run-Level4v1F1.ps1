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
$project = Join-Path $root '46A_Schema18CompositionInterfaceTests/46A_Schema18CompositionInterfaceTests.vcxproj'
$outputRoot = Join-Path $root 'build/tests/level4v1-f1'
$logRoot = Join-Path $root 'docs/test-logs'
New-Item -ItemType Directory -Force -Path $outputRoot, $logRoot | Out-Null
$timestamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$logPath = Join-Path $logRoot "$timestamp-Level4v1-F1.log"

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

function Build-F1([string]$Configuration) {
    if ($NoBuild) { return }
    $msbuild = Get-MSBuild
    $solutionDir = $root.TrimEnd('\', '/') + '\'
    Invoke-Native $msbuild @(
        $project, '/m', '/nologo', '/t:Build',
        "/p:Configuration=$Configuration", '/p:Platform=x64',
        "/p:SolutionDir=$solutionDir")
}

function Run-F1([string]$Configuration, [string]$OutputName) {
    $exe = Join-Path $root "build/bin/x64/$Configuration/46A_Schema18CompositionInterfaceTests.exe"
    if (-not (Test-Path -LiteralPath $exe)) { throw "F1 test executable was not found: $exe" }
    $artifact = Join-Path $outputRoot $OutputName
    Invoke-Native $exe @('--emit', $artifact)
    return $artifact
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

Set-Content -LiteralPath $logPath -Value "SGE4 L4V1-F1 qualification`nStarted=$(Get-Date -Format o)" -Encoding UTF8

try {
    Invoke-Native 'powershell.exe' @('-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',(Join-Path $root 'verify_dependencies.ps1'))
    Invoke-Native 'powershell.exe' @('-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',(Join-Path $testsRoot 'Verify-Level4v1F1Boundaries.ps1'))
    Invoke-Native 'powershell.exe' @('-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',(Join-Path $testsRoot 'tools/Verify-ScriptContracts.ps1'))
    Invoke-Native 'powershell.exe' @('-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',(Join-Path $testsRoot 'tools/Verify-SourceManifest.ps1'))

    Build-F1 'Debug'
    Build-F1 'Release'
    $debugA = Run-F1 'Debug' 'Schema18_Debug_A.sgep'
    $debugB = Run-F1 'Debug' 'Schema18_Debug_B.sgep'
    $release = Run-F1 'Release' 'Schema18_Release.sgep'

    if (-not (Test-ByteIdentity $debugA $debugB)) { throw 'Fresh Debug processes emitted different Schema 18 bytes.' }
    if (-not (Test-ByteIdentity $debugA $release)) { throw 'Debug and Release emitted different Schema 18 bytes.' }
    $digest = (Get-FileHash -Algorithm SHA256 -LiteralPath $debugA).Hash.ToUpperInvariant()

    Write-Host ''
    Write-Host '============================================================'
    Write-Host 'SGE4 L4V1-F1 SCHEMA 18 COMPOSITION INTERFACE PASSED'
    Write-Host 'Schema 17: preserved and still decoded only by Runtime 17'
    Write-Host 'Schema 18: stable endpoint key + ReadOnly/WriteOnly authority'
    Write-Host 'Negative gates: ReadWrite, missing endpoint, access forgery'
    Write-Host 'Determinism: fresh Debug and Debug/Release byte identity'
    Write-Host "Artifact SHA-256: $digest"
    Write-Host '============================================================'
    Add-Content -LiteralPath $logPath -Value "PASSED`nArtifact SHA-256: $digest" -Encoding UTF8
}
catch {
    Add-Content -LiteralPath $logPath -Value "FAILED`n$($_.Exception.Message)" -Encoding UTF8
    Write-Host "L4V1-F1 failed. Log: $logPath" -ForegroundColor Red
    throw
}
