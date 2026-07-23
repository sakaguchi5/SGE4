# Spiral 7 CU6 changeset

Baseline commit:

```text
c9f0b5a62e2a7f3c9e0355cdaa1c683819c6dcfa
```

New projects:

```text
187_Spiral7PerformanceExperiment
188_Spiral7PerformanceTests
```

New capabilities:

- exact 160-case-per-run real-GPU measurement grid,
- D3D12 timestamp-query measurement with explicit zero-dispatch resolution censoring,
- six balanced Candidate orders,
- per-case correctness validation before Timing,
- fixed-control drift rejection and retry,
- deterministic `S7M2` binary evidence with raw timestamp ticks, effective iterations, censor flags and corruption rejection,
- independent evidence reload and decision reproduction,
- two-dimensional Active/Transition crossover evidence,
- Pattern dependence evidence,
- explicit Owner and Runtime-policy boundaries.

Unchanged:

- D3D12 Schema 17,
- Runtime 17,
- Level 4 v1 Canonical bytes,
- Spiral 4/5/6 sidecar bytes,
- Spiral 7 CU1-CU5 Semantic and Authority,
- Recovery behavior,
- Runtime Candidate-policy authorization (`None`).

FIX02 correction:

- `T=0` B/C legitimately use zero Dispatch groups; equal timestamp ticks are no longer rejected.
- zero-dispatch 0-tick observations are recorded as one-tick conservative upper bounds.
- nonzero-dispatch 0-tick observations retry the complete balanced order with doubled iterations.
- output names advance from `*_v1` to `*_v2`.

Deleted files: none.
Git operations: none.
