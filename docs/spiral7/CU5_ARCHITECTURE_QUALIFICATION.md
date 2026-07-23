# Spiral 7 CU5 — Architecture Qualification

## Status

CU5 is complete. The accepted exhaustive Owner run is based on commit:

```text
67cb40b5204e1e06ecac576206ba969ec2db02b6
```

It established the complete 128-invocation A/B/C WARP Architecture, Controlled Recovery, actual device-removal quarantine, fresh-process rematerialization and Debug/Release evidence identity.

## Frozen runtime authority

The Runtime loads exactly 128 independently verified Sparse–Temporal invocations in the fixed order:

```text
A = FullActiveDenseRecompute
B = CompactDeltaIndexHistoryReuse
C = AffectedBlockDeltaHistoryReuse
```

The serialized authority binds Semantic, verification context, invocation, Active/Modified/Transition sets and all Candidate artifacts and certificates. Runtime may materialize this authority but may not choose membership, action, history policy or Candidate.

## Complete qualification

Each complete qualification executes 128 chained invocations and 384 Candidate executions. It covers all frozen Active counts, Transition counts and eight transition kinds. After every invocation, active output matches the exact current-generation oracle, inactive output is the canonical sentinel, B/C preserve `H_t` without a write, observed writes equal the verified legal set and A/B/C full outputs are byte-identical.

## Recovery

Controlled Recovery increments the Device epoch, rejects old epoch, representation, History and Completion handles, requires explicit `A_t/M_t` rebind and Candidate representation rebuild, and forces `W_t=A_t`, `H_t=empty` with exact per-item generations. Actual `ID3D12Device5::RemoveDevice` enters `AwaitingAdapter` and excludes the removed WARP LUID from retry.

## Accepted evidence

```text
Architecture        1F1D09B4E52DBC35E961E0D3751B32292B2C30EEDCFA473800C5BB8E8CB5AB73
Controlled Recovery 7F0247B193F9BC20F4EDA5FEED590C1DA5722EF36B133640292B1ED616CFF62B
Fresh process       091EAEC0D27287FE897F6813043FD75C090D5DA17054461DF314F99B2A6F5A92
```

## Operation

The ordinary runner is now a fast regression against these accepted evidence identities. The complete Debug/Release re-audit is preserved as a separate explicit command. See `CU5_TEST_OPERATION_POLICY.md`.

Successful CU5 evidence establishes:

```text
SGE4-5 SPIRAL 7 ARCHITECTURE COMPLETE
```

It does not establish a universal Candidate winner. CU6 remains real-GPU measurement and Owner decision evidence.
