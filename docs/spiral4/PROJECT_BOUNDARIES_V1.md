# Spiral 4 project and authority boundaries v1

## CU1 rule

CU1 adds contract, evidence, and verification files only.

It does not add or modify:

- C++ projects,
- `SemanticGpuEngine4-5.sln`,
- Package Schema,
- Package Runtime,
- Composition Runtime,
- D3D12 Backend,
- Shader templates,
- Frozen Operation records.

Baseline commit: `8c1125394ba4b45d571d7cba4e7ad685bb90918b`.

## Baseline capability inventory

The existing Package schema contains the `IndirectArgument` Resource state.

It does not contain:

- a Command Signature artifact,
- an ExecuteIndirect/DispatchIndirect Frozen Operation,
- an authorized Runtime/Backend route for indirect Compute dispatch.

This is the precise starting gap. CU1 does not prescribe the final binary layout.

## Reserved future project roles

Names are reserved in documentation only. Empty projects are forbidden.

```text
120_ActiveWorkSemantic
121_Spiral4Contracts
122_Spiral4Corpus
123_ActiveWorkLoweringCandidate
124_ActiveWorkLoweringPlanner
125_ActiveWorkLoweringVerifier
126_Spiral4LeafCompiler
127_Spiral4Scenarios
128_Spiral4Execution
129_Spiral4PerformanceExperiment
130_Spiral4SemanticTests
131_Spiral4CandidateGraphTests
132_Spiral4AuthorityMutationTests
133_Spiral4LeafPackageTests
134_Spiral4CompositionTests
135_Spiral4WarpObservationTests
136_Spiral4RecoveryTests
137_Spiral4FreezeTests
138_Spiral4PerformanceTests
139_Spiral4Launcher
```

Future implementation may revise these names before projects are created.

## Required dependency direction

```text
Active Work Semantic
  -> Candidate
  -> Planner proposal

Active Work Semantic + Target Contract
  -> independent Verifier
  -> opaque Verified Plan
  -> Leaf Compiler
  -> Frozen Package
  -> Frozen Composition
  -> Runtime
  -> Backend
```

Forbidden dependencies:

- Verifier -> Planner,
- Runtime/Backend -> Active Work Semantic,
- Runtime/Backend -> Planner or Raw Candidate,
- Backend -> Shader source compiler,
- Raw Candidate -> Freeze or Runtime,
- Runtime -> CPU readback used to choose Dispatch dimensions,
- Backend -> hidden direct/indirect selection.

## ABI extension rule

Schema 17 / Runtime 17 remain the baseline.

A versioned extension may be added only when CU2 supplies:

1. a concrete operation and artifact requirement,
2. an insufficiency proof for the existing schema,
3. corruption and unknown-operation rejection,
4. canonical serialization and digest coverage,
5. mechanical Backend mapping,
6. Recovery and old-version compatibility rules.
