# Spiral 5 CU2 changeset

Baseline: verified Spiral 5 CU1 applied over Owner baseline `f29d6597ec370d963c7b7dfbbc9af9590e8bd58f`.

## New projects

- `134_TemporalStateSemantic`
- `135_Spiral5TemporalContracts`
- `136_Spiral5TemporalExecution`
- `145_Spiral5TemporalArchitectureTests`

## New scripts

- `tests/Spiral5Common.ps1`
- `tests/tools/Register-Spiral5CU2Projects.ps1`
- `tests/tools/Verify-Spiral5CU2.ps1`
- `tests/Run-Spiral5CU2.ps1`
- `run_sge4_5_spiral5_cu2_prepare.bat`
- `run_sge4_5_spiral5_cu2_global_motor_history.bat`

## New documents

- `CU2_ARCHITECTURE_MANIFEST_V1.json`
- `CU2_BASELINE_SUFFICIENCY_AND_GAP.md`
- `CU2_GLOBAL_MOTOR_HISTORY_ARCHITECTURE.md`
- `CU2_CHANGESET.md`
- `CU2_EVIDENCE_LEDGER.md`

## Updated documents

- `SPIRAL5_PROGRESS.md`
- `PROOF_LEDGER_V1.md`
- `README.md`

## Intentionally unchanged

- D3D12 Schema 17 and Runtime 17,
- canonical Composition binary format,
- canonical D3D12 Backend,
- Spiral 4 indirect sidecar bytes and implementation,
- all Spiral 1–4 evidence,
- Spiral 5 CU1 contract.

The prepare script registers the four projects and regenerates `SOURCE_MANIFEST.sha256` in the destination checkout.
