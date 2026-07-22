# Spiral 4 CU2 changeset

Baseline: `4033e8bf84650b6d1edbb6a8a83d97d5e1c3e4d1`

## New files

- `120_ActiveWorkSemantic/120_ActiveWorkSemantic.vcxproj`
- `120_ActiveWorkSemantic/ActiveWorkSemantic.h`
- `120_ActiveWorkSemantic/ActiveWorkSemantic.cpp`
- `121_Spiral4Contracts/121_Spiral4Contracts.vcxproj`
- `121_Spiral4Contracts/Spiral4Contracts.h`
- `121_Spiral4Contracts/Spiral4Contracts.cpp`
- `122_Spiral4IndirectExecution/122_Spiral4IndirectExecution.vcxproj`
- `122_Spiral4IndirectExecution/Spiral4IndirectExecution.h`
- `122_Spiral4IndirectExecution/Spiral4IndirectExecution.cpp`
- `140_Spiral4IndirectArchitectureTests/140_Spiral4IndirectArchitectureTests.vcxproj`
- `140_Spiral4IndirectArchitectureTests/main.cpp`
- `tests/Spiral4Common.ps1`
- `tests/Run-Spiral4CU2.ps1`
- `tests/tools/Verify-Spiral4CU2.ps1`
- `run_sge4_5_spiral4_cu2_single_execute_indirect.bat`
- `docs/spiral4/CU2_ARCHITECTURE_MANIFEST_V1.json`
- `docs/spiral4/CU2_SCHEMA17_INSUFFICIENCY.md`
- `docs/spiral4/CU2_SINGLE_INDIRECT_ARCHITECTURE.md`
- `docs/spiral4/CU2_CHANGESET.md`
- `docs/spiral4/CU2_EVIDENCE_LEDGER.md`

## Changed files

- `docs/spiral4/SPIRAL4_PROGRESS.md`
- `docs/spiral4/PROOF_LEDGER_V1.md`
- `docs/spiral4/README.md`

## Changed by Fix01 after CU2 file installation

- `SemanticGpuEngine4-5.sln`
  - registers projects 120, 121, 122, and 140,
  - registers Debug|x64 and Release|x64 configurations.

## Intentionally unchanged

- all legacy Schema 17 files
- Package Runtime 17
- Composition Runtime
- canonical D3D12 Backend
- all Spiral 1–3 evidence
- CU1 Contract Manifest and Completion Specification

The original CU2 delivery attempted to defer Solution membership until CU3.
That was invalid because `Verify-SGE4_5Identity.ps1` requires every `.vcxproj`
to be a formal Solution member. Fix01 registers all four CU2 projects without
weakening the identity gate.

## Required generated change

After copying, regenerate and include:

- `SOURCE_MANIFEST.sha256`

## Fix01 new files

- `tests/tools/Register-Spiral4CU2Projects.ps1`
- `run_sge4_5_spiral4_cu2_fix01_register_solution.bat`

## Fix01 changed files

- `tests/tools/Verify-Spiral4CU2.ps1`
- `docs/spiral4/README.md`
- `docs/spiral4/SPIRAL4_PROGRESS.md`
- `docs/spiral4/CU2_CHANGESET.md`
- `docs/spiral4/CU2_EVIDENCE_LEDGER.md`


## Fix02 new files

- `docs/spiral4/CU2_FIX02_PHASE_AWARE_CU1_REGRESSION.md`

## Fix02 changed files

- `tests/tools/Verify-Spiral4CU1.ps1`
- `tests/Run-Spiral4CU1.ps1`
- `tests/Spiral4Common.ps1`
- `tests/tools/Verify-Spiral4CU2.ps1`
- `docs/spiral4/SPIRAL4_PROGRESS.md`
- `docs/spiral4/CU2_CHANGESET.md`
- `docs/spiral4/CU2_EVIDENCE_LEDGER.md`
- `docs/spiral4/README.md`

Fix02 does not delete the CU1 absence proof. It confines that proof to the CU1
Snapshot mode where it is logically valid.
