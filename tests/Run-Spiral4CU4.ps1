$ErrorActionPreference = 'Stop'

$testsRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$root = Split-Path -Parent $testsRoot
. (Join-Path $testsRoot 'Spiral4Common.ps1')

Invoke-Spiral4Checks @(
    'tools\Verify-Spiral4CU2.ps1',
    'tools\Verify-Spiral4CU3.ps1',
    'tools\Verify-Spiral4CU4.ps1'
)

Build-Spiral4Project '142_Spiral4CandidateFamilyTests\142_Spiral4CandidateFamilyTests.vcxproj'

$output = Join-Path $root 'build\tests\spiral4-cu4'
Remove-Item -Recurse -Force -ErrorAction SilentlyContinue $output
New-Item -ItemType Directory -Force $output | Out-Null

$debugExe = Join-Path $root 'build\bin\x64\Debug\142_Spiral4CandidateFamilyTests.exe'
$releaseExe = Join-Path $root 'build\bin\x64\Release\142_Spiral4CandidateFamilyTests.exe'
$debugBundle = Join-Path $output 'debug-candidate-family.bin'
$releaseBundle = Join-Path $output 'release-candidate-family.bin'

Invoke-Checked $debugExe @('--emit', $debugBundle)
Invoke-Checked $releaseExe @('--emit', $releaseBundle)

if (-not (Test-BytesEqual $debugBundle $releaseBundle)) {
    throw 'Spiral 4 CU4 Candidate family bundle differs across Debug/Release.'
}

Write-Host '============================================================'
Write-Host 'SGE4-5 SPIRAL 4 COMPLETION UNIT 4 PASSED'
Write-Host 'Candidates: A.FixedMaximum + B.SingleIndirect + C.BatchedIndirect(B64/B128/B256/B512/B1024)'
Write-Host 'Authority: Planner-independent derivation for all seven Candidates'
Write-Host 'Batch proof: 5 sizes x 19 Active Counts = 95 canonical partitions'
Write-Host 'WARP equivalence: 7 Candidates x Nf=0/65/4096 = 21 cases'
Write-Host 'Mutation gate: 24 invalid or attacker-rehashed proposals rejected'
Write-Host 'Runtime dispatch-dimension decision: None'
Write-Host "CU4 Candidate family bundle SHA-256: $(Get-SGE4FileSha256 $debugBundle)"
Write-Host '============================================================'
