# Spiral 7 progress

- Owner selection: `Versioned Sparse Delta Flow and Verified Incremental History Lowering`.
- Baseline: `d0bb0d406fca8beabed2331daff870ea414dd87d`.
- CU1: PASSED — research contract and exact transition algebra.
- CU2: PASSED — Sparse–Temporal Delta Semantic and Compact Delta History. Accepted commit: `87ae8946f403841199b2d18714348a11c007b65d`. Accepted Architecture evidence: `50B4D4C848721442CC606EE8BFDA207832BA03E7B6FCD5B24F4EEC9A7FE3DDC3`.
- CU3: PASSED — independent Planner/Verifier authority. Accepted commit: `21e7e91e1f85bb5620fca50e9def76b7c9a76291`.
- CU4: PASSED — A/B/C Candidate Family and byte-equivalent chained execution. Accepted commit: `8f3b17e25d2a500beb2b658e6bc0a1d3f646ec26`.
- CU5: PASSED — complete Runtime authority, Recovery and deterministic evidence. Accepted final commit: `c9f0b5a62e2a7f3c9e0355cdaa1c683819c6dcfa`. Accepted exhaustive-audit base commit: `67cb40b5204e1e06ecac576206ba969ec2db02b6`.
- CU6-1: Owner real-GPU measurement completed at commit `903e00be0f7e39ffd1758004c117fbc0233b0164`. It established the raw 160-case surface and exposed that median-only winner sets can disagree with paired-order decisions.
- CU6-2: implementation supplied — paired decision authority, T=0 zero-dispatch equivalence, unresolved-case exclusion and focused high-Transition refinement. Owner run pending.

CU5 accepted evidence:

```text
Architecture        1F1D09B4E52DBC35E961E0D3751B32292B2C30EEDCFA473800C5BB8E8CB5AB73
Controlled Recovery 7F0247B193F9BC20F4EDA5FEED590C1DA5722EF36B133640292B1ED616CFF62B
Fresh process       091EAEC0D27287FE897F6813043FD75C090D5DA17054461DF314F99B2A6F5A92
```

CU6-2 measurement:

```text
CanonicalSurface         = 4 x 5 x 8 = 160 cases/run
HighTransitionRefinement = 4 x 5 x 5 = 100 cases/run
RefinementTransitions    = 1024,1536,2048,3072,4096
DecisionAuthority        = paired agreement against both alternatives
T=0                      = B/C ZeroDispatchEquivalent, no crossover authority
MedianRanking            = descriptive only
```

Successful CU6-2 may declare `SGE4-5 SPIRAL 7 EXPERIMENT COMPLETE`. Runtime Candidate policy remains `None`; `SPIRAL 7 CLOSED` remains Owner-gated.
