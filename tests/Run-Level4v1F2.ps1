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
$outputRoot = Join-Path $root 'build/tests/level4v1-f2'
$logRoot = Join-Path $root 'docs/test-logs'
New-Item -ItemType Directory -Force -Path $outputRoot, $logRoot | Out-Null
$timestamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$logPath = Join-Path $logRoot "$timestamp-Level4v1-F2.log"

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

function Build-F2([string]$Configuration) {
    # Build project files directly rather than the reduced F2 solution. This
    # preserves Platform=x64 across transitive ProjectReference evaluation and
    # avoids MSBuild falling back to Debug|Win32 for projects outside the SLN.
    Build-Project $f1Project $Configuration
    Build-Project $f2Project $Configuration
}

function Run-F1Regression {
    $exe = Join-Path $root 'build/bin/x64/Debug/46A_Schema18CompositionInterfaceTests.exe'
    if (-not (Test-Path -LiteralPath $exe)) { throw "F1 test executable was not found: $exe" }
    Invoke-Native $exe @('--emit', (Join-Path $outputRoot 'F1_Regression.sgep'))
}

function Run-Corpus([string]$Configuration, [string]$Name) {
    $exe = Join-Path $root "build/bin/x64/$Configuration/46B_Schema18LeafCorpusTests.exe"
    if (-not (Test-Path -LiteralPath $exe)) { throw "F2 test executable was not found: $exe" }
    $directory = Join-Path $outputRoot $Name
    $manifest = Join-Path $outputRoot "$Name.manifest.txt"
    Invoke-Native $exe @('--emit-corpus', $directory, $manifest)
    return [pscustomobject]@{ Directory = $directory; Manifest = $manifest }
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

function Get-CorpusRecords([string]$Directory) {
    $files = @(Get-ChildItem -LiteralPath $Directory -File -Filter '*.sgep' | Sort-Object Name)
    if ($files.Count -ne 54) { throw "Corpus directory does not contain 54 Packages: $Directory" }
    return @($files | ForEach-Object {
        [pscustomobject]@{
            Name = $_.Name
            Length = $_.Length
            Hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $_.FullName).Hash.ToUpperInvariant()
        }
    })
}

function Assert-CorpusIdentity([string]$Left, [string]$Right) {
    $a = @(Get-CorpusRecords $Left)
    $b = @(Get-CorpusRecords $Right)
    for ($i = 0; $i -lt $a.Count; ++$i) {
        if ($a[$i].Name -ne $b[$i].Name -or
            $a[$i].Length -ne $b[$i].Length -or
            $a[$i].Hash -ne $b[$i].Hash) {
            throw "Corpus Package mismatch: $($a[$i].Name) versus $($b[$i].Name)"
        }
    }
}

function Get-CorpusAggregate([string]$Directory) {
    $records = @(Get-CorpusRecords $Directory)
    $text = ($records | ForEach-Object { "$($_.Name)|$($_.Length)|$($_.Hash)" }) -join "`n"
    $bytes = [Text.Encoding]::UTF8.GetBytes($text)
    $sha = [Security.Cryptography.SHA256]::Create()
    try { return ([BitConverter]::ToString($sha.ComputeHash($bytes))).Replace('-', '') }
    finally { $sha.Dispose() }
}

Set-Content -LiteralPath $logPath -Value "SGE4 L4V1-F2 qualification`nStarted=$(Get-Date -Format o)" -Encoding UTF8

try {
    Invoke-Native 'powershell.exe' @('-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',(Join-Path $root 'verify_dependencies.ps1'))
    Invoke-Native 'powershell.exe' @('-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',(Join-Path $testsRoot 'Verify-Level4v1F1Boundaries.ps1'))
    Invoke-Native 'powershell.exe' @('-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',(Join-Path $testsRoot 'Verify-Level4v1F2Boundaries.ps1'))
    Invoke-Native 'powershell.exe' @('-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',(Join-Path $testsRoot 'tools/Verify-ScriptContracts.ps1'))
    Invoke-Native 'powershell.exe' @('-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',(Join-Path $testsRoot 'tools/Verify-SourceManifest.ps1'))

    Build-F2 'Debug'
    Build-F2 'Release'
    Run-F1Regression
    $debugA = Run-Corpus 'Debug' 'Schema18Leaf_Debug_A'
    $debugB = Run-Corpus 'Debug' 'Schema18Leaf_Debug_B'
    $release = Run-Corpus 'Release' 'Schema18Leaf_Release'

    if (-not (Test-ByteIdentity $debugA.Manifest $debugB.Manifest)) {
        throw 'Fresh Debug processes emitted different F2 manifests.'
    }
    if (-not (Test-ByteIdentity $debugA.Manifest $release.Manifest)) {
        throw 'Debug and Release emitted different F2 manifests.'
    }
    Assert-CorpusIdentity $debugA.Directory $debugB.Directory
    Assert-CorpusIdentity $debugA.Directory $release.Directory

    $manifestDigest = (Get-FileHash -Algorithm SHA256 -LiteralPath $debugA.Manifest).Hash.ToUpperInvariant()
    $corpusAggregate = Get-CorpusAggregate $debugA.Directory

    Write-Host ''
    Write-Host '============================================================'
    Write-Host 'SGE4 L4V1-F2 SCHEMA 18 LEAF CORPUS PASSED'
    Write-Host 'Source corpus: same 54 accepted Semantic scenarios'
    Write-Host 'Schema 18 corpus: 54 deterministic Leaf Packages'
    Write-Host 'Boundary normalization: 3 explicit Compute export proxies'
    Write-Host 'Endpoints: 12 total, ReadOnly 9, WriteOnly 3'
    Write-Host 'Determinism: fresh Debug and Debug/Release byte identity'
    Write-Host "Manifest SHA-256: $manifestDigest"
    Write-Host "Corpus aggregate SHA-256: $corpusAggregate"
    Write-Host '============================================================'
    Add-Content -LiteralPath $logPath -Value "PASSED`nManifest SHA-256: $manifestDigest`nCorpus aggregate SHA-256: $corpusAggregate" -Encoding UTF8
}
catch {
    Add-Content -LiteralPath $logPath -Value "FAILED`n$($_.Exception.Message)" -Encoding UTF8
    Write-Host "L4V1-F2 failed. Log: $logPath" -ForegroundColor Red
    throw
}
