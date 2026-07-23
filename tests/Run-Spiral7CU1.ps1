$ErrorActionPreference = 'Stop'

$testsRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$root = Split-Path -Parent $testsRoot

& (Join-Path $testsRoot 'tools\Verify-SourceManifest.ps1')
& (Join-Path $testsRoot 'tools\Verify-SGE4_5Identity.ps1')
& (Join-Path $testsRoot 'tools\Verify-Spiral7CU1.ps1') -Mode Auto

Write-Host '============================================================'
Write-Host 'SGE4-5 SPIRAL 7 COMPLETION UNIT 1 PASSED'
Write-Host 'Baseline: d0bb0d406fca8beabed2331daff870ea414dd87d'
Write-Host 'Owner selection: Versioned Sparse Delta Flow and Verified Incremental History Lowering'
Write-Host 'Meaning: exact Active / Modified / Activate / Deactivate / Update / Retain transition authority'
Write-Host 'Candidates: FullActiveDense / CompactDeltaIndex / AffectedBlockDelta'
Write-Host 'Spiral 6 status: CLOSED by Owner selection; Runtime policy remains None'
Write-Host 'CU1 implementation: documents and verification only; no C++ or ABI mutation'
Write-Host 'Next: CU2 baseline insufficiency proof and one CompactDelta architecture path'
Write-Host '============================================================'
