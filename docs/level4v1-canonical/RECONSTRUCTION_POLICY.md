# SGE4 Level 4 v1 Canonical Reconstruction Policy

## 1. Status

This branch is reconstructed directly from commit
`222f4968f307a44bea5b2554cb08a3247a7ec544`.

The discovery implementation ending at
`d05fa9f4cc7146087fd41da75f90e225379237e9` is evidence and a source of
requirements, counterexamples, negative gates, and WARP observations. It is
not a binary-compatibility target and is not the parent of this branch.

## 2. What is preserved

The reconstruction preserves the Level 1-3 foundation and the Level 4 v1
meaning proven by the discovery implementation:

- a Leaf remains authoritative for its internal executable decisions;
- composition access, shape, state, and synchronization are derived from a
  validated Leaf interface rather than authored by the composition caller;
- a raw composition plan is a proposal and has no execution or freeze authority;
- an independent verifier is the only authority that can admit a plan;
- runtime executes frozen decisions and does not rediscover schedule, binding,
  state transitions, waits, or a replacement Leaf;
- one composition-owned Buffer represents one internal resource flow;
- shared resources and completion tokens belong to one DeviceDomain and epoch;
- the complete composition is the recovery unit;
- removed-adapter exclusion survives retry;
- an unavailable replacement adapter leaves the runtime in AwaitingAdapter and
  execution is rejected;
- fresh processes can rematerialize the same frozen composition without source,
  compiler, planner, or linker;
- the new implementation must be deterministic across fresh Debug processes and
  Debug/Release builds.

## 3. What is not preserved

The following discovery details are not compatibility requirements:

- Schema-18 bytes and section layout;
- endpoint record stride;
- contract, plan, seal, and frozen-composition encoding;
- C++ APIs and namespaces;
- F1-F11 project boundaries and runner structure;
- patch-based backend integration;
- any discovery artifact digest.

No migration reader or compatibility mode is required because the discovery
ABI was not published.

## 4. No speculative abstraction

This reconstruction models only the capability already proven for Level 4 v1:

- embedded frozen Schema-17 Leaf packages;
- required per-frame Buffer endpoints;
- ReadOnly and WriteOnly access;
- finite acyclic package graphs;
- exactly one producer and one or more consumers for an internal shared Buffer;
- canonical static scheduling;
- one DeviceDomain;
- zero or one presenting Leaf;
- whole-composition recovery.

It does not add placeholders or general models for Texture sharing, multiple
writers, conditional regions, graph cycles, package variants, libraries,
streaming, residency, partial recovery, or multiple devices.

When a later Level 4 capability is experimentally proven, the canonical model
may be reconstructed again. Implementation and binary ABI may change; the
proof ledger may only lose an invariant through an explicit constitutional
change record.

## 5. Completion rule

This branch is complete only after all R0-R5 evidence is present and the normal
Windows qualification runner succeeds in Debug and Release with D3D12 WARP.
Repository contents alone are not proof of completion.
