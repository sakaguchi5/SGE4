# Spiral 1 CU2 Evidence Ledger

The Stage 03 Proof Ledger remains unchanged. This file accumulates CU2 implementation evidence.

| Invariant | CU2 evidence |
|---|---|
| S1-I02 | Stage 06 produces Matrix and Direct PGA candidates with one Semantic identity and distinct Candidate identities. |
| S1-I03 | Raw Candidate has no Freeze/execute conversion; Verified Plan constructor is private and Verifier-only. |
| S1-I04 | Verifier independently rederives all 12 Matrix binary32 coefficients and payload bytes. |
| S1-I05 | Verifier independently rederives qr/qd binary32 coefficients and payload bytes. |
| S1-I06 | Template/source/compile recipe/binary/reflection/binding digests are checked against the versioned catalog and target profile. |
| S1-I07 | Candidate and Verifier require identical Float4Point input/output schemas and Observation Contract V1. |

Final qualification evidence is produced by `run_sge4_5_cu2_representation_authority.bat` with Debug A, Debug B and Release byte comparison.
