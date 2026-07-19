# SGE4-5 Spiral 1 Completion Unit 2

- Foundation commit: `583337df754c27c3c828783c260763a82dab844d`
- Scope: Stage 06 Raw Representation Candidates + Stage 07 Independent Representation Authority

## Implemented projects

```text
63_D3D12RepresentationCandidate
64_D3D12RepresentationPlanner
65_D3D12RepresentationVerifier
71_Spiral1RepresentationPlanningTests
72_Spiral1RepresentationAuthorityTests
```

The same canonical PGA Semantic produces `MatrixExpandedComputeV1` and `DirectPgaMotorComputeV1` Raw Candidates. Candidate identity binds semantic, target profile, canonical template, program binary evidence, I/O schema, observation contract, dispatch and constant payload.

The Verifier has no ProjectReference to the Planner. It independently rederives the 64-byte Matrix constants or 48-byte Direct PGA constants, verifies the versioned template allowlist, and alone creates the opaque `VerifiedRepresentationPlanV1` seal.

No Frozen Leaf, Package writer, Runtime execution, Composition or WARP implementation is added in CU2.
