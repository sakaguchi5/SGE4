# Spiral 4 CU1 changeset

Baseline: `8c1125394ba4b45d571d7cba4e7ad685bb90918b`

## New files

- `docs/spiral4/CONTRACT_MANIFEST_V1.json`
- `docs/spiral4/SGE4-5_Spiral4_Completion_Spec_v0.1.md`
- `docs/spiral4/NEXT_CAPABILITY_SELECTION_FROM_SPIRAL3.md`
- `docs/spiral4/NON_GOALS_V1.md`
- `docs/spiral4/PROJECT_BOUNDARIES_V1.md`
- `docs/spiral4/CORPUS_V1.md`
- `docs/spiral4/PROOF_LEDGER_V1.md`
- `docs/spiral4/STAGE_MAP_V1.md`
- `docs/spiral4/SPIRAL4_PROGRESS.md`
- `docs/spiral4/CU1_CHANGESET.md`
- `docs/spiral4/CU1_EVIDENCE_LEDGER.md`
- `docs/spiral4/README.md`
- `tests/tools/Verify-Spiral4CU1.ps1`
- `tests/Run-Spiral4CU1.ps1`
- `run_sge4_5_spiral4_cu1_research_contract.bat`

## Intentionally unchanged

- all C++ source and project files,
- `SemanticGpuEngine4-5.sln`,
- Schema 17 and Runtime 17,
- Composition Runtime and D3D12 Backend,
- all Spiral 1–3 evidence and reports.

## Required generated change after copying

Run the existing repository tool:

```powershell
powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass `
  -File .\tests\tools\Update-SourceManifest.ps1
```

Review and include the resulting changed `SOURCE_MANIFEST.sha256`.

The manifest is deliberately generated in the destination Git checkout because the repository tool inventories tracked and untracked non-ignored files from that exact checkout.
