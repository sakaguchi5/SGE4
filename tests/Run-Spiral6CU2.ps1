$ErrorActionPreference = 'Stop'
$testsRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$root = Split-Path -Parent $testsRoot
. (Join-Path $testsRoot 'Spiral6Common.ps1')

Invoke-Spiral6Checks @('tools\Verify-Spiral6CU2.ps1')
Build-Spiral6Project '166_Spiral6SparseArchitectureTests\166_Spiral6SparseArchitectureTests.vcxproj'

$output = Join-Path $root 'build\tests\spiral6-cu2'
Remove-Item -Recurse -Force -ErrorAction SilentlyContinue $output
New-Item -ItemType Directory -Force $output | Out-Null
$debugExe = Join-Path $root 'build\bin\x64\Debug\166_Spiral6SparseArchitectureTests.exe'
$releaseExe = Join-Path $root 'build\bin\x64\Release\166_Spiral6SparseArchitectureTests.exe'
$debugBundle = Join-Path $output 'debug.bin'
$releaseBundle = Join-Path $output 'release.bin'

Invoke-Checked $debugExe @('--emit',$debugBundle)
Invoke-Checked $releaseExe @('--emit',$releaseBundle)
if (-not (Test-BytesEqual $debugBundle $releaseBundle)) {
    throw 'Spiral 6 CU2 Compact Index architecture evidence differs across Debug/Release.'
}

Write-Host '============================================================'
Write-Host 'SGE4-5 SPIRAL 6 COMPLETION UNIT 2 PASSED'
Write-Host 'Architecture: Exact Sparse Set -> fixed-capacity Compact Index List -> ceil(K/64) Dispatch -> original-index output'
Write-Host 'Cases: 12 WARP cases including K=0/1/65/1024/4095/4096 and all four Sparse patterns'
Write-Host 'Write set: exactly S; every inactive output record remains byte-identical float4(0,0,0,0)'
Write-Host 'Sidecar: VersionedSidecarSparseExtensionV1; unused index capacity is canonical 0xFFFFFFFF'
Write-Host 'Legacy boundary: Schema 17 / Runtime 17 / canonical Backend unchanged'
Write-Host 'Runtime sparse-policy decision: None'
Write-Host "CU2 architecture evidence SHA-256: $(Get-SGE4FileSha256 $debugBundle)"
Write-Host '============================================================'
