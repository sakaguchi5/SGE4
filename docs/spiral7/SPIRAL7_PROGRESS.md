# Spiral 7 progress

- Owner selection: `Versioned Sparse Delta Flow and Verified Incremental History Lowering`.
- Baseline: `d0bb0d406fca8beabed2331daff870ea414dd87d`.
- Purpose: prove the final missing `Sparse x Temporal x Indirect` authority relation before Level 4 v2 Canonical reconstruction.
- CU1: PASSED — research contract, exact transition algebra, corpus, boundaries and verifier.
- CU2: PASSED — Sparse–Temporal Delta Semantic, minimal sidecar, Compact Delta Index History and 14-case WARP Architecture. Accepted commit: `87ae8946f403841199b2d18714348a11c007b65d`. Evidence: `50B4D4C848721442CC606EE8BFDA207832BA03E7B6FCD5B24F4EEC9A7FE3DDC3`.
- CU3: PASSED — Raw Candidate / Planner / independent Verifier / opaque Verified plan / resource and Device-epoch binding. Accepted commit: `21e7e91e1f85bb5620fca50e9def76b7c9a76291`.
- CU4: PASSED — A/B/C Candidate Family, Block-mask reconstruction, 18 chained invocations and pairwise full-output equivalence. Accepted commit: `8f3b17e25d2a500beb2b658e6bc0a1d3f646ec26`.
- CU5: PASSED — Frozen 128-invocation Runtime authority, 384-Candidate WARP qualification, Controlled and actual Recovery, stale-handle rejection, explicit `A_t/M_t` rebind, exact-generation full-active seed, fresh-process rematerialization and Debug/Release evidence identity. Accepted exhaustive-audit base commit: `67cb40b5204e1e06ecac576206ba969ec2db02b6`.
- CU6: pending.

CU5 accepted evidence:

```text
Architecture        1F1D09B4E52DBC35E961E0D3751B32292B2C30EEDCFA473800C5BB8E8CB5AB73
Controlled Recovery 7F0247B193F9BC20F4EDA5FEED590C1DA5722EF36B133640292B1ED616CFF62B
Fresh process       091EAEC0D27287FE897F6813043FD75C090D5DA17054461DF314F99B2A6F5A92
```

The ordinary CU5 gate is now a fast regression against these frozen evidence identities. The complete Debug/Release path remains available through the explicit exhaustive audit runner.

Successful CU5 establishes:

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
