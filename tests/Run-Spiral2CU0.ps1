param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('Stage00','Stage01','Stage02','CU0')]
    [string]$Mode
)
$ErrorActionPreference = 'Stop'
$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
[Console]::InputEncoding = $utf8NoBom
[Console]::OutputEncoding = $utf8NoBom
$OutputEncoding = $utf8NoBom
$testsRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$root = Split-Path -Parent $testsRoot
function Invoke-Checked([string]$FilePath,[string[]]$Arguments=@()) { & $FilePath @Arguments; if ($LASTEXITCODE -ne 0) { throw "Command failed: $FilePath" } }

Invoke-Checked 'powershell.exe' @('-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',(Join-Path $testsRoot 'tools\Verify-SGE4_5Identity.ps1'))
Invoke-Checked 'powershell.exe' @('-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',(Join-Path $root 'verify_dependencies.ps1'))
Invoke-Checked 'powershell.exe' @('-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',(Join-Path $testsRoot 'tools\Verify-ScriptContracts.ps1'))
Invoke-Checked 'powershell.exe' @('-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',(Join-Path $testsRoot 'tools\Verify-SourceManifest.ps1'))
Invoke-Checked 'powershell.exe' @('-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',(Join-Path $testsRoot 'tools\Verify-Spiral2ResearchContract.ps1'),'-EnforceCU0ProjectAbsence')

if ($Mode -in @('Stage02','CU0')) {
    Invoke-Checked (Join-Path $root 'run_sge4_5_foundation.bat')
    Invoke-Checked (Join-Path $root 'run_sge4_5_gate.bat')
}

Write-Host '============================================================'
Write-Host 'SGE4-5 SPIRAL 2 CU0 RESEARCH CONTRACT AND BASELINE PASSED'
Write-Host 'Stages: S2-00 Decision / S2-01 Specification / S2-02 Baseline'
Write-Host 'Baseline: afaed65d9400f3f80a7f5a9094edd9029082f95d is an ancestor'
Write-Host 'Target ABI: D3D12 Schema 17 / Runtime 17'
Write-Host 'Independent software verifier remains mandatory'
Write-Host 'NextCapabilitySelection = DeferredByOwner'
Write-Host 'SelectionStatus = OWNER_DECISION_REQUIRED'
Write-Host '============================================================'

