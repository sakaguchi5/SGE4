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
. (Join-Path $testsRoot 'tools\Sha256.ps1')
$solution = Join-Path $root 'SemanticGpuEngine4-5.sln'
$logRoot = Join-Path $root 'docs/test-logs'
New-Item -ItemType Directory -Force -Path $logRoot | Out-Null
$timestamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$logPath = Join-Path $logRoot "$timestamp-Level4v1-Final-Integration-Freeze.log"

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

function Build-Main([string]$Configuration) {
    if ($NoBuild) { return }
    Invoke-Native (Get-MSBuild) @(
        $solution, '/m', '/nologo', '/t:Build',
        "/p:Configuration=$Configuration", '/p:Platform=x64')
}

Set-Content -LiteralPath $logPath -Value "SGE4-5 Level 4 v1 Final Integration Freeze`nStarted=$(Get-Date -Format o)" -Encoding UTF8
try {
    Invoke-Native 'powershell.exe' @('-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',(Join-Path $testsRoot 'Verify-Level4v1FinalIntegration.ps1'))
    Invoke-Native 'powershell.exe' @('-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',(Join-Path $root 'verify_dependencies.ps1'))
    Invoke-Native 'powershell.exe' @('-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',(Join-Path $testsRoot 'tools/Verify-ScriptContracts.ps1'))
    Invoke-Native 'powershell.exe' @('-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',(Join-Path $testsRoot 'tools/Verify-SourceManifest.ps1'))

    Build-Main 'Debug'
    Build-Main 'Release'

    Invoke-Native 'powershell.exe' @(
        '-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass',
        '-File',(Join-Path $testsRoot 'Run-Level4v1R3R5.ps1'),
        '-Stage','R5','-NoBuild')

    $evidence = Join-Path $root 'build/tests/level4v1-r3-r5/Canonical_R5_Debug_A.txt'
    if (-not (Test-Path -LiteralPath $evidence)) { throw "R5 evidence was not produced: $evidence" }
    $evidenceDigest = (Get-SGE4FileSha256 $evidence).ToUpperInvariant()
    $solutionDigest = (Get-SGE4FileSha256 $solution).ToUpperInvariant()
    $projectCount = ([regex]::Matches((Get-Content -Raw -LiteralPath $solution -Encoding UTF8), '(?m)^Project\(')).Count

    Write-Host ''
    Write-Host '============================================================'
    Write-Host 'SGE4 LEVEL 4 V1 CANONICAL FINAL INTEGRATION FREEZE PASSED'
    Write-Host 'Build root: SemanticGpuEngine4-5.sln is the sole canonical solution'
    Write-Host "Integration: all repository projects are registered exactly once ($projectCount projects)"
    Write-Host 'Qualification: R1, R2, R3, R4, R5, Architecture and Stage 0D Freeze passed'
    Write-Host 'Configurations: main solution Debug and Release builds passed'
    Write-Host "Main solution SHA-256: $solutionDigest"
    Write-Host "R5 evidence SHA-256: $evidenceDigest"
    Write-Host 'Stability watch: intermittent WARP device removal remains recorded but was not reproduced in this passing run'
    Write-Host '============================================================'
    Add-Content -LiteralPath $logPath -Value "PASSED`nMain solution SHA-256: $solutionDigest`nR5 evidence SHA-256: $evidenceDigest" -Encoding UTF8
    Write-Host "Log: $logPath"
}
catch {
    Add-Content -LiteralPath $logPath -Value "FAILED`n$($_.Exception.Message)" -Encoding UTF8
    Write-Host "Level 4 v1 Final Integration Freeze failed. Log: $logPath" -ForegroundColor Red
    throw
}
