# Spiral 4 CU5 changeset

Baseline: `8577ad1e4bb99f2a1c4d7125c6e586b33fb54154`

## Changed execution files

- `122_Spiral4IndirectExecution/Spiral4IndirectExecution.h`
- `122_Spiral4IndirectExecution/Spiral4IndirectExecution.cpp`
- `131_Spiral4CandidateFamilyExecution/CandidateFamilyExecution.h`
- `131_Spiral4CandidateFamilyExecution/CandidateFamilyExecution.cpp`

These add native-device entry points. Existing WARP entry points remain and
delegate to them. Candidate B therefore executes on the same supplied device as
A and C inside the CU5 DeviceDomain.

## New projects

- `132_Spiral4CandidateFamilyRuntime`
- `143_Spiral4ArchitectureQualificationTests`

## New scripts

- `tests/tools/Register-Spiral4CU5Projects.ps1`
- `tests/tools/Verify-Spiral4CU5.ps1`
- `tests/Run-Spiral4CU5.ps1`
- `run_sge4_5_spiral4_cu5_prepare.bat`
- `run_sge4_5_spiral4_cu5_architecture_qualification.bat`

## New documents

- `CU5_ARCHITECTURE_MANIFEST_V1.json`
- `CU5_ARCHITECTURE_COMPLETE.md`
- `CU5_RECOVERY_MODEL.md`
- `CU5_EVIDENCE_LEDGER.md`
- `CU5_CHANGESET.md`

## Changed documents

- `SPIRAL4_PROGRESS.md`
- `PROOF_LEDGER_V1.md`
- `README.md`

## Generated destination changes

The prepare runner changes:

- `SemanticGpuEngine4-5.sln`
- `SOURCE_MANIFEST.sha256`

## Intentionally unchanged

- CU3 and CU4 authority types and candidate identities,
- D3D12 Package Schema 17,
- Package Runtime 17,
- canonical Composition Runtime and Backend,
- Spiral 1–3 historical evidence.
