$ErrorActionPreference = 'Stop'

$testsRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$root = Split-Path -Parent $testsRoot
. (Join-Path $testsRoot 'Spiral4Common.ps1')

Invoke-Spiral4Checks @('tools\Verify-Spiral4CU2.ps1')

Build-Spiral4Project '140_Spiral4IndirectArchitectureTests\140_Spiral4IndirectArchitectureTests.vcxproj'

$output = Join-Path $root 'build\tests\spiral4-cu2'
Remove-Item -Recurse -Force -ErrorAction SilentlyContinue $output
New-Item -ItemType Directory -Force $output | Out-Null

$debugExe = Join-Path $root 'build\bin\x64\Debug\140_Spiral4IndirectArchitectureTests.exe'
$releaseExe = Join-Path $root 'build\bin\x64\Release\140_Spiral4IndirectArchitectureTests.exe'
$debugBundle = Join-Path $output 'debug.bin'
$releaseBundle = Join-Path $output 'release.bin'

Invoke-Checked $debugExe @('--emit', $debugBundle)
Invoke-Checked $releaseExe @('--emit', $releaseBundle)

if (-not (Test-BytesEqual $debugBundle $releaseBundle)) {
    throw 'Spiral 4 CU2 architecture artifact differs across Debug/Release.'
}

Write-Host '============================================================'
Write-Host 'SGE4-5 SPIRAL 4 COMPLETION UNIT 2 PASSED'
Write-Host 'Architecture: GPU Argument Producer -> verified Dispatch Command Signature -> ExecuteIndirect'
Write-Host 'Active Count: Nf=0/1/63/64/65/1024/4096 qualified on WARP'
Write-Host 'Legacy boundary: D3D12 Schema 17 / Runtime 17 files remain byte-unchanged'
Write-Host 'Runtime dispatch-dimension decision: None'
Write-Host "CU2 artifact SHA-256: $(Get-SGE4FileSha256 $debugBundle)"
Write-Host '============================================================'
