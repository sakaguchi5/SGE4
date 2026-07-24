$ErrorActionPreference = 'Stop'
$testsRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$root = Split-Path -Parent $testsRoot

function Invoke-Step([string]$Name,[string]$File,[string[]]$Arguments=@()){
    Write-Host "[REFERENCE] START $Name"
    & $File @Arguments
    if($LASTEXITCODE -ne 0){throw "Reference step failed: $Name"}
    Write-Host "[REFERENCE] DONE  $Name"
}

$powershell = 'powershell.exe'
$common = @('-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File')

Invoke-Step 'Spiral 7 CU1 contract lineage' $powershell ($common + @((Join-Path $testsRoot 'tools\Verify-Spiral7CU1.ps1')))
Invoke-Step 'Spiral 7 CU2 architecture lineage' $powershell ($common + @((Join-Path $testsRoot 'tools\Verify-Spiral7CU2.ps1')))
Invoke-Step 'Spiral 7 CU3 authority lineage' $powershell ($common + @((Join-Path $testsRoot 'tools\Verify-Spiral7CU3.ps1')))
Invoke-Step 'Spiral 7 CU4 Candidate family lineage' $powershell ($common + @((Join-Path $testsRoot 'tools\Verify-Spiral7CU4.ps1')))
Invoke-Step 'Spiral 7 CU5 Architecture lineage' $powershell ($common + @((Join-Path $testsRoot 'tools\Verify-Spiral7CU5.ps1')))
Invoke-Step 'Spiral 7 CU6 paired Decision lineage' $powershell ($common + @((Join-Path $testsRoot 'tools\Verify-Spiral7CU6.ps1')))
Invoke-Step 'Level 4 v2 handoff' $powershell ($common + @((Join-Path $testsRoot 'tools\Verify-Level4V2Handoff.ps1')))
Invoke-Step 'Source Manifest' $powershell ($common + @((Join-Path $testsRoot 'tools\Verify-SourceManifest.ps1')))

Write-Host '============================================================'
Write-Host 'SGE4-5 SPIRAL 7 FROZEN REFERENCE GATE PASSED'
Write-Host 'Spiral 7: CLOSED'
Write-Host 'Next program: Level 4 v2 Canonical reconstruction'
Write-Host 'Runtime Candidate-policy authorization: None'
Write-Host 'Heavy CU5 WARP and CU6 real-GPU gates were not repeated.'
Write-Host '============================================================'
