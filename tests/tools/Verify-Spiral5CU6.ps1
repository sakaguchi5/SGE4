$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
function Require([bool]$Condition,[string]$Message) { if (-not $Condition) { throw $Message } }

$manifestPath = Join-Path $root 'docs\spiral5\CU6_MEASUREMENT_MANIFEST_V1.json'
Require (Test-Path -LiteralPath $manifestPath -PathType Leaf) 'Spiral 5 CU6 manifest missing.'
$m = Get-Content -Raw -LiteralPath $manifestPath -Encoding UTF8 | ConvertFrom-Json
Require ($m.schema -eq 'SGE4-5.Spiral5.CU6MeasurementManifest.V1') 'CU6 manifest schema mismatch.'
Require ($m.completionUnit -eq 'CU6') 'CU6 completion unit mismatch.'
Require ($m.baselineCommit -eq 'a3d9a19a1a3033b723df918e70f5af6a24d884a0') 'CU6 baseline mismatch.'
Require ($m.predecessor.architectureEvidenceSha256 -eq 'EB4937B1181E3377FD5367D02F218C682AA1F402A5C4099D8B5F452C103D84A1') 'CU6 Architecture evidence binding mismatch.'
Require ($m.predecessor.controlledRecoveryEvidenceSha256 -eq '6DFBA1F98F1338BE96E7985C7532A20A1B4144B81408BEA0FFD8C03ADCA4DB17') 'CU6 Controlled Recovery evidence binding mismatch.'
Require ($m.predecessor.freshRematerializationEvidenceSha256 -eq '6B0D6F9A617D257268C84AFF523FBA6D4F9A727A0EA130B956E7DB992123300B') 'CU6 fresh evidence binding mismatch.'
Require ([int]$m.candidateCount -eq 3) 'CU6 Candidate count mismatch.'
Require ([int]$m.updateIntervalCount -eq 7) 'CU6 U count mismatch.'
Require (($m.updateIntervals -join ',') -eq '1,2,4,8,16,32,64') 'CU6 U corpus mismatch.'
Require ([int]$m.balancedOrderCount -eq 6) 'CU6 order count mismatch.'
Require ($m.orderBalance -eq 'EachCandidateAppearsTwiceAtEveryOrderPosition') 'CU6 order balance mismatch.'
Require ($m.primaryMetric -eq 'GpuCandidateTimelineNanoseconds') 'CU6 primary metric mismatch.'
Require ($m.runtimePolicyAuthorization -eq 'None') 'CU6 Runtime authorization must remain None.'
Require ($m.nextCapabilitySelection -eq 'DeferredByOwner') 'CU6 next capability must remain Owner-deferred.'
Require ($m.selectionStatus -eq 'OWNER_DECISION_REQUIRED') 'CU6 selection status mismatch.'
Require ([bool]$m.experimentCompleteOnPass) 'CU6 Experiment Complete declaration missing.'
Require (-not [bool]$m.spiralClosedOnPass) 'CU6 must not close Spiral 5.'

$required = @(
 '152_Spiral5PerformanceExperiment\152_Spiral5PerformanceExperiment.vcxproj',
 '152_Spiral5PerformanceExperiment\Spiral5PerformanceExperiment.h',
 '152_Spiral5PerformanceExperiment\Spiral5PerformanceExperiment.cpp',
 '152_Spiral5PerformanceExperiment\Spiral5RealGpuMeasurement.cpp',
 '153_Spiral5PerformanceTests\153_Spiral5PerformanceTests.vcxproj',
 '153_Spiral5PerformanceTests\main.cpp',
 'tests\tools\Register-Spiral5CU6Projects.ps1',
 'tests\tools\Verify-Spiral5CU6.ps1',
 'tests\Run-Spiral5CU6.ps1',
 'run_sge4_5_spiral5_cu6_prepare.bat',
 'run_sge4_5_spiral5_cu6_measurement_decision_evidence.bat',
 'docs\spiral5\CU6_MEASUREMENT_MANIFEST_V1.json',
 'docs\spiral5\CU6_REAL_GPU_MEASUREMENT.md',
 'docs\spiral5\CU6_DECISION_RULES.md',
 'docs\spiral5\CU6_CHANGESET.md',
 'docs\spiral5\CU6_EVIDENCE_LEDGER.md',
 'docs\spiral5\SPIRAL5_STATUS_AFTER_CU6.md'
)
foreach ($relative in $required) { Require (Test-Path -LiteralPath (Join-Path $root $relative) -PathType Leaf) "Missing CU6 file: $relative" }

