# Level 4 v2 R1 Canonical vocabulary

## Purpose

R1 implements the neutral vocabulary frozen by R0. It creates types that can carry the proven meanings into later reconstruction stages without importing staged experiment names, Backend APIs, application policy or Adapter-specific performance decisions.

R1 does not implement planning, independent verification, composition, freezing, materialization, submission or Recovery behavior. Those remain later-stage responsibilities.

## Strong identities

The following identities are distinct C++ types even though each uses the same canonical 256-bit representation:

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
```

A digest belonging to one identity domain cannot be passed where another identity type is required without an explicit, visible reconstruction step.

## Strong dynamic concepts

```text
ActiveCount
TransitionCount
ReuseCount
UpdateInterval
BatchSize
AffectedBlockCount
TimelineOrdinal
DeviceEpoch
```

Equal numeric values do not make these concepts substitutable. `ActiveCount{n}` is not a `TransitionCount{n}`. `TimelineOrdinal` is not a `DeviceEpoch`.

`DeviceEpoch` is additionally nonzero by construction. Zero denotes no valid Device generation and is rejected before a handle can be formed.

## Neutral records

### SemanticDescriptorV1

Owns only canonical Semantic identity and version. It contains no execution representation, Target API or application policy.

### InvocationDescriptorV1

References a Semantic identity and carries explicit runtime-varying dimensions, timeline ordinal and Device epoch. Candidate selection is intentionally absent.

### RawCandidateProposalV1

Carries a proposed representation identity and the identities it claims to bind. It is not executable and cannot construct `OpaqueVerifiedPlanV1`.

### OpaqueVerifiedPlanV1

Has no public default constructor and no constructor from a Raw Candidate. R1 reserves construction for the future independent Verifier access boundary but does not implement that authority.

### FrozenArtifactDescriptorV1

Names the immutable artifact and the Verified Plan, Target profile and resource contract identities it is expected to bind. R1 defines the vocabulary only; R2 will establish the legal construction chain.

### HistoryValidityDescriptorV1

Carries exact validity identity, Semantic and source Invocation identities, generation, Device epoch and validity state.

### Epoch-bound handles

Representation, History, Completion and Submission handles are distinct types. Every handle carries a resource-instance identity and a nonzero Device epoch.

## Canonical identity material

R1 canonical identity material consists only of:

```text
Domain
SchemaVersion
CanonicalPayload
```

The serialized order is fixed and SHA-256 is applied after explicit length-prefixing. Filesystem paths, process timestamps, random identifiers, live handles and performance-winner policy are not parameters and cannot affect the result.

## Completion boundary

Successful R1 qualification establishes:

```text
Canonical vocabulary implemented
Count and identity substitution rejected at compile time
Raw Candidate cannot construct an opaque Verified Plan
Canonical public source has no Backend dependency
Generic vocabulary has no frozen qualification limits
Debug and Release vocabulary evidence is byte-identical
```

It does not establish:

```text
Planner behavior
Verifier correctness
Verified Plan construction
Frozen artifact construction
Composition behavior
Runtime behavior
Recovery behavior
Candidate-selection policy
```
