$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
function Require([bool]$Condition,[string]$Message){if(-not $Condition){throw $Message}}

$manifestPath = Join-Path $root 'docs\spiral6\CU5_ARCHITECTURE_MANIFEST_V1.json'
Require (Test-Path -LiteralPath $manifestPath -PathType Leaf) 'Spiral 6 CU5 Architecture manifest missing.'
$m = Get-Content -Raw -LiteralPath $manifestPath -Encoding UTF8 | ConvertFrom-Json
Require ($m.schema -eq 'SGE4-5.Spiral6.CU5ArchitectureManifest.V1') 'CU5 manifest schema mismatch.'
Require ($m.completionUnit -eq 'CU5') 'CU5 completion unit mismatch.'
Require ($m.baselineCommit -eq '698b86510dda5d11b7259f01ffff288a07076f56') 'CU5 baseline commit mismatch.'
Require ($m.predecessor.cu3AuthorityEvidenceSha256 -eq 'C04FCA6674BDA7EA2FBBB9BEE3208757A960EA7B606BE266EA737023573D398C') 'CU5 CU3 evidence mismatch.'
Require ($m.predecessor.cu4AcceptedCommit -eq '698b86510dda5d11b7259f01ffff288a07076f56') 'CU5 CU4 accepted commit mismatch.'
Require ($m.completionDeclaration -eq 'SGE4-5 SPIRAL 6 ARCHITECTURE COMPLETE') 'CU5 completion declaration mismatch.'
Require ((@($m.qualificationCardinalities | ForEach-Object {[int]$_}) -join ',') -eq '0,1,2,31,32,63,64,65,127,128,129,255,256,257,511,512,1024,2048,3072,4095,4096') 'CU5 cardinality corpus mismatch.'
Require (($m.qualificationPatterns -join ',') -eq 'PrefixControl,UniformStride,BlockClusteredPermutation,HashScatterPermutation') 'CU5 pattern corpus mismatch.'
Require ([int]$m.exactSparseSetCountPerQualification -eq 84) 'CU5 exact Sparse set count mismatch.'
Require (($m.candidateKinds -join ',') -eq 'DenseMembershipMaskPredicate,CompactIndexListDispatch,ActiveBlockListLocalMask') 'CU5 Candidate family mismatch.'
Require ([int]$m.candidateExecutionsPerQualification -eq 252) 'CU5 Candidate execution count mismatch.'
Require ([bool]$m.qualificationProofs.exactWriteSet -and [bool]$m.qualificationProofs.inactiveFloat4ZeroByteStable -and [bool]$m.qualificationProofs.pairwiseFullOutputByteIdentical) 'CU5 exact-output proofs missing.'
Require ([int]$m.qualificationProofs.zeroWorkDenseDispatchGroups -eq 64 -and [int]$m.qualificationProofs.zeroWorkCompactDispatchGroups -eq 0 -and [int]$m.qualificationProofs.zeroWorkActiveBlockDispatchGroups -eq 0) 'CU5 zero-work rules mismatch.'
Require ([bool]$m.controlledRecovery.staleSetBindingRejected -and [bool]$m.controlledRecovery.staleRepresentationRejected -and [bool]$m.controlledRecovery.staleCompletionRejected) 'CU5 stale-handle rejection missing.'
Require ([bool]$m.controlledRecovery.explicitSparseRebindRequired -and [bool]$m.controlledRecovery.explicitRepresentationRebuildRequired) 'CU5 explicit rebind/rebuild boundary missing.'
Require ($m.actualRecovery.mechanism -eq 'ID3D12Device5::RemoveDevice') 'CU5 actual removal mechanism mismatch.'
Require ($m.actualRecovery.quarantineState -eq 'AwaitingAdapter') 'CU5 quarantine state mismatch.'
Require ($m.runtimeSparsePolicyDecision -eq 'None') 'CU5 Runtime Sparse policy must remain None.'
Require ($m.legacySchemaMutation -eq 'None' -and $m.legacyRuntimeMutation -eq 'None' -and $m.legacyBackendMutation -eq 'None' -and $m.compositionContractMutation -eq 'None') 'CU5 legacy mutation boundary mismatch.'

$required = @(
'163_Spiral6SparseFamilyExecution\Spiral6SparseFamilyExecution.h',
'163_Spiral6SparseFamilyExecution\Spiral6SparseFamilyExecution.cpp',
'169_Spiral6ArchitectureQualificationTests\169_Spiral6ArchitectureQualificationTests.vcxproj',
'169_Spiral6ArchitectureQualificationTests\main.cpp',
'docs\spiral6\CU5_ARCHITECTURE_MANIFEST_V1.json',
'docs\spiral6\CU5_ARCHITECTURE_COMPLETE.md',
'docs\spiral6\CU5_RECOVERY_MODEL.md',
'docs\spiral6\CU5_CHANGESET.md',
'docs\spiral6\CU5_EVIDENCE_LEDGER.md',
'tests\Run-Spiral6CU5.ps1',
'tests\tools\Register-Spiral6CU5Projects.ps1',
'tests\tools\Verify-Spiral6CU5.ps1',
'run_sge4_5_spiral6_cu5_prepare.bat',
'run_sge4_5_spiral6_cu5_architecture_qualification.bat')
foreach($relative in $required){Require (Test-Path -LiteralPath (Join-Path $root $relative) -PathType Leaf) "Missing Spiral 6 CU5 file: $relative"}

