# Spiral 1 Authority and Project Boundaries V1

- Contract ID: `SGE4-5.Spiral1.AuthorityBoundaries.V1`
- Status: Frozen

## 1. Authority chain

```text
PgaRigidTransformExperimentSpec
  ↓
Canonical Pga Semantic
  ↓
Representation Candidate Planner
  ↓
Raw Representation Candidate
  ↓
Independent Representation Verifier
  ↓
Verified Representation Plan
  ↓
Spiral1 Leaf Compiler
  ↓
Existing ExecutionPlan Candidate / Verifier authority
  ↓
Frozen Leaf Package
  ↓
Existing Level 4 v1 Composition Contract / Planner / Verifier
  ↓
Frozen Composition
  ↓
Composition Runtime / D3D12 Backend
```

Raw Candidate、Raw ExecutionPlan、Raw Composition Planは実行もFreezeもできない。

## 2. Planned projects

Stage 03では空Projectを作らない。必要になるStageで次を追加する。

```text
60_PgaRigidTransformSemantic
61_Spiral1Contracts
62_Spiral1Corpus
63_D3D12RepresentationCandidate
64_D3D12RepresentationPlanner
65_D3D12RepresentationVerifier
66_Spiral1LeafCompiler
67_Spiral1Observer
68_Spiral1Scenarios
70_Spiral1SemanticTests
71_Spiral1RepresentationPlanningTests
72_Spiral1RepresentationAuthorityTests
73_Spiral1LeafPackageTests
74_Spiral1CompositionTests
75_Spiral1WarpObservationTests
76_Spiral1RecoveryTests
77_Spiral1FreezeTests
78_Spiral1PerformanceExperiment
79_Spiral1Launcher
```

## 3. Allowed dependency direction

```text
60 Semantic
  ↓
61 Contracts
  ↓
63 Candidate ← 64 Planner
  ↓
65 Verifier
  ↓
66 Leaf Compiler
  ↓
existing ExecutionPlan / Package authority
  ↓
existing Composition authority
  ↓
Runtime / Backend
```

`62 Corpus`は60/61だけを参照できる。`67 Observer`は61/62とstable package/readback typesだけを参照する。

## 4. Forbidden dependencies

- 65 Verifier -> 64 Planner
- Runtime / Backend -> 60..65
- Frozen Package -> PGA Semantic
- Composition Runtime -> Representation Planner／Verifier
- Semantic -> D3D12、Package、Runtime、Backend
- Planner -> Frozen Package writer
- Raw Candidate -> Runtime
- Frontend間依存

D3D12 BackendはSpiral 1新規Projectを一つも参照しない。

## 5. Ownership

- Semantic owns mathematical meaning only.
- Planner proposes lowering-specific constants and template selection.
- Representation Verifier independently rederives represented decisions.
- Leaf Compiler alone bridges a sealed Representation Plan into existing execution planning.
- Existing SGE4-5 Compiler alone lowers verifier-sealed ExecutionPlanIR to Package.
- Composition Verifier owns cross-Leaf schedule/resource/synchronization authority.
- Runtime materializes and executes Frozen decisions without reselection.

## 6. Seal rules

Verified Representation sealはopaqueで、Semantic identity、Candidate identity、Target profile、Observation Contract、Verifier schema versionへ結びつく。copy/replay、field再計算、public constructorによる偽装を拒否する。

## 7. ABI rule

Spiral 1はSchema 17 / Runtime 17で表現可能な限りABIを変更しない。変更が必要なら、既存ABIで表現不能な具体的Negative evidenceと別version仕様を先に作る。
