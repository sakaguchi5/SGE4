# Spiral 5 CU4 — Temporal Candidate Family

CU4 places three physical history-materialization choices under one unchanged `LastUpdateWinsPiecewiseConstant` Semantic.

| Candidate | Update | Hold | Retained state |
|---|---|---|---|
| A.EveryInvocationRecompute | Hierarchy + Consumer | Hierarchy + Consumer | none |
| B.GlobalMotorHistoryReuse | Hierarchy + Consumer | Consumer only | eight Global PGA Motors |
| C.FinalOutputHistoryReuse | Hierarchy + Consumer | neither | 4096 final output records |

The schedule, Source Generation, Local Motor bytes, point corpus, output schema and Observation Contract are identical. Candidate identity changes only the physical issuance and the location at which exact state is retained.

A has no Temporal History Resource authority. B binds a 256-byte Global Motor History. C binds a 65536-byte Final Output History. Absence of a retained resource in A is itself identity-bound and cannot be replayed as B or C.

B and C reuse the Spiral 4 verified Dispatch command-signature identity. A remains direct. Runtime and Backend do not select among A/B/C and do not inspect the Update interval to change the physical route.

CU4 qualification covers P1, P4, P64 and Irregular. At every Invocation all three candidates must match the independent CPU reference and each other byte-for-byte. Hold stability is checked at the candidate-specific retained boundary.
