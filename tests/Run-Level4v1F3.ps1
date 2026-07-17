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
$f1Project = Join-Path $root '46A_Schema18CompositionInterfaceTests/46A_Schema18CompositionInterfaceTests.vcxproj'
$f2Project = Join-Path $root '46B_Schema18LeafCorpusTests/46B_Schema18LeafCorpusTests.vcxproj'
$f3Project = Join-Path $root '46C_Schema18CompositionContractTests/46C_Schema18CompositionContractTests.vcxproj'
$outputRoot = Join-Path $root 'build/tests/level4v1-f3'
$logRoot = Join-Path $root 'docs/test-logs'
New-Item -ItemType Directory -Force -Path $outputRoot, $logRoot | Out-Null
$timestamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$logPath = Join-Path $logRoot "$timestamp-Level4v1-F3.log"

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

function Run-F3([string]$Configuration, [string]$Name) {
    $exe = Join-Path $root "build/bin/x64/$Configuration/46C_Schema18CompositionContractTests.exe"
    if (-not (Test-Path -LiteralPath $exe)) { throw "F3 test executable was not found: $exe" }
    $artifact = Join-Path $outputRoot "$Name.sgec"
    Invoke-Native $exe @('--emit', $artifact)
    return $artifact
}

Set-Content -LiteralPath $logPath -Value "SGE4 L4V1-F3 qualification`nStarted=$(Get-Date -Format o)" -Encoding UTF8

try {
    Invoke-Native 'powershell.exe' @('-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',(Join-Path $root 'verify_dependencies.ps1'))
    foreach ($gate in @('Verify-Level4v1F1Boundaries.ps1', 'Verify-Level4v1F2Boundaries.ps1',
        'Verify-Level4v1F3Boundaries.ps1', 'tools/Verify-ScriptContracts.ps1',
        'tools/Verify-SourceManifest.ps1')) {
        Invoke-Native 'powershell.exe' @('-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',(Join-Path $testsRoot $gate))
    }

    Build-Project $f1Project 'Debug'
    Build-Project $f2Project 'Debug'
    Build-Project $f2Project 'Release'
    Build-Project $f3Project 'Debug'
    Build-Project $f3Project 'Release'

    $f1Exe = Join-Path $root 'build/bin/x64/Debug/46A_Schema18CompositionInterfaceTests.exe'
    $f2DebugExe = Join-Path $root 'build/bin/x64/Debug/46B_Schema18LeafCorpusTests.exe'
    $f2ReleaseExe = Join-Path $root 'build/bin/x64/Release/46B_Schema18LeafCorpusTests.exe'
    Invoke-Native $f1Exe
    Invoke-Native $f2DebugExe
    Invoke-Native $f2ReleaseExe

    $debugA = Run-F3 'Debug' 'Schema18Contract_Debug_A'
    $debugB = Run-F3 'Debug' 'Schema18Contract_Debug_B'
    $release = Run-F3 'Release' 'Schema18Contract_Release'
    if (-not (Test-ByteIdentity $debugA $debugB)) {
        throw 'Fresh Debug processes emitted different F3 Contract bytes.'
    }
    if (-not (Test-ByteIdentity $debugA $release)) {
        throw 'Debug and Release emitted different F3 Contract bytes.'
    }

    $artifactDigest = (Get-FileHash -Algorithm SHA256 -LiteralPath $debugA).Hash.ToUpperInvariant()
    $artifactBytes = (Get-Item -LiteralPath $debugA).Length

    Write-Host ''
    Write-Host '============================================================'
    Write-Host 'SGE4 L4V1-F3 SCHEMA 18 COMPOSITION CONTRACT PASSED'
    Write-Host 'Contract graph: 3 Leaves, 6 endpoints, 4 Resource Flows, 6 bindings'
    Write-Host 'Fan-out: one authoritative WriteOnly producer to two ReadOnly consumers'
    Write-Host 'Authority: access, shape, size, states, and synchronization come only from Leaf Packages'
    Write-Host 'Negative gates: Schema 17, missing/duplicate binding, role forgery, unknown endpoint, two presenters'
    Write-Host 'Determinism: fresh Debug and Debug/Release canonical byte identity'
    Write-Host "Contract bytes: $artifactBytes"
    Write-Host "Contract artifact SHA-256: $artifactDigest"
    Write-Host '============================================================'
    Add-Content -LiteralPath $logPath -Value "PASSED`nContract bytes: $artifactBytes`nContract artifact SHA-256: $artifactDigest" -Encoding UTF8
}
catch {
    Add-Content -LiteralPath $logPath -Value "FAILED`n$($_.Exception.Message)" -Encoding UTF8
    Write-Host "L4V1-F3 failed. Log: $logPath" -ForegroundColor Red
    throw
}
