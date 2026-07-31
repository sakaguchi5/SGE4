# Level 4 v2 R1 Canonical vocabulary entry contract V1

## Status

```text
R1 implementation = NOT STARTED
R1 input contract = FROZEN BY R0
```

R1 may begin only from the completed R0 manifest. It defines neutral strong types and identities; it does not yet reconstruct Planner, Verifier, Composition or Runtime behavior.

## Required strong concepts

The initial vocabulary must distinguish at compile time:

```text
SemanticIdentity
InvocationIdentity
CandidateIdentity
VerifiedPlanIdentity
FrozenArtifactIdentity
ResourceContractIdentity
ResourceInstanceIdentity
WriteSetIdentity
HistoryValidityIdentity
TargetProfileIdentity
ObservationContractIdentity

ActiveCount
TransitionCount
ReuseCount
UpdateInterval
BatchSize
AffectedBlockCount
TimelineOrdinal
DeviceEpoch
```

Numerically equal values do not make these types substitutable.

## Required neutral records

R1 must propose neutral, Spiral-free names for semantic meaning identity/version, explicit dynamic invocation inputs, Candidate proposal identity, opaque Verified Plan boundary, Frozen artifact identity, exact history validity/generation and Device-epoch-bound runtime handles.

## Public-name restrictions

Public v2 names must not contain:

```text
Spiral
CU1
CU2
CU3
CU4
CU5
CU6
RTX4070
WARP
D3D12
Game
```

D3D12 may appear only in Backend- or Target-specific implementation namespaces, never in Canonical Semantic or Invocation records.

## R1 negative tests required

- Count-type substitution must fail to compile.
- Identity-type substitution must fail to compile.
- Raw Candidate construction must not produce a Verified type.
- Canonical Semantic headers must not depend on D3D12 headers.
- Canonical Invocation must not contain Candidate choice or game policy.
- Generic types must not freeze 4096, 128 or 64 as universal limits.
- Process paths, timestamps and random UUIDs must not affect canonical identity.

## Completion boundary

R1 is complete only when the vocabulary and negative tests pass while:

```text
Runtime capability added = None
Schema 17 mutation = None
Runtime 17 mutation = None
Reference project deletion = None
```
