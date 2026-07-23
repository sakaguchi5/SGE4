# Spiral 7 CU2 Compact Delta Index History architecture

## Candidate

```text
CompactDeltaIndexHistoryReuse
```

CU2 implements only the first incremental Candidate. Full Active recompute and affected-block delta remain deferred to CU4.

## Semantic input

For each invocation, external verified input supplies:

```text
A_t  exact current Active Set
M_t  exact modified survivors
```

CU2 independently derives:

```text
N_t = A_t \ A_(t-1)
R_t = A_(t-1) \ A_t
W_t = N_t union M_t
H_t = (A_t intersect A_(t-1)) \ M_t
T_t = W_t union R_t
```

`M_t` is rejected unless it is a subset of `A_t intersect A_(t-1)`.

## Per-item generation

- newly activated records begin at generation zero,
- modified surviving records increment by one,
- retained records preserve their generation,
- inactive records carry the invalid generation sentinel.

Generation changes alter the canonical point input by exactly representable binary offsets before the fixed Direct PGA consumer. This makes a false Hold observable.

## Artifact layout

The fixed capacity is 4096 records:

```text
record[0 .. transitionCount-1]
    strictly increasing universeIndex
    action = Update or Clear

record[transitionCount .. 4095]
    universeIndex = 0xFFFFFFFF
    action = Unused
```

Record stride is 8 bytes and total record capacity is 32768 bytes.

The artifact binds:

- Sparse–Temporal Delta Semantic identity,
- invocation identity,
- previous and current Active Set identities,
- Modified Survivor Set identity,
- exact Transition identity,
- Spiral 4 indirect identity,
- Spiral 5 Temporal extension identity,
- Spiral 6 Sparse extension identity,
- target, program and observation identities.

## WARP execution

The output Buffer is first initialized from the exact previous-history bytes. A single ExecuteIndirect Dispatch consumes the compact record list.

```text
Update -> transform the current per-item generation point and write output[index]
Clear  -> write float4(0,0,0,0)
Hold   -> no transition record and no GPU write
```

The Dispatch group count is:

```text
transitionCount == 0 ? 0 : ceil(transitionCount / 64)
```

## CU2 observations

Fourteen WARP cases cover:

- initial activation,
- exact Hold with zero Dispatch,
- one dirty survivor,
- activation and deactivation boundaries,
- balanced churn,
- pattern migration,
- empty reset,
- 1024-record activation,
- 256 dirty records,
- large deactivation,
- complete invalidation of a surviving set.

Every case verifies:

- active output against the current-generation CPU oracle,
- retained bytes are byte-identical to previous valid history,
- inactive bytes are byte-identical to the canonical zero sentinel,
- Update and Clear counts reconstruct exactly `T_t`,
- Debug and Release evidence bytes are identical.

## Deferred work

CU2 does not prove:

- Planner-independent authority or opaque Verified Plan,
- Resource, history generation or device epoch binding,
- the other two Candidates,
- Candidate output equivalence,
- Recovery,
- real-GPU performance.
