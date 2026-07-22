$ErrorActionPreference = 'Stop'

$testsRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$root = Split-Path -Parent $testsRoot

& (Join-Path $testsRoot 'tools\Verify-SourceManifest.ps1')
& (Join-Path $testsRoot 'tools\Verify-SGE4_5Identity.ps1')
& (Join-Path $testsRoot 'tools\Verify-Spiral4CU1.ps1') -Mode Auto

Write-Host '============================================================'
Write-Host 'SGE4-5 SPIRAL 4 COMPLETION UNIT 1 PASSED'
Write-Host 'Baseline: 8c1125394ba4b45d571d7cba4e7ad685bb90918b'
Write-Host 'Research contract: dynamic Active Work Count / verified indirect dispatch / Batch lowering'
Write-Host 'CU1 implementation: documents and verification only; no C++ or ABI mutation'
Write-Host '============================================================'
