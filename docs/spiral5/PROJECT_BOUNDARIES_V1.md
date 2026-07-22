# Spiral 5 project and authority boundaries v1

## CU1 rule

CU1 adds contract, evidence, and verification files only.

It does not add or modify:

- C++ projects,
- `SemanticGpuEngine4-5.sln`,
- Package Schema 17 or Runtime 17,
- the Spiral 4 indirect sidecar,
- Composition Runtime,
- D3D12 Backend,
- Shader templates,
- Frozen Operation records.

Baseline commit: `f29d6597ec370d963c7b7dfbbc9af9590e8bd58f`.

## Baseline capability inventory

The existing runtime has:

- an invocation `frameNumber`,
- per-invocation dynamic Leaf bytes,
- whole-composition Recovery and device epochs,
- verified direct/indirect Work issuance from Spiral 4.

The existing canonical Composition Contract does not explicitly represent:

- a Temporal History Flow role,
- history source generation,
- an update schedule artifact,
- retained-history completion,
- legal history read/write generations.

CU1 does not prescribe the final binary layout.

## Reserved future project roles

Names are reserved in documentation only. Empty projects are forbidden.

```text
134_TemporalStateSemantic
135_Spiral5TemporalContracts
136_Spiral5TemporalExecution
137_TemporalLoweringCandidate
138_TemporalLoweringPlanner
139_TemporalLoweringVerifier
145_Spiral5TemporalArchitectureTests
146_Spiral5AuthorityMutationTests
147_Spiral5CandidateFamilyTests
148_Spiral5ArchitectureQualificationTests
149_Spiral5PerformanceExperiment
150_Spiral5PerformanceTests
```

Future implementation may revise these names before projects are created.

## Required dependency direction

```text
Temporal Semantic + Update Schedule
  -> Raw Candidate
  -> Planner proposal

Temporal Semantic + Update Schedule + Target Contract
  -> independent Verifier
  -> opaque Verified Temporal Plan
  -> temporal Compiler / sidecar builder
  -> Frozen Temporal Artifact
  -> Frozen Composition
  -> epoch-bound History Binding
  -> Runtime
  -> Backend
```

Forbidden dependencies:

- Verifier -> Planner,
- Runtime/Backend -> Temporal Semantic,
- Runtime/Backend -> Planner or Raw Candidate,
- Raw Candidate -> Freeze or Runtime,
- Runtime -> choosing update or hold,
- Runtime -> choosing history placement,
- Backend -> hidden skip/recompute selection,
- Backend -> treating stale history as valid,
- measurement results -> automatic Runtime policy.

## ABI extension rule

Schema 17 / Runtime 17 and the Spiral 4 sidecar remain the baseline.

A versioned temporal extension may be added only when CU2 supplies:

1. a concrete history role and generation requirement,
2. an insufficiency proof for existing contracts,
3. corruption and unknown-record rejection,
4. canonical serialization and digest coverage,
5. mechanical Backend mapping,
6. Recovery, reseed, and old-version compatibility rules.
