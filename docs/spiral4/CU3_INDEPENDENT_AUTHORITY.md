# Spiral 4 CU3 — independent Active Work lowering authority

Baseline commit: `ca29f228687691769355afe33f390adf04c1ed24`

## Goal

CU2 proved that a GPU Argument Producer and `ExecuteIndirect` can execute a
runtime-varying Active Work Count without a CPU or Runtime dispatch decision.

CU3 proves who is authorized to choose and freeze that execution structure.

```text
Canonical Active Work Semantic
        ↓
Raw Candidate proposal
        ↓
Planner-independent Verifier
        ↓
opaque Verified Plan and certificate
        ↓
actual sidecar artifact binding
        ↓
verified ExecuteIndirect execution
```

## Raw Candidate

`RawActiveWorkLoweringCandidateV1` is a proposal only.

It carries proposed:

- Semantic, Target, Active Count, Observation, and Command Signature identities,
- producer and Consumer Program-template identities,
- indirect artifact identity,
- Command kind and Operation code,
- active-prefix, state-transition, and zero-work policies,
- maximum Work, group size, record strides,
- argument stride, offset, and maximum command count,
- Argument Producer dispatch dimensions.

The Raw type has no Freeze or execution member and is not convertible to the
Verified or Frozen Verified types.

## Planner independence

`125_ActiveWorkLoweringVerifier` has no project or source dependency on
`124_ActiveWorkLoweringPlanner`.

The Verifier independently reconstructs:

- canonical verification context,
- producer and Consumer template identities,
- `Nmax = 4096`,
- `threadsPerGroup = 64`,
- 12-byte Dispatch argument contract,
- zero argument offset,
- one maximum command,
- one-thread Argument Producer dispatch,
- UAV barrier followed by `INDIRECT_ARGUMENT`,
- zero Work as zero Dispatch groups,
- actual CU2 sidecar artifact identity.

It compares every proposed field rather than accepting the Planner's candidate
identity as authority.

## Opaque Verified type

`VerifiedActiveWorkLoweringV1` is:

- non-aggregate,
- non-default-constructible,
- not constructible from a public Plan/certificate pair,
- not convertible from Raw Candidate.

Only `VerifyActiveWorkLoweringCandidateV1` can mint it.

Its certificate binds:

- verified Plan identity,
- Semantic identity,
- Target profile,
- Active Count contract,
- Observation contract,
- Command Signature contract,
- verified Operation identity,
- actual sidecar artifact identity.

## Actual execution binding

CU2's sidecar artifact is an explicit Level 4 extension artifact, not yet a
canonical Package Schema revision.

CU3 therefore binds the Verified Plan to:

```text
FrozenSingleIndirectArtifactV1 identity
+ Verified Operation identity
+ producer/Consumer Program-template identities
+ compiled producer/Consumer shader bytecode digests
```

`FrozenVerifiedSingleIndirectExecutionV1` can only be created from a valid
Verified Plan in the same verification context.

The low-level CU2 executor remains an architectural mechanism. The CU3
Level-5-authorized path is `RunVerifiedSingleIndirectOnWarpV1`, which accepts
only the Frozen Verified binding.

## Replay rejection

CU3 rejects a valid Verified Plan when replayed under a different:

- Target profile identity,
- Command Signature contract identity,
- Observation contract identity.

This is context replay rejection, not a mutable runtime epoch mechanism.
Device-epoch and Recovery replay remain CU5.

## Completion boundary

CU3 completes independent authority for one Single ExecuteIndirect lowering.

It does not yet add:

- Fixed Maximum Candidate,
- Batch-size Candidate family,
- Batch partition or multiple argument records,
- whole-composition Frozen Leaf groups,
- Recovery,
- real-hardware performance evidence.
