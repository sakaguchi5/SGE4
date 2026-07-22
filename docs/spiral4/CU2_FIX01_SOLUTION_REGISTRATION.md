# Spiral 4 CU2 Fix01 — Solution registration correction

## Failure

```text
Project missing from solution:
120_ActiveWorkSemantic\120_ActiveWorkSemantic.vcxproj
```

## Cause

CU2 added four `.vcxproj` files but intentionally deferred Solution
registration. That decision contradicted the repository invariant enforced by
`Verify-SGE4_5Identity.ps1`: every Project file must occur in
`SemanticGpuEngine4-5.sln`.

## Correction

Fix01 registers:

- `120_ActiveWorkSemantic`
- `121_Spiral4Contracts`
- `122_Spiral4IndirectExecution`
- `140_Spiral4IndirectArchitectureTests`

with their exact Project GUIDs and Debug/Release x64 configurations.

The registration script is deterministic and idempotent. It rejects duplicate
paths, partial configuration registration, missing Project files, and
ProjectGuid mismatches.

## Required order

```powershell
.\run_sge4_5_spiral4_cu2_fix01_register_solution.bat

powershell.exe -NoLogo -NoProfile -NonInteractive `
  -ExecutionPolicy Bypass `
  -File .\tests\tools\Update-SourceManifest.ps1

.\run_sge4_5_spiral4_cu2_single_execute_indirect.bat
```

Do not delete or weaken `Verify-SGE4_5Identity.ps1`.
