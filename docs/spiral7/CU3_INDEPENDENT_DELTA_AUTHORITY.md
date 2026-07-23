# Spiral 7 CU3 — Independent Delta Authority

CU3 changes the CU2 Compact Delta path from a directly constructed valid artifact into an authority-separated path.

```text
Sparse–Temporal Semantic + exact Invocation
                |
                v
RawDeltaLoweringCandidateV1          (untrusted proposal)
                |
                v
Planner-independent Delta Verifier   (rebuilds expected plan)
                |
                v
Opaque VerifiedDeltaLoweringV1       (no public constructor)
                |
                v
FrozenVerifiedCompactDeltaExecutionV1
```

The Planner may propose a Candidate, but it cannot create a Verified value. The Verifier reconstructs the canonical Compact Delta artifact, resource identities, transition authority and all counts from the supplied Semantic, Invocation and independent Context.

The Verified seal binds:

- `A_(t-1)`, `A_t`, `M_t`, `T_t` and canonical transition records,
- Update, Clear and Retain counts,
- Spiral 4 Indirect, Spiral 5 Temporal and Spiral 6 Sparse identities,
- target, observation, resource contracts and completion policy,
- Compact Delta, previous History and output History logical resource identities.

Freezing additionally binds a nonzero Device epoch and actual resource roles. Previous History is ReadOnly, output History is WriteOnly, and their identities must be distinct.

CU3 does not authorize Runtime Candidate selection. It proves only that the one CU2 Candidate cannot execute through the verified entry point without independent reconstruction and exact resource/epoch binding.
