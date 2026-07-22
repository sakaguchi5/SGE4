$ErrorActionPreference = 'Stop'

$testsRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$root = Split-Path -Parent $testsRoot

& (Join-Path $testsRoot 'tools\Verify-SourceManifest.ps1')
& (Join-Path $testsRoot 'tools\Verify-SGE4_5Identity.ps1')
& (Join-Path $testsRoot 'tools\Verify-Spiral5CU1.ps1') -Mode Auto

Write-Host '============================================================'
Write-Host 'SGE4-5 SPIRAL 5 COMPLETION UNIT 1 PASSED'
Write-Host 'Baseline: f29d6597ec370d963c7b7dfbbc9af9590e8bd58f'
Write-Host 'Research contract: Temporal State Flow / update frequency / verified history reuse'
Write-Host 'Temporal meaning: exact piecewise constant; interpolation is not in V1'
Write-Host 'Candidates: EveryInvocation / GlobalMotorHistory / FinalOutputHistory'
Write-Host 'CU1 implementation: documents and verification only; no C++ or ABI mutation'
Write-Host '============================================================'
