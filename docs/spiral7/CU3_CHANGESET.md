# Spiral 7 CU3 changeset

## New projects

- `175_SparseTemporalDeltaLoweringCandidate`
- `176_SparseTemporalDeltaLoweringPlanner`
- `177_SparseTemporalDeltaLoweringVerifier`
- `183_Spiral7AuthorityMutationTests`

## Modified project

- `174_Spiral7DeltaExecution` now accepts only opaque Verified Delta plans through an explicit Frozen resource/epoch binding path.

## New operations

- plan the CU2 Compact Delta Candidate as an untrusted Raw value,
- independently reconstruct and verify all Delta authority,
- freeze verified execution against Delta, previous History and output History resources,
- reject stale Device epochs,
- execute one verified WARP transition and emit deterministic evidence.

No Schema 17, Runtime 17, canonical Backend or Composition Contract mutation is included.
