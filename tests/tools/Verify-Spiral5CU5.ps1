$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
function Require([bool]$Condition, [string]$Message) { if (-not $Condition) { throw $Message } }

$manifestPath = Join-Path $root 'docs\spiral5\CU5_ARCHITECTURE_MANIFEST_V1.json'
Require (Test-Path -LiteralPath $manifestPath -PathType Leaf) 'Spiral 5 CU5 manifest missing.'
$m = Get-Content -Raw -LiteralPath $manifestPath -Encoding UTF8 | ConvertFrom-Json
Require ($m.schema -eq 'SGE4-5.Spiral5.CU5ArchitectureManifest.V1') 'CU5 manifest schema mismatch.'
Require ($m.completionUnit -eq 'CU5') 'CU5 completion unit mismatch.'
Require ($m.baselineCommit -eq 'a3d9a19a1a3033b723df918e70f5af6a24d884a0') 'CU5 baseline commit mismatch.'
Require ($m.predecessor.cu4CandidateFamilyEvidenceSha256 -eq '74643570F20C88230CF3BBAD94FE6E10489AA164C83C876E6255A5F7D6BA3B17') 'CU5 predecessor evidence mismatch.'
Require ($m.completionDeclaration -eq 'SGE4-5 SPIRAL 5 ARCHITECTURE COMPLETE') 'CU5 completion declaration mismatch.'
Require (($m.scheduleCorpus -join ',') -eq 'P1,P2,P4,P8,P16,P32,P64,BurstThenHold,Irregular') 'CU5 schedule corpus mismatch.'
Require (($m.candidateKinds -join ',') -eq 'EveryInvocationRecompute,GlobalMotorHistoryReuse,FinalOutputHistoryReuse') 'CU5 Candidate family mismatch.'
Require ([int]$m.timelineLength -eq 128) 'CU5 Timeline length mismatch.'
Require ([int]$m.candidateInvocationCountPerQualification -eq 3456) 'CU5 Candidate invocation count mismatch.'
Require ([bool]$m.controlledRecovery.staleHistoryRejected) 'CU5 stale History rejection missing.'
Require ([bool]$m.controlledRecovery.generationZeroReseedRequired) 'CU5 explicit reseed boundary missing.'
Require ($m.actualRecovery.mechanism -eq 'ID3D12Device5::RemoveDevice') 'CU5 actual removal mechanism mismatch.'
Require ($m.actualRecovery.quarantineState -eq 'AwaitingAdapter') 'CU5 quarantine state mismatch.'
Require ($m.runtimeTemporalPolicyDecision -eq 'None') 'CU5 Runtime policy must remain None.'
Require ($m.legacySchemaMutation -eq 'None' -and $m.legacyRuntimeMutation -eq 'None' -and $m.legacyBackendMutation -eq 'None') 'CU5 legacy mutation boundary mismatch.'

$projectPath = Join-Path $root '151_Spiral5ArchitectureQualificationTests\151_Spiral5ArchitectureQualificationTests.vcxproj'
Require (Test-Path -LiteralPath $projectPath -PathType Leaf) 'CU5 qualification project missing.'
[xml]$project = Get-Content -Raw -LiteralPath $projectPath -Encoding UTF8
$actualGuid = [string](
    $project.Project.PropertyGroup |
    Where-Object { $_.Label -eq 'Globals' } |
    Select-Object -First 1
).ProjectGuid
Require ($actualGuid.ToUpperInvariant() -eq '{00000097-0000-5000-8000-000000000097}') 'CU5 ProjectGuid mismatch.'
$solution = Get-Content -Raw -LiteralPath (Join-Path $root 'SemanticGpuEngine4-5.sln') -Encoding UTF8
Require (([regex]::Matches($solution,[regex]::Escape('151_Spiral5ArchitectureQualificationTests\151_Spiral5ArchitectureQualificationTests.vcxproj'))).Count -eq 1) 'CU5 Solution registration mismatch.'