$solution = Get-Content -Raw -LiteralPath (Join-Path $root 'SemanticGpuEngine4-5.sln') -Encoding UTF8
$projects = @(
 [pscustomobject]@{ Path='152_Spiral5PerformanceExperiment\152_Spiral5PerformanceExperiment.vcxproj'; Guid='{00000098-0000-5000-8000-000000000098}' },
 [pscustomobject]@{ Path='153_Spiral5PerformanceTests\153_Spiral5PerformanceTests.vcxproj'; Guid='{00000099-0000-5000-8000-000000000099}' }
)
foreach ($project in $projects) {
 Require ($solution.Contains($project.Path)) "CU6 project missing from Solution: $($project.Path)"
 foreach ($entry in @("$($project.Guid).Debug|x64.ActiveCfg = Debug|x64","$($project.Guid).Debug|x64.Build.0 = Debug|x64","$($project.Guid).Release|x64.ActiveCfg = Release|x64","$($project.Guid).Release|x64.Build.0 = Release|x64")) {
  Require ($solution.Contains($entry)) "CU6 Solution configuration missing: $entry"
 }
}

$header = Get-Content -Raw -LiteralPath (Join-Path $root '152_Spiral5PerformanceExperiment\Spiral5PerformanceExperiment.h') -Encoding UTF8
foreach ($token in @('CanonicalCandidateCountV1 = 3','CanonicalUpdateIntervalCountV1 = 7','CanonicalOrderCountV1 = 6','GpuCandidateTimelineNanoseconds','gpuUpdatePathNanoseconds','gpuHoldPathNanoseconds','NoiseEquivalentOrUnresolved')) {
 Require ($header.Contains($token)) "CU6 header token missing: $token"
}
$portable = Get-Content -Raw -LiteralPath (Join-Path $root '152_Spiral5PerformanceExperiment\Spiral5PerformanceExperiment.cpp') -Encoding UTF8
foreach ($token in @('Each Candidate must appear twice in every order position.','Measurement evidence checksum mismatch.','Corrupted measurement evidence was accepted.','Synthetic A/B crossover classification failed.','Synthetic B/C crossover classification failed.','RuntimePolicyAuthorization = None','NextCapabilitySelection = DeferredByOwner','SelectionStatus = OWNER_DECISION_REQUIRED','SPIRAL5_DECISION_EVIDENCE_REPORT.md')) {
 Require ($portable.Contains($token)) "CU6 portable token missing: $token"
}
$gpu = Get-Content -Raw -LiteralPath (Join-Path $root '152_Spiral5PerformanceExperiment\Spiral5RealGpuMeasurement.cpp') -Encoding UTF8
foreach ($token in @('DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE','DXGI_ADAPTER_FLAG_SOFTWARE','D3D12_QUERY_HEAP_TYPE_TIMESTAMP','GetTimestampFrequency','D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH','ExecuteIndirect','QueriesPerInvocation = 5','gpuUpdatePathNanoseconds','gpuHoldPathNanoseconds','[MEASURED] U=')) {
 Require ($gpu.Contains($token)) "CU6 real-GPU token missing: $token"
}
$tests = Get-Content -Raw -LiteralPath (Join-Path $root '153_Spiral5PerformanceTests\main.cpp') -Encoding UTF8
foreach ($token in @('--self-test','--measure','--report','--warmup-per-order','--measurement-per-order','--adapter-index','RuntimePolicyAuthorization: None','OWNER_DECISION_REQUIRED','[CROSSOVER] A/B','[CROSSOVER] B/C')) {
 Require ($tests.Contains($token)) "CU6 test token missing: $token"
}

$performanceProject = Get-Content -Raw -LiteralPath (Join-Path $root '152_Spiral5PerformanceExperiment\152_Spiral5PerformanceExperiment.vcxproj') -Encoding UTF8
Require ($performanceProject.Contains('149_TemporalCandidateFamilyVerifier')) 'CU6 measurement must bind independently Verified Candidate authority.'
$runtimeProject = Get-Content -Raw -LiteralPath (Join-Path $root '136_Spiral5TemporalExecution\136_Spiral5TemporalExecution.vcxproj') -Encoding UTF8
Require (-not $runtimeProject.Contains('152_Spiral5PerformanceExperiment')) 'Runtime must not depend on measurement or Decision code.'

Write-Host 'SGE4-5 Spiral 5 CU6 balanced real-GPU measurement, corruption rejection, crossover classification, and Owner-only decision boundaries passed.'
