# Spiral 3 proof ledger V1

This ledger is monotonic. CU0/CU1 are implemented here; later invariants remain planned.

| Invariant | Status | Evidence |
|---|---|---|
| S3-I01 | Implemented in CU1 | `ReuseSweepSemanticV1` retains hierarchy, Motor meaning, point corpus and observation identity, with no Matrix or Backend concept. |
| S3-I02 | Implemented in CU1 | R1/R4/R16/R64/R256/R512, explicit Bone/Reuse/Point types, zero and over-bound rejection, distinct deterministic Semantic identities. |
| S3-I03-S3-I15 | Planned | Candidate, verifier, Leaf, Composition, WARP, recovery and measurement units are not included yet. |
| S3-I16 | Frozen in CU0 | Next capability remains Owner-deferred. |
