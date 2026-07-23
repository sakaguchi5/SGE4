$ErrorActionPreference = 'Stop'
$testsRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$root = Split-Path -Parent $testsRoot
. (Join-Path $testsRoot 'Spiral7Common.ps1')

function Invoke-CU5Stage([string]$Name, [scriptblock]$Action) {
    Write-Host "[CU5-ROUTINE] START $Name"
    $watch = [Diagnostics.Stopwatch]::StartNew()
    & $Action
    $watch.Stop()
    Write-Host ("[CU5-ROUTINE] DONE  {0} elapsed={1}" -f $Name,$watch.Elapsed)
}

function Require-FrozenEvidence([string]$Path, [string]$Expected, [string]$Name) {
    $actual = (Get-SGE4FileSha256 $Path).ToUpperInvariant()
    if ($actual -ne $Expected) {
        throw "$Name evidence differs from the accepted CU5 exhaustive-audit digest. expected=$Expected actual=$actual"
    }
}

$expectedArchitecture = '1F1D09B4E52DBC35E961E0D3751B32292B2C30EEDCFA473800C5BB8E8CB5AB73'
$expectedControlled = '7F0247B193F9BC20F4EDA5FEED590C1DA5722EF36B133640292B1ED616CFF62B'
$expectedFresh = '091EAEC0D27287FE897F6813043FD75C090D5DA17054461DF314F99B2A6F5A92'
$acceptedAuditCommit = '67cb40b5204e1e06ecac576206ba969ec2db02b6'

$total = [Diagnostics.Stopwatch]::StartNew()
Invoke-CU5Stage 'Static boundary and manifest verification' {
    Invoke-Spiral7Checks @('tools\Verify-Spiral7CU2.ps1','tools\Verify-Spiral7CU3.ps1','tools\Verify-Spiral7CU4.ps1','tools\Verify-Spiral7CU5.ps1')
}
Invoke-CU5Stage 'Build Debug and Release' {
    Build-Spiral7Project '186_Spiral7ArchitectureQualificationTests\186_Spiral7ArchitectureQualificationTests.vcxproj'
}

$output = Join-Path $root 'build\tests\spiral7-cu5-routine'
Remove-Item -Recurse -Force -ErrorAction SilentlyContinue $output
New-Item -ItemType Directory -Force $output | Out-Null
$debugExe = Join-Path $root 'build\bin\x64\Debug\186_Spiral7ArchitectureQualificationTests.exe'
$releaseExe = Join-Path $root 'build\bin\x64\Release\186_Spiral7ArchitectureQualificationTests.exe'

Invoke-CU5Stage 'Debug representative WARP smoke' {
    Invoke-Checked $debugExe @('--debug-smoke')
}
Invoke-CU5Stage 'Release portable full-authority self-test' {
    Invoke-Checked $releaseExe @('--portable-self-test')
}

$architecture = Join-Path $output 'architecture-release.bin'
Invoke-CU5Stage 'Release 128-invocation WARP qualification' {
    Invoke-Checked $releaseExe @('--qualification','--emit',$architecture)
}
Require-FrozenEvidence $architecture $expectedArchitecture 'Architecture'

$controlled = Join-Path $output 'controlled-recovery-release.bin'
Invoke-CU5Stage 'Release Controlled Recovery' {
    Invoke-Checked $releaseExe @('--controlled','--emit',$controlled)
}
Require-FrozenEvidence $controlled $expectedControlled 'Controlled Recovery'

Invoke-CU5Stage 'Release ID3D12Device5::RemoveDevice quarantine' {
    Invoke-Checked $releaseExe @('--actual-removal')
}

$fresh = Join-Path $output 'fresh-release.bin'
Invoke-CU5Stage 'Release Fresh rematerialization' {
    Invoke-Checked $releaseExe @('--fresh-rematerialization','--emit',$fresh)
}
Require-FrozenEvidence $fresh $expectedFresh 'Fresh-process'
$total.Stop()

Write-Host '============================================================'
Write-Host 'SGE4-5 SPIRAL 7 CU5 ROUTINE REGRESSION PASSED'
Write-Host 'SGE4-5 SPIRAL 7 ARCHITECTURE COMPLETE EVIDENCE PRESERVED'
Write-Host 'Debug boundary: four-invocation A-B-C WARP smoke passed'
Write-Host 'Release qualification: 128 exact Sparse-Temporal invocations / 384 Candidate executions passed'
Write-Host 'Recovery and removal: Controlled Recovery, stale-handle rejection and RemoveDevice quarantine passed'
Write-Host "Frozen exhaustive audit commit: $acceptedAuditCommit"
Write-Host 'Determinism policy: routine Release evidence must equal the accepted Debug/Release exhaustive-audit digests'
Write-Host 'Runtime Candidate-policy decision: None'
Write-Host "CU5 routine elapsed: $($total.Elapsed)"
Write-Host "CU5 Architecture evidence SHA-256: $(Get-SGE4FileSha256 $architecture)"
Write-Host "CU5 Controlled Recovery evidence SHA-256: $(Get-SGE4FileSha256 $controlled)"
Write-Host "CU5 Fresh-process evidence SHA-256: $(Get-SGE4FileSha256 $fresh)"
Write-Host 'Full audit command: .\run_sge4_5_spiral7_cu5_exhaustive_audit.bat'
Write-Host '============================================================'
