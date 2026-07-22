$ErrorActionPreference = 'Stop'
$testsRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$root = Split-Path -Parent $testsRoot
. (Join-Path $testsRoot 'Spiral6Common.ps1')

Invoke-Spiral6Checks @('tools\Verify-Spiral6CU2.ps1','tools\Verify-Spiral6CU3.ps1','tools\Verify-Spiral6CU4.ps1')
Build-Spiral6Project '168_Spiral6SparseCandidateFamilyTests\168_Spiral6SparseCandidateFamilyTests.vcxproj'

$output = Join-Path $root 'build\tests\spiral6-cu4'
Remove-Item -Recurse -Force -ErrorAction SilentlyContinue $output
New-Item -ItemType Directory -Force $output | Out-Null
$debugExe = Join-Path $root 'build\bin\x64\Debug\168_Spiral6SparseCandidateFamilyTests.exe'
$releaseExe = Join-Path $root 'build\bin\x64\Release\168_Spiral6SparseCandidateFamilyTests.exe'
$debugBundle = Join-Path $output 'debug.bin'
$releaseBundle = Join-Path $output 'release.bin'

Invoke-Checked $debugExe @('--emit',$debugBundle)
Invoke-Checked $releaseExe @('--emit',$releaseBundle)
if (-not (Test-BytesEqual $debugBundle $releaseBundle)) {
    throw 'Spiral 6 CU4 Sparse Candidate family evidence differs across Debug/Release.'
}

Write-Host '============================================================'
Write-Host 'SGE4-5 SPIRAL 6 COMPLETION UNIT 4 PASSED'
Write-Host 'Candidates: A.DenseMembershipMaskPredicate / B.CompactIndexListDispatch / C.ActiveBlockListLocalMask'
Write-Host 'Representations: 512-byte dense mask / 16384-byte compact index list / 64 x 16-byte active-block records'
Write-Host 'Cases: 12 Sparse sets x 3 Candidates = 36 WARP Candidate executions per build'
Write-Host 'Pairwise full-output equivalence: byte-identical for all 65536 output bytes'
Write-Host 'Authority: Exact Set / Candidate kind / Representation role / Dispatch / Resource / Device epoch'
Write-Host 'Mutations: 40 Raw family attacks plus set/resource/role/layout/epoch replay gates rejected'
Write-Host 'Runtime sparse-policy decision: None'
Write-Host "CU4 Candidate family evidence SHA-256: $(Get-SGE4FileSha256 $debugBundle)"
Write-Host '============================================================'
