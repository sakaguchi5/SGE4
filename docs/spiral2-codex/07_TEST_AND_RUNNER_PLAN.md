# Test and runner plan

## Preserve the existing hierarchy

Existing baseline tiers:

```text
run_sge4_5_dev.bat
run_sge4_5_gate.bat
run_sge4_5_regression.bat
run_sge4_5_freeze.bat
```

Spiral 2 should add:

```text
run_sge4_5_spiral2_dev.bat
run_sge4_5_spiral2_gate.bat
run_sge4_5_spiral2_regression.bat
run_sge4_5_spiral2_freeze.bat

run_sge4_5_spiral2_cu0_contract_baseline.bat
run_sge4_5_spiral2_cu1_semantic_invocation.bat
run_sge4_5_spiral2_cu2_candidate_authority.bat
run_sge4_5_spiral2_cu3_leaf_groups.bat
run_sge4_5_spiral2_cu4_composition_sufficiency.bat
run_sge4_5_spiral2_cu5_warp_determinism_recovery.bat
run_sge4_5_spiral2_cu6_measurement_decision_evidence.bat
```

Prefer one PowerShell runner per CU with a `ValidateSet` Stage mode, mirroring the existing Spiral 1 pattern, rather than duplicating logic across many `.bat` files.

Recommended:

```text
tests/Run-Spiral2ResearchContract.ps1
tests/Run-Spiral2CU0.ps1
...
tests/Run-Spiral2CU6.ps1

tests/tools/Verify-Spiral2ResearchContract.ps1
tests/tools/Verify-Spiral2CU0.ps1
...
tests/tools/Verify-Spiral2CU6.ps1
```

## During editing

Run the narrowest relevant executable/project first.

Then:

```powershell
cmd /c .\run_sge4_5_spiral2_dev.bat
```

Dev should include fast semantic, candidate, authority, and exact-byte tests but exclude long WARP, removal, Release, and full fresh-process comparisons.

## CU boundary

At minimum:

```powershell
cmd /c .\run_sge4_5_spiral2_gate.bat
cmd /c .\run_sge4_5_gate.bat
```

If Package, Runtime, Backend, Composition, dynamic binding, or recovery changed:

```powershell
cmd /c .\run_sge4_5_spiral2_regression.bat
cmd /c .\run_sge4_5_regression.bat
```

## Formal freeze

For Architecture and CU completion:

```powershell
cmd /c .\run_sge4_5_spiral2_freeze.bat
cmd /c .\run_sge4_5_freeze.bat
```

The Spiral 2 Freeze must include Debug and Release, fresh-process artifact comparison, WARP, multi-frame, controlled recovery, actual removal qualification, and stale epoch rejection.

## Mutation test rule

Each authority field added by Spiral 2 needs:

1. one positive control showing it affects the intended Frozen result or verification,
2. one negative mutation showing tampering is rejected,
3. the expected rejection boundary.

Do not use only digest mismatch tests. Prove the semantic authority field is actually consumed.

## Script contracts

Follow existing repository script conventions:

- PowerShell sets `$ErrorActionPreference = 'Stop'`.
- Use UTF-8 encoding consistently.
- Discover MSBuild through `vswhere.exe`.
- Propagate nonzero exit codes.
- Use ASCII script filenames.
- Do not hide a failure with `continue`, ignored exit codes, or a stale evidence file.

## Source manifest

After intentional inventory changes:

```powershell
powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass `
  -File .\tests\tools\Update-SourceManifest.ps1

cmd /c .\tests\run_architecture.bat
```

## Real GPU measurement

Stage S2-20 uses M0-M5 and all six candidate orders:

```text
ABC ACB BAC BCA CAB CBA
```

Record Adapter, driver, clock policy, topology, invocation, queue, timestamp frequency, iteration counts, and raw evidence.

Do not use WARP for performance claims.

If hardware is unavailable, write `HARDWARE_MEASUREMENT_PENDING.md`, preserve the exact command line, and continue all non-hardware evidence work.
