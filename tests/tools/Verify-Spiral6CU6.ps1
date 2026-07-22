$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
function Require([bool]$Condition,[string]$Message) { if (-not $Condition) { throw $Message } }

$manifestPath = Join-Path $root 'docs\spiral6\CU6_MEASUREMENT_MANIFEST_V1.json'
Require (Test-Path -LiteralPath $manifestPath -PathType Leaf) 'Spiral 6 CU6 manifest missing.'
$m = Get-Content -Raw -LiteralPath $manifestPath -Encoding UTF8 | ConvertFrom-Json
Require ($m.schema -eq 'SGE4-5.Spiral6.CU6MeasurementManifest.V1') 'CU6 manifest schema mismatch.'
Require ($m.completionUnit -eq 'CU6') 'CU6 completion unit mismatch.'
Require ($m.baselineCommit -eq '18cc16db82c5cf7b5d7be00d51345196df046447') 'CU6 baseline mismatch.'
Require ($m.predecessor.architectureEvidenceSha256 -eq '4A634317E988E3F3F9639F249A35AF0CE25E18E19EA52A0500CE75AB09674B7E') 'CU6 Architecture evidence binding mismatch.'
Require ($m.predecessor.controlledRecoveryEvidenceSha256 -eq '65AEC68060DD68AE42A191E79C5CF2FFFC8538442B8D6C029FE59927709BD77F') 'CU6 Controlled Recovery evidence binding mismatch.'
Require ($m.predecessor.freshRematerializationEvidenceSha256 -eq 'A9398025D0C5235E2370BFD5D0C34BDFCDC114836A763F8B6151BD95D80FF032') 'CU6 fresh evidence binding mismatch.'
Require ([int]$m.candidateCount -eq 3) 'CU6 Candidate count mismatch.'
Require ([int]$m.patternCount -eq 4) 'CU6 Pattern count mismatch.'
Require (($m.patterns -join ',') -eq 'PrefixControl,UniformStride,BlockClusteredPermutation,HashScatterPermutation') 'CU6 Pattern corpus mismatch.'
Require ([int]$m.measurementCardinalityCount -eq 11) 'CU6 K count mismatch.'
Require ((@($m.measurementCardinalities | ForEach-Object {[int]$_}) -join ',') -eq '1,8,32,64,128,256,512,1024,2048,3072,4096') 'CU6 K corpus mismatch.'
Require ([int]$m.balancedOrderCount -eq 6) 'CU6 order count mismatch.'
Require ($m.orderBalance -eq 'EachCandidateAppearsTwiceAtEveryOrderPosition') 'CU6 order balance mismatch.'
Require ([int]$m.canonicalMeasurementConfig.samplesPerCandidatePatternAndCardinality -eq 24) 'CU6 sample count mismatch.'
Require ([int]$m.canonicalMeasurementConfig.totalMeasuredCandidateSamples -eq 3168) 'CU6 total sample count mismatch.'
Require ($m.primaryMetric -eq 'GpuCandidatePathNanoseconds') 'CU6 primary metric mismatch.'
Require ($m.runtimePolicyAuthorization -eq 'None') 'CU6 Runtime authorization must remain None.'
Require ($m.nextCapabilitySelection -eq 'DeferredByOwner') 'CU6 next capability must remain Owner-deferred.'
Require ($m.selectionStatus -eq 'OWNER_DECISION_REQUIRED') 'CU6 selection status mismatch.'
Require ([bool]$m.experimentCompleteOnPass) 'CU6 Experiment Complete declaration missing.'
Require (-not [bool]$m.spiralClosedOnPass) 'CU6 must not close Spiral 6.'

$required = @(
 '170_Spiral6PerformanceExperiment\170_Spiral6PerformanceExperiment.vcxproj',
 '170_Spiral6PerformanceExperiment\Spiral6PerformanceExperiment.h',
 '170_Spiral6PerformanceExperiment\Spiral6PerformanceExperiment.cpp',
 '170_Spiral6PerformanceExperiment\Spiral6RealGpuMeasurement.cpp',
 '171_Spiral6PerformanceTests\171_Spiral6PerformanceTests.vcxproj',
 '171_Spiral6PerformanceTests\main.cpp',
 'tests\tools\Register-Spiral6CU6Projects.ps1',
 'tests\tools\Verify-Spiral6CU6.ps1',
 'tests\Run-Spiral6CU6.ps1',
 'run_sge4_5_spiral6_cu6_prepare.bat',
 'run_sge4_5_spiral6_cu6_measurement_decision_evidence.bat',
 'docs\spiral6\CU6_MEASUREMENT_MANIFEST_V1.json',
 'docs\spiral6\CU6_REAL_GPU_MEASUREMENT.md',
 'docs\spiral6\CU6_DECISION_RULES.md',
 'docs\spiral6\CU6_CHANGESET.md',
 'docs\spiral6\CU6_EVIDENCE_LEDGER.md',
 'docs\spiral6\SPIRAL6_STATUS_AFTER_CU6.md')
foreach ($relative in $required) { Require (Test-Path -LiteralPath (Join-Path $root $relative) -PathType Leaf) "Missing CU6 file: $relative" }

$solution = Get-Content -Raw -LiteralPath (Join-Path $root 'SemanticGpuEngine4-5.sln') -Encoding UTF8
$projects = @(
 [pscustomobject]@{ Path='170_Spiral6PerformanceExperiment\170_Spiral6PerformanceExperiment.vcxproj'; Guid='{000000AA-0000-5000-8000-0000000000AA}' },
 [pscustomobject]@{ Path='171_Spiral6PerformanceTests\171_Spiral6PerformanceTests.vcxproj'; Guid='{000000AB-0000-5000-8000-0000000000AB}' })
