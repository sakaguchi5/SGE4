# Spiral 5 CU1 changeset

Baseline: `f29d6597ec370d963c7b7dfbbc9af9590e8bd58f`

## New files

- `docs/spiral5/CONTRACT_MANIFEST_V1.json`
- `docs/spiral5/SGE4-5_Spiral5_Completion_Spec_v0.1.md`
- `docs/spiral5/NEXT_CAPABILITY_SELECTION_FROM_SPIRAL4.md`
- `docs/spiral5/NON_GOALS_V1.md`
- `docs/spiral5/PROJECT_BOUNDARIES_V1.md`
- `docs/spiral5/CORPUS_V1.md`
- `docs/spiral5/PROOF_LEDGER_V1.md`
- `docs/spiral5/STAGE_MAP_V1.md`
- `docs/spiral5/SPIRAL5_PROGRESS.md`
- `docs/spiral5/CU1_CHANGESET.md`
- `docs/spiral5/CU1_EVIDENCE_LEDGER.md`
- `docs/spiral5/README.md`
- `tests/tools/Verify-Spiral5CU1.ps1`
- `tests/Run-Spiral5CU1.ps1`
- `run_sge4_5_spiral5_cu1_research_contract.bat`

## Intentionally unchanged

- all C++ source and project files,
- `SemanticGpuEngine4-5.sln`,
- Schema 17 and Runtime 17,
- Spiral 4 indirect sidecar,
- Composition Runtime and D3D12 Backend,
- all Spiral 1–4 evidence and reports.

## Required generated change after copying

Run the existing repository tool:

```powershell
powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass `
  -File .\tests\tools\Update-SourceManifest.ps1
```

Review and include the resulting changed `SOURCE_MANIFEST.sha256`.

The manifest is generated in the destination Git checkout because the repository tool inventories that exact checkout.
