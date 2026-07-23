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
| S7-I17 | Runtime and Backend cannot choose membership, action, Candidate or history policy. | CU1–CU6 boundary retained |
| S7-I18 | Device epoch change rejects stale history and forces a full rebuild of `A_t`. | CU5 passed |
| S7-I19 | Recovery requires explicit external set rebind and deterministic representation rebuild. | CU5 passed with exact `A_t/M_t` and generation reconstruction |
| S7-I20 | Debug/Release and fresh-process Frozen evidence are deterministic. | CU5 exhaustive audit passed; routine gate enforces accepted SHA-256 values |
| S7-I21 | Real-GPU evidence is observational and does not authorize a universal winner. | CU6 implementation supplied; Owner real-GPU run pending |
| S7-I22 | Spiral closure and next program remain Owner-gated. | Retained; CU6 cannot close Spiral 7 automatically |
| S7-I23 | Every frozen `(ActiveCount,TransitionCount)` measurement coordinate is represented by an exact legal transition without silent clipping. | CU6 canonical Hold/DirtyOnly/ReplaceAndClear constructor supplied |
| S7-I24 | Candidate timing uses all six A/B/C orders and block-local paired controls. | CU6 supplied |
| S7-I25 | Decision evidence exposes Transition, Active and Pattern-dependent winner surfaces rather than one scalar winner. | CU6 supplied |

Accepted CU5 commit: `c9f0b5a62e2a7f3c9e0355cdaa1c683819c6dcfa`.

CU5 establishes `SGE4-5 SPIRAL 7 ARCHITECTURE COMPLETE`. CU6 does not reopen that Architecture. It measures the fixed Candidate family on a real hardware Adapter, binds the output to the CU5 evidence identities, and produces an Owner-gated local relative Decision map. Successful execution may establish `SGE4-5 SPIRAL 7 EXPERIMENT COMPLETE`; `SPIRAL 7 CLOSED` remains a separate Owner action.
