$ErrorActionPreference = 'Stop'

$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
[Console]::InputEncoding = $utf8NoBom
[Console]::OutputEncoding = $utf8NoBom
$OutputEncoding = $utf8NoBom

$testsRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$root = Split-Path -Parent $testsRoot

function Invoke-Checked([string]$FilePath, [string[]]$Arguments = @()) {
    & $FilePath @Arguments
    if ($LASTEXITCODE -ne 0) { throw "Command failed: $FilePath" }
}

Invoke-Checked 'powershell.exe' @('-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',(Join-Path $testsRoot 'tools\Verify-SGE4_5Identity.ps1'))
Invoke-Checked 'powershell.exe' @('-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',(Join-Path $root 'verify_dependencies.ps1'))
Invoke-Checked 'powershell.exe' @('-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',(Join-Path $testsRoot 'tools\Verify-ScriptContracts.ps1'))
Invoke-Checked 'powershell.exe' @('-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',(Join-Path $testsRoot 'tools\Verify-SourceManifest.ps1'))
Invoke-Checked (Join-Path $root 'run_sge4_5_level4v1_final.bat')

Write-Host '============================================================'
Write-Host 'SGE4-5 SPIRAL FOUNDATION BOOTSTRAP PASSED'
Write-Host 'Baseline archive: SGE4v1.zip'
Write-Host 'Independent solution: SemanticGpuEngine4-5.sln'
Write-Host 'Independent namespace: sge4_5'
Write-Host 'Level 1-3 execution and planning authority preserved'
Write-Host 'Level 4 v1 composition, runtime and recovery requalified'
Write-Host 'Ready for Spiral 1 Research Contract'
Write-Host '============================================================'
