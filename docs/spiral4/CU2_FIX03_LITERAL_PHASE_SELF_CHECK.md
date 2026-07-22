# Spiral 4 CU2 Fix03 — literal phase self-check

## Observed failure

```text
CU1 Snapshot branch missing.
```

Immediately before this message, CU1 itself reported:

```text
SGE4-5 Spiral 4 CU1 research contract passed. Mode: Regression
```

Therefore the Snapshot/Regression implementation was present and executable.
The failure came from CU2's source-text self-check.

## Cause

Fix02 used double-quoted PowerShell regular-expression strings such as:

```powershell
"if \(\$Mode -eq 'Snapshot'\)"
```

In PowerShell, backslash does not escape `$`. The current script therefore
expanded `$Mode` before running the regular expression, changing the pattern
and making the existing branch appear absent.

## Correction

Fix03 removes regular expressions from this source-text check.

It constructs literal tokens with single-quoted PowerShell strings:

```powershell
$snapshotToken = 'if ($Mode -eq ''Snapshot'')'
$regressionToken = 'elseif ($Mode -eq ''Regression'')'
```

and verifies them with `.Contains()`.

The actual CU1 Regression execution remains authoritative. No CU1 contract,
Solution, C++, ABI, Runtime, Backend, or ExecuteIndirect implementation is
changed.

## Rerun

After extracting Fix03:

```powershell
powershell.exe -NoLogo -NoProfile -NonInteractive `
  -ExecutionPolicy Bypass `
  -File .\tests\tools\Update-SourceManifest.ps1

.\run_sge4_5_spiral4_cu2_single_execute_indirect.bat
```
