# Spiral 7 CU5 — Architecture Qualification

## Status represented by this overlay

CU5 is the Architecture Qualification unit for `Versioned Sparse Delta Flow and Verified Incremental History Lowering`.

The accepted input boundary is CU4 commit:

```text
8f3b17e25d2a500beb2b658e6bc0a1d3f646ec26
```

CU5 does not choose a Candidate. It qualifies the frozen A/B/C family as one deterministic Architecture.

## Frozen runtime authority

The CU5 Runtime loads exactly 128 independently verified Sparse–Temporal invocations. Every invocation contains the fixed Candidate order:

```text
A = FullActiveDenseRecompute
B = CompactDeltaIndexHistoryReuse
C = AffectedBlockDeltaHistoryReuse
```

The serialized authority binds:

- Sparse–Temporal Semantic identity,
- verification-context identities,
- all 128 invocation identities,
- previous/current Active identities,
- Modified Survivor and Transition identities,
- the Candidate artifact/certificate set for every invocation.

Runtime may materialize this authority, but may not choose membership, modified membership, transition action, history policy or Candidate.

## Complete qualification corpus

The WARP qualification executes 128 chained invocations and therefore 384 Candidate executions per build configuration.

Required Active counts:

```text
0, 1, 32, 63, 64, 65, 256, 1024, 2048, 4095, 4096
```

Required Transition counts:

```text
0, 1, 8, 31, 32, 63, 64, 65, 256, 1024, 4096
```

Required transition kinds:

```text
Hold
DirtyOnly
ActivateOnly
DeactivateOnly
BalancedChurn
PatternMigration
FullInvalidation
EmptyReset
```

After every invocation:

- all active records equal the exact current-generation oracle,
- all inactive records equal the canonical zero sentinel,
- B/C do not write retained `H_t`,
- observed writes equal each Candidate's verified legal write set,
- A/B/C full 4096-record outputs are byte-identical.

## Runtime materialization boundary

A submission requires, in order:

```text
current Device-epoch handle
    -> explicit timeline bind
    -> explicit A/B/C representation rebuild
    -> verified WARP submission
    -> Device-epoch-bound History and Completion handles
```

The gate rejects forged current-epoch representation, History and Completion identities.

## Architecture result

A successful CU5 gate establishes:

```text
SGE4-5 SPIRAL 7 ARCHITECTURE COMPLETE
```

This does not establish an empirical winner. Real-GPU measurement and Owner decision evidence remain CU6 responsibilities.

## Execution optimization V2

The accepted Architecture proof is executed through reusable fixed-capacity WARP resources. A/B/C remain three independent `ExecuteIndirect` dispatches, but all three are recorded in one CommandList and completed by one Fence wait per invocation. The executor still reads back and validates all three full outputs and all three Write-Audit buffers. Evidence serialization is unchanged. See `CU5_EXECUTION_OPTIMIZATION_V2.md`.
