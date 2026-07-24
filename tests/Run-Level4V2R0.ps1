$ErrorActionPreference = 'Stop'
$testsRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
function Invoke-Step([string]$Name,[string]$File,[string[]]$Arguments=@()){
    Write-Host "[L4V2-R0] START $Name"
    & $File @Arguments
    if($LASTEXITCODE -ne 0){throw "Level 4 v2 R0 step failed: $Name"}
    Write-Host "[L4V2-R0] DONE  $Name"
}
$powershell = 'powershell.exe'
$common = @('-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File')
Invoke-Step 'Frozen Spiral 7 reference gate' $powershell ($common + @((Join-Path $testsRoot 'Run-Spiral7ReferenceGate.ps1')))
Invoke-Step 'R0 input freeze and ownership map' $powershell ($common + @((Join-Path $testsRoot 'tools\Verify-Level4V2R0.ps1')))
Invoke-Step 'Source Manifest' $powershell ($common + @((Join-Path $testsRoot 'tools\Verify-SourceManifest.ps1')))
Write-Host '============================================================'
Write-Host 'SGE4 LEVEL 4 V2 R0 INPUT FREEZE PASSED'
Write-Host 'R0 COMPLETE'
Write-Host 'Carried invariants: 40'
Write-Host 'Implementation mutation: None'
Write-Host 'Runtime Candidate-policy authorization: None'
Write-Host 'NEXT: R1 CANONICAL VOCABULARY'
Write-Host '============================================================'
