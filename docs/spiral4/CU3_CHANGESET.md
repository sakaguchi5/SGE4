# Spiral 4 CU3 changeset

Baseline: `ca29f228687691769355afe33f390adf04c1ed24`

## New projects and files

- `123_ActiveWorkLoweringCandidate/*`
- `124_ActiveWorkLoweringPlanner/*`
- `125_ActiveWorkLoweringVerifier/*`
- `126_Spiral4VerifiedExecution/*`
- `141_Spiral4AuthorityMutationTests/*`

## Changed CU2 execution files

- `122_Spiral4IndirectExecution/Spiral4IndirectExecution.h`
- `122_Spiral4IndirectExecution/Spiral4IndirectExecution.cpp`

The change adds actual compiled producer/Consumer shader bytecode digests to
execution evidence. It does not alter Dispatch, Resource, or observation
behavior.

## New operation files

- `tests/tools/Register-Spiral4CU3Projects.ps1`
- `tests/tools/Verify-Spiral4CU3.ps1`
- `tests/Run-Spiral4CU3.ps1`
- `run_sge4_5_spiral4_cu3_prepare.bat`
- `run_sge4_5_spiral4_cu3_independent_authority.bat`

## New documents

- `docs/spiral4/CU3_AUTHORITY_MANIFEST_V1.json`
- `docs/spiral4/CU3_INDEPENDENT_AUTHORITY.md`
- `docs/spiral4/CU3_MUTATION_MATRIX.md`
- `docs/spiral4/CU3_CHANGESET.md`
- `docs/spiral4/CU3_EVIDENCE_LEDGER.md`

## Changed documents

- `docs/spiral4/SPIRAL4_PROGRESS.md`
- `docs/spiral4/PROOF_LEDGER_V1.md`
- `docs/spiral4/README.md`

## Generated destination-checkout changes

`run_sge4_5_spiral4_cu3_prepare.bat` changes:

- `SemanticGpuEngine4-5.sln`
- `SOURCE_MANIFEST.sha256`

## Intentionally unchanged

- D3D12 Package Schema 17,
- Package Runtime 17,
- Composition Runtime,
- canonical D3D12 Backend,
- all Spiral 1–3 evidence,
- CU1 and CU2 authoritative manifests.
