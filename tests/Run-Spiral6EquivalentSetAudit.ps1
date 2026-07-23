param([int]$AdapterIndex = 0)
$ErrorActionPreference = 'Stop'
$testsRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$root = Split-Path -Parent $testsRoot
. (Join-Path $testsRoot 'Spiral6Common.ps1')

Invoke-Spiral6Checks @(
    'tools\Verify-Spiral6CU6.ps1',
    'tools\Verify-Spiral6EquivalentSetAudit.ps1')
Build-Spiral6Project '172_Spiral6EquivalentSetAuditTests\172_Spiral6EquivalentSetAuditTests.vcxproj'

$debugExe = Join-Path $root 'build\bin\x64\Debug\172_Spiral6EquivalentSetAuditTests.exe'
$releaseExe = Join-Path $root 'build\bin\x64\Release\172_Spiral6EquivalentSetAuditTests.exe'
Invoke-Checked $debugExe @('--self-test')
Invoke-Checked $releaseExe @('--self-test')

$baseline = Join-Path $root 'build\tests\spiral6-cu6\measurement\spiral6_measurement_samples_v1.csv'
if (-not (Test-Path -LiteralPath $baseline -PathType Leaf)) {
    throw 'Equivalent Set Audit requires the original CU6 spiral6_measurement_samples_v1.csv.'
}
$output = Join-Path $root 'build\tests\spiral6-cu6\equivalent-set-audit'
Remove-Item -Recurse -Force -ErrorAction SilentlyContinue $output
$balancedA = Join-Path $output 'balanced-a'
$fixed = Join-Path $output 'fixed'
$balancedB = Join-Path $output 'balanced-b'
New-Item -ItemType Directory -Force $balancedA,$fixed,$balancedB | Out-Null

# Each process records 432 samples. Balanced schedules give every equivalent label
# 24 samples per Candidate with Pattern and Candidate position balance.
Invoke-Checked $releaseExe @('--audit','--schedule','balanced','--rounds','1','--adapter-index',"$AdapterIndex",'--output',$balancedA)
Invoke-Checked $releaseExe @('--audit','--schedule','fixed','--rounds','4','--adapter-index',"$AdapterIndex",'--output',$fixed)
Invoke-Checked $releaseExe @('--audit','--schedule','balanced','--rounds','1','--adapter-index',"$AdapterIndex",'--output',$balancedB)

$report = Join-Path $output 'SPIRAL6_EQUIVALENT_SET_AUDIT_REPORT.md'
& (Join-Path $testsRoot 'tools\Analyze-Spiral6EquivalentSetAudit.ps1') -BaselineCsv $baseline -AuditRoot $output -OutputPath $report
if ($LASTEXITCODE -ne 0) { throw "Equivalent Set Audit analyzer failed with exit code $LASTEXITCODE." }
if (-not (Test-Path -LiteralPath $report -PathType Leaf)) { throw 'Equivalent Set Audit report was not generated.' }

$reportDigest = (Get-FileHash -Algorithm SHA256 -LiteralPath $report).Hash
Write-Host '============================================================'
Write-Host 'SGE4-5 SPIRAL 6 CU6 EQUIVALENT SET AUDIT COMPLETE'
Write-Host 'Controls: K=1 Prefix=Uniform={0}; K=4096 all Patterns=full universe'
Write-Host 'Fresh processes: Balanced A / Fixed Pattern-major / Balanced B'
Write-Host 'Samples: 432 per process, 1296 total'
Write-Host 'Structural identity: Set / Artifact / Resource authority / Dispatch / Output enforced'
Write-Host 'Telemetry: D3D12 timestamp calibration and DXGI memory budget/usage retained'
Write-Host "Report: $report"
Write-Host "Report SHA-256: $reportDigest"
Write-Host '============================================================'
