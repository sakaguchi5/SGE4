# Spiral 6 proof ledger V1

| ID | Claim | Current proof boundary |
|---|---|---|
| S6-P01 | Canonical set bytes denote exactly one subset of the 4096-index universe. | CU2 PASSED: strict order, uniqueness, range, deterministic identity and parser rejection |
| S6-P02 | Dense mask, compact list and block-list masks reconstruct the same set. | CU4 supplied: canonical fixed-capacity artifacts are independently derived from one Exact Sparse Set |
| S6-P03 | Candidate Dispatch dimensions are fully derived from Verified set authority. | CU3: Planner-independent Verifier independently rederives `ceil(K/64)` and binds it into the opaque Plan |
| S6-P04 | Candidate output write set is exactly `S`. | CU4 supplied: all A/B/C WARP paths require exactly `K` active writes and byte-stable inactive sentinel |
| S6-P05 | Inactive output bytes remain the canonical sentinel. | CU2 single Compact List WARP proof; complete corpus planned CU5 |
| S6-P06 | Full output Buffers are pairwise byte-identical across A/B/C. | CU4 supplied: all 65536 Output bytes must match in all twelve cases |
| S6-P07 | Planner cannot mint Verified or Frozen types. | CU3 compile-time boundary: opaque non-default-constructible Verified/Frozen types; Raw conversion unavailable |
| S6-P08 | Cross-set, cross-role, cross-Candidate and cross-epoch seals are rejected. | CU4 supplied: 40 family mutations plus set, Resource, role, layout and epoch replay gates |
| S6-P09 | Recovery invalidates derived sparse Resources and requires explicit rebuild. | Planned CU5 controlled/actual Recovery |
| S6-P10 | Architecture evidence is deterministic across fresh Debug A/B/Release. | CU2 Debug/Release single architecture; CU3 Debug/Release authority evidence; full fresh-process proof planned CU5 |
| S6-P11 | Real-GPU evidence separates representation and sparse execution costs. | Planned CU6 |
| S6-P12 | Decision evidence does not authorize Runtime policy. | Runtime policy remains `None`; formal Decision Report planned CU6 |
