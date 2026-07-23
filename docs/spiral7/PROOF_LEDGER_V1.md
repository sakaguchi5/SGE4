# Spiral 7 proof ledger V1

| Invariant | Required proof |
|---|---|
| S7-I01 | `A_t` is a canonical exact subset of the 4096-record universe. |
| S7-I02 | `M_t` is canonical and is a subset of `A_t intersect A_(t-1)`. |
| S7-I03 | `N_t = A_t \ A_(t-1)` and `R_t = A_(t-1) \ A_t`. |
| S7-I04 | `W_t = N_t union M_t`; no newly activated member may be retained. |
| S7-I05 | `H_t = (A_t intersect A_(t-1)) \ M_t`. |
| S7-I06 | `W_t`, `H_t` and `R_t` are pairwise disjoint. |
| S7-I07 | At the first invocation, all active members update and no history is retained. |
| S7-I08 | Active final bytes equal the current per-item generation transform oracle. |
| S7-I09 | Inactive final bytes equal the canonical zero sentinel. |
| S7-I10 | Retained members receive no GPU write and remain byte-identical to valid previous history. |
| S7-I11 | Compact transition records denote exactly `T_t=W_t union R_t`. |
| S7-I12 | Block update and clear masks are disjoint and reconstruct exactly `W_t` and `R_t`. |
| S7-I13 | A/B/C full outputs are byte-identical after every invocation. |
| S7-I14 | Raw Candidate cannot be converted to Verified Candidate without the independent Verifier. |
| S7-I15 | Planner and Verifier independently derive transition identities. |
| S7-I16 | Replay is rejected across Active, Modified, Transition, Candidate, Resource, generation, Target and Device epoch identities. |
| S7-I17 | Runtime and Backend cannot choose membership, action, Candidate or history policy. |
| S7-I18 | Device epoch change rejects stale history and forces a full rebuild of `A_t`. |
| S7-I19 | Recovery requires explicit external set rebind and deterministic derived-representation rebuild. |
| S7-I20 | Debug/Release and fresh-process Frozen evidence are deterministic within the qualified boundary. |
| S7-I21 | Real-GPU evidence is observational and does not authorize a universal winner. |
| S7-I22 | Spiral closure and next capability selection remain Owner-gated. |

CU1 freezes these proof obligations. It proves only that the research contract, prior-capability inventory and absence boundary are internally consistent.
