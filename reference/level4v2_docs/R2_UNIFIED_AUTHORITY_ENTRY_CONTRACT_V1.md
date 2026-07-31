# Level 4 v2 R2 unified authority entry contract V1

## Entry condition

R2 may begin only after the R1 vocabulary gate passes in Debug and Release and the emitted vocabulary Evidence is byte-identical.

## R2 responsibility

R2 reconstructs one authority chain over the R1 vocabulary:

```text
Canonical Semantic and Invocation
  -> Raw Candidate proposal
  -> Candidate Planner
  -> Planner-independent Verifier
  -> opaque Verified Plan
  -> Frozen Artifact
```

R2 must provide the only legal construction path for `OpaqueVerifiedPlanV1`. It must independently reconstruct expected operations, identities, legal write sets, resource contracts and Target bindings rather than trusting Planner-provided copies.

## R2 must not yet implement

```text
Composition execution
Dynamic sparse/temporal lowering behavior
Runtime materialization or submission
Recovery
Performance-based Candidate policy
```

## Required R2 negative boundaries

- Planner cannot construct the opaque Verified Plan.
- Raw Candidate cannot freeze or execute.
- Mutation of any identity or expected operation is rejected.
- Resource-contract, Target-profile and write-set replay is rejected.
- The same authoritative fact is not serialized independently in multiple layers.
- Existing Schema 17 and Runtime 17 remain unchanged until migration evidence explicitly supersedes them.
