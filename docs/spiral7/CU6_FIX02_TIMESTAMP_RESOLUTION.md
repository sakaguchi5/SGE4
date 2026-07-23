# Spiral 7 CU6 FIX02 — Timestamp Resolution Contract

## Failure

The first real-GPU measurement failed with `Non-positive balanced-order timestamp interval.`

The failure is expected at legal Hold cases: `T=0` gives Candidate B and C `DispatchGroupsX=0`. A zero-work `ExecuteIndirect` may not advance the D3D12 GPU timestamp counter between adjacent queries.

## Corrected contract

```text
zero dispatch + zero ticks
  => resolution-censored observation
  => raw ticks = 0
  => effective time used for conservative comparison = one timestamp tick upper bound

nonzero dispatch + zero ticks
  => never censored
  => rerun the complete balanced A/B/C order with doubled iterations
  => fail only if still unresolved at 65536 iterations
```

Every timing sample records raw timestamp ticks, effective iterations and whether the observation was resolution-censored. The evidence magic/schema is `S7M2`. Architecture, Candidate authority, Shader semantics and Runtime policy remain unchanged.
