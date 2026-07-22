# Spiral 4 CU2 Fix02 — phase-aware CU1 regression

## Observed failure

```text
CU1 must not pre-create future project: 120_ActiveWorkSemantic
```

## Cause

CU1 correctly proved that its contract-only checkout had not pre-created future
implementation projects. That statement was true at CU1 and was part of the
CU1 snapshot proof.

It is not a monotonic invariant. CU2 is specifically responsible for creating
`120_ActiveWorkSemantic`, `121_Spiral4Contracts`, and the other CU2 projects.

The CU2 runner reused the CU1 verifier without telling it that the repository
had advanced to a later Completion Unit. The verifier therefore reapplied a
historical absence condition to the current checkout.

## Correct verification model

### Snapshot mode

Used to qualify the exact CU1 checkout.

It verifies:

- CU1 contract and corpus,
- no CU1 ABI mutation,
- no ExecuteIndirect implementation,
- no future Spiral 4 project registration.

### Regression mode

Used by CU2 and later.

It verifies:

- the same CU1 contract manifest,
- fixed semantic dimensions,
- Candidate family,
- non-goals and Runtime-policy prohibition,
- required CU1 documents and historical Spiral 3 evidence.

It does not require later projects or capabilities to remain absent.

## Why this is not weakening the proof

The absence of later projects is still checked in Snapshot mode.

Regression mode removes only predicates that are expected to become false when
the planned later Completion Units are implemented. All immutable CU1 contract
predicates remain active.

## Apply and rerun

After extracting Fix02:

```powershell
powershell.exe -NoLogo -NoProfile -NonInteractive `
  -ExecutionPolicy Bypass `
  -File .\tests\tools\Update-SourceManifest.ps1

.\run_sge4_5_spiral4_cu2_single_execute_indirect.bat
```

No Solution registration step is needed again unless the Solution was reverted.
