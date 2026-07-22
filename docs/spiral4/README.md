# SGE4-5 Spiral 4

Spiral 4 studies dynamic Active Work Count and verified indirect Batch Lowering.

Start here:

1. `SGE4-5_Spiral4_Completion_Spec_v0.1.md`
2. `CONTRACT_MANIFEST_V1.json`
3. `NEXT_CAPABILITY_SELECTION_FROM_SPIRAL3.md`
4. `CORPUS_V1.md`
5. `PROJECT_BOUNDARIES_V1.md`
6. `PROOF_LEDGER_V1.md`
7. `STAGE_MAP_V1.md`

CU1 is contract-only. It creates no C++ project and makes no ABI change.

After copying the CU1 files into the baseline checkout:

```bat
powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File tests\tools\Update-SourceManifest.ps1
run_sge4_5_spiral4_cu1_research_contract.bat
```
