# SGE4-5 Stage F0 - Foundation Bootstrap

Stage F0 combines the former baseline-import, independent-identity, and Level 4 v1 requalification stages.

## F0-A Baseline capture

- Preserve the exact `SGE4v1.zip` SHA-256.
- Preserve its original source manifest.
- Record that `SGE4v1.zip`, not `SGE4.zip`, is the sole source archive.

## F0-B Independent identity

- Root: `SGE4-5`
- Solution: `SemanticGpuEngine4-5.sln`
- Namespace: `sge4_5`
- Compiler project: `12_SGE4_5Compiler`
- Project GUIDs are deterministically regenerated.
- Build, test and package outputs remain under this root.
- No ProjectReference points outside this tree.

## F0-C Level 4 v1 requalification

Run:

```bat
run_sge4_5_foundation.bat
```

The runner performs identity checks, dependency checks, script checks, source-manifest checks, and the inherited Level 4 v1 final integration suite.

## Completion condition

F0 is complete only after the Windows/Visual Studio environment prints:

```text
============================================================
SGE4-5 SPIRAL FOUNDATION BOOTSTRAP PASSED
Baseline archive: SGE4v1.zip
Independent solution: SemanticGpuEngine4-5.sln
Independent namespace: sge4_5
Level 1-3 execution and planning authority preserved
Level 4 v1 composition, runtime and recovery requalified
Ready for Spiral 1 Research Contract
============================================================
```

This ZIP prepares the independent source tree and verification commands. The final PASSED banner must be earned on the target Windows environment; it is not pre-claimed by packaging the files.

## Windows script encoding

- `.ps1` files are UTF-8 with BOM for Windows PowerShell 5.1.
- `.bat` and `.cmd` files are UTF-8 without BOM so `cmd.exe` reads `@echo off` correctly.
- `Verify-SGE4_5Identity.ps1` resolves the repository root from `tests\tools` by walking up three directory levels.
