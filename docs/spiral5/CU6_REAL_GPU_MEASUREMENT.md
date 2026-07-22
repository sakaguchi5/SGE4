# Spiral 5 CU6 — real-GPU measurement

CU6 measures the three CU4/CU5-qualified Temporal Candidates on one explicitly selected eligible hardware D3D12 adapter.

## Frozen independent variable

The only measurement schedule variable is periodic update interval:

```text
U = 1, 2, 4, 8, 16, 32, 64
```

Every timeline contains 128 invocations and 4096 Work records. The Temporal meaning remains `LastUpdateWinsPiecewiseConstant`.

## Candidate variable

```text
A.EveryInvocationRecompute
B.GlobalMotorHistoryReuse
C.FinalOutputHistoryReuse
```

The Candidates differ only by history materialization position and the already Verified update/hold execution rule.

## Balanced order

CU6 uses all six permutations:

```text
ABC ACB BAC BCA CAB CBA
```

Each Candidate appears twice in each order position. Default measurement produces 24 measured timeline samples per Candidate and U:

```text
6 orders × 2 measured timelines × 2 runs = 24
```

Each sample is one complete 128-invocation timeline.

## GPU timestamp decomposition

Five timestamps are emitted per invocation. They separate:

- update/hold argument generation and transition to indirect state,
- hierarchy execution and history state transition,
- Consumer execution,
- retained-history/output completion,
- the next invocation boundary.

The primary metric is:

```text
GpuCandidateTimelineNanoseconds
  = GpuArgumentGenerationNanoseconds
  + GpuHierarchyExecutionNanoseconds
  + GpuConsumerExecutionNanoseconds
  + GpuRetainedCompletionNanoseconds
```

`GpuUpdatePathNanoseconds` and `GpuHoldPathNanoseconds` are accumulated independently from the same per-invocation timestamp evidence.

Observation copy, CPU input preparation, command recording, submit/fence wait, numeric observation and CPU end-to-end time remain separate.

## Hardware eligibility

The adapter is enumerated with `DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE`. Software adapters are rejected. `adapterIndex` selects among eligible non-software D3D12 adapters, not among the raw DXGI list.

The evidence stores the adapter description, IDs, LUID, driver version, UMA properties, timestamp frequency and fingerprint.

## Numeric validity

Every measured timeline finishes with an output readback. The final output is checked against the independent CPU PGA reference. All A/B/C samples for one U must have the same output digest.

CU5 remains the authority for per-invocation equivalence, Hold stability, no-unauthorized-write, Recovery and stale-history rejection. CU6 does not weaken or replace those proofs.
