# Project and dependency plan

Use the roadmap's 80-99 range.

## Production and experiment projects

| Project | Responsibility |
|---|---|
| `80_HierarchySemantic` | Fixed hierarchy meaning, validation, canonical ordering, depth layers, serialization, identity |
| `81_Spiral2Contracts` | Dynamic palette schema, observation schema, candidate kinds, versioned identities |
| `82_Spiral2Corpus` | H00-H08, F00-F11, fixed seeds, independent CPU reference |
| `83_HierarchyRepresentationCandidate` | Raw candidate graph data types only |
| `84_HierarchyRepresentationPlanner` | Generate A/B/C raw candidate graphs; Package-free |
| `85_HierarchyRepresentationVerifier` | Independently rederive and seal candidate facts; must not reference Planner implementation |
| `86_Spiral2LeafCompiler` | Compile only verified representation graphs into Frozen Leaf Packages |
| `87_Spiral2Observer` | Deterministic comparison, rigidity metrics, report aggregation |
| `88_Spiral2Scenarios` | Vertical slice, corpus scenarios, Composition assembly inputs |
| `89_Spiral2PerformanceExperiment` | Measurement profile, six-order scheduling, binary evidence and report |

## Test and launcher projects

| Project | Responsibility |
|---|---|
| `90_Spiral2SemanticTests` | Hierarchy and canonical semantic tests |
| `91_Spiral2DynamicInvocationTests` | Dynamic builder, exact bytes, multi-frame authority |
| `92_Spiral2RepresentationPlanningTests` | Three candidate graph generation |
| `93_Spiral2RepresentationAuthorityTests` | Verifier seal and mutation suite |
| `94_Spiral2LeafPackageTests` | Matrix, Direct PGA, Hybrid Leaf qualification |
| `95_Spiral2CompositionTests` | Ten-Leaf Composition and Level 4 sufficiency |
| `96_Spiral2WarpObservationTests` | H00-H08 x F00-F11 observation corpus |
| `97_Spiral2RecoveryTests` | Controlled/actual recovery and stale epoch rejection |
| `98_Spiral2FreezeTests` | Debug/Release/fresh-process determinism |
| `99_Spiral2Launcher` | Self-test, WARP, measurement, evidence, report CLI |

## Reuse policy

Reuse proven baseline components when their contracts match:

- `00_Foundation`
- `01_MathVocabulary`
- the canonical Motor representation from Spiral 1 where suitable
- Package Schema 17 and Runtime 17
- existing Level 4 Composition projects 16-23
- existing source manifest and test infrastructure

Do not duplicate an entire Package Runtime, D3D12 Backend, or Composition Runtime inside projects 80-99.

Do not refactor stable Spiral 1 code merely for aesthetic uniformity. Refactor only when a concrete Spiral 2 contract requires a shared stable abstraction and regression evidence protects the move.

## Required dependency direction

```text
80 Hierarchy Semantic
  -> 81 Contracts / 82 Corpus
  -> 83 Raw Candidate
  -> 84 Planner
  -> 85 independent Verifier
  -> 86 Leaf Compiler
  -> Frozen Leaf Packages
  -> existing Level 4 Composition
  -> existing Frozen Runtime / Backend
```

The conceptual arrow is data flow, not permission for every downstream-to-upstream ProjectReference. Keep references minimal.

## Forbidden dependencies

- `85_HierarchyRepresentationVerifier -> 84_HierarchyRepresentationPlanner` implementation
- Runtime or Backend -> `80_HierarchySemantic`
- Runtime or Backend -> `83/84/85` representation projects
- Backend -> representation Planner
- Frozen Leaf artifact -> source Semantic
- Candidate A implementation -> Candidate B or C implementation
- Candidate B implementation -> Matrix helper that materializes a hidden Matrix
- Planner -> Frozen Package or Runtime implementation
- Observer -> Planner implementation for expected values
- CPU reference -> any candidate GPU algorithm implementation

## Solution integration

- Add all projects to `SemanticGpuEngine4-5.sln`.
- Use stable ASCII paths and filenames.
- Keep Debug/Release x64 configurations complete.
- Follow existing v145, `/std:c++latest`, `/utf-8`, and strict floating-point conventions where relevant.
- Add dependency verification rules before declaring Architecture Complete.
