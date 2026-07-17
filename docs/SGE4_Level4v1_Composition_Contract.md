# SGE4 Level 4 v1 Composition Contract

## Identity model

All IDs are dense 32-bit indices in canonical tables. Canonical ordering is by immutable content identity, never caller insertion order. Digests use SHA-256 over the specified canonical binary encoding.

```text
LeafPackageId       index into canonical embedded Leaf table
SharedResourceId    index into canonical shared-resource table
CompositionEndpointId
CompositionLinkId
```

A Leaf identity contains its execution digest, complete file digest, target-profile digest, target kind, schema version, and minimum runtime version. The embedded bytes must decode to exactly those values. Different Leaf profile digests are accepted only when their device requirements match; surface-image and descriptor capacities are Leaf-local and may differ for the sole presenter.

## Leaf endpoint extraction

Each Schema-17 external slot produces one endpoint with:

- Leaf identity and external slot index;
- direction: Import, Export, or ImportExport as explicitly declared by the composition author;
- required resource kind and format;
- minimum bytes;
- required incoming state and guaranteed outgoing state;
- required completion-token synchronization contract.

Extraction copies facts from the decoded Leaf Package. It does not infer direction from shader access or operation contents. An authored endpoint that contradicts its Leaf slot is rejected.

## Shared Buffer contract

A shared resource declares its stable author key, Buffer kind, format, byte size, initial state, and ownership policy `DeviceDomain`. It binds exactly one producer endpoint and one or more consumer endpoints. The byte size must satisfy every bound endpoint. Every binding is explicit.

For each producer-to-consumer link:

```text
producer guaranteed outgoing state
  == declared handoff state
  == consumer required incoming state
```

The producer release completion is the only authority that may satisfy the consumer wait. Fan-out reuses the same resource and release token. A consumer with multiple inputs waits on every declared predecessor.

## Link plan

`LinkPlanIR` fixes:

- the complete canonical Leaf set;
- a canonical topological Package schedule;
- the physical owner and size of each shared Buffer;
- every endpoint-to-resource binding;
- every producer release to consumer acquisition dependency;
- every state handoff;
- the single DeviceDomain recovery policy;
- the composition identity computed from all execution-affecting fields.

The plan does not contain or override Leaf-internal operations, queues, barriers, allocations, or signal-point selection. Cross-Package synchronization references the Leaf external release token identified by external slot.

## Independent verification

The LinkVerifier rejects at least:

- absent, duplicated, corrupt, or digest-mismatched Leaf Packages;
- unknown or duplicated external slots and endpoint bindings;
- non-Buffer endpoints, format mismatch, or undersized shared resources;
- incoming/outgoing/handoff state mismatch;
- zero producers, multiple producers, multiple writers, or unbound required endpoints;
- missing waits, extra waits, self-links, duplicate links, or schedule violations;
- Package cycles or a schedule that is not the canonical topological schedule;
- device-requirement profile, target kind, schema, or runtime incompatibility;
- more than one surface-presenting Leaf;
- invalid recovery policy or composition identity.

Successful verification returns a move-only `VerifiedLinkPlan`. No public constructor or conversion from raw plans exists.

## Frozen composition format v1

The standalone artifact uses fixed-width little-endian integers, an 8-byte magic, fixed header and section descriptors, aligned canonical sections, per-section SHA-256, an execution digest, and a file digest. Required v1 sections are:

```text
Manifest
EmbeddedLeafTable
EmbeddedLeafBytes
SharedResourceTable
EndpointBindingTable
LinkTable
PackageSchedule
RecoveryPlan
```

All Leaf bytes are embedded. Unknown required sections, duplicate sections, overlap, non-zero reserved fields, invalid alignment, inconsistent table strides, and any digest mismatch are rejected before runtime load. `Read -> Write` reproduces identical bytes. Input Leaf order does not affect the result.

## Runtime execution and recovery

Runtime creates one DeviceDomain, loads every embedded Leaf into it, creates each shared Buffer once, and executes the verified schedule. For each Leaf it binds only declared endpoints, passes only declared predecessor tokens, submits exactly one complete Leaf frame stream, and captures declared release tokens.

Recovery invalidates all old resources and tokens, all Leaf instances, and temporal state. A successful adapter reacquisition increments the domain epoch and recreates all Leaves and shared Buffers from the same frozen bytes and verified plan. If the adapter cannot be reacquired after actual removal, the domain enters `AwaitingAdapter`, exposes no executable Leaf instances, and rejects submission. The artifact remains sufficient for retry or fresh-process rematerialization. No compiler, linker, or planner is invoked.
