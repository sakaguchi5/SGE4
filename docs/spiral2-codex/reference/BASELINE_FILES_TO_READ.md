# Baseline files to read

Codex should inspect these before designing new APIs.

## Architecture and operations

```text
README_JA.md
SemanticGpuEngine4-5.sln
SOURCE_MANIFEST.sha256
tests/OPERATIONS_GUIDE_JA.md
tests/Invoke-SGE4_5Tests.ps1
tests/tools/Verify-SGE4_5Identity.ps1
tests/tools/Verify-ScriptContracts.ps1
tests/tools/Verify-SourceManifest.ps1
tests/tools/Update-SourceManifest.ps1
verify_dependencies.ps1
```

## Level 4 v1

```text
16_FrozenCompositionArtifact/
17_CompositionContract/
18_CompositionPlan/
19_CompositionVerifier/
20_CompositionDeviceDomain/
21_CompositionSharedResources/
22_CompositionRuntime/
23_CompositionRecovery/

46_CanonicalCompositionArtifactTests/
47_CanonicalCompositionAuthorityTests/
48_CanonicalCompositionRuntimeTests/
49_CanonicalCompositionRecoveryTests/
49A_CanonicalLevel4v1FreezeTests/

docs/level4v1-canonical/COMPLETION_SPECIFICATION.md
docs/level4v1-canonical/PROOF_LEDGER.md
docs/level4v1-canonical/RECONSTRUCTION_POLICY.md
```

## Spiral 1 implementation patterns

```text
60_PgaRigidTransformSemantic/
61_Spiral1Contracts/
62_Spiral1Corpus/
63_D3D12RepresentationCandidate/
64_D3D12RepresentationPlanner/
65_D3D12RepresentationVerifier/
66_Spiral1LeafCompiler/
67_Spiral1Observer/
68_Spiral1Scenarios/
70_Spiral1SemanticTests/
71_Spiral1RepresentationPlanningTests/
72_Spiral1RepresentationAuthorityTests/
73_Spiral1LeafPackageTests/
74_Spiral1CompositionTests/
75_Spiral1WarpObservationTests/
76_Spiral1RecoveryTests/
77_Spiral1FreezeTests/
78_Spiral1PerformanceExperiment/
79_Spiral1Launcher/
```

Inspect patterns; do not mechanically clone every type or project.

## Spiral 1 evidence and final deferral pattern

```text
docs/spiral1/SGE4-5_Spiral1_Completion_Spec_v0.3.md
docs/spiral1/ARCHITECTURE_FINAL_FREEZE_V1.md
docs/spiral1/STAGE13_REAL_GPU_MEASUREMENT_V1.md
docs/spiral1/STAGE14_DECISION_EVIDENCE_V1.md
docs/spiral1/SPIRAL1_STATUS_AFTER_CU6.md
tests/Run-Spiral1CU6.ps1
tests/tools/Verify-Spiral1CU6.ps1
```

The Spiral 2 final Owner deferral should use the same evidence discipline, adapted to S2-22A/S2-22B.
