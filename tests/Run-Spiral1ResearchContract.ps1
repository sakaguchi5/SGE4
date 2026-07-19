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
Invoke-Checked 'powershell.exe' @('-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',(Join-Path $testsRoot 'tools\Verify-Spiral1ResearchContract.ps1'),'-EnforceStage03ProjectAbsence')

Write-Host '============================================================'
Write-Host 'SGE4-5 STAGE 03 SPIRAL 1 RESEARCH CONTRACT FREEZE PASSED'
Write-Host 'Foundation commit: 2e79dbf96255f1cac4dbb9fc133662123e78cf4a'
Write-Host 'Foundation: Level 4 v1 / Schema 17 / Runtime 17 preserved'
Write-Host 'Research question: PGA rigid-transform representation retention'
Write-Host 'Lowering contracts: MatrixExpandedComputeV1 / DirectPgaMotorComputeV1'
Write-Host 'Observation: Corpus V1 and Observation Contract V1 frozen'
Write-Host 'Authority: Candidate, Verifier, Frozen Leaf and Composition boundaries frozen'
Write-Host 'Implementation: no Spiral 1 C++ feature code added in Stage 03'
Write-Host 'Ready for Stage 04 PGA Math and Canonical Semantic'
Write-Host '============================================================'
