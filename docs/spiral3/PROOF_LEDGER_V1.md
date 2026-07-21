# Spiral 3 proof ledger V1

This ledger is monotonic. CU0-CU3 are implemented here; later invariants remain planned.

| Invariant | Status | Evidence |
|---|---|---|
| S3-I01 | Qualified in CU1 | `ReuseSweepSemanticV1` retains hierarchy, Motor meaning, point corpus and observation identity, with no Matrix or Backend concept. |
| S3-I02 | Qualified in CU1 | R1/R4/R16/R64/R256/R512, explicit Bone/Reuse/Point types, zero and over-bound rejection, distinct deterministic Semantic identities. |
| S3-I03 | Implemented in CU2 | Six reuse workloads each generate Matrix-before, Direct-PGA and Matrix-after plans: 18 verified graph-level candidates. |
| S3-I04 | Implemented in CU2 | Raw Candidate is non-executable and cannot construct or implicitly convert to `VerifiedReuseRepresentationV1`; only the verifier can issue the sealed type. |
| S3-I05 | Implemented in CU2 | Verifier has no Planner dependency, independently rederives operations, edges, materialization position, strides, counts, dispatch and Program identities, and rejects 24 mutations. |
| S3-I06 | Implemented in CU3 | Only verifier-sealed plans enter the Leaf Compiler; 18 A/B/C groups lower into 60 Package-bound Schema 17 Leaves and 80 authority mutations are rejected. |
| S3-I07-S3-I15 | Planned | Composition, WARP, recovery and measurement units are not included yet. |
| S3-I16 | Frozen in CU0 | Next capability remains Owner-deferred. |
