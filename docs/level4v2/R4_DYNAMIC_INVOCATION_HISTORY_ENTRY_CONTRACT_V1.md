# Level 4 v2 R4 Dynamic invocation and history entry contract V1

## Entry condition

R4 may begin only after R3 valid-graph, contract/proposal rejection and deterministic Evidence gates pass.

## Responsibility

R4 integrates the already proven dynamic dimensions with the R2 authority chain and R3 verified composition identities:

```text
explicit Invocation inputs
verified indirect quantity
history validity and generation
exact sparse membership
versioned activation/deactivation/update/retain/transition sets
```

## Required boundaries

- Dynamic input values remain explicit and strongly typed.
- Membership and transition algebra are independently reconstructed.
- History validity is bound to Semantic generation and Device epoch.
- Incremental write authority is exact; retained members are not writable.
- R4 may reference R3 Verified/Frozen Composition identities but may not rewrite flow, schedule, allocation or synchronization authority.
- No Runtime Candidate policy, native execution or Recovery behavior is introduced in R4.

## Non-goals

R4 does not add Texture flows, graph cycles, conditionals, streaming, residency, partial Recovery or multi-device execution.
