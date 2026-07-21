$ErrorActionPreference='Stop'
. (Join-Path (Split-Path -Parent $MyInvocation.MyCommand.Path) 'Spiral3Common.ps1')
Invoke-Spiral3Checks @('tools\Verify-Spiral3CU0.ps1')
Write-Host '============================================================'
Write-Host 'SGE4-5 SPIRAL 3 COMPLETION UNIT 0 PASSED'
Write-Host 'Baseline: 398891936bf498fed532b26c06884bbfbf66f3a8'
Write-Host 'Research contract: fixed-count reuse sweep / materialization boundary'
Write-Host 'Target ABI: D3D12 Schema 17 / Runtime 17'
Write-Host '============================================================'
