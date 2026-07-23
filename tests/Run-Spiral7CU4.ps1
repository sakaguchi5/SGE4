$ErrorActionPreference = 'Stop'
$testsRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$root = Split-Path -Parent $testsRoot
. (Join-Path $testsRoot 'Spiral7Common.ps1')

Invoke-Spiral7Checks @('tools\Verify-Spiral7CU2.ps1','tools\Verify-Spiral7CU3.ps1','tools\Verify-Spiral7CU4.ps1')
Build-Spiral7Project '184_Spiral7CandidateFamilyTests\184_Spiral7CandidateFamilyTests.vcxproj'

$output = Join-Path $root 'build\tests\spiral7-cu4'
Remove-Item -Recurse -Force -ErrorAction SilentlyContinue $output
New-Item -ItemType Directory -Force $output | Out-Null
$debugExe = Join-Path $root 'build\bin\x64\Debug\184_Spiral7CandidateFamilyTests.exe'
$releaseExe = Join-Path $root 'build\bin\x64\Release\184_Spiral7CandidateFamilyTests.exe'
$debugBundle = Join-Path $output 'debug.bin'
$releaseBundle = Join-Path $output 'release.bin'

Invoke-Checked $debugExe @('--emit',$debugBundle)
Invoke-Checked $releaseExe @('--emit',$releaseBundle)
if (-not (Test-BytesEqual $debugBundle $releaseBundle)) {
    throw 'Spiral 7 CU4 Candidate Family evidence differs across Debug/Release.'
}

Write-Host '============================================================'
Write-Host 'SGE4-5 SPIRAL 7 COMPLETION UNIT 4 PASSED'
Write-Host 'Candidate family: A=FullActiveDenseRecompute, B=CompactDeltaIndexHistoryReuse, C=AffectedBlockDeltaHistoryReuse'
Write-Host 'Timeline: 18 chained invocations / 54 WARP Candidate executions'
Write-Host 'Equivalence: A/B/C full-output bytes are pairwise identical after every invocation'
Write-Host 'Write authority: A=entire 4096-record universe; B/C=exactly T_t; H_t remains byte-identical for B/C'
Write-Host 'Block proof: disjoint Update/Clear masks reconstruct exactly W_t and R_t'
Write-Host 'Mutation gates: 16 Raw Candidate attacks plus cross-invocation Verified replay rejection'
Write-Host 'Runtime Candidate-policy decision: None'
Write-Host "CU4 Candidate Family evidence SHA-256: $(Get-SGE4FileSha256 $debugBundle)"
Write-Host '============================================================'
