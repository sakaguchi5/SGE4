# Spiral 7 progress

- Owner selection: `Versioned Sparse Delta Flow and Verified Incremental History Lowering`.
- Baseline: `d0bb0d406fca8beabed2331daff870ea414dd87d`.
- Purpose: prove the final missing `Sparse x Temporal x Indirect` authority relation before Level 4 v2 Canonical reconstruction.
- CU1: PASSED — research contract, exact transition algebra, corpus, boundaries and verifier.
- CU2: PASSED — Sparse–Temporal Delta Semantic, minimal versioned sidecar, Compact Delta Index History Candidate, 14-case WARP architecture and deterministic evidence. Accepted commit: `87ae8946f403841199b2d18714348a11c007b65d`. Evidence: `50B4D4C848721442CC606EE8BFDA207832BA03E7B6FCD5B24F4EEC9A7FE3DDC3`.
- CU3: PASSED — Raw Candidate / Planner / independent Verifier / opaque Verified plan / Frozen resource and Device-epoch binding, 50 mutation attacks and replay gates. Accepted commit: `21e7e91e1f85bb5620fca50e9def76b7c9a76291`. Owner-run evidence digest was not supplied in this conversation.
- CU4: PASSED — A/B/C incremental-history Candidate Family, Block-mask reconstruction, 18 chained invocations, 54 WARP executions and pairwise full-output equivalence. Accepted commit: `8f3b17e25d2a500beb2b658e6bc0a1d3f646ec26`. Owner-run evidence digest was not supplied in this conversation.
- CU5: implementation supplied — Frozen 128-invocation Runtime authority, complete WARP Architecture Qualification, Controlled/actual Recovery, stale-handle rejection, explicit `A_t/M_t` rebind, exact-generation full-active seed and fresh-process determinism.
- CU6: pending.

Successful CU2 execution establishes:

```text
A_(t-1), A_t, M_t -> W_t, H_t, R_t, T_t
CompactDeltaIndexHistoryReuse
WriteSet = exactly T_t
RetainWritePolicy = Forbidden
```

Successful CU3 execution establishes:

```text
Raw Candidate is untrusted
Planner cannot mint Verified values
Verifier independently reconstructs Delta authority
Frozen execution binds Delta + previous History + output History + Device epoch
RuntimePolicyAuthorization = None
```

Successful CU4 execution establishes:

```text
A = FullActiveDenseRecompute
B = CompactDeltaIndexHistoryReuse
C = AffectedBlockDeltaHistoryReuse
Block Update/Clear masks reconstruct exactly W_t/R_t
A/B/C full outputs are byte-identical after every invocation
RuntimeCandidatePolicyAuthorization = None
```

Successful CU5 execution will establish:

```text
FrozenAuthority = exact 128-invocation A/B/C timeline
CompleteQualification = 128 invocations / 384 Candidate executions
OldDeviceEpochHistory = Rejected
ExternalRecoveryInputs = explicit A_t and M_t rebind
FirstPostRecoverySemanticInvocation = W_t=A_t and H_t=empty
RecoveryMaterialization = exact per-item generations, no stale History retention
ArchitectureStatus = Complete
RuntimeCandidatePolicyAuthorization = None
```

No CU5 result authorizes a universal Candidate winner. CU6 remains observational measurement and decision evidence.
