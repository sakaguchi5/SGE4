# Spiral 4 CU6 changeset

Baseline: `302e31569c2cec5d69fbc53f3ddb363cd0af777c`

## New projects

- `133_Spiral4PerformanceExperiment`
- `144_Spiral4PerformanceTests`

## New scripts

- `tests/tools/Register-Spiral4CU6Projects.ps1`
- `tests/tools/Verify-Spiral4CU6.ps1`
- `tests/Run-Spiral4CU6.ps1`
- `run_sge4_5_spiral4_cu6_prepare.bat`
- `run_sge4_5_spiral4_cu6_measurement_decision_evidence.bat`

## New documents

- `CU6_MEASUREMENT_MANIFEST_V1.json`
- `CU6_REAL_GPU_MEASUREMENT.md`
- `CU6_DECISION_RULES.md`
- `CU6_CHANGESET.md`
- `CU6_EVIDENCE_LEDGER.md`

## Changed documents

- `SPIRAL4_PROGRESS.md`
- `PROOF_LEDGER_V1.md`
- `README.md`

## Generated destination changes

The prepare runner updates:

- `SemanticGpuEngine4-5.sln`
- `SOURCE_MANIFEST.sha256`

The measurement runner writes only under:

- `build/tests/spiral4-cu6/`

## Intentionally unchanged

- projects 120–132 and 140–143,
- CU1–CU5 manifests and evidence,
- D3D12 Package Schema 17,
- Package Runtime 17,
- Composition Runtime and canonical Backend,
- Runtime policy authorization.

## Deletions

None.
