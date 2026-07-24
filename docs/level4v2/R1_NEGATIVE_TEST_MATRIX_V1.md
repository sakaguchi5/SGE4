# Level 4 v2 R1 negative-test matrix V1

| Boundary | Test | Required result |
|---|---|---|
| Dynamic dimensions | Brace construction of one count type from another | Ill-formed expression detected at compile time |
| Identities | Construction of one identity type from another | Ill-formed expression detected at compile time |
| Raw/Verified | Construct or convert `RawCandidateProposalV1` to `OpaqueVerifiedPlanV1` | Compile-time unavailable |
| Verified opacity | Default-construct `OpaqueVerifiedPlanV1` | Compile-time unavailable |
| Handle kinds | Treat Representation and History handles as the same type | Compile-time false |
| Device epoch | Construct epoch from zero | Rejected |
| Public dependencies | Include platform or graphics API headers from Canonical vocabulary | Static verifier rejection |
| Invocation ownership | Add Candidate choice or application policy to Invocation record | Static verifier rejection |
| Generic limits | Add qualification-profile literals as universal limits | Static verifier rejection |
| Identity inputs | Add process path, timestamp, random identifier or live handle to canonical identity material | Static verifier rejection |
| Determinism | Emit vocabulary evidence in Debug and Release | Files must be byte-identical |

The compile-time tests use `requires` and type traits so the build succeeds only when the prohibited expressions remain ill-formed.
