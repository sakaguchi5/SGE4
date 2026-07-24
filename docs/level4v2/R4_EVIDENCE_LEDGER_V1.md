# R4 Evidence ledger V1

| Evidence | Required result |
|---|---|
| Project dependency boundary | Verifier has no Planner dependency; Frozen builder has no Planner dependency |
| Opaque construction boundary | Raw request and Planner proposal cannot construct Verified/Frozen authority |
| Dynamic scenarios | 8 accepted |
| Mutation and replay matrix | 27 rejected |
| Exact-set canonicalization | declaration order does not change set identity |
| Set algebra audit | `W_t ∪ H_t = A_t`, `T_t = W_t ∪ R_t`, and the represented partitions are disjoint |
| Retain/write audit | Retain ∩ Transition is empty |
| Indirect quantity | equals exact Transition count |
| Initial seed | all current Active members updated |
| Recovery seed | all current Active members updated at the new epoch |
| Per-item generation | activation starts at zero, modified survivors advance, retained members remain unchanged, inactive members use the invalid sentinel |
| History replay | wrong Composition, Semantic or epoch rejected |
| Determinism | Debug and Release Evidence byte-identical |
| Expected Evidence SHA-256 | `78f16d2fa4185412144205dc54f842f2929e6e269711dfca9ced9967f37cfbe1` |

Windows MSVC execution is the Owner acceptance gate. The package itself does not claim that gate has already run.
