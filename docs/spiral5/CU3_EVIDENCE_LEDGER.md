# Spiral 5 CU3 evidence ledger

## Static evidence

- Verifier project has no dependency on `138_TemporalLoweringPlanner`.
- Raw, Verified, and Frozen types are not implicitly convertible.
- Verified and Frozen constructors are unavailable to callers.
- `136_Spiral5TemporalExecution` depends on the opaque Verifier type, not directly on Raw Candidate or Planner.

## Executable evidence

The Debug and Release authority-test processes each perform:

- canonical P4 planning and independent verification,
- 40 Raw proposal mutation rejections,
- cross-schedule and cross-context seal rejection,
- actual CU2 artifact and History Resource binding,
- wrong Resource/Role/size/epoch rejection,
- P4 verified WARP execution for 128 invocations,
- generation, Hold stability, output, and completion checks inherited from CU2.

The emitted authority bundles must be byte-identical across fresh Debug and Release processes.

## Deferred

- A/C Candidate authority,
- Candidate Family comparison,
- controlled and actual Recovery,
- stale native objects after device loss,
- all nine schedule qualification,
- real-GPU measurement.
