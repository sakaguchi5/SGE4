# Spiral 6 proof ledger V1

| ID | Claim | Current proof boundary |
|---|---|---|
| S6-P01 | Canonical set bytes denote exactly one subset of the 4096-index universe. | CU2 PASSED boundary: strict order, uniqueness, range, deterministic identity and parser rejection |
| S6-P02 | Dense mask, compact list and block-list masks reconstruct the same set. | Planned CU3 independent Verifier |
| S6-P03 | Candidate Dispatch dimensions are fully derived from Verified set authority. | CU2 canonical Compact List derivation; full independent authority planned CU3 |
| S6-P04 | Candidate output write set is exactly `S`. | CU2 single Compact List WARP proof; Candidate-family proof planned CU4 |
| S6-P05 | Inactive output bytes remain the canonical sentinel. | CU2 single Compact List WARP proof; complete corpus planned CU5 |
| S6-P06 | Full output Buffers are pairwise byte-identical across A/B/C. | Planned CU4 Candidate family |
| S6-P07 | Planner cannot mint Verified or Frozen types. | Planned CU3 compile-time boundary |
| S6-P08 | Cross-set, cross-role, cross-Candidate and cross-epoch seals are rejected. | CU2 cross-set artifact rejection; full seal boundary planned CU3/CU5 |
| S6-P09 | Recovery invalidates derived sparse Resources and requires explicit rebuild. | Planned CU5 controlled/actual Recovery |
| S6-P10 | Architecture evidence is deterministic across fresh Debug A/B/Release. | CU2 Debug/Release single-architecture evidence; full fresh-process proof planned CU5 |
| S6-P11 | Real-GPU evidence separates representation and sparse execution costs. | Planned CU6 |
| S6-P12 | Decision evidence does not authorize Runtime policy. | Planned CU6 report verifier |
