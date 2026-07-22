$ErrorActionPreference = 'Stop'
$testsRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$root = Split-Path -Parent $testsRoot
. (Join-Path $testsRoot 'Spiral5Common.ps1')

Invoke-Spiral5Checks @(
    'tools\Verify-Spiral5CU2.ps1',
    'tools\Verify-Spiral5CU3.ps1',
    'tools\Verify-Spiral5CU4.ps1',
    'tools\Verify-Spiral5CU5.ps1'
)

Build-Spiral5Project '151_Spiral5ArchitectureQualificationTests\151_Spiral5ArchitectureQualificationTests.vcxproj'

$output = Join-Path $root 'build\tests\spiral5-cu5'
Remove-Item -Recurse -Force -ErrorAction SilentlyContinue $output
New-Item -ItemType Directory -Force $output | Out-Null

$debugExe = Join-Path $root 'build\bin\x64\Debug\151_Spiral5ArchitectureQualificationTests.exe'
$releaseExe = Join-Path $root 'build\bin\x64\Release\151_Spiral5ArchitectureQualificationTests.exe'

$architectureDebugA = Join-Path $output 'architecture-debug-a.bin'
$architectureDebugB = Join-Path $output 'architecture-debug-b.bin'
$architectureRelease = Join-Path $output 'architecture-release.bin'
Invoke-Checked $debugExe @('--qualification','--emit',$architectureDebugA)
Invoke-Checked $debugExe @('--qualification','--emit',$architectureDebugB)
Invoke-Checked $releaseExe @('--qualification','--emit',$architectureRelease)
if (-not (Test-BytesEqual $architectureDebugA $architectureDebugB) -or
    -not (Test-BytesEqual $architectureDebugA $architectureRelease)) {
    throw 'CU5 Architecture evidence differs across fresh Debug A, Debug B, and Release processes.'
}

$controlledDebugA = Join-Path $output 'controlled-debug-a.bin'
$controlledDebugB = Join-Path $output 'controlled-debug-b.bin'
$controlledRelease = Join-Path $output 'controlled-release.bin'
Invoke-Checked $debugExe @('--controlled','--emit',$controlledDebugA)
Invoke-Checked $debugExe @('--controlled','--emit',$controlledDebugB)
Invoke-Checked $releaseExe @('--controlled','--emit',$controlledRelease)
if (-not (Test-BytesEqual $controlledDebugA $controlledDebugB) -or
    -not (Test-BytesEqual $controlledDebugA $controlledRelease)) {
    throw 'CU5 Controlled Recovery evidence differs across Debug A, Debug B, and Release.'
}

Invoke-Checked $debugExe @('--actual-removal')

$freshDebugA = Join-Path $output 'fresh-rematerialization-debug-a.bin'
$freshDebugB = Join-Path $output 'fresh-rematerialization-debug-b.bin'
$freshRelease = Join-Path $output 'fresh-rematerialization-release.bin'
Invoke-Checked $debugExe @('--fresh-rematerialization','--emit',$freshDebugA)
Invoke-Checked $debugExe @('--fresh-rematerialization','--emit',$freshDebugB)
Invoke-Checked $releaseExe @('--fresh-rematerialization','--emit',$freshRelease)
if (-not (Test-BytesEqual $freshDebugA $freshDebugB) -or
    -not (Test-BytesEqual $freshDebugA $freshRelease)) {
    throw 'CU5 fresh-process rematerialization evidence differs across Debug A, Debug B, and Release.'
}

Write-Host '============================================================'
Write-Host 'SGE4-5 SPIRAL 5 ARCHITECTURE COMPLETE'
Write-Host 'Completion Unit 5: PASSED'
Write-Host 'WARP corpus: 9 Schedules x 3 Candidates x 128 Invocations = 3456 Candidate invocations'
Write-Host 'Hold stability and no-unauthorized-write proofs: qualified'
Write-Host 'Determinism: fresh Debug A / Debug B / Release byte-identical'
Write-Host 'Controlled Recovery: epoch advance, stale history rejection, explicit rebind and reseed'
Write-Host 'Actual Recovery: ID3D12Device5::RemoveDevice -> AwaitingAdapter quarantine'
Write-Host 'Fresh process: same Frozen Temporal authority rematerialized and observed'
Write-Host 'Runtime temporal-policy decision: None'
Write-Host "Architecture evidence SHA-256: $(Get-SGE4FileSha256 $architectureDebugA)"
Write-Host "Controlled Recovery evidence SHA-256: $(Get-SGE4FileSha256 $controlledDebugA)"
Write-Host "Fresh rematerialization evidence SHA-256: $(Get-SGE4FileSha256 $freshDebugA)"
Write-Host 'Next: CU6 real-GPU measurement and Decision Report'
Write-Host '============================================================'
