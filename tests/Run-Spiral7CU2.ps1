$ErrorActionPreference = 'Stop'
$testsRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$root = Split-Path -Parent $testsRoot
. (Join-Path $testsRoot 'Spiral7Common.ps1')

Invoke-Spiral7Checks @('tools\Verify-Spiral7CU2.ps1')
Build-Spiral7Project '182_Spiral7DeltaArchitectureTests\182_Spiral7DeltaArchitectureTests.vcxproj'

$output = Join-Path $root 'build\tests\spiral7-cu2'
Remove-Item -Recurse -Force -ErrorAction SilentlyContinue $output
New-Item -ItemType Directory -Force $output | Out-Null
$debugExe = Join-Path $root 'build\bin\x64\Debug\182_Spiral7DeltaArchitectureTests.exe'
$releaseExe = Join-Path $root 'build\bin\x64\Release\182_Spiral7DeltaArchitectureTests.exe'
$debugBundle = Join-Path $output 'debug.bin'
$releaseBundle = Join-Path $output 'release.bin'

Invoke-Checked $debugExe @('--emit',$debugBundle)
Invoke-Checked $releaseExe @('--emit',$releaseBundle)
if (-not (Test-BytesEqual $debugBundle $releaseBundle)) {
    throw 'Spiral 7 CU2 Compact Delta architecture evidence differs across Debug/Release.'
}

Write-Host '============================================================'
Write-Host 'SGE4-5 SPIRAL 7 COMPLETION UNIT 2 PASSED'
Write-Host 'Architecture: previous history + exact A_t/M_t -> W_t/H_t/R_t/T_t -> Compact Delta Index Action List'
Write-Host 'Execution: ExecuteIndirect ceil(|T_t|/64), Update W_t, Clear R_t, no write to H_t'
Write-Host 'Cases: 14 WARP transitions including Hold, Dirty, Activate, Deactivate, Churn, Migration and Empty Reset'
Write-Host 'Sidecar: VersionedSparseTemporalDeltaExtensionV1; fixed capacity 4096 uint2 records / 32768 bytes'
Write-Host 'Legacy boundary: Schema 17 / Runtime 17 / canonical Backend / Composition Contract unchanged'
Write-Host 'Runtime transition-policy decision: None'
Write-Host "CU2 architecture evidence SHA-256: $(Get-SGE4FileSha256 $debugBundle)"
Write-Host '============================================================'
