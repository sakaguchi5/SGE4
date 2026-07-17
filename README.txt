SGE4 L4V1-F1 script contract fix v2

Apply over the repository root after the original F1 patch.

Changes:
- prepare_sge4_level4v1_f1.bat: UTF-8 without BOM so cmd.exe starts at @echo off.
- run_sge4_level4v1_f1.bat: same preventive correction.
- Prepare-Level4v1F1.ps1: ${expectedCommit}: delimiter fix; UTF-8 BOM retained.
- tests/Run-Level4v1F1.ps1: ${LASTEXITCODE}: delimiter fix.
- tests/tools/Verify-ScriptContracts.ps1: PS1 still requires UTF-8 BOM; BAT/CMD may be BOM or BOM-less.

Run:
  .\prepare_sge4_level4v1_f1.bat
  .\run_sge4_level4v1_f1.bat
