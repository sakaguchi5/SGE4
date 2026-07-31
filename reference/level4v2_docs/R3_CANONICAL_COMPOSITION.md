# Level 4 v2 R3 Canonical composition

## Purpose

R3 reconstructs only the finite static Buffer-flow DAG subset already proven by Level 4 v1. It attaches composition authority to R2 opaque Frozen leaf artifacts without allowing Composition to reinterpret Semantic meaning or mint a Verified Plan.

```text
R2 Opaque Frozen leaf authority
  -> Raw Composition Contract
  -> Composition Planner proposal
  -> Planner-independent Composition Verifier
  -> Opaque Verified Composition
  -> Opaque Frozen Composition
```

## Contract authority

A leaf enters R3 only through `MakeLeafAuthorityV1(..., const OpaqueFrozenArtifactV1&)`. The resulting leaf identity binds the R2 Frozen artifact, Verified Plan, Target profile and Resource contract identities.

Endpoint records are projections bound to that leaf Resource-contract identity. Flow declarations provide stable flow identity, boundary kind and endpoint membership. They do not carry an execution or freeze entry.

## Proven subset

```text
Finite static DAG
Buffer flows only
Exactly one writer per Internal or CompositionOutput flow
CompositionInput has no writer and at least one reader
Zero or one Presenter
One TargetProfileIdentity across the composition
Whole-composition Recovery scope
```

Texture sharing, graph cycles, conditionals, variants, streaming, residency, partial Recovery and multiple target domains remain outside R3.

## Planner proposal

The Planner proposes:

- one allocation identity, ownership and maximum required size per flow;
- canonical topological leaf order;
- endpoint-to-flow bindings;
- producer-outgoing to consumer-incoming state handoffs;
- signal/wait synchronization edges;
- the optional Presenter binding;
- the complete whole-composition Recovery set.

The proposal has no execute, submit or freeze authority.

## Independent verification

`CompositionVerifierV1` has no Project dependency on `CompositionPlannerV1`. It independently validates the Contract, reconstructs the graph and every represented decision, then compares the supplied proposal field by field. Only exact agreement produces `VerifiedCompositionV1` and its seal.

## Frozen boundary

`FrozenCompositionBuilderV1::Freeze` accepts only `VerifiedCompositionV1`. There is no Raw Contract or Planner Proposal overload.

The Frozen identity binds Contract, Plan, seal, Schedule and Recovery identities plus leaf/flow counts. It contains no native handle, filesystem path, process timestamp or performance policy.

## Non-claims

R3 adds no Runtime materialization, command submission, native allocation, frame execution or Recovery behavior. Those remain later-stage responsibilities.
