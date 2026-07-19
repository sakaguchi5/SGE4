# SGE4-5 Spiral 1 Completion Unit 1

# Stage 04 + Stage 05: Semantic and Observation Foundation

- Base commit: `025218ca31e3b9445bfca0fb36a9d6361f10e698`
- Status: Qualification target; complete only when `run_sge4_5_cu1_semantic_observation.bat` passes
- Scope: CPU-only. No D3D12, Shader, Frozen Package, Runtime or Composition implementation is added.

## 1. Completion unit

```text
Stage 04 PGA Math and Canonical Semantic
  +
Stage 05 Corpus, CPU Reference and Observation Foundation
```

The internal Stage gates remain separate, while the development artifact and completion declaration are one unit.

## 2. Projects

```text
60_PgaRigidTransformSemantic
61_Spiral1Contracts
62_Spiral1Corpus
67_Spiral1Observer
70_Spiral1SemanticTests
```

`60` depends only on `00_Foundation`. It contains no Cartesian Matrix, D3D12, Shader, Package, Runtime or Backend concept.

## 3. Implemented contracts

- Canonical binary64 unit quaternion and PGA Motor construction
- Hamilton product, active rotation and post-rotation translation convention
- canonical sign and negative-zero normalization
- 72-byte Motor serialization and semantic identity
- 16-byte `Float4Point` schema and point negative gates
- Observation Contract V1 identity and 96-byte `PointComparisonRecordV1`
- SplitMix64 Corpus V1 generator for S00-S15
- independent CPU binary64 reference evaluation
- point-index-ordered deterministic Observer and report serialization
- byte-emitted Stage04, Stage05 and CU1 bundles

## 4. Required gates

```text
run_sge4_5_stage04_semantic.bat
run_sge4_5_stage05_observation.bat
run_sge4_5_cu1_semantic_observation.bat
```

The final CU1 gate builds Debug and Release, executes positive and negative tests, emits artifacts in fresh processes and byte-compares Debug A, Debug B and Release.

## 5. Non-goals preserved

No Representation Candidate, Verifier seal, Matrix lowering, Direct PGA shader, Frozen Leaf, Composition, WARP or physical-GPU benchmark is implemented in CU1.

## 6. Promotion

CU1 completion qualifies the CPU-side meaning and observation foundation only. Stage 06 may consume these contracts to propose Raw Representation Candidates, but may not alter their identities or rules silently.