$projectPath = Join-Path $root '169_Spiral6ArchitectureQualificationTests\169_Spiral6ArchitectureQualificationTests.vcxproj'
[xml]$project = Get-Content -Raw -LiteralPath $projectPath -Encoding UTF8
$actualGuid = [string](($project.Project.PropertyGroup | Where-Object {$_.Label -eq 'Globals'} | Select-Object -First 1).ProjectGuid)
Require ($actualGuid.ToUpperInvariant() -eq '{000000A9-0000-5000-8000-0000000000A9}') 'CU5 ProjectGuid mismatch.'
$solution = Get-Content -Raw -LiteralPath (Join-Path $root 'SemanticGpuEngine4-5.sln') -Encoding UTF8
Require (([regex]::Matches($solution,[regex]::Escape('169_Spiral6ArchitectureQualificationTests\169_Spiral6ArchitectureQualificationTests.vcxproj'))).Count -eq 1) 'CU5 Solution registration mismatch.'

$header = Get-Content -Raw -LiteralPath (Join-Path $root '163_Spiral6SparseFamilyExecution\Spiral6SparseFamilyExecution.h') -Encoding UTF8
foreach($token in @(
'LoadedSparseCandidateFamilyRuntimeV1',
'SparseRuntimeSetBindingV1',
'SparseRepresentationSetHandleV1',
'ControlledRebuild',
'ForceRemovalForTest',
'RetryAdapterReacquisition',
'BindSparseWorkSetV1',
'RebuildSparseRepresentationsV1',
'ValidateSparseRepresentationSetHandleV1')){
    Require ($header -match [regex]::Escape($token)) "CU5 Runtime header token missing: $token"
}

$source = Get-Content -Raw -LiteralPath (Join-Path $root '163_Spiral6SparseFamilyExecution\Spiral6SparseFamilyExecution.cpp') -Encoding UTF8
foreach($token in @(
'ID3D12Device5',
'RemoveDevice',
'sparse-runtime/stale-epoch',
'sparse-runtime/rebind-required',
'sparse-runtime/rebuild-required',
'AwaitingAdapter',
'BuildSparseRepresentationSetIdentityV1')){
    Require ($source -match [regex]::Escape($token)) "CU5 Runtime source token missing: $token"
}
Require ($source -notmatch '\bRawSparseFamilyCandidateV1\b') 'CU5 Sparse Runtime must not consume Raw Candidates.'

$executionProject = Get-Content -Raw -LiteralPath (Join-Path $root '163_Spiral6SparseFamilyExecution\163_Spiral6SparseFamilyExecution.vcxproj') -Encoding UTF8
Require ($executionProject -match '162_SparseCandidateFamilyVerifier') 'CU5 Runtime must consume opaque Verified family types.'
Require ($executionProject -notmatch '161_SparseCandidateFamilyPlanner') 'CU5 Runtime must not depend on family Planner.'

$test = Get-Content -Raw -LiteralPath (Join-Path $root '169_Spiral6ArchitectureQualificationTests\main.cpp') -Encoding UTF8
foreach($token in @(
'21u * 4u * 3u',
'84 exact Sparse sets',
'inactiveSentinelByteStable',
'zero-work Dispatch behavior differs',
'sparse-runtime/stale-epoch',
'sparse-runtime/rebind-required',
'sparse-runtime/rebuild-required',
'Actual Sparse ID3D12Device5::RemoveDevice quarantine passed',
'Fresh-process Sparse rematerialization passed')){
    Require ($test -match [regex]::Escape($token)) "CU5 qualification token missing: $token"
}

$runner = Get-Content -Raw -LiteralPath (Join-Path $root 'tests\Run-Spiral6CU5.ps1') -Encoding UTF8
foreach($token in @(
'architecture-debug-a.bin',
'architecture-debug-b.bin',
'architecture-release.bin',
'controlled-debug-a.bin',
'--actual-removal',
'fresh-rematerialization-release.bin',
'SGE4-5 SPIRAL 6 ARCHITECTURE COMPLETE')){
    Require ($runner -match [regex]::Escape($token)) "CU5 runner token missing: $token"
}

$legacyHashes = @{
    '10_D3D12PackageSchema\D3D12Schema.h' = '48B9E8695AB0808BE5B2E50CC6C74B61D27B0A627945BF05409C0378887BB44F'
    '13_PackageRuntime\PackageRuntime.cpp' = '1AF97D5C594A5D4A4B9062A44F2662A6E754B66110AE6F31BFE0AD3B0608ED38'
    '14_D3D12Backend\D3D12Backend.cpp' = '2AC76F2284CE7984859A360BD498716CCB0D28764DA1047E1E0DA96CFCE0C462'
    '22_CompositionRuntime\CompositionRuntime.cpp' = '56163B62C4AA4C615A64702C4C953B89803C05B552381BA3C34E07A5C704F3BF'
}
foreach($entry in $legacyHashes.GetEnumerator()){
    $actual = (Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $root $entry.Key)).Hash
    Require ($actual -eq $entry.Value) "CU5 legacy boundary hash mismatch: $($entry.Key)"
}

Write-Host 'SGE4-5 Spiral 6 CU5 Architecture Qualification boundaries passed.'
Write-Host 'Complete Sparse corpus, deterministic evidence, Recovery, stale representation rejection and explicit rebuild contracts are present.'