foreach ($project in $projects) {
 Require ($solution.Contains($project.Path)) "CU6 project missing from Solution: $($project.Path)"
 foreach ($entry in @(
  "$($project.Guid).Debug|x64.ActiveCfg = Debug|x64",
  "$($project.Guid).Debug|x64.Build.0 = Debug|x64",
  "$($project.Guid).Release|x64.ActiveCfg = Release|x64",
  "$($project.Guid).Release|x64.Build.0 = Release|x64")) {
  Require ($solution.Contains($entry)) "CU6 Solution configuration missing: $entry"
 }
}

$header = Get-Content -Raw -LiteralPath (Join-Path $root '170_Spiral6PerformanceExperiment\Spiral6PerformanceExperiment.h') -Encoding UTF8
foreach ($token in @(
 'CanonicalCandidateCountV1 = 3',
 'CanonicalPatternCountV1 = 4',
 'CanonicalMeasurementCardinalityCountV1 = 11',
 'CanonicalOrderCountV1 = 6',
 'GpuCandidatePathNanoseconds',
 'gpuSentinelInitializationNanoseconds',
 'gpuIndirectArgumentGenerationNanoseconds',
 'gpuSparseConsumerExecutionNanoseconds',
 'sameCardinalityWinnerDependsOnPattern',
 'NoiseEquivalentOrUnresolved')) {
 Require ($header.Contains($token)) "CU6 header token missing: $token"
}

$portable = Get-Content -Raw -LiteralPath (Join-Path $root '170_Spiral6PerformanceExperiment\Spiral6PerformanceExperiment.cpp') -Encoding UTF8
foreach ($token in @(
 'SGE4-5.Spiral6.MeasurementEvidence.V1',
 'Measurement evidence checksum mismatch',
 'Every Candidate must appear twice at every order position',
 'Synthetic decision evidence did not prove Pattern-sensitive winner selection',
 'RuntimePolicyAuthorization = None',
 'NextCapabilitySelection = DeferredByOwner')) {
 Require ($portable.Contains($token)) "CU6 portable evidence token missing: $token"
}

$realGpu = Get-Content -Raw -LiteralPath (Join-Path $root '170_Spiral6PerformanceExperiment\Spiral6RealGpuMeasurement.cpp') -Encoding UTF8
foreach ($token in @(
 'DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE',
 'DXGI_ADAPTER_FLAG_SOFTWARE',
 'D3D12_QUERY_HEAP_TYPE_TIMESTAMP',
 'D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT',
 'GpuCandidatePathNanoseconds',
 'Real-GPU Sparse observation failed exact write-set qualification',
 'Candidate output digest changed across repeated measurements',
 'A/B/C output digests are not byte-identical')) {
 Require ($realGpu.Contains($token)) "CU6 real-GPU token missing: $token"
}
Require ($realGpu -notmatch 'EnumWarpAdapter') 'CU6 real-GPU measurement must not select WARP.'

$test = Get-Content -Raw -LiteralPath (Join-Path $root '171_Spiral6PerformanceTests\main.cpp') -Encoding UTF8
foreach ($token in @(
 'format, corruption rejection, six-order balance, crossover, and Pattern-sensitivity self-test passed',
 '[RANKING] Pattern=',
 '[CROSSOVER] Pattern=',
 '[PATTERN-SENSITIVITY]',
 'RuntimePolicyAuthorization: None')) {
 Require ($test.Contains($token)) "CU6 test token missing: $token"
}

$runner = Get-Content -Raw -LiteralPath (Join-Path $root 'tests\Run-Spiral6CU6.ps1') -Encoding UTF8
foreach ($token in @(
 'spiral6_measurement_evidence_v1.bin',
 'SPIRAL6_DECISION_EVIDENCE_REPORT.md',
 'Measured Candidate samples',
 '3168',
 'SGE4-5 SPIRAL 6 EXPERIMENT COMPLETE',
 'Spiral 6 Closed banner remains withheld')) {
 Require ($runner.Contains($token)) "CU6 runner token missing: $token"
}

$performanceProject = Get-Content -Raw -LiteralPath (Join-Path $root '170_Spiral6PerformanceExperiment\170_Spiral6PerformanceExperiment.vcxproj') -Encoding UTF8
foreach ($dependency in @(
 '160_SparseCandidateFamily',
 '161_SparseCandidateFamilyPlanner',
 '162_SparseCandidateFamilyVerifier',
 '163_Spiral6SparseFamilyExecution')) {
 Require ($performanceProject.Contains($dependency)) "CU6 Performance project dependency missing: $dependency"
}

$legacyHashes = @{
    '10_D3D12PackageSchema\D3D12Schema.h' = '48B9E8695AB0808BE5B2E50CC6C74B61D27B0A627945BF05409C0378887BB44F'
    '13_PackageRuntime\PackageRuntime.cpp' = '1AF97D5C594A5D4A4B9062A44F2662A6E754B66110AE6F31BFE0AD3B0608ED38'
    '14_D3D12Backend\D3D12Backend.cpp' = '2AC76F2284CE7984859A360BD498716CCB0D28764DA1047E1E0DA96CFCE0C462'
    '22_CompositionRuntime\CompositionRuntime.cpp' = '56163B62C4AA4C615A64702C4C953B89803C05B552381BA3C34E07A5C704F3BF'
}
foreach ($entry in $legacyHashes.GetEnumerator()) {
    $actual = (Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $root $entry.Key)).Hash
    Require ($actual -eq $entry.Value) "CU6 legacy boundary hash mismatch: $($entry.Key)"
}

Write-Host 'SGE4-5 Spiral 6 CU6 measurement, evidence-format, balanced-order, Decision Report and Owner boundaries passed.'
