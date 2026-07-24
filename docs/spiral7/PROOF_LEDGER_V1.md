# Spiral 7 proof ledger V1

| Invariant | Required proof | Current status |
|---|---|---|
| S7-I01 | `A_t` is a canonical exact subset of the 4096-record universe. | CU2 passed |
| S7-I02 | `M_t` is canonical and is a subset of `A_t intersect A_(t-1)`. | CU2 passed |
| S7-I03 | `N_t = A_t \ A_(t-1)` and `R_t = A_(t-1) \ A_t`. | CU2 passed |
| S7-I04 | `W_t = N_t union M_t`; newly activated members cannot be retained. | CU2 passed |
| S7-I05 | `H_t = (A_t intersect A_(t-1)) \ M_t`. | CU2 passed |
| S7-I06 | `W_t`, `H_t` and `R_t` are pairwise disjoint. | CU2 passed |
| S7-I07 | The first invocation updates all active members and retains no history. | CU2 passed |
| S7-I08 | Active final bytes equal the current per-item generation transform oracle. | CU2 and complete CU5 corpus passed |
| S7-I09 | Inactive final bytes equal the canonical zero sentinel. | CU2 and complete CU5 corpus passed |
| S7-I10 | Retained members receive no GPU write and preserve valid previous bytes. | CU2 and A/B/C CU5 corpus passed |
| S7-I11 | Compact transition records denote exactly `T_t=W_t union R_t`. | CU2 passed |
| S7-I12 | Block update/clear masks are disjoint and reconstruct exactly `W_t/R_t`. | CU4 passed |
| S7-I13 | A/B/C full outputs are byte-identical after every invocation. | CU4 and 128-invocation CU5 qualification passed |
| S7-I14 | Raw Candidate cannot become Verified without the independent Verifier. | CU3 passed |
| S7-I15 | Planner and Verifier independently derive transition identities. | CU3 passed |
| S7-I16 | Replay is rejected across Semantic, transition, Candidate, resource, generation, Target and Device epoch identities. | CU3/CU4 and CU5 Runtime-handle gates passed |
| S7-I17 | Runtime and Backend cannot choose membership, action, Candidate or history policy. | CU1–CU6 passed; retained after closure |
| S7-I18 | Device epoch change rejects stale history and forces a full rebuild of `A_t`. | CU5 passed |
| S7-I19 | Recovery requires explicit external set rebind and deterministic representation rebuild. | CU5 passed with exact `A_t/M_t` and generation reconstruction |
| S7-I20 | Debug/Release and fresh-process Frozen evidence are deterministic. | CU5 exhaustive audit passed; routine gate enforces accepted SHA-256 values |
| S7-I21 | Real-GPU evidence is observational and does not authorize a universal winner. | CU6-2 Owner real-GPU evidence accepted |
| S7-I22 | Spiral closure and next program remain Owner-gated. | Owner closed Spiral 7 and selected Level 4 v2 Canonical reconstruction |
| S7-I23 | Every frozen `(ActiveCount,TransitionCount)` measurement coordinate is represented by an exact legal transition without silent clipping. | CU6 passed |
| S7-I24 | Candidate timing uses all six A/B/C orders and block-local paired controls. | CU6 passed |
| S7-I25 | Decision evidence exposes Transition, Active and Pattern-dependent winner surfaces rather than one scalar winner. | CU6 passed |
| S7-I26 | Median ranking is descriptive only and cannot authorize a winner or crossover. | CU6-2 passed |
| S7-I27 | A StableWinner requires paired victory against both alternative Candidates. | CU6-2 passed |
| S7-I28 | `T=0` is B/C ZeroDispatchEquivalent and is excluded from crossover analysis. | CU6-2 passed |
| S7-I29 | Unresolved coordinates are excluded from crossover; reported transitions are resolved-coordinate brackets. | CU6-2 passed |
| S7-I30 | High-Transition refinement binds the same Adapter, Semantic, verification context, Candidate order and Shader digests as the canonical pass. | CU6-2 passed |

Accepted CU5 exhaustive-audit base commit: `67cb40b5204e1e06ecac576206ba969ec2db02b6`.
Accepted CU5 final commit: `c9f0b5a62e2a7f3c9e0355cdaa1c683819c6dcfa`.
CU6-1 base commit: `903e00be0f7e39ffd1758004c117fbc0233b0164`.
Accepted Spiral 7 final commit: `f802c12b162569a869c214da22b80142b3a4a0dd`.

Spiral 7 is closed. Its code, manifests, evidence ledgers and verifiers remain a frozen reference implementation. Level 4 v2 reconstruction may reorganize implementation, but it must reproduce or deliberately supersede every invariant in this ledger before the Spiral 7 projects can be physically retired.
