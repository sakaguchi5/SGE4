# Spiral 5 CU3 changeset

Owner baseline: `f29d6597ec370d963c7b7dfbbc9af9590e8bd58f`

## New projects

- `137_TemporalLoweringCandidate`
- `138_TemporalLoweringPlanner`
- `139_TemporalLoweringVerifier`
- `146_Spiral5AuthorityMutationTests`

## Changed project

- `136_Spiral5TemporalExecution`
  - accepts only an opaque Verified Temporal Plan for the CU3 verified route,
  - binds the CU2 artifact to an actual History Resource identity and device epoch,
  - emits a verified actual-execution identity.

## New scripts

- `tests/tools/Register-Spiral5CU3Projects.ps1`
- `tests/tools/Verify-Spiral5CU3.ps1`
- `tests/Run-Spiral5CU3.ps1`
- `run_sge4_5_spiral5_cu3_prepare.bat`
- `run_sge4_5_spiral5_cu3_independent_authority.bat`

## Changed CU2 verifier

`Verify-Spiral5CU2.ps1` now has Snapshot and Regression modes. CU2 Snapshot still forbids future authority dependencies. CU3 Regression permits the intentional `136 -> 139` Verified-type dependency while continuing to forbid direct Runtime execution dependencies on Raw Candidate or Planner.

## Intentionally unchanged

- Schema 17 and Runtime 17,
- canonical Composition Runtime and D3D12 Backend,
- CU2 Temporal artifact byte layout,
- Spiral 4 indirect sidecar,
- A and C Temporal Candidates.

No files are deleted.
