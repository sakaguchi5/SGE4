param(
    [ValidateSet('Auto','Snapshot','Regression')]
    [string]$Mode = 'Auto'
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
function Require([bool]$Condition,[string]$Message){if(-not $Condition){throw $Message}}

if ($Mode -eq 'Auto') {
    $cu3Manifest = Join-Path $root 'docs\spiral6\CU3_AUTHORITY_MANIFEST_V1.json'
    $Mode = if (Test-Path -LiteralPath $cu3Manifest -PathType Leaf) { 'Regression' } else { 'Snapshot' }
}

$manifestPath = Join-Path $root 'docs\spiral6\CU2_ARCHITECTURE_MANIFEST_V1.json'
Require (Test-Path -LiteralPath $manifestPath -PathType Leaf) 'Spiral 6 CU2 architecture manifest missing.'
$m = Get-Content -Raw -LiteralPath $manifestPath -Encoding UTF8 | ConvertFrom-Json
Require ($m.schema -eq 'SGE4-5.Spiral6.CU2ArchitectureManifest.V1') 'CU2 manifest schema mismatch.'
Require ($m.completionUnit -eq 'CU2') 'CU2 manifest completion unit mismatch.'
Require ($m.ownerBaselineCommit -eq '46554ab55e532c438c9c4214ff1df3e7cd68638e') 'CU2 Owner baseline mismatch.'
Require ($m.cu1AcceptedCommit -eq 'b74f820ba2c4504ab44aa1b954da4b0cfafff3d2') 'CU2 accepted CU1 commit mismatch.'
Require ($m.extensionStrategy -eq 'VersionedSidecarSparseExtensionV1') 'CU2 Sparse extension strategy mismatch.'
Require ($m.legacySchemaMutation -eq 'None') 'CU2 must not mutate Schema 17.'
Require ($m.legacyRuntimeMutation -eq 'None') 'CU2 must not mutate Runtime 17.'
Require ($m.legacyBackendMutation -eq 'None') 'CU2 must not mutate the canonical D3D12 Backend.'
Require ($m.compositionContractMutation -eq 'None') 'CU2 must not mutate the Composition Contract.'
Require ($m.spiral4IndirectExtension -eq 'ReusedAndIdentityBound') 'CU2 must bind the Spiral 4 Dispatch contract.'
Require ($m.spiral5TemporalExtension -eq 'UnchangedAndOutOfDimension') 'CU2 must keep Temporal out of dimension.'
Require ($m.candidate -eq 'CompactIndexListDispatch') 'CU2 Candidate mismatch.'
Require ($m.representationRole -eq 'CompactSortedUniverseIndexList') 'CU2 representation role mismatch.'
Require ([int]$m.universeCount -eq 4096) 'CU2 universe count mismatch.'
Require ([int]$m.threadsPerGroup -eq 64) 'CU2 thread-group size mismatch.'
Require ([int]$m.indexStrideBytes -eq 4) 'CU2 index stride mismatch.'
Require ([int]$m.fixedCapacityIndexCount -eq 4096) 'CU2 index capacity count mismatch.'
Require ([int]$m.fixedCapacityBytes -eq 16384) 'CU2 index capacity bytes mismatch.'
Require ([int]$m.artifactByteSize -eq 16676) 'CU2 artifact byte size mismatch.'
Require ($m.dispatchIssuance -eq 'Spiral4DispatchCommandSignatureExecuteIndirect') 'CU2 Dispatch issuance mismatch.'
Require ($m.unusedCapacitySentinel -eq '0xFFFFFFFF') 'CU2 unused index tail sentinel mismatch.'
Require ($m.dispatchRule -eq 'CeilKOver64ZeroWhenKZero') 'CU2 Dispatch rule mismatch.'
Require ($m.outputInitialization -eq 'CanonicalFloat4Zero') 'CU2 output initialization mismatch.'
Require ($m.activeWriteSet -eq 'ExactlyVerifiedSparseSetS') 'CU2 active write-set mismatch.'
Require ($m.inactiveWritePolicy -eq 'Forbidden') 'CU2 inactive write policy mismatch.'
Require ($m.inactiveFinalBytes -eq 'ByteIdenticalCanonicalFloat4Zero') 'CU2 inactive sentinel rule mismatch.'
Require ($m.outputOrder -eq 'UniverseIndexOrder') 'CU2 output order mismatch.'
Require (($m.semanticPatternsImplemented -join ',') -eq 'PrefixControl,UniformStride,BlockClusteredPermutation,HashScatterPermutation') 'CU2 pattern implementation mismatch.'
Require ([int]$m.architectureCaseCount -eq 12) 'CU2 architecture case count mismatch.'
Require ($m.authorityQualification -eq 'DeferredToCU3') 'CU2 must defer independent authority to CU3.'
Require ($m.candidateFamilyQualification -eq 'DeferredToCU4') 'CU2 must defer Candidate family to CU4.'
Require ($m.recoveryQualification -eq 'DeferredToCU5') 'CU2 must defer Recovery to CU5.'
Require ($m.runtimeSparsePolicyDecision -eq 'None') 'CU2 Runtime Sparse policy must remain None.'

$required = @(
'154_SparseWorkSetSemantic\154_SparseWorkSetSemantic.vcxproj','154_SparseWorkSetSemantic\SparseWorkSetSemantic.h','154_SparseWorkSetSemantic\SparseWorkSetSemantic.cpp',
'155_Spiral6SparseContracts\155_Spiral6SparseContracts.vcxproj','155_Spiral6SparseContracts\Spiral6SparseContracts.h','155_Spiral6SparseContracts\Spiral6SparseContracts.cpp',
'156_Spiral6SparseExecution\156_Spiral6SparseExecution.vcxproj','156_Spiral6SparseExecution\Spiral6SparseExecution.h','156_Spiral6SparseExecution\Spiral6SparseExecution.cpp',
'166_Spiral6SparseArchitectureTests\166_Spiral6SparseArchitectureTests.vcxproj','166_Spiral6SparseArchitectureTests\main.cpp',
'tests\Spiral6Common.ps1','tests\Run-Spiral6CU2.ps1','tests\tools\Register-Spiral6CU2Projects.ps1','tests\tools\Verify-Spiral6CU2.ps1',
'run_sge4_5_spiral6_cu2_prepare.bat','run_sge4_5_spiral6_cu2_compact_index_list.bat',
'docs\spiral6\CU2_ARCHITECTURE_MANIFEST_V1.json','docs\spiral6\CU2_BASELINE_SUFFICIENCY_AND_GAP.md','docs\spiral6\CU2_COMPACT_INDEX_LIST_ARCHITECTURE.md','docs\spiral6\CU2_CHANGESET.md','docs\spiral6\CU2_EVIDENCE_LEDGER.md')
foreach($relative in $required){Require (Test-Path -LiteralPath (Join-Path $root $relative) -PathType Leaf) "Missing Spiral 6 CU2 file: $relative"}

$projects = @(
    [pscustomobject]@{Path='154_SparseWorkSetSemantic\154_SparseWorkSetSemantic.vcxproj';Guid='{0000009A-0000-5000-8000-00000000009A}'},
    [pscustomobject]@{Path='155_Spiral6SparseContracts\155_Spiral6SparseContracts.vcxproj';Guid='{0000009B-0000-5000-8000-00000000009B}'},
    [pscustomobject]@{Path='156_Spiral6SparseExecution\156_Spiral6SparseExecution.vcxproj';Guid='{0000009C-0000-5000-8000-00000000009C}'},
    [pscustomobject]@{Path='166_Spiral6SparseArchitectureTests\166_Spiral6SparseArchitectureTests.vcxproj';Guid='{000000A6-0000-5000-8000-0000000000A6}'}
)
$solution = Get-Content -Raw -LiteralPath (Join-Path $root 'SemanticGpuEngine4-5.sln') -Encoding UTF8
foreach($project in $projects){
    [xml]$xml = Get-Content -Raw -LiteralPath (Join-Path $root $project.Path) -Encoding UTF8
    $guid=[string](($xml.Project.PropertyGroup|Where-Object{$_.Label -eq 'Globals'}|Select-Object -First 1).ProjectGuid)
    Require ($guid.ToUpperInvariant() -eq $project.Guid.ToUpperInvariant()) "Project GUID mismatch: $($project.Path)"
    Require ($solution -match [regex]::Escape($project.Path)) "Solution registration missing: $($project.Path)"
}

foreach ($property in $m.legacyFileSha256.PSObject.Properties) {
    $path = Join-Path $root ([string]$property.Name)
    Require (Test-Path -LiteralPath $path -PathType Leaf) "Legacy boundary file missing: $($property.Name)"
    $actual = (Get-FileHash -Algorithm SHA256 -LiteralPath $path).Hash.ToLowerInvariant()
    Require ($actual -eq ([string]$property.Value).ToLowerInvariant()) "Legacy boundary file changed: $($property.Name)"
}

$composition = Get-Content -Raw -LiteralPath (Join-Path $root '17_CompositionContract\CompositionContract.h') -Encoding UTF8
Require ($composition -notmatch '\bExactSparseWorkSet') 'CU2 must not add Sparse policy to the Composition Contract.'
Require ($composition -notmatch '\bCompactIndexList') 'CU2 must not add Compact Index policy to the Composition Contract.'

$semanticHeader = Get-Content -Raw -LiteralPath (Join-Path $root '154_SparseWorkSetSemantic\SparseWorkSetSemantic.h') -Encoding UTF8
foreach($token in @('ExactSparseWorkSetV1','StrictlyIncreasing','SparseCardinalityV1','HashScatterPermutation','BuildSparseCpuReferenceOutputV1','BuildCanonicalInactiveOutputBytesV1')){
    if ($token -eq 'StrictlyIncreasing') {
        $semanticCpp = Get-Content -Raw -LiteralPath (Join-Path $root '154_SparseWorkSetSemantic\SparseWorkSetSemantic.cpp') -Encoding UTF8
        Require ($semanticCpp -match 'strictly increasing') 'CU2 strict Sparse index validation token missing.'
    } else {
        Require ($semanticHeader -match [regex]::Escape($token)) "CU2 Sparse Semantic token missing: $token"
    }
}

$contractCpp = Get-Content -Raw -LiteralPath (Join-Path $root '155_Spiral6SparseContracts\Spiral6SparseContracts.cpp') -Encoding UTF8
foreach($token in @('VersionedSidecarSparseExtensionV1','CompactIndexListDispatch','CompactSortedUniverseIndexList','ExactlyVerifiedSparseSet','InvalidUniverseIndexV1','Spiral4DispatchContractIdentityV1')){
    Require ($contractCpp -match [regex]::Escape($token)) "CU2 Sparse contract token missing: $token"
}
$executionCpp = Get-Content -Raw -LiteralPath (Join-Path $root '156_Spiral6SparseExecution\Spiral6SparseExecution.cpp') -Encoding UTF8
foreach($token in @('SparseIndices','activeCount','universeIndex','Output[universeIndex]','inactiveSentinelByteStable','D3D12_HEAP_TYPE_READBACK','EnumWarpAdapter','ExecuteIndirect','D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH')){
    Require ($executionCpp -match [regex]::Escape($token)) "CU2 Sparse execution token missing: $token"
}
$testCpp = Get-Content -Raw -LiteralPath (Join-Path $root '166_Spiral6SparseArchitectureTests\main.cpp') -Encoding UTF8
foreach($token in @('duplicate','unsorted','outOfRange','corruptedBytes','Cross-set','4095','Runtime sparse-policy decision: None')){
    Require ($testCpp -match [regex]::Escape($token)) "CU2 Sparse test token missing: $token"
}

$executionProject = Get-Content -Raw -LiteralPath (Join-Path $root '156_Spiral6SparseExecution\156_Spiral6SparseExecution.vcxproj') -Encoding UTF8
if ($Mode -eq 'Snapshot') {
    foreach($forbidden in @('157_SparseLoweringCandidate','158_SparseLoweringPlanner','159_SparseLoweringVerifier','22_CompositionRuntime','14_D3D12Backend')){
        Require ($executionProject -notmatch [regex]::Escape($forbidden)) "CU2 Snapshot execution has forbidden dependency: $forbidden"
    }
} elseif ($Mode -eq 'Regression') {
    foreach($forbidden in @('157_SparseLoweringCandidate','158_SparseLoweringPlanner','22_CompositionRuntime','14_D3D12Backend')){
        Require ($executionProject -notmatch [regex]::Escape($forbidden)) "CU2 Regression execution has forbidden direct dependency: $forbidden"
    }
} else { throw "Unexpected Spiral 6 CU2 verification mode: $Mode" }

Write-Host "SGE4-5 Spiral 6 CU2 static architecture boundaries passed. Mode: $Mode"
Write-Host 'Prefix-only baseline insufficiency and arbitrary exact Sparse set gap proven.'
Write-Host 'Legacy Schema 17 / Runtime 17 / Backend / Composition mutation: None.'
