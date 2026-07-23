# Spiral 7 CU5 changeset

## Added projects

```text
185_Spiral7DeltaFamilyRuntime
186_Spiral7ArchitectureQualificationTests
```

## Added behavior

- Frozen 128-invocation Delta Family Runtime authority.
- Explicit timeline binding and Candidate representation rebuild.
- Device-epoch-bound representation, History and Completion handles.
- Complete WARP qualification corpus: 128 invocations / 384 Candidate executions.
- Controlled whole-Composition Recovery with explicit `A_t/M_t` rebind.
- Exact-generation forced full-active post-Recovery materialization.
- Actual `ID3D12Device5::RemoveDevice` quarantine and removed-LUID exclusion.
- Fresh-process rematerialization and Debug/Release evidence comparison.

## Changed documents

```text
docs/spiral7/README.md
docs/spiral7/SPIRAL7_PROGRESS.md
docs/spiral7/PROOF_LEDGER_V1.md
```

## Deleted files

None.

## Git operations

None.

## Execution optimization V2 amendment

- Replaced per-Candidate committed-resource creation with one reusable fixed-capacity resource set.
- Batched A/B/C into one CommandList submission and one Fence wait per invocation.
- Added 16-invocation WARP progress reporting and per-stage elapsed-time reporting.
- Preserved Candidate count, observation checks, evidence serialization and Debug/Release equality requirements.
