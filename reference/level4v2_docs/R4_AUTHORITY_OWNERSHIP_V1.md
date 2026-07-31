# R4 authority ownership V1

| Fact | Sole owner | Other layers may do |
|---|---|---|
| Current Active membership | Explicit Invocation input | Reference its identity |
| Modified-survivor membership | Explicit Invocation input | Verify subset relations |
| Previous valid history | Verified History state | Reject replay or stale epoch |
| Activation/Deactivation/Update/Retain/Transition | Independent Dynamic Verifier | Planner may propose |
| Indirect work quantity | Verified Dynamic decision | Frozen form may retain it |
| Incremental write set | Verified Dynamic decision | Runtime may later execute exactly it |
| Next History validity and generation | Verified-only Frozen builder | Future Runtime may retain or invalidate it |
| Composition graph/schedule/allocation/synchronization | R3 Frozen Composition | R4 may only reference its identity |

Single-fact rule: R4 does not copy or reinterpret R3 graph decisions. It binds the exact dynamic decision to the already Frozen Composition identity.
