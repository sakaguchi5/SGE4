# Spiral 7 CU4 — Incremental History Candidate Family

CU4 introduces three physical lowerings for the same verified invocation meaning.

| ID | Candidate | Representation | Dispatch | Legal write set |
|---|---|---|---|---|
| A | `FullActiveDenseRecompute` | 4096-bit Active mask, 512 bytes | fixed 64 groups | entire 4096-record universe |
| B | `CompactDeltaIndexHistoryReuse` | sorted `uint2(index, action)`, 32768 bytes | `ceil(|T_t|/64)` | exactly `T_t` |
| C | `AffectedBlockDeltaHistoryReuse` | 64 fixed block records, 24-byte stride, 1536 bytes | one group per affected block | exactly `T_t` |

Candidate C stores disjoint 64-bit Update and Clear masks. Canonical validation reconstructs the exact `W_t` and `R_t` sets from those masks and rejects overlap or a non-canonical unused tail.

The three Candidates are executed from the same previous GPU History for every invocation. Candidate A's output becomes the next invocation's shared History only after A/B/C outputs are byte-identical. CU4 therefore proves semantic equivalence without selecting a performance winner.

Runtime Candidate-policy authorization remains `None`.
