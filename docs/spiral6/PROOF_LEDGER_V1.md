# Spiral 6 proof ledger V1

| ID | Claim | Current proof boundary |
|---|---|---|
| S6-P01 | Canonical set bytes denote exactly one subset of the 4096-index universe. | CU2 PASSED: strict order, uniqueness, range, deterministic identity and parser rejection |
| S6-P02 | Dense mask, compact list and block-list masks reconstruct the same set. | CU4 PASSED: canonical fixed-capacity artifacts are independently derived from one Exact Sparse Set |
| S6-P03 | Candidate Dispatch dimensions are fully derived from Verified set authority. | CU3: Planner-independent Verifier independently rederives `ceil(K/64)` and binds it into the opaque Plan |
| S6-P04 | Candidate output write set is exactly `S`. | CU4 PASSED for twelve family cases; CU5 supplied for the complete 84-set corpus |
| S6-P05 | Inactive output bytes remain the canonical sentinel. | CU5 supplied: all 84 exact sets require byte-identical inactive Float4Zero records |
| S6-P06 | Full output Buffers are pairwise byte-identical across A/B/C. | CU4 PASSED for twelve cases; CU5 supplied for all 84 exact sets |
| S6-P07 | Planner cannot mint Verified or Frozen types. | CU3 compile-time boundary: opaque non-default-constructible Verified/Frozen types; Raw conversion unavailable |
| S6-P08 | Cross-set, cross-role, cross-Candidate and cross-epoch seals are rejected. | CU4 PASSED: 40 family mutations plus set, Resource, role, layout and epoch replay gates |
| S6-P09 | Recovery invalidates derived sparse Resources and requires explicit rebuild. | CU5 supplied: controlled epoch advance, stale handle rejection, explicit rebind/rebuild and actual RemoveDevice quarantine |
| S6-P10 | Architecture evidence is deterministic across fresh Debug A/B/Release. | CU5 supplied: Architecture, Controlled Recovery and Fresh Rematerialization evidence require Debug A/Debug B/Release byte identity |
| S6-P11 | Real-GPU evidence separates representation and sparse execution costs. | Planned CU6 |
| S6-P12 | Decision evidence does not authorize Runtime policy. | Runtime policy remains `None`; formal Decision Report planned CU6 |

## CU6 — Real-GPU Sparse measurement and Decision Evidence

- [implementation supplied] all four Patterns and eleven measurement cardinalities are represented.
- [implementation supplied] all six A/B/C orders are position-balanced.
- [implementation supplied] GPU Candidate Path separates sentinel, argument, Sparse Consumer and completion phases.
- [implementation supplied] construction/upload and observation costs remain separate.
- [implementation supplied] binary evidence round-trip and corruption rejection are deterministic.
- [implementation supplied] Decision Report classifies A/B and B/C per Pattern and tests same-K Pattern sensitivity.
- [retained] RuntimePolicyAuthorization = None.
- [retained] Spiral closure remains Owner-gated.
