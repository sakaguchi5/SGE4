$ErrorActionPreference = 'Stop'
$testsRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$root = Split-Path -Parent $testsRoot

function Invoke-Checked([string]$File,[string[]]$Arguments=@()){
    & $File @Arguments
    if($LASTEXITCODE -ne 0){throw "Command failed: $File $Arguments"}
}

function Invoke-Stage([string]$Name,[scriptblock]$Action){
    Write-Host "[L4V2-R1] START $Name"
    $watch=[Diagnostics.Stopwatch]::StartNew()
    & $Action
    $watch.Stop()
    Write-Host ("[L4V2-R1] DONE  {0} elapsed={1}" -f $Name,$watch.Elapsed)
}

function Get-MSBuild {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if(-not (Test-Path -LiteralPath $vswhere -PathType Leaf)){throw 'vswhere.exe missing.'}
    $msbuild = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -Last 1
    if(-not $msbuild){throw 'MSBuild missing.'}
    return $msbuild
}

function Test-BytesEqual([string]$A,[string]$B){
    $x=[IO.File]::ReadAllBytes($A); $y=[IO.File]::ReadAllBytes($B)
    if($x.Length -ne $y.Length){return $false}
    for($i=0;$i -lt $x.Length;++$i){if($x[$i] -ne $y[$i]){return $false}}
    return $true
}

$powershell='powershell.exe'
$common=@('-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File')

Invoke-Stage 'Frozen Spiral 7 reference gate' {
    Invoke-Checked $powershell ($common + @((Join-Path $testsRoot 'Run-Spiral7ReferenceGate.ps1')))
}
Invoke-Stage 'R0 regression freeze' {
    Invoke-Checked $powershell ($common + @((Join-Path $testsRoot 'tools\Verify-Level4V2R0.ps1'),'-Mode','Regression'))
}
Invoke-Stage 'R1 static vocabulary boundaries' {
    Invoke-Checked $powershell ($common + @((Join-Path $testsRoot 'tools\Verify-Level4V2R1.ps1')))
}

$msbuild=Get-MSBuild
$project=Join-Path $root '190_Level4V2CanonicalVocabularyTests\190_Level4V2CanonicalVocabularyTests.vcxproj'
Invoke-Stage 'Build Debug and Release' {
    foreach($configuration in @('Debug','Release')){
        Invoke-Checked $msbuild @($project,'/m','/nr:false','/nologo',"/p:Configuration=$configuration",'/p:Platform=x64',"/p:SolutionDir=$root\",'/v:minimal')
    }
}

$output=Join-Path $root 'build\tests\level4v2-r1'
Remove-Item -Recurse -Force -ErrorAction SilentlyContinue $output
New-Item -ItemType Directory -Force $output | Out-Null
$debugEvidence=Join-Path $output 'vocabulary-evidence-debug.bin'
$releaseEvidence=Join-Path $output 'vocabulary-evidence-release.bin'
$debugExe=Join-Path $root 'build\bin\x64\Debug\190_Level4V2CanonicalVocabularyTests.exe'
$releaseExe=Join-Path $root 'build\bin\x64\Release\190_Level4V2CanonicalVocabularyTests.exe'

Invoke-Stage 'Debug vocabulary self-test' {
    Invoke-Checked $debugExe @('--self-test','--emit',$debugEvidence)
}
Invoke-Stage 'Release vocabulary self-test' {
    Invoke-Checked $releaseExe @('--self-test','--emit',$releaseEvidence)
}
if(-not (Test-BytesEqual $debugEvidence $releaseEvidence)){throw 'R1 deterministic vocabulary evidence differs across Debug and Release.'}
$evidenceHash=(Get-FileHash -Algorithm SHA256 -LiteralPath $debugEvidence).Hash.ToLowerInvariant()
if($evidenceHash -ne '35e751e131dcf4c88cad1b2da7bbc8e745f497e56d5ba22bd23079c7e2ec2abf'){throw "R1 deterministic vocabulary evidence identity mismatch: $evidenceHash"}
Write-Host "R1 deterministic vocabulary evidence SHA-256: $evidenceHash"

Invoke-Stage 'Source Manifest' {
    Invoke-Checked $powershell ($common + @((Join-Path $testsRoot 'tools\Verify-SourceManifest.ps1')))
}

Write-Host '============================================================'
Write-Host 'SGE4 LEVEL 4 V2 R1 CANONICAL VOCABULARY PASSED'
Write-Host 'R1 COMPLETE'
Write-Host 'Strong identities: 11'
Write-Host 'Strong dynamic concepts: 8'
Write-Host 'Runtime capability added: None'
Write-Host 'Runtime Candidate-policy authorization: None'
Write-Host 'NEXT: R2 UNIFIED AUTHORITY CHAIN'
Write-Host '============================================================'
