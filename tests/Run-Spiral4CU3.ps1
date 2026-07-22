$ErrorActionPreference = 'Stop'

$testsRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$root = Split-Path -Parent $testsRoot
. (Join-Path $testsRoot 'Spiral4Common.ps1')

Invoke-Spiral4Checks @(
    'tools\Verify-Spiral4CU2.ps1',
    'tools\Verify-Spiral4CU3.ps1'
)

Build-Spiral4Project '141_Spiral4AuthorityMutationTests\141_Spiral4AuthorityMutationTests.vcxproj'

$output = Join-Path $root 'build\tests\spiral4-cu3'
Remove-Item -Recurse -Force -ErrorAction SilentlyContinue $output
New-Item -ItemType Directory -Force $output | Out-Null

$debugExe = Join-Path $root 'build\bin\x64\Debug\141_Spiral4AuthorityMutationTests.exe'
$releaseExe = Join-Path $root 'build\bin\x64\Release\141_Spiral4AuthorityMutationTests.exe'
$debugBundle = Join-Path $output 'debug-authority.bin'
$releaseBundle = Join-Path $output 'release-authority.bin'

Invoke-Checked $debugExe @('--emit', $debugBundle)
Invoke-Checked $releaseExe @('--emit', $releaseBundle)

if (-not (Test-BytesEqual $debugBundle $releaseBundle)) {
    throw 'Spiral 4 CU3 authority bundle differs across Debug/Release.'
}

Write-Host '============================================================'
Write-Host 'SGE4-5 SPIRAL 4 COMPLETION UNIT 3 PASSED'
Write-Host 'Authority: Raw Candidate -> Planner-independent Verifier -> opaque Verified Plan'
Write-Host 'Binding: Verified Plan -> actual sidecar artifact -> compiled shader digests'
Write-Host 'Mutations: 27 invalid proposals rejected'
Write-Host 'Replay: target / command-signature / observation contexts rejected'
Write-Host 'Verified WARP: Nf=0/65/4096'
Write-Host 'Runtime dispatch-dimension decision: None'
Write-Host "CU3 authority bundle SHA-256: $(Get-SGE4FileSha256 $debugBundle)"
Write-Host '============================================================'
