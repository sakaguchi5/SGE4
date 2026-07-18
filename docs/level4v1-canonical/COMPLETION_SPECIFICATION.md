# SGE4 Level 4 v1 Canonical Completion Specification

## Final capability boundary

The canonical implementation proves only the following concrete capability:

- embedded Schema-17 executable Leaf Packages;
- validated composition Buffer endpoints with derived ReadOnly or WriteOnly access;
- a finite static same-frame acyclic package graph;
- exactly one writer and one or more readers for an internal shared Buffer;
- composition input and composition output Buffers;
- one D3D12 DeviceDomain and one current device epoch;
- zero or one presenting Leaf;
- deterministic contract, verified execution decisions, and frozen artifact;
- WARP execution of independent, linear, fan-out, multi-input, diamond,
  headless, and single-presenter scenarios;
- whole-composition controlled rebuild;
- actual RemoveDevice, removed-adapter exclusion, AwaitingAdapter, stale-object
  rejection, and fresh-process rematerialization.

The implementation does not provide placeholders or speculative abstractions
for Texture sharing, graph cycles, conditional execution, package variants,
library resolution, incremental compilation, streaming, residency, indirect
work, multiple writers, partial recovery, multiple devices, or multiple
adapters.

## Authority chain

```text
Validated executable Leaf
  -> Composition Buffer Interface
  -> Canonical Composition Contract
  -> Raw Execution Proposal
  -> Independent Verification
  -> Verified Composition Plan
  -> Frozen Composition
  -> Composition Runtime
  -> Whole-composition Recovery
  -> Main Solution Final Integration Freeze
```

No stage may be bypassed.

## R0-R5 products

### R0 - Proof ledger and completion specification

Normative policy, proof ledger, explicit exclusions, project dependency map,
and qualification matrix.

### R1 - Frozen composition boundary

A deterministic fixed-width binary container and an immutable decoded memory
model. The reader performs complete static validation before D3D12
materialization. The format is new and has no compatibility obligation to the
discovery Schema 18 or discovery Frozen Composition bytes.

### R2 - Contract, planner, and independent verifier

The Leaf interface is derived from validated executable Package evidence. The
contract author chooses stable identities and connections, not access or
resource facts. The planner only proposes. The independent verifier re-derives
every represented decision and alone constructs the verified plan.

### R3 - Shared DeviceDomain runtime

The runtime loads only the frozen artifact, extracts executable Leaves, creates
one DeviceDomain, materializes one Buffer per Resource Flow, follows the frozen
schedule, performs explicit state handoffs, and propagates completion tokens.
It never calls Source, Compiler, Planner, or freezing operations.

### R4 - Recovery

The loaded composition owns all Leaf instances, shared Resources, endpoint
state, and completion tokens. Controlled rebuild destroys the old materialized
world, advances the epoch, and rematerializes only from the frozen artifact.
Actual removal excludes the removed LUID. Same-topology WARP retry remains
AwaitingAdapter and execution is rejected until an eligible adapter exists.

### R5 - Final qualification

The canonical runner checks architecture, script contracts, source inventory,
Debug and Release evidence, positive and negative authority tests, WARP
scenarios, controlled and actual recovery, fresh-process determinism, and all
existing Level 1-3 regressions.

### Final Integration Freeze

`SemanticGpuEngine4.sln` contains every repository C++ project exactly once.
All canonical R1-R5 runners build or consume this main solution; stage-specific
solutions are non-authoritative historical views. The integration runner builds
the main solution in Debug and Release before executing the complete R1-R5 and
Foundation qualification chain.

## Final banner

Only a zero exit status and the following banner constitute repository-level
Level 4 v1 completion:

```text
SGE4 LEVEL 4 V1 CANONICAL FINAL INTEGRATION FREEZE PASSED
```

## Validation honesty

Repository authoring or successful static inspection is not the final proof.
The Windows MSVC and D3D12 WARP run is authoritative for runtime qualification.
An intermittent WARP device-removal observation remains under stability watch;
it is not hidden by automatic retry and does not count as a pass unless the
complete final runner exits successfully.
