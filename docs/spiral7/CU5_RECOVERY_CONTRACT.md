# Spiral 7 CU5 Recovery contract

## Whole-Composition Recovery

Recovery remains a whole-Composition operation. The Frozen Semantic and Candidate authority survives, while all Device-derived execution state is invalidated.

Invalidated state:

```text
Candidate representation resources
ExecuteIndirect arguments
History state
Completion state
Readback state
```

The Device epoch advances only after an adapter and native Device are successfully rematerialized.

## Stale-handle rejection

After an epoch change, the old values are rejected:

```text
DeltaRuntimeEpochHandleV1
DeltaRepresentationSetHandleV1
DeltaHistoryHandleV1
DeltaRuntimeSubmissionTokenV1
```

A handle cannot be made current by changing only its epoch field because each identity is recomputed from its authority and binding dimensions.

## Explicit external rebind

The first post-Recovery Semantic invocation must explicitly rebind:

```text
A_t = exact current Active Set
M_t = exact modified surviving members
```

Runtime does not infer either set.

The Semantic recovery invocation must prove:

```text
PreviousActive = A_t
ForceFullActiveRebuild = true
W_t = A_t
H_t = empty
R_t = empty
```

`M_t` still controls exact per-item generation advancement. CU5's controlled test rebinds 64 modified survivors.

## Exact-generation GPU rematerialization

The CU4 family executor requires a timeline beginning at invocation zero. CU5 therefore uses an internal two-invocation materialization sequence without changing the single external Semantic recovery invocation:

```text
internal invocation 0:
    empty membership -> A_t
    establishes canonical initialized GPU storage

internal invocation 1:
    A_t -> A_t
    exact rebound M_t and predecessor generations
    ForceFullActiveRebuild = true
    writes every member of A_t at its exact current generation
```

The second invocation overwrites every active record, so no byte from stale pre-Recovery history is retained. Inactive records remain the canonical sentinel. This construction works for nonzero and nonuniform per-item generations; it is not limited to generation zero.

## Actual removal quarantine

The actual-removal test invokes `ID3D12Device5::RemoveDevice` on WARP and requires:

```text
Active -> AwaitingAdapter
old handles rejected by Device state
removed WARP LUID recorded and excluded
retry does not reacquire the removed adapter
Device epoch remains unchanged while no replacement is accepted
```
