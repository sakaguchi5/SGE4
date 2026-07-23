# CU6-2 Changeset

Base commit: `903e00be0f7e39ffd1758004c117fbc0233b0164`.

Changed:

- Evidence schema `S7M2` -> `S7M3`, adding the measurement-pass identity.
- Replaced median-only winner authority with paired-authority classification.
- Added `B/C ZeroDispatchEquivalent` for T=0 and removed T=0 from crossover analysis.
- Excluded unresolved cases from crossover analysis.
- Added the high-Transition refinement schedule `{1024,1536,2048,3072,4096}`.
- Added canonical/refinement Evidence merge with refinement override at duplicate coordinates.
- Added combined paired Decision CSV/report.
- Updated runner, verifier, manifest, proof/progress and operation documents.

Unchanged:

- A/B/C Candidate meaning and order.
- CU5 Architecture, Runtime and Recovery evidence.
- Validation and timing Shaders.
- D3D12 Timestamp/ExecuteIndirect measurement mechanism.
- Legacy Schema 17, Runtime 17, Backend and Composition.
- Runtime Candidate-policy authorization (`None`).

Deleted files: none.
Git operations: none.
