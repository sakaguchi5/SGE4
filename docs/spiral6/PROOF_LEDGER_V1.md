# Spiral 6 proof ledger V1

| ID | Claim | Planned proof boundary |
|---|---|---|
| S6-P01 | Canonical set bytes denote exactly one subset of the 4096-index universe. | CU2 semantic and parser tests |
| S6-P02 | Dense mask, compact list and block-list masks reconstruct the same set. | CU3 independent Verifier |
| S6-P03 | Candidate Dispatch dimensions are fully derived from Verified set authority. | CU3 mutation gates |
| S6-P04 | Candidate output write set is exactly `S`. | CU4 WARP observation |
| S6-P05 | Inactive output bytes remain the canonical sentinel. | CU4/CU5 observation |
| S6-P06 | Full output Buffers are pairwise byte-identical across A/B/C. | CU4 Candidate family |
| S6-P07 | Planner cannot mint Verified or Frozen types. | CU3 compile-time boundary |
| S6-P08 | Cross-set, cross-role, cross-Candidate and cross-epoch seals are rejected. | CU3/CU5 negative gates |
| S6-P09 | Recovery invalidates derived sparse Resources and requires explicit rebuild. | CU5 controlled/actual Recovery |
| S6-P10 | Architecture evidence is deterministic across fresh Debug A/B/Release. | CU5 |
| S6-P11 | Real-GPU evidence separates representation and sparse execution costs. | CU6 |
| S6-P12 | Decision evidence does not authorize Runtime policy. | CU6 report verifier |
