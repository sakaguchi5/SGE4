# Level 4 v2 R2 unified authority chain

## Purpose

R2 turns the neutral R1 vocabulary into one legal authority path:

```text
Canonical Semantic and Invocation
  -> Raw Candidate proposal
  -> Candidate Planner
  -> Planner-independent Verifier
  -> opaque Verified Plan
  -> opaque Frozen Artifact
```

R2 does not execute a plan. It proves who may propose, who must independently reconstruct, who may mint a Verified boundary, and what a Frozen artifact is allowed to reference.

## Independent reconstruction

Planner and Verifier are separate Projects. The Verifier does not link against or include the Planner Project. Both receive the same explicit Canonical request and Candidate input, but the Verifier rebuilds its expected operation sequence in its own implementation.

The Verifier recomputes:

```text
Semantic identity
Invocation identity
Target-profile identity
Resource-contract identity
Legal write-set identity
Candidate identity
Expected authority operations
Planner-proposal identity
Verified Plan identity
Verification seal identity
```

Planner-provided copies are treated only as claims.

## Six authority operations

The operation sequence is an authority description, not a GPU command list:

```text
BindSemantic
BindInvocation
BindCandidate
BindTargetProfile
BindResourceContract
BindLegalWriteSet
```

Composition scheduling, dynamic sparse/temporal lowering and Backend commands remain outside R2.

## Verified construction boundary

`VerifiedPlanConstructionAccessV1` is the friend already reserved by R1. R2 defines it with a private creation method whose only friend is `IndependentVerifierV1`.

Therefore:

```text
Raw Candidate cannot mint Verified
Planner cannot mint Verified
Tests cannot mint Verified
Frozen builder cannot mint Verified
Independent Verifier alone may mint Verified after all comparisons pass
```

## Frozen boundary

The R1 `FrozenArtifactDescriptorV1` remains vocabulary only and is not execution or freeze authority by itself. `OpaqueFrozenArtifactV1` can be built only from `VerifiedAuthorityV1`. It records identity references to the Verified Plan, verification seal, Target profile, resource contract, legal write set and operation sequence. It does not copy mutable operations or materialize runtime objects.

## Completion boundary

R2 establishes authority construction, mutation rejection, replay rejection and deterministic Frozen identity. It does not establish Composition execution, Runtime materialization, Recovery or performance policy.
