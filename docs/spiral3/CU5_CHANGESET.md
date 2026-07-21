# Spiral 3 CU5 changeset

Baseline commit: `e5161687c87cd2d3ab4d19d4fd6d520f35c0ef2d`.

CU5 adds execution qualification without changing the CU4 Frozen Composition, D3D12 Schema 17, or Runtime 17.

## New projects

- `108_Spiral3Execution`: Runtime-only execution and CPU/GPU observation reconstruction.
- `115_Spiral3WarpObservationTests`: six R cases by twelve frame palettes on WARP.
- `116_Spiral3RecoveryTests`: controlled whole-composition rebuild and actual removal quarantine.
- `117_Spiral3FreezeTests`: deterministic architecture bundle.

## Qualification boundaries

- The static CU4 scenario project remains Runtime/Backend-free.
- Dynamic Motor and independent reference data are rebound explicitly for every frame.
- Package-owned local points are read back and compared byte-for-byte.
- A/B/C output points are checked against the independent reference and pairwise.
- GPU Observer records are reconstructed and validated on the CPU.
- Frozen Composition bytes must remain unchanged across all twelve palettes.
- Controlled recovery rejects stale epoch handles and requires explicit dynamic rebind.
- Actual device removal enters `AwaitingAdapter`, releases all runtime objects, excludes the removed adapter and rejects stale submission.