$header = Get-Content -Raw -LiteralPath (Join-Path $root '136_Spiral5TemporalExecution\Spiral5TemporalExecution.h') -Encoding UTF8
foreach ($token in @(
    'LoadedTemporalCandidateFamilyRuntimeV1',
    'TemporalHistorySeedV1',
    'TemporalHistoryHandleV1',
    'ControlledRebuild',
    'ForceRemovalForTest',
    'RetryAdapterReacquisition',
    'ReseedTemporalHistoriesV1',
    'ValidateTemporalHistoryHandleV1'
)) {
    Require ($header -match [regex]::Escape($token)) "CU5 Runtime header token missing: $token"
}

$source = Get-Content -Raw -LiteralPath (Join-Path $root '136_Spiral5TemporalExecution\Spiral5TemporalExecution.cpp') -Encoding UTF8
foreach ($token in @(
    'ID3D12Device5',
    'RemoveDevice',
    'temporal-runtime/stale-epoch',
    'temporal-runtime/rebind-required',
    'temporal-runtime/reseed-required',
    'TemporalRetainedHistoryHandle',
    'AwaitingAdapter'
)) {
    Require ($source -match [regex]::Escape($token)) "CU5 Runtime source token missing: $token"
}
Require ($source -notmatch 'const semantic::TemporalStateSemanticV1& semanticValue,\s*const semantic::UpdateScheduleV1& schedule,\s*const FrozenVerifiedTemporalCandidateV1& frozen') 'CU4 unused semanticValue parameter returned.'

$test = Get-Content -Raw -LiteralPath (Join-Path $root '151_Spiral5ArchitectureQualificationTests\main.cpp') -Encoding UTF8
foreach ($token in @(
    '9u * 3u * 128u',
    'BurstThenHold',
    'retainedHistoryByteStable',
    'no-unauthorized-write',
    'temporal-runtime/stale-epoch',
    'temporal-runtime/rebind-required',
    'temporal-runtime/reseed-required',
    'ID3D12Device5::RemoveDevice quarantine passed',
    'Fresh-process Temporal rematerialization passed'
)) {
    Require ($test -match [regex]::Escape($token)) "CU5 qualification token missing: $token"
}

$runner = Get-Content -Raw -LiteralPath (Join-Path $root 'tests\Run-Spiral5CU5.ps1') -Encoding UTF8
foreach ($token in @(
    'architecture-debug-a.bin',
    'architecture-debug-b.bin',
    'architecture-release.bin',
    'controlled-debug-a.bin',
    '--actual-removal',
    'fresh-rematerialization-release.bin',
    'SGE4-5 SPIRAL 5 ARCHITECTURE COMPLETE'
)) {
    Require ($runner -match [regex]::Escape($token)) "CU5 runner token missing: $token"
}

$executionProject = Get-Content -Raw -LiteralPath (Join-Path $root '136_Spiral5TemporalExecution\136_Spiral5TemporalExecution.vcxproj') -Encoding UTF8
Require ($executionProject -match '149_TemporalCandidateFamilyVerifier') 'CU5 Runtime must consume opaque family Verified types.'
Require ($executionProject -notmatch '148_TemporalCandidateFamilyPlanner') 'CU5 Runtime must not depend on family Planner.'

$legacyHashes = @{
    '10_D3D12PackageSchema\D3D12Schema.h' = '48B9E8695AB0808BE5B2E50CC6C74B61D27B0A627945BF05409C0378887BB44F'
    '13_PackageRuntime\PackageRuntime.cpp' = '1AF97D5C594A5D4A4B9062A44F2662A6E754B66110AE6F31BFE0AD3B0608ED38'
    '14_D3D12Backend\D3D12Backend.cpp' = '2AC76F2284CE7984859A360BD498716CCB0D28764DA1047E1E0DA96CFCE0C462'
    '22_CompositionRuntime\CompositionRuntime.cpp' = '56163B62C4AA4C615A64702C4C953B89803C05B552381BA3C34E07A5C704F3BF'
}
foreach ($entry in $legacyHashes.GetEnumerator()) {
    $actual = (Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $root $entry.Key)).Hash
    Require ($actual -eq $entry.Value) "CU5 legacy boundary hash mismatch: $($entry.Key)"
}

Write-Host 'SGE4-5 Spiral 5 CU5 Architecture Qualification boundaries passed.'
Write-Host 'All schedules, deterministic evidence, Recovery, stale History and explicit reseed contracts are present.'
