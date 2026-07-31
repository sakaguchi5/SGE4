# Level 4 v2 R0 canonical layer ownership V1

R0 freezes where each fact is allowed to be authoritative. R1 may rename types, but it may not move policy across these boundaries without an explicit supersession record.

## Ownership table

| Layer | Owns | Must not own |
|---|---|---|
| Canonical Semantic | Mathematical meaning, exact derived relations, canonical semantic identity | D3D12 objects, Adapter policy, game policy, representation winner |
| Canonical Invocation | Explicit runtime-varying verified inputs, timeline ordinal, exact set/count identities | Inference of membership, Candidate choice, Backend scheduling judgment |
| Candidate Proposal | Alternative representation and materialization proposals | Verification seal, execution permission, Runtime policy |
| Candidate Planner | Proposal construction from Canonical inputs | Independent acceptance authority |
| Independent Verifier | Expected operation reconstruction, legal write set, Target/resource binding, opaque seal | Planner trust, runtime measurement policy |
| Frozen Artifact | Immutable verified executable identity and versioned payload | Process timestamps, filesystem paths, live handles |
| Canonical Composition | Finite Buffer DAG, flow, allocation, schedule, state and synchronization authority | Semantic reinterpretation, unproven Texture/cycle/variant features |
| Canonical Runtime | Materialization and submission of already-authorized decisions | Membership, action, Candidate, history or hardware-winner policy |
| Canonical History | Validity, generation, previous/output relation and no-write retention contract | Hidden temporal inference |
| Canonical Recovery | Epoch invalidation, explicit rebind, full-active seed and Adapter exclusion | Partial recovery claims not yet proven |
| Qualification Evidence | Correctness and observational measurement records | Canonical ABI policy |
| Migration Program | Old-to-v2 invariant correspondence and retirement authority | New execution capability |

## Single-fact rule

One fact has one authoritative owner. Other layers store identities or references, not independently editable copies.

Examples:

```text
ActiveCount value          -> Canonical Invocation
Expected DispatchGroupsX   -> independently reconstructed by Verifier
Frozen operation           -> references verified identity
Runtime                    -> consumes; never recalculates policy
```

```text
DeviceEpoch                -> Runtime/Recovery authority
History validity           -> Canonical History bound to DeviceEpoch
Candidate                  -> cannot override either fact
```

## Forbidden shortcuts

- Copying the same mutable count into Semantic, Candidate, Plan and Runtime records.
- Treating an integer with the same numeric value as a compatible semantic dimension.
- Letting serialization layout become the owner of meaning.
- Letting Backend convenience decide a missing semantic or composition field.
- Promoting one Adapter's CU6 winner map into Runtime policy.
