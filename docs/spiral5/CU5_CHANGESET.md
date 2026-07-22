# Spiral 5 CU5 changeset

Baseline: `a3d9a19a1a3033b723df918e70f5af6a24d884a0`

## New files

- `151_Spiral5ArchitectureQualificationTests/151_Spiral5ArchitectureQualificationTests.vcxproj`
- `151_Spiral5ArchitectureQualificationTests/main.cpp`
- `docs/spiral5/CU5_ARCHITECTURE_MANIFEST_V1.json`
- `docs/spiral5/CU5_ARCHITECTURE_COMPLETE.md`
- `docs/spiral5/CU5_RECOVERY_MODEL.md`
- `docs/spiral5/CU5_EVIDENCE_LEDGER.md`
- `docs/spiral5/CU5_CHANGESET.md`
- `tests/tools/Register-Spiral5CU5Projects.ps1`
- `tests/tools/Verify-Spiral5CU5.ps1`
- `tests/Run-Spiral5CU5.ps1`
- `run_sge4_5_spiral5_cu5_prepare.bat`
- `run_sge4_5_spiral5_cu5_architecture_qualification.bat`

## Changed files

- `136_Spiral5TemporalExecution/Spiral5TemporalExecution.h`
- `136_Spiral5TemporalExecution/Spiral5TemporalExecution.cpp`
- `docs/spiral5/SPIRAL5_PROGRESS.md`
- `docs/spiral5/PROOF_LEDGER_V1.md`
- `docs/spiral5/README.md`

## Intentionally unchanged

- D3D12 Package Schema,
- canonical Package Runtime,
- canonical D3D12 Backend,
- Composition Runtime,
- Spiral 4 indirect sidecar,
- CU1-CU4 Semantic, Candidate and Verifier authority.

## Generated destination changes

After copying, run `run_sge4_5_spiral5_cu5_prepare.bat`. It registers project 151 and regenerates `SOURCE_MANIFEST.sha256` in the destination checkout.
