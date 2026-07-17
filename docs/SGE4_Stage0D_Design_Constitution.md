# SGE4 Stage 0D Design Constitution v0.1

## 1. Generation boundary

SGE3 is frozen as the completed Level-3 reference. SGE4 is an independent source tree, solution, namespace, Project GUID set, and ProjectReference graph.

Forbidden production dependencies:

```text
SGE4 production -> SGE3 project/binary at runtime
Runtime/Backend -> Semantic/Analysis/Planning/Compiler
Package ABI -> Source IR/Compiler/Runtime/Backend
Raw ExecutionPlanIR -> Package Lowering
CandidatePlanner -> Package Schema/Package Lowering/Runtime/Backend
Frontend A -> Frontend B
```

## 2. Stage 0D compiler model

```text
CompilationInput
  -> CandidatePlanner
       candidate generation
       independent verification report
       cost calculation
  -> SGE4Compiler
       fresh VerifyAndSeal capability
       Package lowering
       policy/profile selection finalization
  -> FrozenExecutablePackage
```

Canonical behavior remains a Candidate policy with a single allowed candidate. It does not bypass verification.

## 3. Ownership of decisions

- Semantic projects own logical resources, uses, works, programs, and temporal meaning.
- `05A_CompilationInput` owns shared semantic-analysis and Schema-17 feasibility validation.
- Planning projects own schedule, queues, synchronization, instances, allocation, aliasing, state, binding, and boundaries.
- The independent verifier alone mints `VerifiedExecutionPlan`.
- CandidatePlanner never creates a Package.
- SGE4Compiler alone combines verified planning with D3D12 Package lowering.
- Frozen Package owns the complete represented execution decision.
- Runtime owns invocation validation and device-epoch state.
- Backend mechanically maps Package artifacts and Operations to D3D12.

## 4. Project topology

```text
00-05   Foundation, Math, Semantic, Analysis, Target contract
05A     neutral CompilationInput validation
06      ExecutionPlan model
07      independent ExecutionPlan verifier
08      Package-free Candidate planner
09-10   Frozen container and stable D3D12 Package schema
11      verified Plan -> Package lowering
12      only normal SGE4 compiler orchestrator
13-15   Runtime, D3D12 Backend, Win32 platform
20-23   experiment domain and independent frontends
24-27   qualification corpora
28      isolated SGE3 compatibility/planning Oracle
30-45   proof and qualification executables
50-51   launcher and package compiler applications
```

## 5. Compatibility Oracle

`28_SGE3CompatibilityOracle` is test support only. Only `33_PlanningTests` may reference it. It retains:

```text
SGE3 reference canonical path
SGE3-style Level-3 planning orchestration
```

Stage 0D compares those results with the new SGE4Compiler orchestration for Package bytes, candidate manifest bytes, and selected Plan identity.

## 6. Authority invariant

A field may exist in `ExecutionPlanIR` only when all three are true:

1. it participates in canonical Plan identity;
2. the independent verifier validates it;
3. Package lowering obeys it or constrains it to the only legal canonical value.

CandidatePlanner verification reports are not capability tokens. SGE4Compiler must re-run `VerifyAndSeal` immediately before lowering.

## 7. Stage 0D freeze invariant

Stage 0D is not complete until one command proves:

- dependency graph validity and CandidatePlanner Package isolation;
- all Debug and Release tests;
- Authority gate in both configurations;
- SGE3 canonical and planning regression checks;
- canonical/planning manifests identical across fresh processes;
- manifests identical between Debug and Release;
- WARP/readback, external, temporal, aliasing, recovery, and frontend observations remain qualified.
