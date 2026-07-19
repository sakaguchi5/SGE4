# Spiral 2 invariant ledger

This ledger is monotonic. Implementation may be redesigned, but a proven invariant must not be silently removed.

| ID | Invariant | Primary Stage | Minimum evidence | Initial status |
|---|---|---|---|---|
| S2-I01 | Canonical Hierarchy Semantic contains no Matrix, Queue, or Shader. | S2-03 | Semantic serialization and forbidden-field tests | NotStarted |
| S2-I02 | Bone count and topology are included in Frozen identity. | S2-03/04 | Identity mutation tests | NotStarted |
| S2-I03 | Per-frame Motor values are excluded from Frozen identity. | S2-04 | Palette-change artifact byte comparison | NotStarted |
| S2-I04 | Different palettes can be submitted to the same Frozen Composition. | S2-04/16 | Multi-frame submit test | NotStarted |
| S2-I05 | All three candidates read identical Motor input bytes. | S2-06/13 | Input digest equality | NotStarted |
| S2-I06 | No Candidate-A-only CPU Matrix Palette is generated. | S2-10 | Upload schema and source-path negative test | NotStarted |
| S2-I07 | One Semantic generates all three Candidate Graphs. | S2-06 | Candidate set test | NotStarted |
| S2-I08 | Each Candidate fixes Leaf count, Flow, and lowering position. | S2-06/07 | Verifier rederivation | NotStarted |
| S2-I09 | Raw Candidate Graph cannot Freeze or execute. | S2-07 | Compile-time/API and runtime rejection | NotStarted |
| S2-I10 | Verifier independently rederives hierarchy topology. | S2-07 | Planner isolation and topology mutations | NotStarted |
| S2-I11 | Verifier independently checks Matrix materialization position. | S2-07/08 | Boundary mutation tests | NotStarted |
| S2-I12 | All candidate Observation schemas are identical. | S2-06/13 | Schema identity equality | NotStarted |
| S2-I13 | Every internal Flow has exactly one writer. | S2-13 | Composition verifier | NotStarted |
| S2-I14 | Runtime does not redecide candidate, schedule, allocation, state, or sync. | S2-13/14 | Dependency and mutation authority tests | NotStarted |
| S2-I15 | Dynamic input changes do not cause recompilation. | S2-04/16 | Frozen byte identity across frames | NotStarted |
| S2-I16 | Topology or bone-count changes create a different Frozen identity. | S2-03/04 | Identity divergence tests | NotStarted |
| S2-I17 | All WARP corpus scenarios satisfy CPU Reference. | S2-15 | WARP corpus report | NotStarted |
| S2-I18 | All pairwise candidate differences satisfy the error contract. | S2-15 | Pairwise report | NotStarted |
| S2-I19 | The same Invocation produces the same readback in fresh processes. | S2-17 | Fresh-process readback digest | NotStarted |
| S2-I20 | Frozen artifacts are byte-identical across Debug, Release, and fresh processes. | S2-17 | Freeze manifests | NotStarted |
| S2-I21 | Recovery requires rebinding the dynamic Palette. | S2-18 | Old-binding rejection | NotStarted |
| S2-I22 | Stale-epoch Buffer, token, and Invocation are rejected. | S2-18 | Stale epoch mutation tests | NotStarted |
| S2-I23 | A fair three-candidate real-GPU report is generated. | S2-20 | M0-M5 six-order measurement evidence | NotStarted |
| S2-I24 | No next capability is added without Decision Evidence and Owner selection. | S2-21/22 | DeferredByOwner final gate | NotStarted |

## Status vocabulary

```text
NotStarted
Implemented
PositivePass
NegativePass
Qualified
BlockedByEnvironment
OwnerDecisionRequired
```

For each `Qualified` invariant, the repository's live evidence ledger must record:

- owner project,
- positive scenario,
- negative mutation,
- expected rejection stage,
- runner command,
- evidence path,
- SHA-256 digest when applicable,
- recovery observation when applicable.
