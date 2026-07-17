# SGE4 Level 4 v1 Capability Constitution

## Status and purpose

This document is the normative capability boundary for Level 4 v1. Level 4 v1 proves static, verified composition of already-frozen Leaf Packages. It does not weaken or replace any Stage 0D theorem.

## Preserved Leaf boundary

- D3D12 Package Schema 17 and Runtime 17 remain unchanged.
- The 54 canonical Leaf Package files, candidate manifest, selected Plan identities, and all Level 1–3 observations remain byte-for-byte frozen.
- A Leaf Package remains the sole authority for its internal operations, resources, queues, barriers, signal points, and frame stream.
- Composition is a distinct layer above Leaf Packages. It may bind declared external endpoints, order complete Leaf frame submissions, and carry cross-Package completion tokens. It may not rewrite a Leaf.

## Level 4 v1 capability

Level 4 v1 supports a finite static DAG containing at least two embedded Leaf Packages, Buffer-only shared resources, one D3D12 DeviceDomain, same-frame producer-to-consumer handoff, fan-out, multi-input consumers, explicit Package scheduling, explicit completion-token dependencies, deterministic frozen composition bytes, and whole-domain recovery.

Every executable composition is produced through this authority chain:

```text
PackageCompositionContract
  -> PackageLinker proposes LinkPlanIR
  -> independent LinkVerifier
  -> VerifiedLinkPlan
  -> FrozenComposition
  -> CompositionRuntime
```

## Mandatory authority rules

- A raw `LinkPlanIR` is never executable or serializable as a frozen artifact.
- `VerifyAndSeal` is the only constructor of `VerifiedLinkPlan`.
- The linker and verifier do not share validation helpers or private capability tokens.
- Runtime reads the verified Package schedule, endpoint bindings, state handoffs, and waits. It does not rediscover or optimize them.
- Backend never searches for a compatible endpoint, infers cross-Package state, removes waits, changes Package order, or selects a different Leaf.
- A shared resource and every cross-Package completion token are owned by one DeviceDomain and one device epoch, not by a Leaf instance.
- Device loss invalidates all Leaf instances, shared resources, and completion tokens in the domain. Recovery rematerializes the complete domain from the frozen composition without compiling or linking again. If the removed adapter is unavailable, the domain remains `AwaitingAdapter` and rejects execution until reacquisition; fresh-process recovery uses only the same frozen artifact.

## Accepted Level 4 v1 surface

- Embedded D3D12 Schema-17 Leaf Package bytes only.
- Buffer external slots only; exact kind and format match; allocated bytes are the maximum declared minimum.
- One producer and one or more consumers per shared Buffer. Multiple writers are rejected.
- Acyclic Package graph with a canonical topological schedule.
- Device-compatible target profiles, one supported schema/runtime pair, and one DeviceDomain per composition. Feature level, shader model, root-signature version, barrier model, queue topology, frames-in-flight, and required feature bits must match. Surface-image and descriptor capacities remain Leaf-local so that headless producers can precede the sole presenting Leaf.
- Zero or one surface-presenting Leaf; when present it is scheduled after all of its predecessors.

## Explicit exclusions

Level 4 v1 excludes Texture sharing, Package families and runtime variant selection, external-path or digest library resolution, incremental compilation, streaming or residency, conditional regions, indirect work, graph cycles, multi-device or multi-adapter execution, and partial execution of a Leaf frame stream.

Adding any excluded capability requires a later versioned constitution and must not silently broaden Level 4 v1 artifacts.

## Completion rule

Level 4 v1 is complete only when existing Stage 0D qualification remains unchanged and the Level 4 v1 qualification proves canonical contract/link/artifact bytes, independent verifier authority, WARP execution for linear/fan-out/diamond graphs, stale-object rejection, and whole-domain recovery.
