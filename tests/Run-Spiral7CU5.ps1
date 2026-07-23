$ErrorActionPreference = 'Stop'
$testsRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$root = Split-Path -Parent $testsRoot
. (Join-Path $testsRoot 'Spiral7Common.ps1')

function Invoke-CU5Stage([string]$Name, [scriptblock]$Action) {
    Write-Host "[CU5] START $Name"
    $watch = [Diagnostics.Stopwatch]::StartNew()
    & $Action
    $watch.Stop()
    Write-Host ("[CU5] DONE  {0} elapsed={1}" -f $Name,$watch.Elapsed)
}

$total = [Diagnostics.Stopwatch]::StartNew()
Invoke-CU5Stage 'Static boundary and manifest verification' {
    Invoke-Spiral7Checks @('tools\Verify-Spiral7CU2.ps1','tools\Verify-Spiral7CU3.ps1','tools\Verify-Spiral7CU4.ps1','tools\Verify-Spiral7CU5.ps1')
}
Invoke-CU5Stage 'Build Debug and Release' {
    Build-Spiral7Project '186_Spiral7ArchitectureQualificationTests\186_Spiral7ArchitectureQualificationTests.vcxproj'
}

$output = Join-Path $root 'build\tests\spiral7-cu5'
Remove-Item -Recurse -Force -ErrorAction SilentlyContinue $output
New-Item -ItemType Directory -Force $output | Out-Null
$debugExe = Join-Path $root 'build\bin\x64\Debug\186_Spiral7ArchitectureQualificationTests.exe'
$releaseExe = Join-Path $root 'build\bin\x64\Release\186_Spiral7ArchitectureQualificationTests.exe'

Invoke-CU5Stage 'Portable authority self-test Debug' { Invoke-Checked $debugExe @('--portable-self-test') }
Invoke-CU5Stage 'Portable authority self-test Release' { Invoke-Checked $releaseExe @('--portable-self-test') }

$debugArchitecture = Join-Path $output 'architecture-debug.bin'
$releaseArchitecture = Join-Path $output 'architecture-release.bin'
Invoke-CU5Stage '128-invocation WARP qualification Debug' {
    Invoke-Checked $debugExe @('--qualification','--emit',$debugArchitecture)
}
Invoke-CU5Stage '128-invocation WARP qualification Release' {
    Invoke-Checked $releaseExe @('--qualification','--emit',$releaseArchitecture)
}
if (-not (Test-BytesEqual $debugArchitecture $releaseArchitecture)) {
    throw 'Spiral 7 CU5 Architecture evidence differs across Debug/Release.'
}

$debugControlled = Join-Path $output 'controlled-recovery-debug.bin'
$releaseControlled = Join-Path $output 'controlled-recovery-release.bin'
Invoke-CU5Stage 'Controlled Recovery Debug' {
    Invoke-Checked $debugExe @('--controlled','--emit',$debugControlled)
}
Invoke-CU5Stage 'Controlled Recovery Release' {
    Invoke-Checked $releaseExe @('--controlled','--emit',$releaseControlled)
}
if (-not (Test-BytesEqual $debugControlled $releaseControlled)) {
    throw 'Spiral 7 CU5 Controlled Recovery evidence differs across Debug/Release.'
}

Invoke-CU5Stage 'Actual ID3D12Device5::RemoveDevice quarantine Debug' {
    Invoke-Checked $debugExe @('--actual-removal')
}

$debugFresh = Join-Path $output 'fresh-debug.bin'
$releaseFresh = Join-Path $output 'fresh-release.bin'
Invoke-CU5Stage 'Fresh rematerialization Debug' {
    Invoke-Checked $debugExe @('--fresh-rematerialization','--emit',$debugFresh)
}
Invoke-CU5Stage 'Fresh rematerialization Release' {
    Invoke-Checked $releaseExe @('--fresh-rematerialization','--emit',$releaseFresh)
}
if (-not (Test-BytesEqual $debugFresh $releaseFresh)) {
    throw 'Spiral 7 CU5 Fresh-process evidence differs across Debug/Release.'
}
$total.Stop()

Write-Host '============================================================'
Write-Host 'SGE4-5 SPIRAL 7 COMPLETION UNIT 5 PASSED'
Write-Host 'SGE4-5 SPIRAL 7 ARCHITECTURE COMPLETE'
Write-Host 'Qualification: 128 exact Sparse-Temporal invocations / 384 A-B-C WARP Candidate executions per configuration'
Write-Host 'Optimized execution: A/B/C share one CommandList submission and one Fence wait per invocation'
Write-Host 'Resource lifetime: fixed-capacity Upload/UAV/Readback buffers are reused across the complete timeline'
Write-Host 'Coverage: all frozen Active counts, Transition counts and eight transition kinds'
Write-Host 'Recovery: old epoch handles rejected; explicit A_t/M_t rebind and exact-generation full-active rebuild required'
Write-Host 'Actual removal: ID3D12Device5::RemoveDevice quarantine and removed-WARP-LUID exclusion passed'
Write-Host 'Determinism: Architecture, Controlled Recovery and Fresh-process evidence are Debug/Release byte-identical'
Write-Host 'Runtime Candidate-policy decision: None'
Write-Host "CU5 total elapsed: $($total.Elapsed)"
Write-Host "CU5 Architecture evidence SHA-256: $(Get-SGE4FileSha256 $debugArchitecture)"
Write-Host "CU5 Controlled Recovery evidence SHA-256: $(Get-SGE4FileSha256 $debugControlled)"
Write-Host "CU5 Fresh-process evidence SHA-256: $(Get-SGE4FileSha256 $debugFresh)"
Write-Host '============================================================'
