$ErrorActionPreference = 'Stop'

$testsRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$root = Split-Path -Parent $testsRoot

& (Join-Path $testsRoot 'tools\Verify-SourceManifest.ps1')
& (Join-Path $testsRoot 'tools\Verify-SGE4_5Identity.ps1')
& (Join-Path $testsRoot 'tools\Verify-Spiral6CU1.ps1') -Mode Auto

Write-Host '============================================================'
Write-Host 'SGE4-5 SPIRAL 6 COMPLETION UNIT 1 PASSED'
Write-Host 'Baseline: 46554ab55e532c438c9c4214ff1df3e7cd68638e'
Write-Host 'Owner selection: Exact Sparse Work-Set Flow and Verified Sparse Lowering'
Write-Host 'Sparse meaning: exact non-prefix subset S of a fixed 4096-record universe'
Write-Host 'Candidates: DenseMask / CompactIndexList / ActiveBlockListLocalMask'
Write-Host 'Spiral 5 status: CLOSED by Owner selection; Runtime policy remains None'
Write-Host 'CU1 implementation: documents and verification only; no C++ or ABI mutation'
Write-Host '============================================================'
