# SGE4 Level 4 v1 Canonical Proof Ledger

This ledger is monotonic. Implementations, APIs, project boundaries, and binary
formats may be reconstructed. An invariant may be removed only by a recorded
constitutional change that explains why the old theorem is no longer required.

## ABI authority

- ABI-001: Composition-visible access is derived from a validated Leaf and is
  never authored as ReadOnly or WriteOnly by the composition author.
- ABI-002: Every required composition endpoint is connected exactly once.
- ABI-003: Endpoint kind, format, minimum bytes, incoming state, outgoing state,
  and synchronization are checked against the embedded executable Leaf.
- ABI-004: Corrupt or inconsistent Leaf interface data is rejected before any
  D3D12 object is created.

## Contract authority

- CONTRACT-001: A Resource Flow has exactly one producer when internal or output.
- CONTRACT-002: An internal Resource Flow has at least one consumer.
- CONTRACT-003: A composition input has no producer and at least one consumer.
- CONTRACT-004: A composition output has exactly one producer and no consumers.
- CONTRACT-005: Multiple writers, missing endpoints, duplicate endpoints, and
  incompatible Buffer shapes are rejected.
- CONTRACT-006: Zero or one presenting Leaf is accepted; two presenters are
  rejected.
- CONTRACT-007: Contract identity is independent of author declaration order.

## Planning and verification authority

- PLAN-001: The planner proposes allocation, schedule, endpoint binding, state
  handoff, signal, wait, and recovery decisions.
- PLAN-002: A cyclic same-frame package graph is rejected.
- PLAN-003: The canonical schedule is deterministic.
- VERIFY-001: Raw planning data is neither executable nor freezable.
- VERIFY-002: Only the independent verifier constructs the verified plan type.
- VERIFY-003: The verifier re-derives all represented decisions from the
  contract and does not share private validation helpers with the planner.
- VERIFY-004: Mutating allocation, schedule, binding, state, signal, wait,
  recovery identity, or contract identity is rejected even after recomputing a
  raw-plan digest.

## Frozen artifact authority

- ARTIFACT-001: The frozen composition is self-contained and requires no Source,
  Compiler, Planner, or Linker proposal code to load.
- ARTIFACT-002: The reader validates container integrity, canonical structure,
  embedded Leaves, verified decisions, certificate, and outer digest before
  materialization.
- ARTIFACT-003: Corruption of any execution-affecting field is rejected.
- ARTIFACT-004: Within the canonical implementation, fresh Debug process A,
  fresh Debug process B, and Release emit byte-identical artifacts.

## Runtime authority

- RUNTIME-001: All Leaves execute in one DeviceDomain and one current epoch.
- RUNTIME-002: One internal Resource Flow owns one composition Buffer identity.
- RUNTIME-003: Runtime follows the frozen schedule and does not rediscover or
  optimize package order, endpoint compatibility, state handoffs, or waits.
- RUNTIME-004: Resource and completion token ownership is checked against the
  DeviceDomain, epoch, and Resource Flow identity.
- RUNTIME-005: Independent, linear, fan-out, multi-input, diamond, headless, and
  single-presenter compositions execute on WARP.
- RUNTIME-006: Repeated fan-out frames retire every reader before the next writer
  reuses the shared Buffer.
- RUNTIME-007: Dynamic bindings are complete, unique, and Leaf-local.

## Recovery authority

- RECOVERY-001: The entire composition is one recovery unit.
- RECOVERY-002: Recovery destroys Leaf instances, shared Buffers, completion
  tokens, endpoint tokens, and tracked state before rebuilding.
- RECOVERY-003: Controlled rebuild advances the device epoch and rematerializes
  the complete composition only from the frozen artifact.
- RECOVERY-004: Old Resource and token objects are rejected after epoch change.
- RECOVERY-005: Actual RemoveDevice records and excludes the removed adapter
  LUID for the lifetime of the DeviceDomain.
- RECOVERY-006: Retry rechecks topology but never clears removed-LUID exclusion.
- RECOVERY-007: In WARP-only same-topology qualification, Retry remains
  AwaitingAdapter.
- RECOVERY-008: AwaitingAdapter owns no active Leaves or shared Resources and
  rejects Submit and Readback.
- RECOVERY-009: A fresh process independently rematerializes the same frozen
  composition.

## Preserved foundation

- REGRESSION-001: Schema 17 and Runtime 17 remain the executable Leaf ABI.
- REGRESSION-002: The 54 accepted Level 1-3 Leaf corpus and its qualification
  remain unchanged.
- ARCH-001: Backend has no dependency on Semantic, planning, compiler, or
  frontend projects.
- ARCH-002: Script contracts and the source inventory are qualification gates.

## Final integration authority

- INTEGRATION-001: Every repository C++ project is registered exactly once in
  `SemanticGpuEngine4-5.sln`.
- INTEGRATION-002: Debug and Release qualification builds use
  `SemanticGpuEngine4-5.sln` as the sole canonical solution.
- INTEGRATION-003: R1, R2, and R3-R5 canonical runners do not reference a
  stage-specific solution.
- INTEGRATION-004: The final integration runner executes the full R1-R5,
  Architecture, source inventory, and Stage 0D Foundation qualification chain.

## Completion rule

Level 4 v1 Canonical is complete only when every invariant above is mapped to
at least one positive test or negative gate, Debug and Release pass on Windows
with D3D12 WARP, actual RemoveDevice qualification passes, all Level 1-3
regressions pass, the main solution integration checks pass, and the final
runner prints:

```text
SGE4 LEVEL 4 V1 CANONICAL FINAL INTEGRATION FREEZE PASSED
```
